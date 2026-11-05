#include "Services/TextureChannelPackerTextureService.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTexturePackerOutputDirectoryTest,
	"TextureChannelPacker.Assets.BlockedOutputDirectory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Exercises a real directory creation failure without failure injection or changing access permissions. */
bool FTexturePackerOutputDirectoryTest::RunTest(const FString&)
{
	FTextureChannelPackerRequest Request;
	Request.OutputPackagePath = TEXT("/Game/TexturePackerBlocked_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Request.OutputBaseName = TEXT("T_Test");
	Request.OutputSettings.bUseFirstInputResolution = false;
	Request.OutputSettings.Resolution = 1;
	FTextureChannelPackerMapping Mapping;
	Mapping.Source.bUseConstant = true;
	Mapping.Source.ConstantValue = 64;
	Request.Mappings.Add(Mapping);
	const FString Blocker = FPackageName::LongPackageNameToFilename(Request.OutputPackagePath);
	const FString Marker = TEXT("Test-owned directory blocker");
	if (!TestTrue(TEXT("Test-owned blocker file created"), FFileHelper::SaveStringToFile(Marker, *Blocker)))
	{
		return false;
	}
	ON_SCOPE_EXIT
	{
		TestTrue(TEXT("Only the test-owned blocker is removed"), IFileManager::Get().Delete(*Blocker));
	};
	const auto Result = FTextureChannelPackerTextureService::Execute(Request);
	TestFalse(TEXT("Inaccessible output directory rejects generation"), Result.bSuccess);
	TestEqual(TEXT("Failed generation returns no outputs"), Result.CreatedTextures.Num(), 0);
	TestTrue(TEXT("Error identifies output directory"), Result.Message.ToString().Contains(TEXT("Could not create output directory")));
	const FString Package = Request.OutputPackagePath / Request.OutputBaseName;
	const FString ObjectPath = Package + TEXT(".") + Request.OutputBaseName;
	TestFalse(TEXT("No asset was registered"), FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(FSoftObjectPath(ObjectPath)).IsValid());
	TestNull(TEXT("Failed object was removed from its package"), FindObject<UTexture2D>(nullptr, *ObjectPath));
	if (UPackage* FailedPackage = FindPackage(nullptr, *Package))
	{
		TestFalse(TEXT("Failed package is not dirty"), FailedPackage->IsDirty());
	}
	FString Contents;
	TestTrue(TEXT("Existing blocker remains readable"), FFileHelper::LoadFileToString(Contents, *Blocker));
	TestEqual(TEXT("Existing file was not overwritten"), Contents, Marker);
	return true;
}
#endif
