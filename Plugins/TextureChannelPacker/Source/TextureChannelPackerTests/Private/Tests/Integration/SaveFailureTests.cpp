#include "Processing/TextureChannelPackerTextureAssetWriter.h"
#include "Services/TextureChannelPackerTextureService.h"

#if WITH_DEV_AUTOMATION_TESTS
	#include "AssetRegistry/AssetRegistryModule.h"
	#include "Engine/Texture2D.h"
	#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
	#include "Misc/AutomationTest.h"
	#include "Misc/PackageName.h"
	#include "Misc/ScopeExit.h"
	#include "UObject/Package.h"

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FTexturePackerSaveFailureTest, "TextureChannelPacker.Assets.SaveFailures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Enumerates save rejection, successful rollback and a real read-only rollback failure. */
void FTexturePackerSaveFailureTest::GetTests(TArray<FString>& Names, TArray<FString>& Parameters) const
{
	for (const TCHAR* Name : { TEXT("PackCleanup"), TEXT("UnpackRollback"), TEXT("RollbackDeleteFailure") })
	{
		Names.Add(Name);
		Parameters.Add(Name);
	}
}

/** @brief Fails only saves owned by this test and verifies object, registry and file cleanup. */
bool FTexturePackerSaveFailureTest::RunTest(const FString& Parameter)
{
	using TextureChannelPackerTexture::FTextureAssetWriter;
	FTextureChannelPackerRequest Request;
	Request.OutputPackagePath = TEXT("/Game/TexturePackerFailure_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Request.OutputBaseName = TEXT("T_Test");
	Request.OutputSettings.bUseFirstInputResolution = false;
	Request.OutputSettings.Resolution = 1;
	FTextureChannelPackerMapping Mapping;
	Mapping.Source.bUseConstant = true;
	Mapping.Source.ConstantValue = 64;
	Request.Mappings.Add(Mapping);
	FTextureChannelPackerRequest ControlRequest = Request;
	ControlRequest.OutputBaseName += TEXT("_R");
	const auto Control = FTextureChannelPackerTextureService::Execute(ControlRequest);
	if (!TestTrue(TEXT("Pre-existing control asset saved"), Control.bSuccess))
	{
		return false;
	}
	TArray<FString> AttemptedPackages;
	TArray<TWeakObjectPtr<UTexture2D>> Survivors;
	const bool bDeleteFailure = Parameter == TEXT("RollbackDeleteFailure");
	ON_SCOPE_EXIT
	{
		FTextureAssetWriter::ShouldFailSaveForTesting = nullptr;
		for (const FString& Package : AttemptedPackages)
		{
			const FString Filename = FPackageName::LongPackageNameToFilename(Package, FPackageName::GetAssetPackageExtension());
			if (IFileManager::Get().FileExists(*Filename))
			{
				FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*Filename, false);
			}
		}
		FText CleanupError;
		for (const auto& Texture : Survivors)
		{
			TestTrue(TEXT("Recovery output cleaned after test"), FTextureAssetWriter::DiscardTexture(Texture.Get(), true, CleanupError));
		}
		for (const auto& Texture : Control.CreatedTextures)
		{
			TestTrue(TEXT("Control asset cleaned after test"), FTextureAssetWriter::DiscardTexture(Texture.Get(), true, CleanupError));
		}
		IFileManager::Get().DeleteDirectory(*FPackageName::LongPackageNameToFilename(Request.OutputPackagePath));
	};
	if (Parameter != TEXT("PackCleanup"))
	{
		UTexture2D* Source = NewObject<UTexture2D>();
		const uint8 Pixel[] = { 192, 128, 64, 255 };
		Source->Source.Init(1, 1, 1, 1, TSF_BGRA8, Pixel);
		Request.Operation = ETextureChannelPackerOperation::Unpack;
		Request.UnpackSourceTexture = Source;
		Request.UnpackOutputs = { { ETextureChannelPackerChannel::Red, TEXT("_R"), true }, { ETextureChannelPackerChannel::Green, TEXT("_G"), true } };
	}
	FTextureAssetWriter::ShouldFailSaveForTesting = [&](const FString& Package) {
		AttemptedPackages.Add(Package);
		if (Parameter == TEXT("PackCleanup"))
		{
			return true;
		}
		if (AttemptedPackages.Num() == 2 && bDeleteFailure)
		{
			const FString FirstFile = FPackageName::LongPackageNameToFilename(AttemptedPackages[0], FPackageName::GetAssetPackageExtension());
			TestTrue(TEXT("Test-owned first output is made read-only"), FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*FirstFile, true));
		}
		return AttemptedPackages.Num() == 2;
	};
	const auto Result = FTextureChannelPackerTextureService::Execute(Request);
	Survivors = Result.CreatedTextures;
	TestFalse(TEXT("Injected save failure is reported"), Result.bSuccess);
	TestTrue(TEXT("Failure contains a message"), !Result.Message.IsEmpty());
	TestEqual(TEXT("Only cleanup failures retain outputs"), Result.CreatedTextures.Num(), bDeleteFailure ? 1 : 0);
	for (int32 Index = 0; Index < AttemptedPackages.Num(); ++Index)
	{
		const FString& Package = AttemptedPackages[Index];
		const bool bRetained = bDeleteFailure && Index == 0;
		TestEqual(TEXT("Disk output matches rollback result"), FPackageName::DoesPackageExist(Package), bRetained);
		const FString ObjectPath = Package + TEXT(".") + FPackageName::GetShortName(Package);
		TestEqual(TEXT("Registry matches rollback result"), FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(FSoftObjectPath(ObjectPath)).IsValid(), bRetained);
		TestEqual(TEXT("Object at original path matches rollback result"), FindObject<UTexture2D>(nullptr, *ObjectPath) != nullptr, bRetained);
		if (!bRetained)
		{
			if (UPackage* OldPackage = FindPackage(nullptr, *Package))
			{
				TestFalse(TEXT("Discarded package is not dirty"), OldPackage->IsDirty());
			}
		}
	}
	if (bDeleteFailure && AttemptedPackages.Num() > 0)
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(AttemptedPackages[0], FPackageName::GetAssetPackageExtension());
		TestTrue(TEXT("Cleanup failure names the retained file"), Result.Message.ToString().Contains(Filename));
	}
	TestTrue(TEXT("Existing control asset was not deleted"), FPackageName::DoesPackageExist(Control.CreatedTextures[0]->GetOutermost()->GetName()));
	return true;
}
#endif
