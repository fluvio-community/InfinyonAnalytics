// Copyright Infinyon 2025 All Rights Reserved.
using UnrealBuildTool;

public class InfinyonAnalytics : ModuleRules
{
	public InfinyonAnalytics(ReadOnlyTargetRules Target) : base(Target)
	{
		// PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

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
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Analytics",
				"Json",
				"WebSockets"
			}
			);

        PublicIncludePathModuleNames.Add("Analytics");
	}
}
