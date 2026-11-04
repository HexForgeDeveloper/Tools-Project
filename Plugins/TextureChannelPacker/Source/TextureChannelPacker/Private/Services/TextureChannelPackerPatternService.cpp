// Copyright 2025 DimAlek. All Rights Reserved.

#include "Services/TextureChannelPackerPatternService.h"
#include "Engine/Texture.h"
#include "Services/TextureChannelPackerPathService.h"

/**
 * Internal helper functions used for texture name pattern processing
 */
namespace TextureChannelPackerPattern
{
	/**
	 * Splits a pattern list into individual patterns.
	 *
	 * Supports both ';' and ',' as separators and trims whitespace from
	 * each resulting pattern.
	 *
	 * @param PatternList Patterns separated by commas, semicolons or a mixture of both.
	 * @return Trimmed patterns. Empty entries are ignored by the matching caller.
	 */
	static TArray<FString> SplitPatterns(const FString& PatternList)
	{
		TArray<FString> Patterns;
		const FString NormalizedPatterns = PatternList.Replace(TEXT(","), TEXT(";"));
		NormalizedPatterns.ParseIntoArray(Patterns, TEXT(";"), true);

		for (FString& Pattern : Patterns)
		{
			Pattern.TrimStartAndEndInline();
		}

		return Patterns;
	}
} // namespace TextureChannelPackerPattern

bool FTextureChannelPackerPatternService::DoesNameMatchPattern(const FString& TextureName, const FString& PatternList)
{
	if (PatternList.TrimStartAndEnd().IsEmpty())
	{
		return false;
	}

	for (const FString& Pattern : TextureChannelPackerPattern::SplitPatterns(PatternList))
	{
		if (Pattern.IsEmpty())
		{
			continue;
		}

		if (TextureName.MatchesWildcard(Pattern) || TextureName.Contains(Pattern, ESearchCase::IgnoreCase) || TextureName.EndsWith(Pattern, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

FString FTextureChannelPackerPatternService::BuildBaseNameFromTextures(const TArray<UTexture*>& Textures, const FString& FallbackName)
{
	TArray<FString> Names;
	for (const UTexture* Texture : Textures)
	{
		if (Texture)
		{
			Names.Add(Texture->GetName());
		}
	}

	if (Names.Num() == 0)
	{
		return FTextureChannelPackerPathService::SanitizeAssetName(FallbackName);
	}

	FString CommonPrefix = Names[0];
	for (int32 Index = 1; Index < Names.Num(); ++Index)
	{
		const FString& Name = Names[Index];
		int32 CommonLength = 0;
		const int32 MaxLength = FMath::Min(CommonPrefix.Len(), Name.Len());
		for (int32 CharIndex = 0; CharIndex < MaxLength; ++CharIndex)
		{
			if (CommonPrefix[CharIndex] != Name[CharIndex])
			{
				break;
			}

			++CommonLength;
		}

		CommonPrefix = CommonPrefix.Left(CommonLength);
	}

	CommonPrefix.TrimStartAndEndInline();
	while (CommonPrefix.EndsWith(TEXT("_")) || CommonPrefix.EndsWith(TEXT("-")) || CommonPrefix.EndsWith(TEXT(".")))
	{
		CommonPrefix.LeftChopInline(1);
	}

	if (CommonPrefix.Len() < 3)
	{
		CommonPrefix = Names[0];
	}

	if (!CommonPrefix.StartsWith(TEXT("T_")))
	{
		CommonPrefix = TEXT("T_") + CommonPrefix;
	}

	return FTextureChannelPackerPathService::SanitizeAssetName(CommonPrefix);
}

FTextureChannelPackerPreset FTextureChannelPackerPatternService::CreatePreset(const FTextureChannelPackerRequest& Request, const FString& Name, const FString& OutputSuffix)
{
	FTextureChannelPackerPreset Preset;
	Preset.Name = Name;
	Preset.Operation = Request.Operation;
	Preset.OutputSettings = Request.OutputSettings;
	Preset.OutputSuffix = OutputSuffix;
	if (Request.Operation == ETextureChannelPackerOperation::Unpack)
	{
		for (const FTextureChannelPackerUnpackOutput& Output : Request.UnpackOutputs)
		{
			FTextureChannelPackerPresetChannel Channel;
			Channel.SourceChannel = Output.SourceChannel;
			Channel.OutputSuffix = Output.OutputSuffix;
			Channel.bEnabled = Output.bEnabled;
			Channel.SourcePattern = TEXT("_Packed");
			Preset.Channels.Add(Channel);
		}
	}
	else
	{
		for (const FTextureChannelPackerMapping& Mapping : Request.Mappings)
		{
			FTextureChannelPackerPresetChannel Channel;
			Channel.SourceChannel = Mapping.Source.Channel;
			Channel.OutputChannel = Mapping.OutputChannel;
			Channel.SourcePattern = Mapping.SourcePattern;
			Channel.bUseConstant = Mapping.Source.bUseConstant;
			Channel.ConstantValue = Mapping.Source.ConstantValue;
			Preset.Channels.Add(Channel);
		}
	}
	return Preset;
}

void FTextureChannelPackerPatternService::ApplyPresetPatterns(const FTextureChannelPackerPreset& Preset, const TArray<UTexture*>& Textures, FTextureChannelPackerRequest& Request)
{
	Request.Operation = Preset.Operation;
	Request.OutputSettings = Preset.OutputSettings;
	Request.Mappings.Reset();
	Request.UnpackOutputs.Reset();

	for (const FTextureChannelPackerPresetChannel& PresetChannel : Preset.Channels)
	{
		UTexture* MatchingTexture = nullptr;
		for (UTexture* Texture : Textures)
		{
			if (Texture && DoesNameMatchPattern(Texture->GetName(), PresetChannel.SourcePattern))
			{
				MatchingTexture = Texture;
				break;
			}
		}

		if (Preset.Operation == ETextureChannelPackerOperation::Unpack)
		{
			if (MatchingTexture)
			{
				Request.UnpackSourceTexture = MatchingTexture;
			}

			FTextureChannelPackerUnpackOutput Output;
			Output.SourceChannel = PresetChannel.SourceChannel;
			Output.OutputSuffix = PresetChannel.OutputSuffix;
			Output.bEnabled = PresetChannel.bEnabled;
			Request.UnpackOutputs.Add(Output);
			continue;
		}

		FTextureChannelPackerMapping Mapping;
		Mapping.OutputChannel = PresetChannel.OutputChannel;
		Mapping.Source.Channel = PresetChannel.SourceChannel;
		Mapping.Source.Texture = MatchingTexture;
		Mapping.Source.bUseConstant = PresetChannel.bUseConstant;
		Mapping.Source.ConstantValue = PresetChannel.ConstantValue;
		Mapping.SourcePattern = PresetChannel.SourcePattern;

		if (Preset.Operation == ETextureChannelPackerOperation::Repack && !MatchingTexture && Textures.Num() > 0)
		{
			Mapping.Source.Texture = Textures[0];
		}

		Request.Mappings.Add(Mapping);
	}

	const FString BaseName = BuildBaseNameFromTextures(Textures, Request.OutputBaseName);
	Request.OutputBaseName = FTextureChannelPackerPathService::SanitizeAssetName(BaseName + Preset.OutputSuffix);
}
