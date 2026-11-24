// Copyright 2025 DimAlek. All Rights Reserved.

using UnrealBuildTool;

public class ContentBrowserToolkit : ModuleRules
{
	public ContentBrowserToolkit(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"ContentBrowser",
				"ContentBrowserData",
				"Core",
				"CoreUObject",
				"ToolWidgets",
				"AssetRegistry",
				"Engine",
				"DeveloperSettings",
				"Slate",
				"SlateCore"
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"PhysicsCore",
				"ToolMenus", "UnrealEd", "PropertyEditor", "InputCore"
			});
	}
}
