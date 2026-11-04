#include "Services/TextureChannelPackerTextureService.h"

#if WITH_DEV_AUTOMATION_TESTS
	#include "Engine/Texture2D.h"
	#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextureChannelPackerPreviewFormatTest,
	"TextureChannelPacker.Preview.OutputFormats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/**
 * @brief Checks actual preview pixels for every explicit format and the automatic format.
 * @return True after recording assertions; assertion failures determine the test result.
 */
bool FTextureChannelPackerPreviewFormatTest::RunTest(const FString&)
{
	FTextureChannelPackerRequest Request;
	Request.OutputSettings.bUseFirstInputResolution = false;
	Request.OutputSettings.Resolution = 1;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		FTextureChannelPackerMapping Mapping;
		Mapping.OutputChannel = static_cast<ETextureChannelPackerChannel>(Index);
		Mapping.Source.bUseConstant = true;
		Mapping.Source.ConstantValue = static_cast<uint8>((Index + 1) * 40);
		Request.Mappings.Add(Mapping);
	}
	for (const ETextureChannelPackerOutputFormat Format : {
			 ETextureChannelPackerOutputFormat::Auto, ETextureChannelPackerOutputFormat::G8,
			 ETextureChannelPackerOutputFormat::G16, ETextureChannelPackerOutputFormat::R16F,
			 ETextureChannelPackerOutputFormat::R32F, ETextureChannelPackerOutputFormat::BGRA8,
			 ETextureChannelPackerOutputFormat::RGBA16, ETextureChannelPackerOutputFormat::RGBA16F,
			 ETextureChannelPackerOutputFormat::RGBA32F })
	{
		Request.OutputSettings.SourceFormat = Format;
		const bool bGray = Format == ETextureChannelPackerOutputFormat::G8 || Format == ETextureChannelPackerOutputFormat::G16
			|| Format == ETextureChannelPackerOutputFormat::R16F || Format == ETextureChannelPackerOutputFormat::R32F;
		for (const ETextureChannelPackerPreviewMode Mode : { ETextureChannelPackerPreviewMode::Composite, ETextureChannelPackerPreviewMode::Alpha })
		{
			const FTextureChannelPackerPreviewResult Result = FTextureChannelPackerTextureService::BuildPreview(Request, Mode);
			if (!TestTrue(TEXT("Preview succeeds"), Result.bSuccess) || !TestNotNull(TEXT("Preview texture exists"), Result.PreviewTexture))
			{
				continue;
			}
			FTexture2DMipMap& Mip = Result.PreviewTexture->GetPlatformData()->Mips[0];
			const uint8* Pixels = static_cast<const uint8*>(Mip.BulkData.LockReadOnly());
			if (TestNotNull(TEXT("Preview pixel buffer exists"), Pixels))
			{
				const bool bAlpha = Mode == ETextureChannelPackerPreviewMode::Alpha;
				const uint8 R = bAlpha ? (bGray ? 255 : 160) : 40;
				TestEqual(TEXT("Red matches stored format"), Pixels[2], R);
				TestEqual(TEXT("Green matches stored format"), Pixels[1], bAlpha || bGray ? R : static_cast<uint8>(80));
				TestEqual(TEXT("Blue matches stored format"), Pixels[0], bAlpha || bGray ? R : static_cast<uint8>(120));
				TestEqual(TEXT("Display is opaque"), Pixels[3], static_cast<uint8>(255));
			}
			Mip.BulkData.Unlock();
		}
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextureChannelPackerPreviewPrecisionTest,
	"TextureChannelPacker.Preview.HalfFloatPrecision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/**
 * @brief Verifies that half-float rounding is applied before the preview's byte conversion.
 * @return True after recording assertions; assertion failures determine the test result.
 */
bool FTextureChannelPackerPreviewPrecisionTest::RunTest(const FString&)
{
	UTexture2D* Source = NewObject<UTexture2D>();
	// This value lies above a byte rounding threshold but rounds below it in half precision.
	const float Value = 0.15883f;
	Source->Source.Init(1, 1, 1, 1, TSF_R32F, reinterpret_cast<const uint8*>(&Value));
	FTextureChannelPackerRequest Request;
	FTextureChannelPackerMapping Mapping;
	Mapping.Source.Texture = Source;
	Request.Mappings.Add(Mapping);
	for (const ETextureChannelPackerOutputFormat Format : { ETextureChannelPackerOutputFormat::R16F, ETextureChannelPackerOutputFormat::R32F })
	{
		Request.OutputSettings.SourceFormat = Format;
		const FTextureChannelPackerPreviewResult Result = FTextureChannelPackerTextureService::BuildPreview(Request, ETextureChannelPackerPreviewMode::Composite);
		if (TestTrue(TEXT("Float source preview succeeds"), Result.bSuccess) && TestNotNull(TEXT("Float preview exists"), Result.PreviewTexture))
		{
			FTexture2DMipMap& Mip = Result.PreviewTexture->GetPlatformData()->Mips[0];
			const uint8* Pixels = static_cast<const uint8*>(Mip.BulkData.LockReadOnly());
			if (TestNotNull(TEXT("Float preview pixels exist"), Pixels))
			{
				TestEqual(TEXT("Display reflects output precision"), Pixels[2], static_cast<uint8>(Format == ETextureChannelPackerOutputFormat::R16F ? 40 : 41));
			}
			Mip.BulkData.Unlock();
		}
	}
	return true;
}
#endif
