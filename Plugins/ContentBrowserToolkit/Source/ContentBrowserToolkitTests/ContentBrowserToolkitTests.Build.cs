using UnrealBuildTool;

/** <summary>Builds editor automation tests separately from the production plugin.</summary> */
public class ContentBrowserToolkitTests : ModuleRules
{
    /** <summary>Declares the scanner and automation dependencies of the test module.</summary> */
    public ContentBrowserToolkitTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "AssetRegistry", "ContentBrowserToolkit"
        });
    }
}