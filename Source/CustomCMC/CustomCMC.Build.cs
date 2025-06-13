// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CustomCMC : ModuleRules
{
	public CustomCMC(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"MotionWarping"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"CustomCMC",
			"CustomCMC/Variant_Platforming",
			"CustomCMC/Variant_Combat",
			"CustomCMC/Variant_Combat/AI",
			"CustomCMC/Variant_SideScrolling",
			"CustomCMC/Variant_SideScrolling/Gameplay",
			"CustomCMC/Variant_SideScrolling/AI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
