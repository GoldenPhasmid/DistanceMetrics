#include "DistanceMetricsComponentVisualizer.h"

#include "DistanceMetricsHitProxy.h"
#include "DistanceMetricsRenderingComponent.h"

bool FDistanceMetricsComponentVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	if (VisProxy && VisProxy->Component.IsValid() && Click.GetKey() == EKeys::LeftMouseButton)
	{
		if (const HDistanceMetricsHitProxy* HitProxy = HitProxyCast<HDistanceMetricsHitProxy>(VisProxy))
		{
			HandleMouseClick(VisProxy->Component.Get(), InViewportClient, InViewportClient->Viewport);
			return true;
		}
	}

	return false;
}

bool FDistanceMetricsComponentVisualizer::HandleInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	HHitProxy* HitProxy = Viewport->GetHitProxy(Viewport->GetMouseX(), Viewport->GetMouseY());
	if (const HDistanceMetricsHitProxy* MyHitProxy = HitProxyCast<HDistanceMetricsHitProxy>(HitProxy))
	{
		if (Key == EKeys::LeftMouseButton)
		{
			HandleMouseClick(MyHitProxy->Component.Get(), ViewportClient, Viewport);
			return true;
		}
	}

	return false;
}

void FDistanceMetricsComponentVisualizer::HandleMouseClick(const UActorComponent* Component, FEditorViewportClient* ViewportClient, FViewport* Viewport)
{
	const UDistanceMetricsRenderingComponent* RenderingComp = CastChecked<UDistanceMetricsRenderingComponent>(Component);
	RenderingComp->UpdatePolygonSelection(!ViewportClient->IsCtrlPressed());
}
