// Copyright 2025 DimAlek. All Rights Reserved.

#include "Processing/TextureChannelPackerTextureSourceCodec.h"
#include "Processing/TextureChannelPackerChannelDataOps.h"
#include "Engine/Texture2D.h"
#include "Math/Float16.h"

#define LOCTEXT_NAMESPACE "TextureChannelPackerTextureService"

namespace TextureChannelPackerTexture
{
	bool FTextureSourceCodec::ReadTextureChannel(UTexture* Texture, const ETextureChannelPackerChannel Channel, FChannelData& OutData, FText& OutError)
	{
		UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
		if (!Texture2D)
		{
			OutError = LOCTEXT("UnsupportedTextureClass", "Only Texture2D assets with editor source data can be processed.");
			return false;
		}

#if WITH_EDITORONLY_DATA
		const int32 Width = Texture2D->Source.GetSizeX();
		const int32 Height = Texture2D->Source.GetSizeY();
		const ETextureSourceFormat SourceFormat = Texture2D->Source.GetFormat();
		if (Width <= 0 || Height <= 0 || SourceFormat == TSF_Invalid)
		{
			OutError = FText::Format(LOCTEXT("MissingSourceData", "Texture '{0}' has no readable editor source data."), FText::FromString(Texture2D->GetName()));
			return false;
		}

		const int32 ChannelIndex = FChannelDataOps::GetChannelIndex(Channel);
		uint8* SourceData = Texture2D->Source.LockMip(0);
		if (!SourceData)
		{
			OutError = FText::Format(LOCTEXT("LockMipFailed", "Could not lock source data for texture '{0}'."), FText::FromString(Texture2D->GetName()));
			return false;
		}

		const int32 PixelCount = Width * Height;
		switch (SourceFormat)
		{
			case TSF_BGRA8:
			{
				FChannelDataOps::InitializeChannel(OutData, Width, Height, EChannelValueType::UInt8);
				for (int32 Index = 0; Index < PixelCount; ++Index)
				{
					const uint8* Pixel = SourceData + Index * 4;
					const uint8 Value = ChannelIndex == 0 ? Pixel[2] : ChannelIndex == 1 ? Pixel[1]
						: ChannelIndex == 2												 ? Pixel[0]
																						 : Pixel[3];
					FChannelDataOps::SetRawUInt8(OutData, Index, Value);
				}
				break;
			}
			case TSF_G8:
			{
				FChannelDataOps::InitializeChannel(OutData, Width, Height, EChannelValueType::UInt8);
				for (int32 Index = 0; Index < PixelCount; ++Index)
				{
					FChannelDataOps::SetRawUInt8(OutData, Index, Channel == ETextureChannelPackerChannel::Alpha ? 255 : SourceData[Index]);
				}
				break;
			}
			case TSF_G16:
			{
				FChannelDataOps::InitializeChannel(OutData, Width, Height, EChannelValueType::UInt16);
				const uint16* SourcePixels = reinterpret_cast<const uint16*>(SourceData);
				for (int32 Index = 0; Index < PixelCount; ++Index)
				{
					FChannelDataOps::SetRawUInt16(OutData, Index, Channel == ETextureChannelPackerChannel::Alpha ? 65535 : SourcePixels[Index]);
				}
				break;
			}
			case TSF_R16F:
			{
				FChannelDataOps::InitializeChannel(OutData, Width, Height, EChannelValueType::Float16);
				const FFloat16* SourcePixels = reinterpret_cast<const FFloat16*>(SourceData);
				for (int32 Index = 0; Index < PixelCount; ++Index)
				{
					FChannelDataOps::SetRawFloat16(OutData, Index, Channel == ETextureChannelPackerChannel::Alpha ? FFloat16(1.0f) : SourcePixels[Index]);
				}
				break;
			}
			case TSF_R32F:
			{
				FChannelDataOps::InitializeChannel(OutData, Width, Height, EChannelValueType::Float32);
				const float* SourcePixels = reinterpret_cast<const float*>(SourceData);
				for (int32 Index = 0; Index < PixelCount; ++Index)
				{
					FChannelDataOps::SetRawFloat32(OutData, Index, Channel == ETextureChannelPackerChannel::Alpha ? 1.0f : SourcePixels[Index]);
				}
				break;
			}
			case TSF_RGBA16:
			{
				FChannelDataOps::InitializeChannel(OutData, Width, Height, EChannelValueType::UInt16);
				const uint16* SourcePixels = reinterpret_cast<const uint16*>(SourceData);
				for (int32 Index = 0; Index < PixelCount; ++Index)
				{
					FChannelDataOps::SetRawUInt16(OutData, Index, SourcePixels[Index * 4 + ChannelIndex]);
				}
				break;
			}
			case TSF_RGBA16F:
			{
				FChannelDataOps::InitializeChannel(OutData, Width, Height, EChannelValueType::Float16);
				const FFloat16* SourcePixels = reinterpret_cast<const FFloat16*>(SourceData);
				for (int32 Index = 0; Index < PixelCount; ++Index)
				{
					FChannelDataOps::SetRawFloat16(OutData, Index, SourcePixels[Index * 4 + ChannelIndex]);
				}
				break;
			}
			case TSF_RGBA32F:
			{
				FChannelDataOps::InitializeChannel(OutData, Width, Height, EChannelValueType::Float32);
				const float* SourcePixels = reinterpret_cast<const float*>(SourceData);
				for (int32 Index = 0; Index < PixelCount; ++Index)
				{
					FChannelDataOps::SetRawFloat32(OutData, Index, SourcePixels[Index * 4 + ChannelIndex]);
				}
				break;
			}
			default:
				Texture2D->Source.UnlockMip(0);
				OutError = FText::Format(LOCTEXT("UnsupportedSourceFormat", "Texture '{0}' uses unsupported source format {1}."), FText::FromString(Texture2D->GetName()),
					FText::AsNumber(static_cast<int32>(SourceFormat)));
				return false;
		}

		Texture2D->Source.UnlockMip(0);
		return true;
#else
		OutError = LOCTEXT("EditorOnlyDataRequired", "Texture channel packing requires editor-only texture source data.");
		return false;
#endif
	}

	bool FTextureSourceCodec::WriteTextureSource(UTexture2D* Texture, const int32 Width, const int32 Height, const ETextureSourceFormat OutputFormat, const TArray<FResolvedOutputChannel>& Channels, FText& OutError)
	{
#if WITH_EDITORONLY_DATA
		Texture->Source.Init(Width, Height, 1, 1, OutputFormat);
		uint8* DestinationData = Texture->Source.LockMip(0);
		if (!DestinationData)
		{
			OutError = LOCTEXT("DestinationLockFailed", "Could not lock output texture source data.");
			return false;
		}

		const FResolvedOutputChannel* ChannelByOutput[4] = { nullptr, nullptr, nullptr, nullptr };
		for (const FResolvedOutputChannel& Channel : Channels)
		{
			ChannelByOutput[FChannelDataOps::GetChannelIndex(Channel.OutputChannel)] = &Channel;
		}

		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const int32 PixelIndex = Y * Width + X;
				const double R = FChannelDataOps::GetOutputChannelValue(ChannelByOutput[0], ETextureChannelPackerChannel::Red, X, Y, Width, Height);
				const double G = FChannelDataOps::GetOutputChannelValue(ChannelByOutput[1], ETextureChannelPackerChannel::Green, X, Y, Width, Height);
				const double B = FChannelDataOps::GetOutputChannelValue(ChannelByOutput[2], ETextureChannelPackerChannel::Blue, X, Y, Width, Height);
				const double A = FChannelDataOps::GetOutputChannelValue(ChannelByOutput[3], ETextureChannelPackerChannel::Alpha, X, Y, Width, Height);

				switch (OutputFormat)
				{
					case TSF_G8:
					{
						DestinationData[PixelIndex] = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(R * 255.0), 0, 255));
						break;
					}
					case TSF_G16:
					{
						uint16* DestinationPixels = reinterpret_cast<uint16*>(DestinationData);
						DestinationPixels[PixelIndex] = static_cast<uint16>(FMath::Clamp(FMath::RoundToInt(R * 65535.0), 0, 65535));
						break;
					}
					case TSF_R16F:
					{
						FFloat16* DestinationPixels = reinterpret_cast<FFloat16*>(DestinationData);
						DestinationPixels[PixelIndex] = FFloat16(static_cast<float>(R));
						break;
					}
					case TSF_R32F:
					{
						float* DestinationPixels = reinterpret_cast<float*>(DestinationData);
						DestinationPixels[PixelIndex] = static_cast<float>(R);
						break;
					}
					case TSF_BGRA8:
					{
						uint8* Pixel = DestinationData + PixelIndex * 4;
						Pixel[0] = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(B * 255.0), 0, 255));
						Pixel[1] = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(G * 255.0), 0, 255));
						Pixel[2] = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(R * 255.0), 0, 255));
						Pixel[3] = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(A * 255.0), 0, 255));
						break;
					}
					case TSF_RGBA16:
					{
						uint16* Pixel = reinterpret_cast<uint16*>(DestinationData) + PixelIndex * 4;
						Pixel[0] = static_cast<uint16>(FMath::Clamp(FMath::RoundToInt(R * 65535.0), 0, 65535));
						Pixel[1] = static_cast<uint16>(FMath::Clamp(FMath::RoundToInt(G * 65535.0), 0, 65535));
						Pixel[2] = static_cast<uint16>(FMath::Clamp(FMath::RoundToInt(B * 65535.0), 0, 65535));
						Pixel[3] = static_cast<uint16>(FMath::Clamp(FMath::RoundToInt(A * 65535.0), 0, 65535));
						break;
					}
					case TSF_RGBA16F:
					{
						FFloat16* Pixel = reinterpret_cast<FFloat16*>(DestinationData) + PixelIndex * 4;
						Pixel[0] = FFloat16(static_cast<float>(R));
						Pixel[1] = FFloat16(static_cast<float>(G));
						Pixel[2] = FFloat16(static_cast<float>(B));
						Pixel[3] = FFloat16(static_cast<float>(A));
						break;
					}
					case TSF_RGBA32F:
					{
						float* Pixel = reinterpret_cast<float*>(DestinationData) + PixelIndex * 4;
						Pixel[0] = static_cast<float>(R);
						Pixel[1] = static_cast<float>(G);
						Pixel[2] = static_cast<float>(B);
						Pixel[3] = static_cast<float>(A);
						break;
					}
					default:
						Texture->Source.UnlockMip(0);
						OutError = LOCTEXT("UnsupportedOutputFormat", "The requested output source format is not supported.");
						return false;
				}
			}
		}

		Texture->Source.UnlockMip(0);
		return true;
#else
		OutError = LOCTEXT("EditorOnlyOutputRequired", "Texture channel packing requires editor-only texture source data.");
		return false;
#endif
	}
} // namespace TextureChannelPackerTexture

#undef LOCTEXT_NAMESPACE
