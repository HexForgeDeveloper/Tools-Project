#include "Settings/TextureChannelPackerSettings.h"

#if WITH_DEV_AUTOMATION_TESTS
	#include "HAL/FileManager.h"
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
	Writer->Presets = { Preset };
	FConfigFile IsolatedConfig;
	IsolatedConfig.bCanSaveAllSections = true;
	GConfig->Add(Filename, IsolatedConfig);
	Writer->SaveConfig(CPF_Config, *Filename, GConfig, false);
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
	GConfig->UnloadFile(Filename);
	TestTrue(TEXT("Isolated ini file is removed"), IFileManager::Get().Delete(*Filename));
	return true;
}
#endif
