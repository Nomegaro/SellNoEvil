// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SellNoEvil : ModuleRules
{
	public SellNoEvil(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG",
			"Json", "JsonUtilities"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] {
				"DataValidation",
				"AssetTools",
				"UnrealEd",
				"AssetRegistry"
			});
		}
	}
}
