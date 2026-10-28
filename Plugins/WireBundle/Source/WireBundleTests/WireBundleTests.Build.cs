using UnrealBuildTool;

public class WireBundleTests : ModuleRules
{
    public WireBundleTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] {
            "WireBundle", "Core", "CoreUObject", "Engine", "InputCore",
            "UnrealEd", "BlueprintGraph", "GraphEditor", "Slate", "SlateCore",
            "MaterialEditor"
        });
    }
}
