#include "Services/TextureChannelPackerTextureService.h"

#if WITH_DEV_AUTOMATION_TESTS
	#include "AssetRegistry/AssetRegistryModule.h"
	#include "Engine/Texture2D.h"
	#include "HAL/FileManager.h"
	#include "Misc/AutomationTest.h"
	#include "Misc/PackageName.h"
	#include "UObject/Package.h"

namespace TexturePackerAssetTests
{
	/** @brief Removes only the unique packages created by this test, including registry entries and files. */
	static void Cleanup(const TArray<TWeakObjectPtr<UTexture2D>>& Textures, FAutomationTestBase& Test)
	{
		for (const auto& WeakTexture : Textures)
		{
			UTexture2D* Texture = WeakTexture.Get();
			if (!Texture)
			{
				continue;
			}
			UPackage* Package = Texture->GetOutermost();
			const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
			FAssetRegistryModule::AssetDeleted(Texture);
			Texture->ClearFlags(RF_Public | RF_Standalone);
			Texture->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
			Package->SetDirtyFlag(false);
			Test.TestTrue(TEXT("Test asset file removed"), IFileManager::Get().Delete(*Filename));
		}
	}
} // namespace TexturePackerAssetTests

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FTexturePackerAssetOperationTest, "TextureChannelPacker.Assets.PackingOperations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Enumerates persistent packing operation entry points. */
void FTexturePackerAssetOperationTest::GetTests(TArray<FString>& Names, TArray<FString>& Parameters) const
{
	for (const TCHAR* Name : { TEXT("Pack"), TEXT("Repack"), TEXT("Copy") })
	{
		Names.Add(Name);
		Parameters.Add(Name);
	}
}

/** @brief Checks real saved packages, mapped source bytes, unique naming and unchanged inputs. */
bool FTexturePackerAssetOperationTest::RunTest(const FString& Parameter)
{
	UTexture2D* Source = NewObject<UTexture2D>();
	const uint8 Original[] = { 192, 128, 64, 32 };
	Source->Source.Init(1, 1, 1, 1, TSF_BGRA8, Original);
	FTextureChannelPackerRequest Request;
	Request.Operation = Parameter == TEXT("Pack") ? ETextureChannelPackerOperation::Pack
		: Parameter == TEXT("Repack")			  ? ETextureChannelPackerOperation::Repack
												  : ETextureChannelPackerOperation::Copy;
	Request.OutputPackagePath = TEXT("/Game/TexturePackerTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Request.OutputBaseName = TEXT("T_Mapped");
	Request.OutputSettings.SourceFormat = ETextureChannelPackerOutputFormat::BGRA8;
	const ETextureChannelPackerChannel SourceChannels[] = { ETextureChannelPackerChannel::Blue, ETextureChannelPackerChannel::Red, ETextureChannelPackerChannel::Green, ETextureChannelPackerChannel::Alpha };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		FTextureChannelPackerMapping Mapping;
		Mapping.OutputChannel = static_cast<ETextureChannelPackerChannel>(Index);
		Mapping.Source.Channel = SourceChannels[Index];
		Mapping.Source.Texture = Source;
		Request.Mappings.Add(Mapping);
	}
	TArray<TWeakObjectPtr<UTexture2D>> Created;
	FString FirstName;
	for (int32 Iteration = 0; Iteration < 2; ++Iteration)
	{
		const auto Result = FTextureChannelPackerTextureService::Execute(Request);
		Created.Append(Result.CreatedTextures);
		if (!TestTrue(TEXT("Persistent operation succeeds"), Result.bSuccess) || !TestEqual(TEXT("One texture created"), Result.CreatedTextures.Num(), 1))
		{
			continue;
		}
		UTexture2D* Texture = Result.CreatedTextures[0].Get();
		TestTrue(TEXT("Package is saved to disk"), FPackageName::DoesPackageExist(Texture->GetOutermost()->GetName()));
		TestTrue(TEXT("Output source format matches request"), Texture->Source.GetFormat() == TSF_BGRA8);
		const uint8* Data = Texture->Source.LockMipReadOnly(0);
		if (TestNotNull(TEXT("Saved source data exists"), Data))
		{
			const uint8 Expected[] = { 128, 64, 192, 32 };
			for (int32 Index = 0; Index < 4; ++Index)
			{
				TestEqual(TEXT("Saved source byte matches mapping"), Data[Index], Expected[Index]);
			}
		}
		Texture->Source.UnlockMip(0);
		if (Iteration == 0)
		{
			FirstName = Texture->GetPathName();
		}
		else
		{
			TestNotEqual(TEXT("Second operation uses a unique asset name"), Texture->GetPathName(), FirstName);
		}
	}
	const uint8* Data = Source->Source.LockMipReadOnly(0);
	if (TestNotNull(TEXT("Input data remains readable"), Data))
	{
		TestTrue(TEXT("Input pixels are unchanged"), FMemory::Memcmp(Data, Original, sizeof(Original)) == 0);
	}
	Source->Source.UnlockMip(0);
	TexturePackerAssetTests::Cleanup(Created, *this);
	IFileManager::Get().DeleteDirectory(*FPackageName::LongPackageNameToFilename(Request.OutputPackagePath));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTexturePackerUnpackAssetsTest, "TextureChannelPacker.Assets.UnpackEnabledOutputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Checks that Unpack saves only enabled channels with grayscale source bytes and independent names. */
bool FTexturePackerUnpackAssetsTest::RunTest(const FString&)
{
	UTexture2D* Source = NewObject<UTexture2D>();
	const uint8 Original[] = { 192, 128, 64, 32 };
	Source->Source.Init(1, 1, 1, 1, TSF_BGRA8, Original);
	FTextureChannelPackerRequest Request;
	Request.Operation = ETextureChannelPackerOperation::Unpack;
	Request.UnpackSourceTexture = Source;
	Request.OutputPackagePath = TEXT("/Game/TexturePackerTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Request.UnpackOutputs = { { ETextureChannelPackerChannel::Red, TEXT("_R"), true },
		{ ETextureChannelPackerChannel::Green, TEXT("_G"), false }, { ETextureChannelPackerChannel::Alpha, TEXT("_A"), true } };
	const auto Result = FTextureChannelPackerTextureService::Execute(Request);
	TestTrue(TEXT("Unpack succeeds"), Result.bSuccess);
	if (TestEqual(TEXT("Only enabled outputs are saved"), Result.CreatedTextures.Num(), 2))
	{
		for (int32 Index = 0; Index < 2; ++Index)
		{
			UTexture2D* Texture = Result.CreatedTextures[Index].Get();
			TestTrue(TEXT("Unpack output is grayscale"), Texture->Source.GetFormat() == TSF_G8);
			TestFalse(TEXT("Unpack output is linear"), Texture->SRGB);
			const uint8* Data = Texture->Source.LockMipReadOnly(0);
			if (TestNotNull(TEXT("Unpacked source exists"), Data))
			{
				TestEqual(TEXT("Extracted value is correct"), Data[0], static_cast<uint8>(Index == 0 ? 64 : 32));
			}
			Texture->Source.UnlockMip(0);
		}
	}
	TexturePackerAssetTests::Cleanup(Result.CreatedTextures, *this);
	IFileManager::Get().DeleteDirectory(*FPackageName::LongPackageNameToFilename(Request.OutputPackagePath));
	return true;
}
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FTexturePackerOutputFormatsTest, "TextureChannelPacker.Assets.OutputFormats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Enumerates every explicitly selectable output source format. */
void FTexturePackerOutputFormatsTest::GetTests(TArray<FString>& Names, TArray<FString>& Parameters) const
{
	const TCHAR* Formats[] = { TEXT("G8"), TEXT("G16"), TEXT("R16F"), TEXT("R32F"), TEXT("BGRA8"), TEXT("RGBA16"), TEXT("RGBA16F"), TEXT("RGBA32F") };
	for (int32 Index = 0; Index < 8; ++Index)
	{
		Names.Add(Formats[Index]);
		Parameters.Add(FString::FromInt(Index));
	}
}

/** @brief Saves each format and verifies its source layout and component values through an independent read-back. */
bool FTexturePackerOutputFormatsTest::RunTest(const FString& Parameter)
{
	const int32 FormatIndex = FCString::Atoi(*Parameter);
	const ETextureSourceFormat Formats[] = { TSF_G8, TSF_G16, TSF_R16F, TSF_R32F, TSF_BGRA8, TSF_RGBA16, TSF_RGBA16F, TSF_RGBA32F };
	FTextureChannelPackerRequest Request;
	Request.OutputPackagePath = TEXT("/Game/TexturePackerTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Request.OutputSettings.bUseFirstInputResolution = false;
	Request.OutputSettings.Resolution = 1;
	Request.OutputSettings.SourceFormat = static_cast<ETextureChannelPackerOutputFormat>(FormatIndex + 1);
	const uint8 Values[] = { 64, 128, 192, 32 };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		FTextureChannelPackerMapping Mapping;
		Mapping.OutputChannel = static_cast<ETextureChannelPackerChannel>(Index);
		Mapping.Source.bUseConstant = true;
		Mapping.Source.ConstantValue = Values[Index];
		Request.Mappings.Add(Mapping);
	}
	const auto Result = FTextureChannelPackerTextureService::Execute(Request);
	if (TestTrue(TEXT("Output format is saved"), Result.bSuccess) && TestEqual(TEXT("One output is produced"), Result.CreatedTextures.Num(), 1))
	{
		UTexture2D* Texture = Result.CreatedTextures[0].Get();
		TestTrue(TEXT("Source format matches the selected writer format"), Texture->Source.GetFormat() == Formats[FormatIndex]);
		TestTrue(TEXT("Output file exists"), FPackageName::DoesPackageExist(Texture->GetOutermost()->GetName()));
		for (int32 Component = 0; Component < 4; ++Component)
		{
			FTextureChannelPackerRequest ReadBack;
			ReadBack.OutputSettings.SourceFormat = ETextureChannelPackerOutputFormat::R32F;
			FTextureChannelPackerMapping Mapping;
			Mapping.Source.Texture = Texture;
			Mapping.Source.Channel = static_cast<ETextureChannelPackerChannel>(Component);
			ReadBack.Mappings.Add(Mapping);
			const auto Preview = FTextureChannelPackerTextureService::BuildPreview(ReadBack, ETextureChannelPackerPreviewMode::Red);
			if (TestTrue(TEXT("Saved channel reads back"), Preview.bSuccess))
			{
				auto& Mip = Preview.PreviewTexture->GetPlatformData()->Mips[0];
				const uint8* Pixels = static_cast<const uint8*>(Mip.BulkData.LockReadOnly());
				if (TestNotNull(TEXT("Read-back pixels exist"), Pixels))
				{
					const uint8 Expected = FormatIndex < 4 ? static_cast<uint8>(Component == 3 ? 255 : 64) : Values[Component];
					TestEqual(TEXT("Stored value survives output conversion"), Pixels[2], Expected);
				}
				Mip.BulkData.Unlock();
			}
		}
	}
	TexturePackerAssetTests::Cleanup(Result.CreatedTextures, *this);
	IFileManager::Get().DeleteDirectory(*FPackageName::LongPackageNameToFilename(Request.OutputPackagePath));
	return true;
}
#endif
