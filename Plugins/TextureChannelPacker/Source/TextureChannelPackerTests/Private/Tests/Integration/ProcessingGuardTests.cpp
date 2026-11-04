#include "Processing/TextureChannelPackerProcessingGuard.h"
#include "Processing/TextureChannelPackerTextureAssetWriter.h"
#include "Services/TextureChannelPackerTextureService.h"

#if WITH_DEV_AUTOMATION_TESTS
	#include "Engine/Texture2D.h"
	#include "HAL/FileManager.h"
	#include "Misc/AutomationTest.h"
	#include "Misc/PackageName.h"
	#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTexturePackerBufferLimitsTest, "TextureChannelPacker.Processing.BufferLimits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Exercises exact allocation boundaries and extreme inputs without allocating large textures. */
bool FTexturePackerBufferLimitsTest::RunTest(const FString&)
{
	using TextureChannelPackerTexture::FProcessingGuard;
	FText Error;
	TestTrue(TEXT("Smallest valid buffer"), FProcessingGuard::ValidateBuffer(1, 1, 1, Error));
	TestTrue(TEXT("Exact 512 MiB boundary"), FProcessingGuard::ValidateBuffer(8192, 8192, 8, Error));
	TestFalse(TEXT("Over 512 MiB rejected"), FProcessingGuard::ValidateBuffer(8192, 8193, 8, Error));
	TestFalse(TEXT("4 GiB product cannot wrap int32"), FProcessingGuard::ValidateBuffer(16384, 16384, 16, Error));
	TestFalse(TEXT("INT_MAX dimensions rejected"), FProcessingGuard::ValidateBuffer(MAX_int32, MAX_int32, 16, Error));
	TestFalse(TEXT("Negative dimensions rejected"), FProcessingGuard::ValidateBuffer(-1, 128, 4, Error));
	TestFalse(TEXT("Zero dimensions rejected"), FProcessingGuard::ValidateBuffer(128, 0, 4, Error));
	TestFalse(TEXT("Invalid source layout rejected"), FProcessingGuard::ValidateTexture(1, 1, TSF_Invalid, Error));
	FTextureChannelPackerRequest Request;
	Request.OutputSettings.bUseFirstInputResolution = false;
	Request.OutputSettings.Resolution = MAX_int32;
	TestFalse(TEXT("Invalid manual dimensions fail preview before allocation"), FTextureChannelPackerTextureService::BuildPreview(Request, ETextureChannelPackerPreviewMode::Composite).bSuccess);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTexturePackerReadCancellationTest, "TextureChannelPacker.Processing.CancelDuringRead",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Cancels inside source decoding, then decodes the same mip again to verify lock release. */
bool FTexturePackerReadCancellationTest::RunTest(const FString&)
{
	using TextureChannelPackerTexture::FProcessingGuard;
	ON_SCOPE_EXIT
	{
		FProcessingGuard::ShouldCancelForTesting = nullptr;
	};
	UTexture2D* Source = NewObject<UTexture2D>();
	TArray<uint8> Pixels;
	Pixels.Init(64, 128 * 128);
	Source->Source.Init(128, 128, 1, 1, TSF_G8, Pixels.GetData());
	FTextureChannelPackerRequest Request;
	FTextureChannelPackerMapping Mapping;
	Mapping.Source.Texture = Source;
	Request.Mappings.Add(Mapping);
	int32 Checks = 0;
	FProcessingGuard::ShouldCancelForTesting = [&Checks](FProcessingGuard::EStage Stage) { return Stage == FProcessingGuard::EStage::ReadBlock && ++Checks == 2; };
	const auto Cancelled = FTextureChannelPackerTextureService::BuildPreview(Request, ETextureChannelPackerPreviewMode::Red);
	TestFalse(TEXT("Read cancellation fails preview"), Cancelled.bSuccess);
	TestTrue(TEXT("Cancellation is reported"), Cancelled.Message.ToString().Contains(TEXT("cancelled")));
	FProcessingGuard::ShouldCancelForTesting = nullptr;
	const auto Retried = FTextureChannelPackerTextureService::BuildPreview(Request, ETextureChannelPackerPreviewMode::Red);
	TestTrue(TEXT("Same source can be read again after cancellation"), Retried.bSuccess);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTexturePackerWriteCancellationTest, "TextureChannelPacker.Processing.CancelDuringWrite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Cancels in the output row loop and verifies that no package is saved. */
bool FTexturePackerWriteCancellationTest::RunTest(const FString&)
{
	using TextureChannelPackerTexture::FProcessingGuard;
	ON_SCOPE_EXIT
	{
		FProcessingGuard::ShouldCancelForTesting = nullptr;
	};
	FTextureChannelPackerRequest Request;
	Request.OutputPackagePath = TEXT("/Game/TexturePackerCancel_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Request.OutputSettings.bUseFirstInputResolution = false;
	Request.OutputSettings.Resolution = 128;
	FTextureChannelPackerMapping Mapping;
	Mapping.Source.bUseConstant = true;
	Mapping.Source.ConstantValue = 64;
	Request.Mappings.Add(Mapping);
	int32 Checks = 0;
	FProcessingGuard::ShouldCancelForTesting = [&Checks](FProcessingGuard::EStage Stage) { return Stage == FProcessingGuard::EStage::WriteRow && ++Checks == 2; };
	const auto Result = FTextureChannelPackerTextureService::Execute(Request);
	TestFalse(TEXT("Write cancellation fails generation"), Result.bSuccess);
	TestEqual(TEXT("No saved assets reported"), Result.CreatedTextures.Num(), 0);
	TestTrue(TEXT("Write cancellation is reported"), Result.Message.ToString().Contains(TEXT("cancelled")));
	TestFalse(TEXT("Cancelled output has no file"), FPackageName::DoesPackageExist(Request.OutputPackagePath / Request.OutputBaseName));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTexturePackerUnpackCancellationTest, "TextureChannelPacker.Processing.CancelUnpackRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Requests cancellation after the first Unpack save and verifies rollback of that saved output. */
bool FTexturePackerUnpackCancellationTest::RunTest(const FString&)
{
	using TextureChannelPackerTexture::FProcessingGuard;
	using TextureChannelPackerTexture::FTextureAssetWriter;
	ON_SCOPE_EXIT
	{
		FProcessingGuard::ShouldCancelForTesting = nullptr;
		FTextureAssetWriter::ShouldFailSaveForTesting = nullptr;
	};
	UTexture2D* Source = NewObject<UTexture2D>();
	const uint8 Pixel = 64;
	Source->Source.Init(1, 1, 1, 1, TSF_G8, &Pixel);
	FTextureChannelPackerRequest Request;
	Request.Operation = ETextureChannelPackerOperation::Unpack;
	Request.UnpackSourceTexture = Source;
	Request.OutputPackagePath = TEXT("/Game/TexturePackerCancel_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Request.UnpackOutputs = { { ETextureChannelPackerChannel::Red, TEXT("_R"), true }, { ETextureChannelPackerChannel::Green, TEXT("_G"), true } };
	bool bSaved = false;
	FString FirstPackage;
	FTextureAssetWriter::ShouldFailSaveForTesting = [&](const FString& Package) { bSaved = true; FirstPackage = Package; return false; };
	FProcessingGuard::ShouldCancelForTesting = [&](FProcessingGuard::EStage) { return bSaved; };
	const auto Result = FTextureChannelPackerTextureService::Execute(Request);
	TestFalse(TEXT("Cancellation fails Unpack"), Result.bSuccess);
	TestTrue(TEXT("First save was reached"), bSaved);
	TestEqual(TEXT("Unpack reports no surviving outputs"), Result.CreatedTextures.Num(), 0);
	TestFalse(TEXT("First saved package rolled back"), FPackageName::DoesPackageExist(FirstPackage));
	IFileManager::Get().DeleteDirectory(*FPackageName::LongPackageNameToFilename(Request.OutputPackagePath));
	return true;
}
#endif
