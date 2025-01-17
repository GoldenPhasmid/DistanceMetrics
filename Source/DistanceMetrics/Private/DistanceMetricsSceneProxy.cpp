#include "DistanceMetricsSceneProxy.h"

#include "DistanceMetricsRenderingComponent.h"
#include "DistanceMetrics.h"
#include "DistanceMetricsHitProxy.h"
#include "DistanceMetricsSettings.h"
#include "DistanceMetricsSubsystem.h"
#include "NavigationSystem.h"
#include "AI/NavigationSystemBase.h"
#include "Algo/IndexOf.h"
#include "Algo/RemoveIf.h"
#include "Distance/DistTriangle3Triangle3.h"

FDistanceMetricsProxyData::FDistanceMetricsProxyData(const UDistanceMetricsRenderingComponent& Component)
	: Subsystem(*GEditor->GetEditorSubsystem<UDistanceMetricsSubsystem>())
	, Settings(UDistanceMetricsSettings::Get())
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FDistanceMetricsProxyData);

	const ARecastNavMesh& NavMesh = *Component.GetNavMesh();
	
	HitProxy = new HDistanceMetricsHitProxy{&Component};

	PolygonDistanceXYDecrease = Settings.DefaultDistanceXYDecrease;
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(NavMesh.GetWorld()); NavSys && !NavSys->GetSupportedAgents().IsEmpty())
	{
		// find the smallest agent radius and use it to decrease XY distance between polygons
		// @todo: this loop does not account for disabled agents
		float Radius = FLT_MAX;
		for (auto& SupportedAgent: NavSys->GetSupportedAgents())
		{
			if (SupportedAgent.AgentRadius < Radius)
			{
				Radius = SupportedAgent.AgentRadius;
			}
		}
		PolygonDistanceXYDecrease += Radius * 2;
	}
	
	TextDrawOffset = Settings.TextDrawOffset;
	MeshDrawOffset = Settings.MeshDrawOffset;

	// get an initial tile from a project location
	FIntPoint StartingTileIndex{};
	NavMesh.GetNavMeshTileXY(Component.GetProjectedLocation(), StartingTileIndex.X, StartingTileIndex.Y);

	const FIntPoint TileExtent = Settings.TileQueryExtent;
	
	TArray<int32> TileIndices;
	TileIndices.Reserve((TileExtent.X + TileExtent.Y + 2) * 2);

	// gather tile indices around projected location to display
	for (int32 X = StartingTileIndex.X - TileExtent.X; X <= StartingTileIndex.X + TileExtent.X; ++X)
	{
		for (int32 Y = StartingTileIndex.Y - TileExtent.Y; Y <= StartingTileIndex.Y + TileExtent.Y; ++Y)
		{
			NavMesh.GetNavMeshTilesAt(X, Y, TileIndices);
		}
	}

	// get all polys in tiles
	TArray<FNavPoly> NavPolys;
	NavPolys.Reserve(TileIndices.Num() * 4);
	for (const int32 TileIndex: TileIndices)
	{
		NavMesh.GetPolysInTile(TileIndex, NavPolys);
	}

	// add selected polygons back to the pool
	TConstArrayView<NavNodeRef> SelectedNodeRefs = Subsystem.GetSelectedPolygons();
	
	TArray<int32> SelectedPolygonIndices;
	SelectedPolygonIndices.Reserve(SelectedNodeRefs.Num());

	// transform selected node refs into full fledged polygons
	// note that we can (probably) select nodes from different navmeshes, so we have to account for it
	for (NavNodeRef NodeRef: SelectedNodeRefs)
	{
		int32 RefIndex = Algo::IndexOfByPredicate(NavPolys, [NodeRef](const FNavPoly& Poly)
		{
			return Poly.Ref == NodeRef;
		});
		
		if (RefIndex == INDEX_NONE)
		{
			FNavPoly Poly{NodeRef, FVector::ZeroVector};
			if (NavMesh.GetPolyCenter(Poly.Ref, Poly.Center))
			{
				RefIndex = NavPolys.Add(Poly);
			}
		}

		if (RefIndex != INDEX_NONE)
		{
			SelectedPolygonIndices.Add(RefIndex);
		}
	}
	const int32 SelectedNodeCount = SelectedPolygonIndices.Num();
	
	Polygons.Empty(NavPolys.Num());
	// 4 vertices per poly, 3 triangles per poly on average
	Vertices.Empty(NavPolys.Num() * 4);
	Indices.Empty(NavPolys.Num() * 3);
	// for each polygon, extract info about vertices and triangles
	// Polygon data is stored in a contiguous array, while polygon stores indices to respective elements
	for (const FNavPoly& NavPoly: NavPolys)
	{
		TArray<FVector> TempVertices;
		if (!NavMesh.GetPolyVerts(NavPoly.Ref, TempVertices))
		{
			// navmesh has changed while we were doing processing proxy data, simply skip the polygon 
			continue;
		}
		check(TempVertices.Num() > 1);
		
		FPolygon& Polygon = Polygons.Emplace_GetRef(NavPoly);
		Polygon.VertexStart = Vertices.Num();
		Polygon.Color = Settings.UnusedColor;
		
		Vertices.Append(TempVertices);
		Polygon.VertexEnd = Vertices.Num();

		Polygon.TriangleStart = Indices.Num();
		for (int32 VertexIndex = Polygon.VertexStart + 2; VertexIndex < Polygon.VertexEnd; ++VertexIndex)
		{
			// triangle
			Indices.Add(Polygon.VertexStart);
			Indices.Add(VertexIndex);
			Indices.Add(VertexIndex - 1);
		}
		Polygon.TriangleEnd = Indices.Num();
	}
	
	// calculate distance array based on currently selected polygons
	if (SelectedNodeRefs.Num() > 1)
	{
		// more than 2 polygons are selected - find distance between a first selected to all other selected, others are disabled
		Distances.Empty(SelectedNodeRefs.Num());
		FPolygon& PivotPolygon = Polygons[SelectedPolygonIndices[0]];
		
		for (int32 Index = 0; Index < SelectedNodeCount; ++Index)
		{
			FPolygon& Polygon = Polygons[SelectedPolygonIndices[Index]];
			
			int32 Last = Distances.Add(CalculateDistance(PivotPolygon, Polygon));
			Polygon.Color = Distances[Last].Color;
		}
	}
	else if (!Polygons.IsEmpty())
	{
		FPolygon* PivotPolygon = nullptr;

		if (SelectedNodeRefs.Num() == 1)
		{
			PivotPolygon = &Polygons[SelectedPolygonIndices[0]];;
		}
		else
		{
			// zero polygons are selected - use a polygon under cursor as a selected polygon
			const int32 PivotIndex = Algo::IndexOfByPredicate(Polygons, [NodeRef=Component.GetProjectedNode()](const FPolygon& Polygon)
			{
				return Polygon.GetRef() == NodeRef;
			});
			check(PivotIndex != INDEX_NONE);
		
			PivotPolygon = &Polygons[PivotIndex];
		}
		check(PivotPolygon);

		// find out enabled polygons. Disabled polygons are drawn in a default 'Gray' disabled color
		TArray<uint32> EnabledPolygonIndices;
		Component.GetEnabledPolygons(Polygons, EnabledPolygonIndices);
		// selected polygons are always enabled
		EnabledPolygonIndices.Append(SelectedPolygonIndices);

		const int32 EnabledPolygonCount = EnabledPolygonIndices.Num();
		Distances.Empty(EnabledPolygonCount);

		// calculate distance between pivot polygon and enabled polygons
		for (int32 Index = 0; Index < EnabledPolygonCount; ++Index)
		{
			FPolygon& Polygon = Polygons[EnabledPolygonIndices[Index]];
			int32 Last = Distances.Add(CalculateDistance(*PivotPolygon, Polygon));
			Polygon.Color = Distances[Last].Color;
		}
	}

	// create a single debug mesh from all displayed polygons
	FDebugRenderSceneProxy::FMesh DebugMesh{};
	for (int32 PolyIndex = 0; PolyIndex < Polygons.Num(); ++PolyIndex)
	{
		const FPolygon& Polygon = Polygons[PolyIndex];
		for (int32 Index = Polygon.VertexStart; Index < Polygon.VertexEnd; ++Index)
		{
			AddMeshVertex(DebugMesh, Vertices[Index], Polygon.Color);
		}
	}
	DebugMesh.Indices.Append(Indices);
	DebugMeshes.Add(MoveTemp(DebugMesh));

	// always draw selected polygon edges
	if (SelectedNodeCount > 0)
	{
		constexpr float SelectedThickness = 4;
		const FColor SelectedEdgeColor = Settings.SelectedPolyEdgeColor;
		
		for (int32 Index = 0; Index < SelectedNodeCount; ++Index)
		{
			const FPolygon& Polygon = Polygons[SelectedPolygonIndices[Index]];
			DrawPolygon(Polygon, SelectedEdgeColor, SelectedThickness);
		}
	}

	// draw polygon edges
	if (Settings.bDrawPolyEdges)
	{
		constexpr float Thickness = 1;
		FColor EdgeColor = Settings.PolyEdgeColor;
		// draw polygon edges using debug lines
		for (const FPolygon& Polygon: Polygons)
		{
			DrawPolygon(Polygon, EdgeColor, Thickness);
		}
	}

	// draw polygon vertices
	if (Settings.bDrawPolyVertices)
	{
		// draw polygon vertices using debug points 
		for (const FPolygon& Polygon: Polygons)
		{
			for (int32 VertexIndex = Polygon.VertexStart; VertexIndex < Polygon.VertexEnd; ++VertexIndex)
			{
				AddDebugPoint(Vertices[VertexIndex]);
			}
		}
	}

	// draw distance labels
	if (SelectedPolygonIndices.Num() > 0 || Settings.bDrawDistanceLabelsWithoutSelectedPolygons)
	{
		for (const FPolygonDistance& Distance: Distances)
		{
			if (FMath::Abs(Distance.Distance) <= Settings.MaxLabelDrawDistance)
			{
				const FString Text = FString::SanitizeFloat(static_cast<int32>(Distance.Distance) / 100.0);
				AddDebugLabel(Text, Distance.Location, Settings.GetLabelColor(Distance.Color));
			}
		}
	}
	

	// draw distance points
	if (Settings.bDrawDistancePoints)
	{
		for (int32 Index = 0; Index < Distances.Num(); ++Index)
		{
			AddDebugPoint(Distances[Index].Points[0]);
			AddDebugPoint(Distances[Index].Points[1]);
		}
	}

	// draw distance lines
	if (Settings.bDrawDistanceLines)
	{
		for (int32 Index = 0; Index < Distances.Num(); ++Index)
		{
			AddDebugLine(Distances[Index].Points[0], Distances[Index].Points[1], Polygons[Index].Color);
		}
	}
}

void FDistanceMetricsProxyData::DrawPolygon(const FPolygon& Polygon, const FColor& EdgeColor, float Thickness)
{
	AddDebugLine(Vertices[Polygon.VertexStart], Vertices[Polygon.VertexEnd - 1], EdgeColor, Thickness);
	for (int32 VertexIndex = Polygon.VertexStart + 1; VertexIndex < Polygon.VertexEnd; ++VertexIndex)
	{
		AddDebugLine(Vertices[VertexIndex - 1], Vertices[VertexIndex], EdgeColor, Thickness);
	}
}

void FDistanceMetricsProxyData::AddDebugLabel(const FString& Text, const FVector& Location, FColor Color)
{
	DebugLabels.Add(FDebugRenderSceneProxy::FText3d{Text, Location + TextDrawOffset, Color});	
}

void FDistanceMetricsProxyData::AddDebugLine(const FVector& Start, const FVector& End, FColor Color, float Thickness)
{
	DebugLines.Add(FDebugRenderSceneProxy::FDebugLine{Start + TextDrawOffset, End + TextDrawOffset, Color, Thickness});
}

void FDistanceMetricsProxyData::AddDebugPoint(const FVector& Point)
{
	DebugPoints.Add(FDebugPoint{Point + TextDrawOffset, FColor::Black, 5.f});
}

void FDistanceMetricsProxyData::AddMeshVertex(FDebugRenderSceneProxy::FMesh& DebugMesh, const FVector& VertexPosition, FColor VertexColor)
{
	FDynamicMeshVertex Vertex{};
	Vertex.Position = FVector3f(VertexPosition + MeshDrawOffset);
	Vertex.TextureCoordinate[0] = FVector2f::ZeroVector;
	Vertex.TangentX = FVector::ForwardVector;
	Vertex.TangentZ = FVector::UpVector;
	Vertex.TangentZ.Vector.W = -128;
	Vertex.Color = VertexColor;

	DebugMesh.Vertices.Add(Vertex);
}

FDistanceMetricsProxyData::FPolygonDistance FDistanceMetricsProxyData::CalculateDistance(const FPolygon& Lhs, const FPolygon& Rhs) const
{
	FPolygonDistance Result{};
	Result.PolyRefs[0] = Lhs.GetRef();
	Result.PolyRefs[1] = Rhs.GetRef();
	Result.Location = Rhs.GetCenter();
	
	for (int32 Outer = Lhs.TriangleStart; Outer < Lhs.TriangleEnd; Outer += 3)
	{
		for (int32 Inner = Rhs.TriangleStart; Inner < Rhs.TriangleEnd; Inner += 3)
		{
			UE::Geometry::TTriangle3<FVector::FReal> LhsTri{Vertices[Indices[Outer]], Vertices[Indices[Outer + 1]], Vertices[Indices[Outer + 2]]};
			UE::Geometry::TTriangle3<FVector::FReal> RhsTri{Vertices[Indices[Inner]], Vertices[Indices[Inner + 1]], Vertices[Indices[Inner + 2]]};

			UE::Geometry::TDistTriangle3Triangle3<FVector::FReal> Processor{LhsTri, RhsTri};
			const FVector::FReal Distance = Processor.Get();
			if (Distance < Result.Distance)
			{
				Result.Distance = Distance;
				Result.Points[0] = Processor.TriangleClosest[0];
				Result.Points[1] = Processor.TriangleClosest[1];
			}
		}
	}

	Result.UpdateDistance(Subsystem.GetProjectionMode(), PolygonDistanceXYDecrease);
	Result.Color = Settings.GetDistanceColor(Result.Distance, Subsystem.GetProjectionMode());
	return Result;
}

FDistanceMetricsProxyData::FPolygonDistance FDistanceMetricsProxyData::CalculateDistance(const FNavLocation& Location, const FPolygon& Polygon) const
{
	FPolygonDistance Result{};
	Result.PolyRefs[0] = Location.NodeRef;
	Result.PolyRefs[1] = Polygon.GetRef();
	
	for (int32 Index = Polygon.TriangleStart; Index < Polygon.TriangleEnd; Index += 3)
	{
		UE::Geometry::TTriangle3<FVector::FReal> Tri{Vertices[Indices[Index]], Vertices[Indices[Index + 1]], Vertices[Indices[Index + 2]]};
		UE::Geometry::TDistPoint3Triangle3<FVector::FReal> Processor{Location.Location, Tri};
		const FVector::FReal Distance = Processor.Get();
		if (Distance < Result.Distance)
		{
			Result.Distance = Distance;
			Result.Points[0] = Location.Location;
			Result.Points[1] = Processor.Point;
		}
	}

	Result.UpdateDistance(Subsystem.GetProjectionMode(), PolygonDistanceXYDecrease);
	Result.Color = Settings.GetDistanceColor(Result.Distance, Subsystem.GetProjectionMode());
	return Result;
}

void FDistanceMetricsProxyData::FPolygonDistance::UpdateDistance(EDistanceProjectionMode ProjectionMode, float XYDecrease)
{
	Distance = [this, ProjectionMode]()
	{
		switch (ProjectionMode)
		{
		case EDistanceProjectionMode::DistanceXY:	return FVector::DistXY(Points[0], Points[1]);
		case EDistanceProjectionMode::DistanceZ:	return FMath::Abs(Points[1].Z - Points[0].Z) * (Points[1].Z > Points[0].Z ? 1 : -1);
		case EDistanceProjectionMode::Distance3D:	return FVector::Dist(Points[0], Points[1]);
		default: checkNoEntry(); break;
		}

		return 0.0;
	}();

	if (ProjectionMode != EDistanceProjectionMode::DistanceZ)
	{
		// distance for Z projection can be negative
		Distance = FMath::Max(Distance - XYDecrease, 0);
	}
}

FDistanceMetricsSceneProxy::FDistanceMetricsSceneProxy(const UPrimitiveComponent* InComponent, const FDistanceMetricsProxyData& InData)
	: FDebugRenderSceneProxy(InComponent)
	, VertexFactory(GetScene().GetFeatureLevel(), "FDistanceMetricsSceneProxy")
	, ProxyData(InData)
{
	check(IsInGameThread() || IsInParallelGameThread());
	QUICK_SCOPE_CYCLE_COUNTER(STAT_DistanceMetrics_FDistanceMetricsSceneProxy);

	Texts.Append(ProxyData.DebugLabels);

	DrawType = EDrawType::SolidAndWireMeshes;
	ViewFlagName = UE::DistanceMetrics::ModuleName;
	ViewFlagIndex = static_cast<uint32>(FEngineShowFlags::FindIndexByName(*ViewFlagName));
	
	Material = UDistanceMetricsSettings::Get().RenderingMaterial.LoadSynchronous();
	MaterialRelevance = Material->GetRelevance_Concurrent(GetScene().GetFeatureLevel());
	
	MeshElements.Reserve(ProxyData.DebugMeshes.Num());

	TArray<FDynamicMeshVertex> Vertices;
	for (const FDebugRenderSceneProxy::FMesh& DebugMesh: ProxyData.DebugMeshes)
	{
		FMeshBatchElement Element;
		Element.FirstIndex = IndexBuffer.Indices.Num();
		Element.NumPrimitives = FMath::FloorToInt(static_cast<float>(DebugMesh.Indices.Num()) / 3);
		Element.MinVertexIndex = Vertices.Num();
		Element.MaxVertexIndex = Element.MinVertexIndex + DebugMesh.Vertices.Num() - 1;
		Element.IndexBuffer = &IndexBuffer;
		MeshElements.Add(Element);

		const int32 VertexIndexOffset = Vertices.Num();
		Vertices.Append(DebugMesh.Vertices);

		if (VertexIndexOffset == 0)
		{
			IndexBuffer.Indices.Append(DebugMesh.Indices);
		}
		else
		{
			IndexBuffer.Indices.Reserve(IndexBuffer.Indices.Num() + DebugMesh.Indices.Num());
			for (const auto VertIndex : DebugMesh.Indices)
			{
				IndexBuffer.Indices.Add(VertIndex + VertexIndexOffset);
			}
		}
	}

	if (!Vertices.IsEmpty())
	{
		VertexBuffers.InitFromDynamicVertex(&VertexFactory, Vertices);
	}
	if (!IndexBuffer.Indices.IsEmpty())
	{
		BeginInitResource(&IndexBuffer);
	}
}

void FDistanceMetricsSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_DistanceMetricsSceneProxy_GetDynamicMeshElements);
	
	FDebugRenderSceneProxy::GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector);
	
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if ((VisibilityMap & (1 << ViewIndex)) == false)
		{
			continue;
		}
		
		const FSceneView* SceneView = Views[ViewIndex];
		if (!ProxyData.Subsystem.IsEnabled(SceneView->Family->EngineShowFlags))
		{
			continue;
		}
		
		// draw debug meshes
		for (int32 MeshIndex = 0; MeshIndex < MeshElements.Num(); ++MeshIndex)
		{
			if (MeshElements[MeshIndex].NumPrimitives == 0)
			{
				continue;
			}

			FMeshBatch& Mesh = Collector.AllocateMesh();
			Mesh.bWireframe = false;
            Mesh.VertexFactory = &VertexFactory;
			Mesh.BatchHitProxyId = ProxyData.HitProxy.IsValid() ? ProxyData.HitProxy->Id : FHitProxyId{};
            Mesh.MaterialRenderProxy = Material->GetRenderProxy();
            Mesh.ReverseCulling = !IsLocalToWorldDeterminantNegative();
            Mesh.Type = PT_TriangleList;
            Mesh.DepthPriorityGroup = SDPG_World;
            Mesh.bCanApplyViewModeOverrides = false;
			
			FMeshBatchElement& BatchElement = Mesh.Elements[0];
			BatchElement = MeshElements[MeshIndex];

			FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
			DynamicPrimitiveUniformBuffer.Set(Collector.GetRHICommandList(), FMatrix::Identity, FMatrix::Identity, GetBounds(), GetLocalBounds(), false, false, AlwaysHasVelocity());
			BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;
			
			Collector.AddMesh(ViewIndex, Mesh);
		}

		FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
		// draw debug points
		for (const FDistanceMetricsProxyData::FDebugPoint& Point: ProxyData.DebugPoints)
		{
			if (PointInView(*SceneView, Point.Position, DrawDistanceSq))
			{
				PDI->DrawPoint(Point.Position, Point.Color, Point.Size, SDPG_World);
			}
		}

		// draw debug lines (similar functionality to FDebugRenderSceneProxy::Lines
		PDI->AddReserveLines(SDPG_Foreground, ProxyData.DebugLines.Num(), false, true);
		for (const FDebugLine& Line: ProxyData.DebugLines)
		{
			if (LineInView(*SceneView, Line.Start, Line.End, DrawDistanceSq))
			{
				PDI->DrawLine(Line.Start, Line.End, Line.Color, SDPG_World, Line.Thickness, 0, true);;
			}
		}
	}
}

FPrimitiveViewRelevance FDistanceMetricsSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	const FEngineShowFlags& ShowFlags = View->Family->EngineShowFlags;
	
	const bool bShouldShow = ProxyData.Subsystem.IsEnabled(ShowFlags);
	FPrimitiveViewRelevance ViewRelevance{};
	ViewRelevance.bDrawRelevance = IsShown(View) && bShouldShow;
	ViewRelevance.bStaticRelevance = true;
	ViewRelevance.bEditorStaticSelectionRelevance = true;
	ViewRelevance.bDynamicRelevance = true;
	MaterialRelevance.SetPrimitiveViewRelevance(ViewRelevance);
	ViewRelevance.bOpaque = true;
	ViewRelevance.bNormalTranslucency = true;
	ViewRelevance.bSeparateTranslucency = true;

	return ViewRelevance;
}

FDistanceMetricsSceneProxy::~FDistanceMetricsSceneProxy()
{
	VertexBuffers.PositionVertexBuffer.ReleaseResource();
	VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
	VertexBuffers.ColorVertexBuffer.ReleaseResource();
	IndexBuffer.ReleaseResource();
	VertexFactory.ReleaseResource();
}

SIZE_T FDistanceMetricsSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

uint32 FDistanceMetricsSceneProxy::GetMemoryFootprint() const
{
	return FDebugRenderSceneProxy::GetMemoryFootprint();
}

bool FDistanceMetricsSceneProxy::LineInView(const FSceneView& View, const FVector& Start, const FVector& End, float DistanceSq)
{
	if (FVector::DistSquaredXY(Start, View.ViewMatrices.GetViewOrigin()) > DistanceSq)
	{
		return false;
	}

	if (FVector::DistSquaredXY(End, View.ViewMatrices.GetViewOrigin()) > DistanceSq)
	{
		return false;
	}

	for (const FPlane& Plane: View.ViewFrustum.Planes)
	{
		if (Plane.PlaneDot(Start) > 0.f && Plane.PlaneDot(End) > 0.f)
		{
			return false;
		}
	}

	return true;
}

bool FDistanceMetricsSceneProxy::PointInView(const FSceneView& View, const FVector& Point, float DistanceSq)
{
	if (FVector::DistSquaredXY(Point, View.ViewMatrices.GetViewOrigin()) > DistanceSq)
	{
		return false;
	}

	for (const FPlane& Plane: View.ViewFrustum.Planes)
	{
		if (Plane.PlaneDot(Point) > 0.f)
		{
			return false;
		}
	}

	return true;
}
