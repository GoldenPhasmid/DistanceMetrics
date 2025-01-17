// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

namespace UE::DistanceMetrics
{
	extern FString ModuleName;
}

/**
 * DistanceMetrics plugin
 * Requires Realtime=True for updates
 * Automatically adds distance metrics rendering to loaded/newly created navigation data @see UDistanceMetricsRenderingComponent
 * Can be enabled via 'Developer -> Visualize Distance Metrics' show flag or from editor utility, @see UDistanceMetricsSubsystem::SetDistanceMetricsEnabled 
 * @see Project Settings -> Distance Metrics for available display settings
 */
class FDistanceMetrics : public IModuleInterface
{
public:

	static FDistanceMetrics& Get()
	{
		return FModuleManager::Get().GetModuleChecked<FDistanceMetrics>(FName{UE::DistanceMetrics::ModuleName});
	}

protected:
	
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
