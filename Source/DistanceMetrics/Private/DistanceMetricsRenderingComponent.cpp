#include "DistanceMetricsRenderingComponent.h"

#include "AITypes.h"
#include "NavMesh/RecastNavMesh.h"
#include "DistanceMetricsSceneProxy.h"
#include "DistanceMetricsSettings.h"
#include "DistanceMetricsSubsystem.h"
#include "NavigationSystem.h"
#include "Selection.h"
#include "AI/NavigationSystemBase.h"

bool GDrawProjection = false;
FAutoConsoleVariableRef CVarDrawProjection(
	TEXT("DistanceMetrics.DrawProjection"),
	GDrawProjection,
	TEXT("Draws projection debug spheres")
	TEXT("red	- screen center position to world location")
	TEXT("blue	- cursor position to world location")
	TEXT("green - cursor position project to navigation")
);

UDistanceMetricsRenderingComponent::UDistanceMetricsRenderingComponent(const FObjectInitializer& Initializer): Super(Initializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bTickInEditor = true;
	bIsEditorOnly = true;
}

void UDistanceMetricsRenderingComponent::OnRegister()
{
	Super::OnRegister();
	
	NavMesh = GetOwner<ARecastNavMesh>();
	Subsystem = GEditor->GetEditorSubsystem<UDistanceMetricsSubsystem>();
	
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		NavSys->OnNavigationGenerationFinishedDelegate.AddDynamic(this, &ThisClass::HandleNavigationGenerationFinished);
	}
}

void UDistanceMetricsRenderingComponent::OnUnregister()
{
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		NavSys->OnNavigationGenerationFinishedDelegate.RemoveAll(this);
	}

	NavMesh = nullptr;
	Subsystem = nullptr;
	
	Super::OnUnregister();
}

void UDistanceMetricsRenderingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	QUICK_SCOPE_CYCLE_COUNTER(STAT_DistanceMetricsComponent_Tick);
	
	FHitResult Hit{};
	if (Subsystem->DeprojectScreenCenterToWorld(GetWorld(), Hit))
	{
		if (GDrawProjection)
		{
			DrawDebugSphere(GetWorld(), Hit.Location, 20, 20, FColor::Red, false, 0.f);
		}
		UpdateWorldLocation(Hit.Location);
	}
}


FDebugRenderSceneProxy* UDistanceMetricsRenderingComponent::CreateDebugSceneProxy()
{
	FDistanceMetricsSceneProxy* NewSceneProxy = nullptr;
	
	if (IsDrawingEnabled())
	{
		FDistanceMetricsProxyData ProxyData{*this};
		NewSceneProxy = new FDistanceMetricsSceneProxy{this, ProxyData};

		GetDebugDrawDelegateHelper().InitDelegateHelper(NewSceneProxy);
	}
	
	return NewSceneProxy;
}

void UDistanceMetricsRenderingComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	OutMaterials.Add(UDistanceMetricsSettings::Get().RenderingMaterial.LoadSynchronous());
}

int32 UDistanceMetricsRenderingComponent::GetNumMaterials() const
{
	return 1;
}

FSharedConstNavQueryFilter UDistanceMetricsRenderingComponent::GetNavigationQueryFilter() const
{
	return NavMesh->GetDefaultQueryFilter();
}

FBoxSphereBounds UDistanceMetricsRenderingComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (NavMesh != nullptr)
	{
		return FBoxSphereBounds{NavMesh->GetNavMeshBounds()};
	}

	return FBoxSphereBounds{ForceInit};
}

void UDistanceMetricsRenderingComponent::UpdatePolygonSelection(bool bClearSelection) const
{
	check(NavMesh);
	
	FHitResult Hit{};
	if (Subsystem->DeprojectCursorPositionToWorld(GetWorld(), Hit))
	{
		if (GDrawProjection)
		{
			DrawDebugSphere(GetWorld(), Hit.Location, 20, 35, FColor::Blue, false, 2.f);
		}
		
		FNavLocation NewLocation{};
		if (NavMesh->ProjectPoint(Hit.Location, NewLocation, UDistanceMetricsSettings::Get().ProjectExtent, GetNavigationQueryFilter(), nullptr))
		{
			if (NewLocation.HasNodeRef())
			{
				if (GDrawProjection)
				{
					DrawDebugSphere(GetWorld(), NewLocation, 60, 15, FColor::Green, false, 2.f);
				}

				Subsystem->UpdatePolygonSelection(NewLocation.NodeRef, bClearSelection);

				if (USelection* Selection = GEditor->GetSelectedActors())
				{
					Selection->Modify();
					Selection->Deselect(NavMesh);
				}
			}
		}
	}
}

void UDistanceMetricsRenderingComponent::HandleNavigationGenerationFinished(ANavigationData* NavData)
{
	if (NavMesh && NavMesh == NavData)
	{
		RequestDrawingUpdate();
	}
}

void UDistanceMetricsRenderingComponent::UpdateWorldLocation(const FVector& WorldLocation)
{
	if (WorldLocation == FAISystem::InvalidLocation)
	{
		return;
	}

	check(NavMesh);
	const auto& Settings = UDistanceMetricsSettings::Get();
	
	FNavLocation NewLocation{};
	if (NavMesh->ProjectPoint(WorldLocation, NewLocation, Settings.ProjectExtent, GetNavigationQueryFilter(), nullptr))
	{
		if (NewLocation.HasNodeRef())
		{
			if (NewLocation.NodeRef != LastNavMeshLocation.NodeRef)
			{
				LastNavMeshLocation = NewLocation;
				RequestDrawingUpdate();
			}
		}
	}
}

void UDistanceMetricsRenderingComponent::RequestDrawingUpdate()
{
	MarkRenderStateDirty();
}

bool UDistanceMetricsRenderingComponent::IsDrawingEnabled() const
{
	return LastNavMeshLocation.HasNodeRef();
}
