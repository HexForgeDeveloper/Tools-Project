#include "Services/TextureChannelPackerTextureService.h"

#if WITH_DEV_AUTOMATION_TESTS
	#include "Engine/Texture2D.h"
	#include "Math/Float16.h"
	#include "Misc/AutomationTest.h"

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FTexturePackerSourceFormatsTest, "TextureChannelPacker.SourceFormats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Enumerates all eight source formats as independent automation cases. */
void FTexturePackerSourceFormatsTest::GetTests(TArray<FString>& Names, TArray<FString>& Parameters) const
{
	for (const TCHAR* Format : { TEXT("G8"), TEXT("G16"), TEXT("R16F"), TEXT("R32F"), TEXT("BGRA8"), TEXT("RGBA16"), TEXT("RGBA16F"), TEXT("RGBA32F") })
	{
		Names.Add(Format);
		Parameters.Add(Format);
	}
}

/** @brief Validates every source component against known pixels through the public preview API. */
bool FTexturePackerSourceFormatsTest::RunTest(const FString& Parameter)
{
	UTexture2D* Texture = NewObject<UTexture2D>();
	const uint8 Bytes[] = { 192, 128, 64, 32 };
	const uint16 Words[] = { 16384, 32768, 49152, 8192 };
	const FFloat16 Halves[] = { FFloat16(.25f), FFloat16(.5f), FFloat16(.75f), FFloat16(.125f) };
	const float Floats[] = { .25f, .5f, .75f, .125f };
	ETextureSourceFormat Format = TSF_Invalid;
	const uint8* Data = nullptr;
	bool bGray = false;
	if (Parameter == TEXT("G8"))
	{
		Format = TSF_G8;
		Data = &Bytes[2];
		bGray = true;
	}
	else if (Parameter == TEXT("G16"))
	{
		Format = TSF_G16;
		Data = reinterpret_cast<const uint8*>(Words);
		bGray = true;
	}
	else if (Parameter == TEXT("R16F"))
	{
		Format = TSF_R16F;
		Data = reinterpret_cast<const uint8*>(Halves);
		bGray = true;
	}
	else if (Parameter == TEXT("R32F"))
	{
		Format = TSF_R32F;
		Data = reinterpret_cast<const uint8*>(Floats);
		bGray = true;
	}
	else if (Parameter == TEXT("BGRA8"))
	{
		Format = TSF_BGRA8;
		Data = Bytes;
	}
	else if (Parameter == TEXT("RGBA16"))
	{
		Format = TSF_RGBA16;
		Data = reinterpret_cast<const uint8*>(Words);
	}
	else if (Parameter == TEXT("RGBA16F"))
	{
		Format = TSF_RGBA16F;
		Data = reinterpret_cast<const uint8*>(Halves);
	}
	else if (Parameter == TEXT("RGBA32F"))
	{
		Format = TSF_RGBA32F;
		Data = reinterpret_cast<const uint8*>(Floats);
	}
	if (!TestNotNull(TEXT("Source format is recognized"), Data))
	{
		return false;
	}
	Texture->Source.Init(1, 1, 1, 1, Format, Data);
	const uint8 Expected[] = { 64, 128, static_cast<uint8>(Format == TSF_BGRA8 ? 192 : 191), 32 };
	for (int32 Component = 0; Component < 4; ++Component)
	{
		FTextureChannelPackerRequest Request;
		Request.OutputSettings.SourceFormat = ETextureChannelPackerOutputFormat::R32F;
		FTextureChannelPackerMapping Mapping;
		Mapping.Source.Texture = Texture;
		Mapping.Source.Channel = static_cast<ETextureChannelPackerChannel>(Component);
		Request.Mappings.Add(Mapping);
		const auto Result = FTextureChannelPackerTextureService::BuildPreview(Request, ETextureChannelPackerPreviewMode::Composite);
		if (!TestTrue(TEXT("Source channel preview succeeds"), Result.bSuccess))
		{
			continue;
		}
		FTexture2DMipMap& Mip = Result.PreviewTexture->GetPlatformData()->Mips[0];
		const uint8* Pixels = static_cast<const uint8*>(Mip.BulkData.LockReadOnly());
		if (TestNotNull(TEXT("Preview pixels exist"), Pixels))
		{
			TestEqual(TEXT("Extracted component matches known source"), Pixels[2], bGray ? static_cast<uint8>(Component == 3 ? 255 : 64) : Expected[Component]);
		}
		Mip.BulkData.Unlock();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTexturePackerResizeTest, "TextureChannelPacker.Processing.NearestResize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Checks nearest sampling of a rectangular input into a manual square output. */
bool FTexturePackerResizeTest::RunTest(const FString&)
{
	UTexture2D* Texture = NewObject<UTexture2D>();
	const uint8 Data[] = { 10, 50, 150, 250 };
	Texture->Source.Init(4, 1, 1, 1, TSF_G8, Data);
	FTextureChannelPackerRequest Request;
	Request.OutputSettings.bUseFirstInputResolution = false;
	Request.OutputSettings.Resolution = 2;
	FTextureChannelPackerMapping Mapping;
	Mapping.Source.Texture = Texture;
	Request.Mappings.Add(Mapping);
	const auto Result = FTextureChannelPackerTextureService::BuildPreview(Request, ETextureChannelPackerPreviewMode::Red);
	if (TestTrue(TEXT("Resize succeeds"), Result.bSuccess))
	{
		TestEqual(TEXT("Manual dimensions applied"), Result.PreviewSize, FIntPoint(2, 2));
		auto& Mip = Result.PreviewTexture->GetPlatformData()->Mips[0];
		const uint8* Pixels = static_cast<const uint8*>(Mip.BulkData.LockReadOnly());
		if (TestNotNull(TEXT("Resized pixels exist"), Pixels))
		{
			TestEqual(TEXT("First source sample"), Pixels[2], static_cast<uint8>(10));
			TestEqual(TEXT("Nearest sample skips to index two"), Pixels[6], static_cast<uint8>(150));
			TestEqual(TEXT("Single source row repeats vertically"), Pixels[10], static_cast<uint8>(10));
		}
		Mip.BulkData.Unlock();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTexturePackerInvalidInputTest, "TextureChannelPacker.Processing.InvalidInputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Checks empty requests, the alpha-only constant boundary and disabled Unpack outputs. */
bool FTexturePackerInvalidInputTest::RunTest(const FString&)
{
	FTextureChannelPackerRequest Request;
	auto Result = FTextureChannelPackerTextureService::BuildPreview(Request, ETextureChannelPackerPreviewMode::Composite);
	TestFalse(TEXT("Empty mappings fail"), Result.bSuccess);
	TestTrue(TEXT("Failure has a message"), !Result.Message.IsEmpty());
	FTextureChannelPackerMapping Alpha;
	Alpha.OutputChannel = ETextureChannelPackerChannel::Alpha;
	Alpha.Source.bUseConstant = true;
	Alpha.Source.ConstantValue = 255;
	Request.Mappings.Add(Alpha);
	Request.OutputSettings.bUseFirstInputResolution = false;
	Request.OutputSettings.Resolution = 1;
	Result = FTextureChannelPackerTextureService::BuildPreview(Request, ETextureChannelPackerPreviewMode::Alpha);
	TestFalse(TEXT("Default opaque alpha alone is treated as empty input"), Result.bSuccess);
	Request.Mappings[0].Source.ConstantValue = 128;
	Result = FTextureChannelPackerTextureService::BuildPreview(Request, ETextureChannelPackerPreviewMode::Alpha);
	TestTrue(TEXT("Changed alpha constant is a valid input"), Result.bSuccess);
	Request.Operation = ETextureChannelPackerOperation::Unpack;
	Result = FTextureChannelPackerTextureService::BuildPreview(Request, ETextureChannelPackerPreviewMode::Composite);
	TestFalse(TEXT("Missing Unpack source fails"), Result.bSuccess);
	UTexture2D* Source = NewObject<UTexture2D>();
	const uint8 Value = 128;
	Source->Source.Init(1, 1, 1, 1, TSF_G8, &Value);
	Request.UnpackSourceTexture = Source;
	Request.UnpackOutputs = { { ETextureChannelPackerChannel::Red, TEXT("_R"), false } };
	Result = FTextureChannelPackerTextureService::BuildPreview(Request, ETextureChannelPackerPreviewMode::Composite);
	TestFalse(TEXT("All disabled outputs fail"), Result.bSuccess);
	FText Error;
	TestFalse(TEXT("Null texture is rejected"), FTextureChannelPackerTextureService::CanReadTexture(nullptr, Error));
	TestFalse(TEXT("Texture without source is rejected"), FTextureChannelPackerTextureService::CanReadTexture(NewObject<UTexture2D>(), Error));
	TestEqual(TEXT("Null source has zero size"), FTextureChannelPackerTextureService::GetTextureSourceSize(nullptr), FIntPoint::ZeroValue);
	TestEqual(TEXT("Valid source dimensions are returned"), FTextureChannelPackerTextureService::GetTextureSourceSize(Source), FIntPoint(1, 1));
	return true;
}
#endif
