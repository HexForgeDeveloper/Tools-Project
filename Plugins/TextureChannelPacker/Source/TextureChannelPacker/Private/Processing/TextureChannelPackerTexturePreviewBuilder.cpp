// Copyright 2025 DimAlek. All Rights Reserved.

#include "Processing/TextureChannelPackerTexturePreviewBuilder.h"
#include "Processing/TextureChannelPackerChannelDataOps.h"
#include "Processing/TextureChannelPackerTextureOperations.h"
#include "Processing/TextureChannelPackerTextureSourceCodec.h"
#include "Engine/Texture2D.h"
#include "Math/Float16.h"
#include "Processing/TextureChannelPackerProcessingGuard.h"

#define LOCTEXT_NAMESPACE "TextureChannelPackerTextureService"

namespace TextureChannelPackerTexture
{
	FIntPoint FTexturePreviewBuilder::ResolvePreviewSize(const FIntPoint TargetSize)
	{
		constexpr int32 MaxPreviewDimension = 512;
		if (TargetSize.X <= 0 || TargetSize.Y <= 0)
		{
			return FIntPoint::ZeroValue;
		}

		if (TargetSize.X <= MaxPreviewDimension && TargetSize.Y <= MaxPreviewDimension)
		{
			return TargetSize;
		}

		const float Scale = static_cast<float>(MaxPreviewDimension) / static_cast<float>(FMath::Max(TargetSize.X, TargetSize.Y));
		return FIntPoint(FMath::Max(1, FMath::RoundToInt(static_cast<float>(TargetSize.X) * Scale)), FMath::Max(1, FMath::RoundToInt(static_cast<float>(TargetSize.Y) * Scale)));
	}

	uint8 FTexturePreviewBuilder::UnitDoubleToByte(const double Value)
	{
		return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Value * 255.0), 0, 255));
	}

	double FTexturePreviewBuilder::SelectPreviewChannelValue(const ETextureChannelPackerPreviewMode PreviewMode, const double R, const double G, const double B, const double A)
	{
		switch (PreviewMode)
		{
			case ETextureChannelPackerPreviewMode::Red:
			{
				return R;
			}
			case ETextureChannelPackerPreviewMode::Green:
			{
				return G;
			}
			case ETextureChannelPackerPreviewMode::Blue:
			{
				return B;
			}
			case ETextureChannelPackerPreviewMode::Alpha:
			{
				return A;
			}
			case ETextureChannelPackerPreviewMode::Composite:
			default:
			{
				return R;
			}
		}
	}

	double FTexturePreviewBuilder::QuantizePreviewValue(const double Value, const ETextureSourceFormat Format)
	{
		switch (Format)
		{
			case TSF_G8:
			case TSF_BGRA8:
				return UnitDoubleToByte(Value) / 255.0;
			case TSF_G16:
			case TSF_RGBA16:
				return FMath::Clamp(FMath::RoundToInt(Value * 65535.0), 0, 65535) / 65535.0;
			case TSF_R16F:
			case TSF_RGBA16F:
				return FFloat16(static_cast<float>(Value)).GetFloat();
			default:
				return static_cast<float>(Value);
		}
	}

	UTexture2D* FTexturePreviewBuilder::CreatePreviewTexture(const TArray<FResolvedOutputChannel>& Channels, const FIntPoint TargetSize, const ETextureChannelPackerPreviewMode PreviewMode, const ETextureSourceFormat OutputFormat, FText& OutError)
	{
		const bool bSingleChannel = OutputFormat == TSF_G8 || OutputFormat == TSF_G16 || OutputFormat == TSF_R16F || OutputFormat == TSF_R32F;
		const FIntPoint PreviewSize = ResolvePreviewSize(TargetSize);
		if (PreviewSize == FIntPoint::ZeroValue)
		{
			OutError = LOCTEXT("InvalidPreviewSize", "Could not determine a valid preview size.");
			return nullptr;
		}

		const FResolvedOutputChannel* ChannelByOutput[4] = { nullptr, nullptr, nullptr, nullptr };
		for (const FResolvedOutputChannel& Channel : Channels)
		{
			ChannelByOutput[FChannelDataOps::GetChannelIndex(Channel.OutputChannel)] = &Channel;
		}

		if (!FProcessingGuard::ValidateBuffer(PreviewSize.X, PreviewSize.Y, 4, OutError))
		{
			return nullptr;
		}
		TArray<uint8> Pixels;
		Pixels.SetNumUninitialized(PreviewSize.X * PreviewSize.Y * 4);
		for (int32 Y = 0; Y < PreviewSize.Y; ++Y)
		{
			if (FProcessingGuard::IsCancelled(OutError, FProcessingGuard::EStage::PreviewRow))
			{
				return nullptr;
			}
			for (int32 X = 0; X < PreviewSize.X; ++X)
			{
				const int32 PixelIndex = Y * PreviewSize.X + X;
				// Sample the output grid before reducing it to the preview dimensions.
				const int32 OutputX = X * TargetSize.X / PreviewSize.X;
				const int32 OutputY = Y * TargetSize.Y / PreviewSize.Y;
				const double R = QuantizePreviewValue(FChannelDataOps::GetOutputChannelValue(ChannelByOutput[0], ETextureChannelPackerChannel::Red, OutputX, OutputY, TargetSize.X, TargetSize.Y), OutputFormat);
				const double G = bSingleChannel ? R : QuantizePreviewValue(FChannelDataOps::GetOutputChannelValue(ChannelByOutput[1], ETextureChannelPackerChannel::Green, OutputX, OutputY, TargetSize.X, TargetSize.Y), OutputFormat);
				const double B = bSingleChannel ? R : QuantizePreviewValue(FChannelDataOps::GetOutputChannelValue(ChannelByOutput[2], ETextureChannelPackerChannel::Blue, OutputX, OutputY, TargetSize.X, TargetSize.Y), OutputFormat);
				const double A = bSingleChannel ? 1.0 : QuantizePreviewValue(FChannelDataOps::GetOutputChannelValue(ChannelByOutput[3], ETextureChannelPackerChannel::Alpha, OutputX, OutputY, TargetSize.X, TargetSize.Y), OutputFormat);

				uint8 OutR = UnitDoubleToByte(R);
				uint8 OutG = UnitDoubleToByte(G);
				uint8 OutB = UnitDoubleToByte(B);
				if (PreviewMode != ETextureChannelPackerPreviewMode::Composite)
				{
					const uint8 Value = UnitDoubleToByte(SelectPreviewChannelValue(PreviewMode, R, G, B, A));
					OutR = Value;
					OutG = Value;
					OutB = Value;
				}

				uint8* Pixel = Pixels.GetData() + PixelIndex * 4;
				Pixel[0] = OutB;
				Pixel[1] = OutG;
				Pixel[2] = OutR;
				Pixel[3] = 255;
			}
		}

		const FName PreviewTextureName = MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), TEXT("TextureChannelPackerPreview"));
		UTexture2D* PreviewTexture = UTexture2D::CreateTransient(PreviewSize.X, PreviewSize.Y, PF_B8G8R8A8, PreviewTextureName);
		if (!PreviewTexture || !PreviewTexture->GetPlatformData() || PreviewTexture->GetPlatformData()->Mips.Num() == 0)
		{
			OutError = LOCTEXT("PreviewTextureCreateFailed", "Could not create the preview texture.");
			return nullptr;
		}

		PreviewTexture->MipGenSettings = TMGS_NoMipmaps;
		PreviewTexture->Filter = TF_Bilinear;
		PreviewTexture->SRGB = false;
		PreviewTexture->NeverStream = true;

		FTexture2DMipMap& Mip = PreviewTexture->GetPlatformData()->Mips[0];
		void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
		if (!TextureData)
		{
			OutError = LOCTEXT("PreviewTextureLockFailed", "Could not lock the preview texture.");
			return nullptr;
		}

		FMemory::Memcpy(TextureData, Pixels.GetData(), Pixels.Num());
		Mip.BulkData.Unlock();
		PreviewTexture->UpdateResource();
		return PreviewTexture;
	}

	FTextureChannelPackerPreviewResult FTexturePreviewBuilder::BuildPackLikePreview(const FTextureChannelPackerRequest& Request, const ETextureChannelPackerPreviewMode PreviewMode)
	{
		FTextureChannelPackerPreviewResult Result;
		TArray<FResolvedOutputChannel> ResolvedChannels;
		FIntPoint FirstSourceSize;
		if (!FTextureOperations::ResolveChannels(Request.Mappings, ResolvedChannels, FirstSourceSize, Result.Message))
		{
			return Result;
		}

		if (ResolvedChannels.Num() == 0)
		{
			Result.Message = LOCTEXT("PreviewNoChannels", "No channel mappings were provided.");
			return Result;
		}

		const FIntPoint TargetSize = FChannelDataOps::ResolveTargetSize(Request.OutputSettings, FirstSourceSize);
		Result.PreviewTexture = CreatePreviewTexture(ResolvedChannels, TargetSize, PreviewMode, FChannelDataOps::ResolveOutputFormat(Request.OutputSettings, ResolvedChannels, false), Result.Message);
		Result.bSuccess = Result.PreviewTexture != nullptr;
		Result.PreviewSize = Result.PreviewTexture ? FIntPoint(Result.PreviewTexture->GetSizeX(), Result.PreviewTexture->GetSizeY()) : FIntPoint::ZeroValue;
		Result.Message = Result.bSuccess ? LOCTEXT("PreviewBuilt", "Preview updated.") : Result.Message;
		return Result;
	}

	FTextureChannelPackerPreviewResult FTexturePreviewBuilder::BuildUnpackPreview(const FTextureChannelPackerRequest& Request, const ETextureChannelPackerPreviewMode PreviewMode)
	{
		FTextureChannelPackerPreviewResult Result;
		if (!Request.UnpackSourceTexture.IsValid())
		{
			Result.Message = LOCTEXT("PreviewMissingUnpackSource", "Select a packed source texture before previewing unpack.");
			return Result;
		}

		const FTextureChannelPackerUnpackOutput* FirstEnabledOutput = nullptr;
		for (const FTextureChannelPackerUnpackOutput& Output : Request.UnpackOutputs)
		{
			if (Output.bEnabled)
			{
				FirstEnabledOutput = &Output;
				break;
			}
		}

		if (!FirstEnabledOutput)
		{
			Result.Message = LOCTEXT("PreviewNoUnpackChannels", "Select at least one channel to preview.");
			return Result;
		}

		FChannelData SourceChannel;
		if (!FTextureSourceCodec::ReadTextureChannel(Request.UnpackSourceTexture.Get(), FirstEnabledOutput->SourceChannel, SourceChannel, Result.Message))
		{
			return Result;
		}

		FResolvedOutputChannel ResolvedChannel;
		ResolvedChannel.OutputChannel = ETextureChannelPackerChannel::Red;
		ResolvedChannel.bHasData = true;
		ResolvedChannel.Data = MoveTemp(SourceChannel);

		TArray<FResolvedOutputChannel> Channels;
		Channels.Add(MoveTemp(ResolvedChannel));

		const FIntPoint TargetSize = FChannelDataOps::ResolveTargetSize(Request.OutputSettings, FIntPoint(Channels[0].Data.Width, Channels[0].Data.Height));
		Result.PreviewTexture = CreatePreviewTexture(Channels, TargetSize, PreviewMode, FChannelDataOps::ResolveOutputFormat(Request.OutputSettings, Channels, true), Result.Message);
		Result.bSuccess = Result.PreviewTexture != nullptr;
		Result.PreviewSize = Result.PreviewTexture ? FIntPoint(Result.PreviewTexture->GetSizeX(), Result.PreviewTexture->GetSizeY()) : FIntPoint::ZeroValue;
		Result.Message = Result.bSuccess ? LOCTEXT("UnpackPreviewBuilt", "Preview updated.") : Result.Message;
		return Result;
	}
} // namespace TextureChannelPackerTexture

#undef LOCTEXT_NAMESPACE
