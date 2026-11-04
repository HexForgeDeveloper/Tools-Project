using UnrealBuildTool;
using System.IO;

/** @brief Declares dependencies for the editor-side Texture Channel Packer regression tests. */
public class TextureChannelPackerTests : ModuleRules
{
    /** @brief Configures the test module. @param Target Build target that consumes the module. */
    public TextureChannelPackerTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // Internal failure injection is accessible only to this regression module, not plugin consumers.
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "../TextureChannelPacker/Private"));
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine", "DeveloperSettings", "TextureChannelPacker",
            "Slate", "SlateCore", "RenderCore", "RHI", "UnrealEd", "AssetRegistry"
        });
    }
}
