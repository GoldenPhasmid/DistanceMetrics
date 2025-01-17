#pragma once

#include "CoreMinimal.h"
#include "Debug/DebugDrawComponent.h"
#include "AI/Navigation/NavigationTypes.h"
#include "NavMesh/RecastNavMesh.h"

#include "DistanceMetricsRenderingComponent.generated.h"

struct FNavPoly;
class ARecastNavMesh;
class UDistanceMetricsSubsystem;

/**
 * FPolygon
 * Represents a single navigation data polygon
 */
struct DISTANCEMETRICS_API FPolygon
{
	FPolygon() = default;
	FPolygon(const FNavPoly& InPoly)
		: Poly(InPoly)
	{}

	FORCEINLINE NavNodeRef GetRef() const { return Poly.Ref; }
	FORCEINLINE FVector GetCenter() const { return Poly.Center; }
		
	FNavPoly Poly;
	FColor Color;
	int32 VertexStart	= INDEX_NONE;
	int32 VertexEnd		= INDEX_NONE;
	int32 IndexStart	= INDEX_NONE;
	int32 IndexEnd		= INDEX_NONE;
	int32 TriangleStart = INDEX_NONE;
	int32 TriangleEnd	= INDEX_NONE;
};

/**
 * DistanceMetricsRenderingComponent
 * Controls visualization and updates for metrics tool
 */
UCLASS(ClassGroup = Debug)
class DISTANCEMETRICS_API UDistanceMetricsRenderingComponent: public UDebugDrawComponent
{
	GENERATED_BODY()
public:

	UDistanceMetricsRenderingComponent(const FObjectInitializer& Initializer);

	//~Begin PrimitiveComponent interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual FDebugRenderSceneProxy* CreateDebugSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform &LocalToWorld) const override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const override;
	virtual int32 GetNumMaterials() const override;
	//~End PrimitiveComponent interface

	/** @return navigation query filter that should be used for navigation projection queries */
	virtual FSharedConstNavQueryFilter GetNavigationQueryFilter() const;
	/** filter a given array of polygons */
	virtual void GetEnabledPolygons(const TArray<FPolygon>& Polygons, TArray<uint32>& OutIndices) const {}
	/** Request drawing update */
	virtual void RequestDrawingUpdate();
	
	FORCEINLINE ARecastNavMesh* GetNavMesh() const { return NavMesh; }
	FORCEINLINE FVector GetProjectedLocation() const { return LastNavMeshLocation.Location; }
	FORCEINLINE NavNodeRef GetProjectedNode() const { return LastNavMeshLocation.NodeRef; }

	/** Update polygon selection based on polygon under cursor, optionally clears existing selection */
	void UpdatePolygonSelection(bool bClearSelection) const;
	
protected:

	UFUNCTION()
	void HandleNavigationGenerationFinished(ANavigationData* NavData);
	
	void UpdateWorldLocation(const FVector& WorldLocation);

	bool IsDrawingEnabled() const;
	
	UPROPERTY(Transient)
	ARecastNavMesh* NavMesh = nullptr;

	UPROPERTY(Transient)
	UDistanceMetricsSubsystem* Subsystem = nullptr;

	FNavLocation LastNavMeshLocation;
};
