#pragma once

#include "CoreMinimal.h"
#include "ComponentVisualizer.h"

/**
 * HDistanceMetricsHitProxy
 * Hit proxy that identifies distance metrics rendering component
 */
class HDistanceMetricsHitProxy: public HComponentVisProxy
{
	DECLARE_HIT_PROXY()
public:

	HDistanceMetricsHitProxy(const UActorComponent* InComponent)
		: HComponentVisProxy(InComponent, HPP_Foreground)
	{}

	virtual bool AlwaysAllowsTranslucentPrimitives() const override { return true; }
	
};
