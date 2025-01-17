// Copyright Epic Games, Inc. All Rights Reserved.

#include "DistanceMetrics.h"

#include "DistanceMetricsComponentVisualizer.h"
#include "DistanceMetricsRenderingComponent.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"

#define LOCTEXT_NAMESPACE "FDistanceMetricsModule"

namespace UE::DistanceMetrics
{
	FString ModuleName{TEXT("DistanceMetrics")};
	TCustomShowFlag<> ShowDistanceMetrics(TEXT("DistanceMetrics"), false /*DefaultEnabled*/, SFG_Developer, LOCTEXT("ShowDistanceMetrics", "Visualize Distance Metrics"));
} // UE::SmartObject

void FDistanceMetrics::StartupModule()
{
	if (GUnrealEd)
	{
		GUnrealEd->RegisterComponentVisualizer(UDistanceMetricsRenderingComponent::StaticClass()->GetFName(), MakeShared<FDistanceMetricsComponentVisualizer>());
	}
}

void FDistanceMetrics::ShutdownModule()
{
	if (GUnrealEd)
	{
		GUnrealEd->UnregisterComponentVisualizer(UDistanceMetricsRenderingComponent::StaticClass()->GetFName());
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FDistanceMetrics, DistanceMetrics)