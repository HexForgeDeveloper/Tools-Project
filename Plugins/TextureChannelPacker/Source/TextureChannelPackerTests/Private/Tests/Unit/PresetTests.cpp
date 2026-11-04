#include "Services/TextureChannelPackerPatternService.h"

#if WITH_DEV_AUTOMATION_TESTS
	#include "Misc/AutomationTest.h"
	#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextureChannelPackerUnpackPresetTest,
	"TextureChannelPacker.Presets.UnpackRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/**
 * @brief Exercises capture, config text serialization and application of disabled Unpack outputs.
 * @return True after recording assertions; failures determine the automation result.
 */
bool FTextureChannelPackerUnpackPresetTest::RunTest(const FString&)
{
	FTextureChannelPackerRequest Request;
	Request.Operation = ETextureChannelPackerOperation::Unpack;
	Request.OutputSettings.SourceFormat = ETextureChannelPackerOutputFormat::G16;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		FTextureChannelPackerUnpackOutput Output;
		Output.SourceChannel = static_cast<ETextureChannelPackerChannel>(Index);
		Output.OutputSuffix = FString::Printf(TEXT("_Channel%d"), Index);
		Output.bEnabled = Index == 1;
		Request.UnpackOutputs.Add(Output);
	}
	const FTextureChannelPackerPreset Preset = FTextureChannelPackerPatternService::CreatePreset(Request, TEXT("Custom Unpack"), TEXT(""));
	FString Serialized;
	// SaveConfig exports array elements with the value itself as Defaults, forcing all fields to be written.
	FTextureChannelPackerPreset::StaticStruct()->ExportText(Serialized, &Preset, &Preset, nullptr, PPF_None, nullptr);
	FTextureChannelPackerPreset Reloaded;
	TestNotNull(TEXT("Preset config text imports"), FTextureChannelPackerPreset::StaticStruct()->ImportText(*Serialized, &Reloaded, nullptr, PPF_None, GWarn, TEXT("UnpackPreset")));
	TestEqual(TEXT("Empty naming suffix survives serialization"), Reloaded.OutputSuffix, FString());
	FTextureChannelPackerRequest Applied;
	FTextureChannelPackerPatternService::ApplyPresetPatterns(Reloaded, {}, Applied);
	TestTrue(TEXT("Unpack operation survives"), Applied.Operation == Request.Operation);
	TestTrue(TEXT("Output format survives"), Applied.OutputSettings.SourceFormat == Request.OutputSettings.SourceFormat);
	if (TestEqual(TEXT("Disabled outputs remain present"), Applied.UnpackOutputs.Num(), Request.UnpackOutputs.Num()))
	{
		for (int32 Index = 0; Index < Request.UnpackOutputs.Num(); ++Index)
		{
			TestEqual(TEXT("Enabled flag survives"), Applied.UnpackOutputs[Index].bEnabled, Request.UnpackOutputs[Index].bEnabled);
			TestEqual(TEXT("Channel suffix survives"), Applied.UnpackOutputs[Index].OutputSuffix, Request.UnpackOutputs[Index].OutputSuffix);
			TestTrue(TEXT("Source channel survives"), Applied.UnpackOutputs[Index].SourceChannel == Request.UnpackOutputs[Index].SourceChannel);
		}
	}
	FTextureChannelPackerPresetChannel LegacyChannel;
	TestNotNull(TEXT("Legacy channel imports without the new field"), FTextureChannelPackerPresetChannel::StaticStruct()->ImportText(TEXT("(SourceChannel=Green,OutputSuffix=\"_R\")"), &LegacyChannel, nullptr, PPF_None, GWarn, TEXT("LegacyPreset")));
	TestTrue(TEXT("Legacy channels default to enabled"), LegacyChannel.bEnabled);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextureChannelPackerPackingPresetTest,
	"TextureChannelPacker.Presets.PackingRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/**
 * @brief Preserves naming rules, mappings and constants for Pack, Repack and Copy presets.
 * @return True after recording assertions; failures determine the automation result.
 */
bool FTextureChannelPackerPackingPresetTest::RunTest(const FString&)
{
	for (const ETextureChannelPackerOperation Operation : { ETextureChannelPackerOperation::Pack, ETextureChannelPackerOperation::Repack, ETextureChannelPackerOperation::Copy })
	{
		FTextureChannelPackerRequest Request;
		Request.Operation = Operation;
		Request.OutputSettings.bUseFirstInputResolution = false;
		Request.OutputSettings.Resolution = 32;
		FTextureChannelPackerMapping Mapping;
		Mapping.Source.Channel = ETextureChannelPackerChannel::Blue;
		Mapping.OutputChannel = ETextureChannelPackerChannel::Green;
		Mapping.Source.bUseConstant = true;
		Mapping.Source.ConstantValue = 123;
		Mapping.SourcePattern = TEXT("_ORM,_RMO");
		Request.Mappings.Add(Mapping);
		const FTextureChannelPackerPreset Preset = FTextureChannelPackerPatternService::CreatePreset(Request, TEXT("Custom packing"), TEXT("_RMO"));
		TestEqual(TEXT("Active naming suffix is retained"), Preset.OutputSuffix, FString(TEXT("_RMO")));
		TestEqual(TEXT("Preset display name is retained"), Preset.Name, FString(TEXT("Custom packing")));
		FTextureChannelPackerRequest Applied;
		Applied.OutputBaseName = TEXT("T_Wall");
		FTextureChannelPackerPatternService::ApplyPresetPatterns(Preset, {}, Applied);
		TestEqual(TEXT("Retained suffix controls generated name"), Applied.OutputBaseName, FString(TEXT("T_Wall_RMO")));
		TestTrue(TEXT("Operation is retained"), Applied.Operation == Operation);
		TestEqual(TEXT("Manual resolution is retained"), Applied.OutputSettings.Resolution, 32);
		TestFalse(TEXT("Manual resolution mode is retained"), Applied.OutputSettings.bUseFirstInputResolution);
		if (TestEqual(TEXT("Mapping count is retained"), Applied.Mappings.Num(), 1))
		{
			TestTrue(TEXT("Source channel is retained"), Applied.Mappings[0].Source.Channel == Mapping.Source.Channel);
			TestTrue(TEXT("Output channel is retained"), Applied.Mappings[0].OutputChannel == Mapping.OutputChannel);
			TestTrue(TEXT("Constant mode is retained"), Applied.Mappings[0].Source.bUseConstant);
			TestEqual(TEXT("Constant value is retained"), Applied.Mappings[0].Source.ConstantValue, Mapping.Source.ConstantValue);
			TestEqual(TEXT("Matching pattern is retained"), Applied.Mappings[0].SourcePattern, Mapping.SourcePattern);
		}
	}
	return true;
}
#endif
