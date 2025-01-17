// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DistanceMetrics : ModuleRules
{
	public DistanceMetrics(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
				"NavigationSystem",
				"RenderCore",
				"EditorSubsystem",
			}
		);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"CoreUObject",
				"AIModule",
				"LevelEditor",
				"UnrealEd",
				"RHI",
				"InputCore",
				"DeveloperSettings", 
			}
		);
	}
}
