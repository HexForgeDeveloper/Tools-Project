#include "Services/TextureChannelPackerPatternService.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextureChannelPackerPatternSeparatorsTest,
    "TextureChannelPacker.Patterns.Separators",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/**
 * @brief Verifies comma, semicolon and mixed lists through the public matching API.
 * @return True after recording all assertions; assertion failures determine the test result.
 */
bool FTextureChannelPackerPatternSeparatorsTest::RunTest(const FString&)
{
    for (const FString Patterns : { FString(TEXT("_AO;_R;_M")), FString(TEXT("_AO,_R,_M")), FString(TEXT("_AO;_R,_M")) })
    {
        for (const FString Name : { FString(TEXT("T_Wall_AO")), FString(TEXT("T_Wall_R")), FString(TEXT("T_Wall_M")) })
        {
            TestTrue(FString::Printf(TEXT("%s matches %s"), *Patterns, *Name),
                FTextureChannelPackerPatternService::DoesNameMatchPattern(Name, Patterns));
        }
        TestFalse(TEXT("Unrelated texture does not match"),
            FTextureChannelPackerPatternService::DoesNameMatchPattern(TEXT("T_Wall_Normal"), Patterns));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextureChannelPackerPatternEmptyEntriesTest,
    "TextureChannelPacker.Patterns.EmptyEntriesAndWhitespace",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/**
 * @brief Checks whitespace trimming and ensures empty list entries do not match arbitrary names.
 * @return True after recording all assertions; assertion failures determine the test result.
 */
bool FTextureChannelPackerPatternEmptyEntriesTest::RunTest(const FString&)
{
    TestTrue(TEXT("Whitespace and repeated separators are ignored"),
        FTextureChannelPackerPatternService::DoesNameMatchPattern(TEXT("T_Wall_R"), TEXT(" , ; _AO ;;,  _R ,; ")));
    for (const FString Patterns : { FString(), FString(TEXT("   ")), FString(TEXT(",; ;,,")) })
    {
        TestFalse(TEXT("Empty entries never match every name"),
            FTextureChannelPackerPatternService::DoesNameMatchPattern(TEXT("T_Wall_R"), Patterns));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextureChannelPackerPatternMatchingTest,
    "TextureChannelPacker.Patterns.MatchingCompatibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/**
 * @brief Preserves single-pattern, case-insensitive substring and wildcard matching behavior.
 * @return True after recording all assertions; assertion failures determine the test result.
 */
bool FTextureChannelPackerPatternMatchingTest::RunTest(const FString&)
{
    TestTrue(TEXT("Single pattern remains supported"),
        FTextureChannelPackerPatternService::DoesNameMatchPattern(TEXT("T_Wall_Roughness"), TEXT("Roughness")));
    TestTrue(TEXT("Substring matching remains case insensitive"),
        FTextureChannelPackerPatternService::DoesNameMatchPattern(TEXT("T_ROUGHNESS_Wall"), TEXT("roughness")));
    TestTrue(TEXT("Wildcard works in a mixed pattern list"),
        FTextureChannelPackerPatternService::DoesNameMatchPattern(TEXT("T_Wall_R"), TEXT("*_AO;T_*_R,*_M")));
    TestFalse(TEXT("Empty texture name does not match a literal"),
        FTextureChannelPackerPatternService::DoesNameMatchPattern(TEXT(""), TEXT("_AO,_R")));
    return true;
}
#endif
