#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "DebugRenderSceneProxy.h"
#include "NavMesh/NavMeshRenderingComponent.h"
#include "NavMesh/RecastNavMesh.h"

class UDistanceMetricsSettings;
class UDistanceMetricsSubsystem;
enum class EDistanceProjectionMode : uint8;
struct FNavPoly;
class ARecastNavMesh;
class UDistanceMetricsRenderingComponent;
class UMaterialInterface;
struct FPolygon;

/**
 *
 */
struct FDistanceMetricsProxyData: public TSharedFromThis<FDistanceMetricsProxyData, ESPMode::ThreadSafe>
{
	struct FPolygonDistance
	{
		NavNodeRef PolyRefs[2];
		FVector Points[2];
		FVector Location;
		FColor Color;
		double Distance = FLT_MAX;

		void UpdateDistance(EDistanceProjectionMode ProjectionMode, float XYDecrease);
	};
	
	struct FDebugPoint
	{
		FDebugPoint(const FVector& InPosition, const FColor& InColor, const float InSize) : Position(InPosition), Color(InColor), Size(InSize) {}
		FVector Position;
		FColor Color;
		float Size = 0.f;
	};
	
	FDistanceMetricsProxyData(const UDistanceMetricsRenderingComponent& Component);

	/**
	 * Find a shortest distance between two polygons.
	 * Each polygon is split into triangles and the shortest distance between each tri pair is found
	 */
	FPolygonDistance CalculateDistance(const FPolygon& Lhs, const FPolygon& Rhs) const;
	/**
	 * Find a shortest distance between point and polygon.
	 * Polygon is split into triangles and the shortest distance between point and each tri is found
	 */
	FPolygonDistance CalculateDistance(const FNavLocation& Location, const FPolygon& Polygon) const;

	void DrawPolygon(const FPolygon& Polygon, const FColor& EdgeColor, float Thickness = 0);
	void AddDebugLabel(const FString& Text, const FVector& Location, FColor Color);
	void AddDebugLine(const FVector& Start, const FVector& End, FColor Color, float Thickness = 0);
	void AddDebugPoint(const FVector& Point);
	void AddMeshVertex(FDebugRenderSceneProxy::FMesh& DebugMesh, const FVector& VertexPosition, FColor VertexColor);
	
	TRefCountPtr<HHitProxy> HitProxy = nullptr;
	TArray<FDebugRenderSceneProxy::FMesh> DebugMeshes;
	TArray<FDebugRenderSceneProxy::FDebugLine> DebugLines;
	TArray<FDebugRenderSceneProxy::FText3d> DebugLabels;
	TArray<FDebugPoint> DebugPoints;

	TArray<FPolygon> Polygons;
	TArray<FPolygonDistance> Distances;
	TArray<FVector> Vertices;
	TArray<uint32> Indices;

	FVector TextDrawOffset;
	FVector MeshDrawOffset;
	float PolygonDistanceXYDecrease = 0.f;
	const UDistanceMetricsSubsystem& Subsystem;
	const UDistanceMetricsSettings& Settings;
};

/**
 *
 */
class FDistanceMetricsSceneProxy: public FDebugRenderSceneProxy, public FNoncopyable
{
public:
	FDistanceMetricsSceneProxy(const UPrimitiveComponent* InComponent, const FDistanceMetricsProxyData& InData);
	virtual ~FDistanceMetricsSceneProxy() override;

	//~Begin PrimitiveSceneProxy interface
	virtual SIZE_T GetTypeHash() const override;
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual uint32 GetMemoryFootprint() const override;
	//~End PrimitiveSceneProxy interface

	static bool LineInView(const FSceneView& View, const FVector& Start, const FVector& End, float DistanceSq);
	static bool PointInView(const FSceneView& View, const FVector& Point, float DistanceSq);

	FLocalVertexFactory VertexFactory;
	FStaticMeshVertexBuffers VertexBuffers;
	FDynamicMeshIndexBuffer32 IndexBuffer;
	
	TArray<FMeshBatchElement> MeshElements;
	UMaterialInterface* Material = nullptr;
	FMaterialRelevance MaterialRelevance;
	
	FDistanceMetricsProxyData ProxyData;
	float DrawDistanceSq = 5000.f * 5000.f;
};
