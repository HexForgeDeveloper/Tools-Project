#include "UI/TextureChannelPackerUIConversions.h"

#if WITH_DEV_AUTOMATION_TESTS
	#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTexturePackerOptionConversionTest, "TextureChannelPacker.Conversions.SupportedOptions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Verifies exact UI labels and round trips for every selectable option. */
bool FTexturePackerOptionConversionTest::RunTest(const FString&)
{
	using F = FTextureChannelPackerUIConversions;
	/** @brief Checks a known label independently of the reverse conversion. */
	const auto Check = [this](const auto Value, const TCHAR* Label, auto ToString, auto FromString) {
		TestEqual(TEXT("Expected UI label"), ToString(Value), FString(Label));
		TestTrue(TEXT("Label restores the original value"), FromString(FString(Label)) == Value);
	};
	Check(ETextureChannelPackerOperation::Pack, TEXT("Pack"), F::OperationToString, F::StringToOperation);
	Check(ETextureChannelPackerOperation::Repack, TEXT("Repack"), F::OperationToString, F::StringToOperation);
	Check(ETextureChannelPackerOperation::Unpack, TEXT("Unpack"), F::OperationToString, F::StringToOperation);
	Check(ETextureChannelPackerOperation::Copy, TEXT("Copy Channels"), F::OperationToString, F::StringToOperation);
	Check(ETextureChannelPackerChannel::Red, TEXT("R"), F::ChannelToString, F::StringToChannel);
	Check(ETextureChannelPackerChannel::Green, TEXT("G"), F::ChannelToString, F::StringToChannel);
	Check(ETextureChannelPackerChannel::Blue, TEXT("B"), F::ChannelToString, F::StringToChannel);
	Check(ETextureChannelPackerChannel::Alpha, TEXT("A"), F::ChannelToString, F::StringToChannel);
	Check(TC_Default, TEXT("Default"), F::CompressionToString, F::StringToCompression);
	Check(TC_Masks, TEXT("Masks"), F::CompressionToString, F::StringToCompression);
	Check(TC_Grayscale, TEXT("Grayscale"), F::CompressionToString, F::StringToCompression);
	Check(TC_HDR, TEXT("HDR"), F::CompressionToString, F::StringToCompression);
	Check(TC_VectorDisplacementmap, TEXT("Vector Displacement"), F::CompressionToString, F::StringToCompression);
	Check(TMGS_FromTextureGroup, TEXT("From Texture Group"), F::MipToString, F::StringToMip);
	Check(TMGS_NoMipmaps, TEXT("No Mipmaps"), F::MipToString, F::StringToMip);
	Check(TMGS_Sharpen0, TEXT("Sharpen 0"), F::MipToString, F::StringToMip);
	Check(TMGS_Sharpen4, TEXT("Sharpen 4"), F::MipToString, F::StringToMip);
	Check(TF_Default, TEXT("Default"), F::FilterToString, F::StringToFilter);
	Check(TF_Nearest, TEXT("Nearest"), F::FilterToString, F::StringToFilter);
	Check(TF_Bilinear, TEXT("Bilinear"), F::FilterToString, F::StringToFilter);
	Check(TF_Trilinear, TEXT("Trilinear"), F::FilterToString, F::StringToFilter);
	Check(TEXTUREGROUP_World, TEXT("World"), F::LODGroupToString, F::StringToLODGroup);
	Check(TEXTUREGROUP_UI, TEXT("UI"), F::LODGroupToString, F::StringToLODGroup);
	Check(TEXTUREGROUP_Pixels2D, TEXT("Pixels 2D"), F::LODGroupToString, F::StringToLODGroup);
	Check(TEXTUREGROUP_WorldNormalMap, TEXT("World Normal Map"), F::LODGroupToString, F::StringToLODGroup);
	Check(TEXTUREGROUP_WorldSpecular, TEXT("World Specular"), F::LODGroupToString, F::StringToLODGroup);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTexturePackerOptionFallbackTest, "TextureChannelPacker.Conversions.UnknownOptions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Preserves fallback semantics for unknown labels and unsupported enum values. */
bool FTexturePackerOptionFallbackTest::RunTest(const FString&)
{
	using F = FTextureChannelPackerUIConversions;
	for (const FString Label : { FString(), FString(TEXT("Unknown")) })
	{
		TestTrue(TEXT("Unknown operation uses Pack"), F::StringToOperation(Label) == ETextureChannelPackerOperation::Pack);
		TestTrue(TEXT("Unknown channel uses Red"), F::StringToChannel(Label) == ETextureChannelPackerChannel::Red);
		TestTrue(TEXT("Unknown compression uses Default"), F::StringToCompression(Label) == TC_Default);
		TestTrue(TEXT("Unknown mip uses group defaults"), F::StringToMip(Label) == TMGS_FromTextureGroup);
		TestTrue(TEXT("Unknown filter uses Default"), F::StringToFilter(Label) == TF_Default);
		TestTrue(TEXT("Unknown group uses World"), F::StringToLODGroup(Label) == TEXTUREGROUP_World);
	}
	TestEqual(TEXT("Unknown operation label fallback"), F::OperationToString(static_cast<ETextureChannelPackerOperation>(255)), FString(TEXT("Pack")));
	TestEqual(TEXT("Unknown channel label fallback"), F::ChannelToString(static_cast<ETextureChannelPackerChannel>(255)), FString(TEXT("R")));
	TestEqual(TEXT("Unknown compression label fallback"), F::CompressionToString(static_cast<TextureCompressionSettings>(255)), FString(TEXT("Default")));
	TestEqual(TEXT("Unknown mip label fallback"), F::MipToString(static_cast<TextureMipGenSettings>(255)), FString(TEXT("From Texture Group")));
	TestEqual(TEXT("Unknown filter label fallback"), F::FilterToString(static_cast<TextureFilter>(255)), FString(TEXT("Default")));
	TestEqual(TEXT("Unknown group label fallback"), F::LODGroupToString(static_cast<TextureGroup>(255)), FString(TEXT("World")));
	return true;
}
#endif
