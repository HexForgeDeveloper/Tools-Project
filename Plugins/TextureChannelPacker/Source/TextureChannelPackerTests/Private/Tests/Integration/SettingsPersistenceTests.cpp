#include "Settings/TextureChannelPackerSettings.h"

#if WITH_DEV_AUTOMATION_TESTS
	#include "HAL/FileManager.h"
	#include "HAL/PlatformFileManager.h"
	#include "Misc/AutomationTest.h"
	#include "Misc/ConfigCacheIni.h"
	#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextureChannelPackerPresetIniTest,
	"TextureChannelPacker.Presets.IsolatedIniPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/**
 * @brief Verifies real ini persistence without writing project settings or changing the settings CDO.
 * @return True after recording assertions; failures determine the automation result.
 */
bool FTextureChannelPackerPresetIniTest::RunTest(const FString&)
{
	const FString Filename = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
		/ FString::Printf(TEXT("TextureChannelPackerPreset-%s.ini"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	UTextureChannelPackerSettings* Writer = NewObject<UTextureChannelPackerSettings>();
	FTextureChannelPackerPreset Preset;
	Preset.Name = TEXT("Persistence test");
	Preset.Operation = ETextureChannelPackerOperation::Unpack;
	Preset.OutputSuffix.Reset();
	FTextureChannelPackerPresetChannel Channel;
	Channel.bEnabled = false;
	Channel.OutputSuffix = TEXT("_Custom");
	Preset.Channels.Add(Channel);
	Writer->Presets.Reset();
	TestTrue(TEXT("Production preset save path updates isolated ini"), Writer->SavePreset(Preset, Filename));
	TestTrue(TEXT("Isolated ini file is written"), IFileManager::Get().FileExists(*Filename));
	// Discard cached values so the reader must reload the data written to disk.
	GConfig->UnloadFile(Filename);
	UTextureChannelPackerSettings* Reader = NewObject<UTextureChannelPackerSettings>();
	Reader->Presets.Reset();
	Reader->LoadConfig(nullptr, *Filename);
	if (TestEqual(TEXT("Saved preset is reloaded"), Reader->Presets.Num(), 1))
	{
		TestEqual(TEXT("Empty suffix survives ini persistence"), Reader->Presets[0].OutputSuffix, FString());
		if (TestEqual(TEXT("Disabled channel is reloaded"), Reader->Presets[0].Channels.Num(), 1))
		{
			TestFalse(TEXT("Disabled state survives ini persistence"), Reader->Presets[0].Channels[0].bEnabled);
			TestEqual(TEXT("Custom channel suffix survives"), Reader->Presets[0].Channels[0].OutputSuffix, Channel.OutputSuffix);
		}
	}
	TestTrue(TEXT("Destination can be made read-only"), FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*Filename, true));
	FTextureChannelPackerPreset Replacement = Preset;
	Replacement.Name = TEXT("Rejected replacement");
	TestFalse(TEXT("Read-only destination rejects save"), Writer->SavePreset(Replacement, Filename));
	TestEqual(TEXT("Rejected save restores the original array size"), Writer->Presets.Num(), 1);
	TestNull(TEXT("Rejected preset is not retained in memory"), Writer->FindPreset(Replacement.Name));
	TestTrue(TEXT("Destination writable state is restored"), FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*Filename, false));
	GConfig->UnloadFile(Filename);
	TestTrue(TEXT("Isolated ini file is removed"), IFileManager::Get().Delete(*Filename));
	return true;
}
#endif
