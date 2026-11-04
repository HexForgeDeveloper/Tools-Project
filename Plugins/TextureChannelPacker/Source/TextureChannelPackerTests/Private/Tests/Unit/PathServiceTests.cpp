#include "Services/TextureChannelPackerPathService.h"
#include "Services/TextureChannelPackerTextureService.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextureChannelPackerValidPathsTest, "TextureChannelPacker.Paths.ValidDirectories",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Checks project root, nested, absolute and normalized destinations. @return True after recording assertions. */
bool FTextureChannelPackerValidPathsTest::RunTest(const FString&)
{
    TestEqual(TEXT("Root remains valid"), FTextureChannelPackerPathService::NormalizeOutputPath(TEXT("/Game")), FString(TEXT("/Game")));
    TestEqual(TEXT("Whitespace and repeated separators are normalized"), FTextureChannelPackerPathService::NormalizeOutputPath(TEXT("  /Game//Textures/  ")), FString(TEXT("/Game/Textures")));
    TestEqual(TEXT("Backslashes are supported"), FTextureChannelPackerPathService::NormalizeOutputPath(TEXT("\\Game\\Textures\\Masks")), FString(TEXT("/Game/Textures/Masks")));
    const FString Content = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
    TestEqual(TEXT("Absolute Content root maps to Game"), FTextureChannelPackerPathService::NormalizeOutputPath(Content), FString(TEXT("/Game")));
    TestEqual(TEXT("Absolute nested directory maps to Game"), FTextureChannelPackerPathService::NormalizeOutputPath(Content / TEXT("Textures/Masks")), FString(TEXT("/Game/Textures/Masks")));
    TestEqual(TEXT("Valid package is built"), FTextureChannelPackerPathService::BuildPackageName(TEXT("/Game/Textures"), TEXT("T_Test")), FString(TEXT("/Game/Textures/T_Test")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextureChannelPackerInvalidPathsTest, "TextureChannelPacker.Paths.InvalidDirectories",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Rejects prefix siblings, traversal, external roots and invalid names without creating assets. @return True after recording assertions. */
bool FTextureChannelPackerInvalidPathsTest::RunTest(const FString&)
{
    FString Content = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
    FPaths::NormalizeDirectoryName(Content);
    const TArray<FString> InvalidPaths = {
        TEXT(""), TEXT("   "), TEXT("Textures"), TEXT("/GameOther"), TEXT("/GameOther/Textures"),
        TEXT("/Engine/Textures"), TEXT("/Unknown/Textures"), TEXT("/Game/../Outside"),
        TEXT("/Game/Textures/../../GameOther"), TEXT("/Game/Bad.Name"), TEXT("/Game/Bad Name"),
        Content + TEXT("Backup/Textures"), Content / TEXT("../ContentBackup/Textures"), FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir())
    };
    for (const FString& Path : InvalidPaths)
    {
        TestTrue(FString::Printf(TEXT("Invalid destination rejected: %s"), *Path), FTextureChannelPackerPathService::NormalizeOutputPath(Path).IsEmpty());
        TestTrue(TEXT("Invalid destination produces no package name"), FTextureChannelPackerPathService::BuildPackageName(Path, TEXT("T_Test")).IsEmpty());
        FString PackageName = TEXT("StalePackage"), AssetName = TEXT("StaleAsset");
        TestFalse(TEXT("Invalid destination cannot produce a unique package"), FTextureChannelPackerPathService::MakeUniqueAssetPackageName(Path, TEXT("T_Test"), PackageName, AssetName));
        TestTrue(TEXT("Failed request clears output names"), PackageName.IsEmpty() && AssetName.IsEmpty());
        FTextureChannelPackerRequest Request;
        Request.OutputPackagePath = Path;
        const FTextureChannelPackerResult Result = FTextureChannelPackerTextureService::Execute(Request);
        TestFalse(TEXT("Invalid destination fails generation"), Result.bSuccess);
        TestTrue(TEXT("Invalid destination reports an error"), !Result.Message.IsEmpty());
        TestEqual(TEXT("No assets created"), Result.CreatedTextures.Num(), 0);
    }
    return true;
}
#endif
