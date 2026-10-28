using UnrealBuildTool;

public class WireBundle : ModuleRules
{
    public WireBundle(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "InputCore", "DeveloperSettings", "SlateCore"
        });
        PrivateDependencyModuleNames.AddRange(new[] {
            "UnrealEd", "BlueprintGraph",
            "GraphEditor", "Slate", "MaterialEditor"
        });
    }
}
