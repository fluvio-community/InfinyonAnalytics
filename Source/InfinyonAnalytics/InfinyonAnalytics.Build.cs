// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class InfinyonAnalytics : ModuleRules
{
	public InfinyonAnalytics(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// PublicIncludePaths.AddRange(
		// 	new string[] {
		// 		// ... add public include paths required here ...
		// 	}
		// 	);

		// PrivateIncludePaths.AddRange(
		// 	new string[] {
		// 		// ... add other private include paths required here ...
		// 	}
		// 	);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
                "Engine",
				// ... add other public dependencies that you statically link with here ...
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Analytics",
				"WebSockets"
				// ... add private dependencies that you statically link with here ...
			}
			);

        PublicIncludePathModuleNames.Add("Analytics");
	}
}
