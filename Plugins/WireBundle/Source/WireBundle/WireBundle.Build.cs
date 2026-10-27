using UnrealBuildTool;

public class WireBundle : ModuleRules
{
    public WireBundle(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "UnrealEd", "BlueprintGraph",
            "GraphEditor", "Slate", "SlateCore", "InputCore", "MaterialEditor"
        });
    }
}
