#include "DistanceMetricsSubsystem.h"

#include "DistanceMetrics.h"
#include "DistanceMetricsRenderingComponent.h"
#include "DistanceMetricsSettings.h"
#include "LevelEditor.h"
#include "NavigationSystem.h"
#include "SLevelViewport.h"
#include "AI/NavigationSystemBase.h"
#include "Kismet/GameplayStatics.h"
#include "Slate/SceneViewport.h"

FAutoConsoleCommand ClearSelection(
	TEXT("DistanceMetrics.ClearSelection"),
	TEXT(""),
	FConsoleCommandDelegate::CreateLambda([]
	{
		UDistanceMetricsSubsystem::Get()->ClearPolygonSelection();
	})
);

void UDistanceMetricsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	UNavigationSystemBase::OnNavigationInitDoneStaticDelegate().AddUObject(this, &ThisClass::NavigationSystemInitDone);

	ReloadSettings();
}

void UDistanceMetricsSubsystem::Deinitialize()
{
	UNavigationSystemBase::OnNavigationInitDoneStaticDelegate().RemoveAll(this);
	
	Super::Deinitialize();
}

void UDistanceMetricsSubsystem::ReloadSettings()
{
	auto& Settings = UDistanceMetricsSettings::Get();
	ProjectionMode = Settings.DefaultProjectionMode;

	RequestDrawingUpdate();
}

void UDistanceMetricsSubsystem::RequestDrawingUpdate()
{
	for (const TWeakObjectPtr<UDistanceMetricsRenderingComponent> Component: RenderingComponents)
	{
		if (Component.IsValid())
		{
			Component->RequestDrawingUpdate();
		}
	}
}

void UDistanceMetricsSubsystem::SetDistanceProjectionMode(EDistanceProjectionMode NewProjectionMode)
{
	ProjectionMode = NewProjectionMode;
	RequestDrawingUpdate();
}

void UDistanceMetricsSubsystem::SetDistanceMetricsEnabled(bool bEnabled)
{
	bEnabledByUser = bEnabled;
	RequestDrawingUpdate();
}

bool UDistanceMetricsSubsystem::DeprojectCursorPositionToWorld(const UWorld* World, FHitResult& OutHit) const
{
	return DeprojectScreenLocationToWorld(World, true, OutHit);
}

bool UDistanceMetricsSubsystem::DeprojectScreenCenterToWorld(const UWorld* World, FHitResult& OutHit) const
{
	return DeprojectScreenLocationToWorld(World, false, OutHit);
}

void UDistanceMetricsSubsystem::UpdatePolygonSelection(NavNodeRef Polygon, bool bClearSelection)
{
	if (bClearSelection)
	{
		SelectedPolygons.Empty();
		SelectedPolygons.Add(Polygon);
		RequestDrawingUpdate();
		
		return;
	}
	
	if (const int32 Index = SelectedPolygons.IndexOfByKey(Polygon); Index != INDEX_NONE)
	{
		SelectedPolygons.RemoveAtSwap(Index);
	}
	else
	{
		SelectedPolygons.Add(Polygon);
	}
	RequestDrawingUpdate();
}

void UDistanceMetricsSubsystem::ClearPolygonSelection()
{
	SelectedPolygons.Empty(4);
	RequestDrawingUpdate();
}

TSharedPtr<SLevelViewport> UDistanceMetricsSubsystem::GetGlobalLevelViewport() const
{
	FLevelEditorModule& LevelEditor = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));

	TSharedPtr<IAssetViewport> ActiveLevelViewport = LevelEditor.GetFirstActiveViewport();
	if (!ActiveLevelViewport.IsValid())
	{
		return nullptr;
	}

	TSharedPtr<SLevelViewport> LevelViewport = StaticCastSharedPtr<SLevelViewport>(ActiveLevelViewport);
	check(LevelViewport.IsValid());

	return LevelViewport;
}

bool UDistanceMetricsSubsystem::DeprojectScreenLocationToWorld(const UWorld* World, bool bUseCursor, FHitResult& OutHit) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UDistanceMetricsSubsystem_DeprojectScreenLocationToWorld);

	auto GetScreenPosition = [bUseCursor](const FSceneViewport* SceneViewport) -> FVector2D
	{
		if (bUseCursor)
		{
			return FVector2D(SceneViewport->GetMouseX(), SceneViewport->GetMouseY());
		}
		
		const FVector2D ViewportSize{SceneViewport->GetSizeXY()};
		return ViewportSize * 0.5;
	};

	FVector Location{}, Direction{};
	if (World->IsGameWorld())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_Game);
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (UGameViewportClient* ViewportClient = World->GetGameViewport())
			{
				if (IsEnabled(ViewportClient->EngineShowFlags) == false)
				{
					return false;
				}
				
				const FSceneViewport* SceneViewport = ViewportClient->GetGameViewport();
				check(SceneViewport);
				
				UGameplayStatics::DeprojectScreenToWorld(PC, GetScreenPosition(SceneViewport), Location, Direction);
				return true;
			}
		}
	}
	else
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_Editor);

		TSharedPtr<SLevelViewport> LevelViewport = GetGlobalLevelViewport();
		if (!LevelViewport.IsValid())
		{
			return false;
		}
		
		TSharedPtr<FSceneViewport> SceneViewport = LevelViewport->GetSceneViewport();
		check(SceneViewport.IsValid());

		TSharedPtr<FEditorViewportClient> ViewportClient = LevelViewport->GetViewportClient();
		check(ViewportClient.IsValid());

		if (IsEnabled(ViewportClient->EngineShowFlags) == false)
		{
			return false;
		}

		FSceneViewFamilyContext ViewFamily{FSceneViewFamily::ConstructionValues{
			LevelViewport->GetActiveViewport(),
			ViewportClient->GetScene(),
			ViewportClient->EngineShowFlags
		}.SetRealtimeUpdate(LevelViewport->IsRealtime())
		};
		const FSceneView* SceneView = ViewportClient->CalcSceneView(&ViewFamily);
		
		SceneView->DeprojectFVector2D(GetScreenPosition(SceneViewport.Get()), Location, Direction);
	}

	FHitResult Hit{};
	constexpr double ForwardOffset = 50.0;
	if (World->LineTraceSingleByChannel(Hit, Location + Direction * ForwardOffset, Location + Direction * HALF_WORLD_MAX, ECC_WorldStatic))
	{
		OutHit = Hit;
		return true;
	}
	
	return false;
}

bool UDistanceMetricsSubsystem::IsEnabled(const FEngineShowFlags& ShowFlags) const
{
	return bEnabledByUser || ShowFlags.GetSingleFlag(static_cast<uint32>(FEngineShowFlags::FindIndexByName(*UE::DistanceMetrics::ModuleName)));
}

bool UDistanceMetricsSubsystem::IsEnabledForWorld(const UWorld* World) const
{
	if (World != nullptr)
	{
		if (World->IsGameWorld())
		{
			if (const UGameViewportClient* ViewportClient = World->GetGameViewport())
			{
				return IsEnabled(ViewportClient->EngineShowFlags);
			}
		}
		else
		{
			TSharedPtr<SLevelViewport> LevelViewport = GetGlobalLevelViewport();
			if (!LevelViewport.IsValid())
			{
				return false;
			}
			
			TSharedPtr<FEditorViewportClient> ViewportClient = LevelViewport->GetViewportClient();
			check(ViewportClient.IsValid());

			return IsEnabled(ViewportClient->EngineShowFlags);
		}
	}

	return false;
}

void UDistanceMetricsSubsystem::NavigationSystemInitDone(const UNavigationSystemBase& NavigationSystem)
{
	const UNavigationSystemV1* NavSys = CastChecked<UNavigationSystemV1>(&NavigationSystem);
	if (NavSys->GetRunMode() == FNavigationSystemRunMode::EditorMode)
	{
		for (ANavigationData* NavData: NavSys->NavDataSet)
		{
			AddMetricsComponent(NavData);
		}
	
		const_cast<UNavigationSystemV1*>(NavSys)->OnNavDataRegisteredEvent.AddDynamic(this, &ThisClass::AddMetricsComponent);
	}
}

void UDistanceMetricsSubsystem::AddMetricsComponent(ANavigationData* NavData)
{
	TSubclassOf<UDistanceMetricsRenderingComponent> ComponentClass = UDistanceMetricsSettings::Get().ComponentClass;
	UDistanceMetricsRenderingComponent* RenderingComponent = NavData->FindComponentByClass<UDistanceMetricsRenderingComponent>();
	if (RenderingComponent != nullptr && RenderingComponent->GetClass() == ComponentClass)
	{
		RenderingComponents.Add(RenderingComponent);
	}
	else
	{
		if (RenderingComponent != nullptr)
		{
			RenderingComponent->DestroyComponent();
		}
		
		UDistanceMetricsRenderingComponent* Component = NewObject<UDistanceMetricsRenderingComponent>(NavData, ComponentClass);
		Component->RegisterComponentWithWorld(NavData->GetWorld());

		RenderingComponents.Add(Component);
	}
}
