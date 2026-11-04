// Copyright 2025 DimAlek. All Rights Reserved.

#include "Processing/TextureChannelPackerTextureOperations.h"
#include "Processing/TextureChannelPackerChannelDataOps.h"
#include "Processing/TextureChannelPackerTextureAssetWriter.h"
#include "Processing/TextureChannelPackerTextureSourceCodec.h"
#include "Engine/Texture2D.h"
#include "Services/TextureChannelPackerPathService.h"

#define LOCTEXT_NAMESPACE "TextureChannelPackerTextureService"

namespace TextureChannelPackerTexture
{
	bool FTextureOperations::ResolveChannels(const TArray<FTextureChannelPackerMapping>& Mappings, TArray<FResolvedOutputChannel>& OutChannels, FIntPoint& OutFirstSourceSize, FText& OutError)
	{
		OutChannels.Reset();
		OutFirstSourceSize = FIntPoint::ZeroValue;
		bool bHasSourceTexture = false;
		bool bHasExplicitNonDefaultConstant = false;

		for (const FTextureChannelPackerMapping& Mapping : Mappings)
		{
			FResolvedOutputChannel ResolvedChannel;
			ResolvedChannel.OutputChannel = Mapping.OutputChannel;
			ResolvedChannel.bUseConstant = Mapping.Source.bUseConstant;
			ResolvedChannel.ConstantValue = Mapping.Source.ConstantValue;

			if (ResolvedChannel.bUseConstant)
			{
				bHasExplicitNonDefaultConstant |= Mapping.OutputChannel != ETextureChannelPackerChannel::Alpha || Mapping.Source.ConstantValue != 255;
			}
			else if (Mapping.Source.Texture.IsValid())
			{
				if (!FTextureSourceCodec::ReadTextureChannel(Mapping.Source.Texture.Get(), Mapping.Source.Channel, ResolvedChannel.Data, OutError))
				{
					return false;
				}

				ResolvedChannel.bHasData = true;
				bHasSourceTexture = true;
				if (OutFirstSourceSize == FIntPoint::ZeroValue)
				{
					OutFirstSourceSize = FIntPoint(ResolvedChannel.Data.Width, ResolvedChannel.Data.Height);
				}
			}

			OutChannels.Add(MoveTemp(ResolvedChannel));
		}

		if (!bHasSourceTexture && !bHasExplicitNonDefaultConstant)
		{
			OutError = LOCTEXT("NoSourceChannels", "Select at least one source texture or set a non-alpha Const value before generating.");
			return false;
		}

		return true;
	}

	FTextureChannelPackerResult FTextureOperations::ExecutePackLike(const FTextureChannelPackerRequest& Request)
	{
		FTextureChannelPackerResult Result;
		TArray<FResolvedOutputChannel> ResolvedChannels;
		FIntPoint FirstSourceSize;
		if (!ResolveChannels(Request.Mappings, ResolvedChannels, FirstSourceSize, Result.Message))
		{
			return Result;
		}

		if (ResolvedChannels.Num() == 0)
		{
			Result.Message = LOCTEXT("NoChannels", "No channel mappings were provided.");
			return Result;
		}

		const FIntPoint TargetSize = FChannelDataOps::ResolveTargetSize(Request.OutputSettings, FirstSourceSize);
		const ETextureSourceFormat OutputFormat = FChannelDataOps::ResolveOutputFormat(Request.OutputSettings, ResolvedChannels, false);

		FText CreateError;
		UTexture2D* Texture = FTextureAssetWriter::CreateTextureAsset(Request.OutputPackagePath, Request.OutputBaseName, TargetSize.X, TargetSize.Y, OutputFormat, ResolvedChannels, Request.OutputSettings, CreateError);
		if (!Texture)
		{
			Result.Message = CreateError;
			return Result;
		}

		Result.bSuccess = true;
		Result.CreatedTextures.Add(Texture);
		Result.Message = FText::Format(LOCTEXT("CreatedAndSavedTexture", "Created and saved texture: {0}"), FText::FromString(Texture->GetPathName()));
		return Result;
	}

	FTextureChannelPackerResult FTextureOperations::ExecuteUnpack(const FTextureChannelPackerRequest& Request)
	{
		FTextureChannelPackerResult Result;
		if (!Request.UnpackSourceTexture.IsValid())
		{
			Result.Message = LOCTEXT("MissingUnpackSource", "Select a packed source texture before unpacking.");
			return Result;
		}

		TArray<FTextureChannelPackerUnpackOutput> EnabledOutputs;
		for (const FTextureChannelPackerUnpackOutput& Output : Request.UnpackOutputs)
		{
			if (Output.bEnabled)
			{
				EnabledOutputs.Add(Output);
			}
		}

		if (EnabledOutputs.Num() == 0)
		{
			Result.Message = LOCTEXT("NoUnpackChannels", "Select at least one channel to unpack.");
			return Result;
		}

		for (const FTextureChannelPackerUnpackOutput& Output : EnabledOutputs)
		{
			FChannelData SourceChannel;
			if (!FTextureSourceCodec::ReadTextureChannel(Request.UnpackSourceTexture.Get(), Output.SourceChannel, SourceChannel, Result.Message))
			{
				return Result;
			}

			FResolvedOutputChannel ResolvedChannel;
			ResolvedChannel.OutputChannel = ETextureChannelPackerChannel::Red;
			ResolvedChannel.bHasData = true;
			ResolvedChannel.Data = MoveTemp(SourceChannel);

			TArray<FResolvedOutputChannel> Channels;
			Channels.Add(MoveTemp(ResolvedChannel));

			FTextureChannelPackerOutputSettings OutputSettings = Request.OutputSettings;
			OutputSettings.CompressionSettings = TC_Grayscale;
			OutputSettings.bSRGB = false;

			const FIntPoint TargetSize = FChannelDataOps::ResolveTargetSize(OutputSettings, FIntPoint(Channels[0].Data.Width, Channels[0].Data.Height));
			const ETextureSourceFormat OutputFormat = FChannelDataOps::ResolveOutputFormat(OutputSettings, Channels, true);
			const FString AssetName = FTextureChannelPackerPathService::SanitizeAssetName(Request.OutputBaseName + Output.OutputSuffix);

			FText CreateError;
			UTexture2D* Texture = FTextureAssetWriter::CreateTextureAsset(Request.OutputPackagePath, AssetName, TargetSize.X, TargetSize.Y, OutputFormat, Channels, OutputSettings, CreateError);
			if (!Texture)
			{
				Result.Message = CreateError;
				return Result;
			}

			Result.CreatedTextures.Add(Texture);
		}

		Result.bSuccess = Result.CreatedTextures.Num() > 0;
		Result.Message = FText::Format(LOCTEXT("CreatedAndSavedTextures", "Created and saved {0} texture asset(s)."), FText::AsNumber(Result.CreatedTextures.Num()));
		return Result;
	}
} // namespace TextureChannelPackerTexture

#undef LOCTEXT_NAMESPACE
