#pragma once

#include "CoreMinimal.h"

#include "DistanceMetricsSettings.generated.h"

class UDistanceMetricsRenderingComponent;

UENUM()
enum class EDistanceProjectionMode: uint8
{
	DistanceXY,
	DistanceZ,
	Distance3D,
};

UCLASS(Config = Editor, DefaultConfig, DisplayName = "Distance Metrics")
class DISTANCEMETRICS_API UDistanceMetricsSettings: public UDeveloperSettings
{
	GENERATED_BODY()
public:

	UDistanceMetricsSettings(const FObjectInitializer& Initializer);

	FORCEINLINE static const UDistanceMetricsSettings& Get()
	{
		return *GetDefault<UDistanceMetricsSettings>();
	}

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/** convert distance to color based on projection mode */
	FColor GetDistanceColor(float Distance, EDistanceProjectionMode ProjectionMode) const;
	/** convert polygon color to label color based on settings */
	FColor GetLabelColor(FColor PolygonColor) const;

	/** rendering component to that implements distance metrics tool vis */
	UPROPERTY(Config, EditAnywhere, Category = "Navigation", meta = (Validate))
	TSubclassOf<UDistanceMetricsRenderingComponent> ComponentClass;

	/** rendering material used for dynamic mesh visualization */
	UPROPERTY(Config, EditAnywhere, Category = "Navigation", meta = (Validate))
	TSoftObjectPtr<UMaterialInterface> RenderingMaterial;

	UPROPERTY(Config, EditAnywhere, Category = "Navigation")
	FIntPoint TileQueryExtent{5, 5};

	UPROPERTY(Config, EditAnywhere, Category = "Navigation")
	EDistanceProjectionMode DefaultProjectionMode = EDistanceProjectionMode::DistanceXY;

	UPROPERTY(Config, EditAnywhere, Category = "Navigation")
	TArray<FFloatInterval> ValidDistanceXY;

	UPROPERTY(Config, EditAnywhere, Category = "Navigation")
	TArray<FFloatInterval> ValidDistanceZ;

	UPROPERTY(Config, EditAnywhere, Category = "Navigation")
	TArray<FFloatInterval> ValidDistance3D;

	UPROPERTY(Config, EditAnywhere, Category = "Navigation")
	FVector ProjectExtent{10.0};
	
	UPROPERTY(Config, EditAnywhere, Category = "Navigation|Display")
	FVector MeshDrawOffset = FVector{0.0, 0.0, 15.0};

	UPROPERTY(Config, EditAnywhere, Category = "Navigation|Display")
	FVector TextDrawOffset = FVector{0.0, 0.0, 20.0};

	/** magic number. DO NOT TOUCH */
	UPROPERTY(Config, EditAnywhere, Category = "Navigation|Display")
	float DefaultDistanceXYDecrease = 26.f;
	
	UPROPERTY(Config, EditAnywhere, Category = "Navigation|Display")
	FColor ValidDistanceColor{26, 255, 0, 164};

	UPROPERTY(Config, EditAnywhere, Category = "Navigation|Display")
	FColor InvalidDistanceColor{180, 0, 0, 164};

	UPROPERTY(Config, EditAnywhere, Category = "Navigation|Display")
	FColor UnusedColor{128, 128, 128, 164};
	
	UPROPERTY(Config, EditAnywhere, Category = "Navigation|Display")
	FColor PolyEdgeColor = FColor::Black;

	UPROPERTY(Config, EditAnywhere, Category = "Navigation|Display")
	FColor SelectedPolyEdgeColor = FColor::Yellow;

	UPROPERTY(Config, EditAnywhere, Category = "Navigation|Display")
	FColor LabelColor = FColor::Black;

	UPROPERTY(Config, EditAnywhere, Category = "Navigation|Display")
	double MaxLabelDrawDistance = 2000.0;

	UPROPERTY(Config, EditAnywhere, Category = "Navigation|Display")
	uint8 bUsePolygonColorForDistanceLabels : 1 = false;
	
	UPROPERTY(Config, EditAnywhere, Category = "Navigation|Display")
	uint8 bDrawDistanceLabelsWithoutSelectedPolygons : 1 = false;

	UPROPERTY(Config, EditAnywhere, Category = "Navigation|Display")
	uint8 bDrawDistanceLines : 1 = false;

	UPROPERTY(Config, EditAnywhere, Category = "Navigation|Display")
	uint8 bDrawDistancePoints : 1 = true;

	UPROPERTY(Config, EditAnywhere, Category = "Navigation|Display")
	uint8 bDrawPolyVertices : 1 = true;

	UPROPERTY(Config, EditAnywhere, Category = "Navigation|Display")
	uint8 bDrawPolyEdges : 1 = true;

};
