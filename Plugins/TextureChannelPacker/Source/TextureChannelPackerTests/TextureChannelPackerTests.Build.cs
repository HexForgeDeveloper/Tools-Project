using UnrealBuildTool;

public class TextureChannelPackerTests : ModuleRules
{
    public TextureChannelPackerTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine", "TextureChannelPacker"
        });
    }
}
