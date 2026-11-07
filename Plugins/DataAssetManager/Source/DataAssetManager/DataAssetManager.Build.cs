// Copyright (c) 2026 DimAlek. All Rights Reserved.

using UnrealBuildTool;

/** @brief Declares dependencies for the Data Asset Manager editor module. */
public class DataAssetManager : ModuleRules
{
	/** @brief Configures explicit/shared PCH usage and public versus implementation dependencies. */
	public DataAssetManager(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AssetRegistry",
			"ApplicationCore",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Projects",
			"InputCore",
			"EditorFramework",
			"EditorStyle",
			"UnrealEd",
			"ToolMenus",
			"Slate",
			"SlateCore",
			"PropertyEditor",
			"ContentBrowser",
			"DeveloperSettings",
			"EditorScriptingUtilities",
			"AssetManagerEditor",
			"SourceControl",
			"ClassViewer",
			"ToolWidgets",
			"StatusBar",
			"BlueprintGraph",
			"Kismet",
			"SettingsEditor",
			"EditorWidgets",
			"MessageLog",
			"OutputLog",
			"Json",
			"JsonUtilities",
			"LevelEditor",
			"DesktopPlatform",
		});
	}
}
