#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"

#include "DistanceMetricsSubsystem.generated.h"

class ANavigationData;
class SLevelViewport;
class UDistanceMetricsRenderingComponent;
enum class EDistanceProjectionMode: uint8;

/**
 * Distance Metrics Subsystem
 * Stores state and provides functionality to control distance metrics visualization
 */
UCLASS()
class DISTANCEMETRICS_API UDistanceMetricsSubsystem: public UEditorSubsystem
{
	GENERATED_BODY()
public:

	FORCEINLINE static UDistanceMetricsSubsystem* Get()
	{
		return GEditor->GetEditorSubsystem<UDistanceMetricsSubsystem>();
	}

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** re-initializes subsystem with project settings */
	void ReloadSettings();

	/** @return true if either distance metrics is enabled by user or show flag is on */
	bool IsEnabled(const FEngineShowFlags& ShowFlags) const;
	bool IsEnabledForWorld(const UWorld* World) const;

	/** @return distance projection mode */
	UFUNCTION(BlueprintPure)
	FORCEINLINE EDistanceProjectionMode GetProjectionMode() const { return ProjectionMode; }

	/** @return true if distance metrics tool is enabled by user directly */
	UFUNCTION(BlueprintPure)
	FORCEINLINE bool IsDistanceMetricsEnabled() const { return bEnabledByUser; }

	/** updates visualization for all active rendering components */
	UFUNCTION(BlueprintCallable)
	void RequestDrawingUpdate();

	/** set distance projection mode */
	UFUNCTION(BlueprintCallable)
	void SetDistanceProjectionMode(EDistanceProjectionMode NewProjectionMode);

	/** enable or disable distance metrics tool visualization */
	UFUNCTION(BlueprintCallable)
	void SetDistanceMetricsEnabled(bool bEnabled);
	
	UFUNCTION(BlueprintCallable)
	bool DeprojectCursorPositionToWorld(const UWorld* World, FHitResult& OutHit) const;

	UFUNCTION(BlueprintCallable)
	bool DeprojectScreenCenterToWorld(const UWorld* World, FHitResult& OutHit) const;

	/** @return all currently selected polygons */
	FORCEINLINE TConstArrayView<NavNodeRef> GetSelectedPolygons() const { return SelectedPolygons; }

	/** Update selection for a given polygon, optionally clears selection before it */
	void UpdatePolygonSelection(NavNodeRef Polygon, bool bClearSelection = false);
	void ClearPolygonSelection();
	
protected:

	TSharedPtr<SLevelViewport> GetGlobalLevelViewport() const;
	bool DeprojectScreenLocationToWorld(const UWorld* World, bool bUseCursor, FHitResult& OutHit) const;

	void NavigationSystemInitDone(const UNavigationSystemBase& NavigationSystem);

	/** Adds metrics component to a given navigation data, if it doesn't already exist */
	UFUNCTION()
	void AddMetricsComponent(ANavigationData* NavData);

	bool bEnabledByUser = false;
	EDistanceProjectionMode ProjectionMode;
	/** A list of polygons selected by user via component's hit proxy */
	TArray<NavNodeRef, TInlineAllocator<4>> SelectedPolygons;
	/** A list of spawned/existing rendering components  */
	TArray<TWeakObjectPtr<UDistanceMetricsRenderingComponent>> RenderingComponents;
};
