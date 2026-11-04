// Copyright 2025 DimAlek. All Rights Reserved.
using UnrealBuildTool;

/** @brief Defines editor tooling dependencies and exports dependencies required by public headers. */
public class TextureChannelPacker : ModuleRules
{
    /** @brief Configures the editor-only module for the consuming Unreal target. */
    public TextureChannelPacker(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine", "Slate", "SlateCore", "DeveloperSettings"
        });
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "InputCore", "UnrealEd", "MainFrame", "ToolMenus", "PropertyEditor",
            "ImageCore", "RenderCore", "AssetRegistry", "ContentBrowser", "AssetTools"
        });
    }
}
