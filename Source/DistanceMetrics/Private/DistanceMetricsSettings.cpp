#include "DistanceMetricsSettings.h"

#include "DistanceMetricsRenderingComponent.h"
#include "DistanceMetricsSubsystem.h"

UDistanceMetricsSettings::UDistanceMetricsSettings(const FObjectInitializer& Initializer): Super(Initializer)
{
	ComponentClass = UDistanceMetricsRenderingComponent::StaticClass();
}

void UDistanceMetricsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		if (UDistanceMetricsSubsystem* Subsystem = GEditor->GetEditorSubsystem<UDistanceMetricsSubsystem>())
        {
        	Subsystem->ReloadSettings();
        }
	}
}

FColor UDistanceMetricsSettings::GetLabelColor(FColor PolygonColor) const
{
	return bUsePolygonColorForDistanceLabels ? PolygonColor : LabelColor;
}


FColor UDistanceMetricsSettings::GetDistanceColor(float Distance, EDistanceProjectionMode ProjectionMode) const
{
	const UDistanceMetricsSettings& Settings = Get();
	const TArray<FFloatInterval>& DistanceArray = [&Settings](EDistanceProjectionMode Mode)
	{
		switch (Mode)
		{
		case EDistanceProjectionMode::DistanceXY:
			return Settings.ValidDistanceXY;
		case EDistanceProjectionMode::DistanceZ:
			return Settings.ValidDistanceZ;
		case EDistanceProjectionMode::Distance3D:
			return Settings.ValidDistance3D;
		default:
			checkNoEntry();
			break;
		}

		static TArray<FFloatInterval> Invalid;
		return Invalid;
	}(ProjectionMode);

	for (const FFloatInterval& DistanceRange: DistanceArray)
	{
		if (DistanceRange.Contains(Distance))
		{
			return Settings.ValidDistanceColor;
		}
	}

	// color with a zero opacity
	return Settings.InvalidDistanceColor;
}
