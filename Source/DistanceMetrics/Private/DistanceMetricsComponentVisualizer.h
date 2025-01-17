#pragma once

#include "CoreMinimal.h"
#include "ComponentVisualizer.h"

class FDistanceMetricsComponentVisualizer: public FComponentVisualizer
{
public:
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;
	virtual bool HandleInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;

	void HandleMouseClick(const UActorComponent* Component, FEditorViewportClient* ViewportClient, FViewport* Viewport);
};
