// Copyright 2025 DimAlek. All Rights Reserved.

#include "Processing/TextureChannelPackerChannelDataOps.h"

#define LOCTEXT_NAMESPACE "TextureChannelPackerTextureService"

namespace TextureChannelPackerTexture
{
	int32 FChannelDataOps::GetChannelIndex(const ETextureChannelPackerChannel Channel)
	{
		switch (Channel)
		{
			case ETextureChannelPackerChannel::Red:
			{
				return 0;
			}
			case ETextureChannelPackerChannel::Green:
			{
				return 1;
			}
			case ETextureChannelPackerChannel::Blue:
			{
				return 2;
			}
			case ETextureChannelPackerChannel::Alpha:
			{
				return 3;
			}
			default:
			{
				return 0;
			}
		}
	}

	int32 FChannelDataOps::GetBytesPerSample(const EChannelValueType ValueType)
	{
		switch (ValueType)
		{
			case EChannelValueType::UInt8:
			{
				return sizeof(uint8);
			}
			case EChannelValueType::UInt16:
			{
				return sizeof(uint16);
			}
			case EChannelValueType::Float16:
			{
				return sizeof(FFloat16);
			}
			case EChannelValueType::Float32:
			{
				return sizeof(float);
			}
			default:
			{
				return sizeof(uint8);
			}
		}
	}

	void FChannelDataOps::InitializeChannel(FChannelData& ChannelData, const int32 Width, const int32 Height, const EChannelValueType ValueType)
	{
		ChannelData.Width = Width;
		ChannelData.Height = Height;
		ChannelData.ValueType = ValueType;
		ChannelData.Values.SetNumUninitialized(Width * Height * GetBytesPerSample(ValueType));
	}

	void FChannelDataOps::SetValue(FChannelData& ChannelData, const int32 PixelIndex, const double Value)
	{
		uint8* Destination = ChannelData.Values.GetData() + PixelIndex * GetBytesPerSample(ChannelData.ValueType);
		switch (ChannelData.ValueType)
		{
			case EChannelValueType::UInt8:
			{
				const uint8 ConvertedValue = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Value * 255.0), 0, 255));
				FMemory::Memcpy(Destination, &ConvertedValue, sizeof(ConvertedValue));
				break;
			}
			case EChannelValueType::UInt16:
			{
				const uint16 ConvertedValue = static_cast<uint16>(FMath::Clamp(FMath::RoundToInt(Value * 65535.0), 0, 65535));
				FMemory::Memcpy(Destination, &ConvertedValue, sizeof(ConvertedValue));
				break;
			}
			case EChannelValueType::Float16:
			{
				const FFloat16 ConvertedValue(static_cast<float>(Value));
				FMemory::Memcpy(Destination, &ConvertedValue, sizeof(ConvertedValue));
				break;
			}
			case EChannelValueType::Float32:
			{
				const float ConvertedValue = static_cast<float>(Value);
				FMemory::Memcpy(Destination, &ConvertedValue, sizeof(ConvertedValue));
				break;
			}
			default:
				break;
		}
	}

	void FChannelDataOps::SetRawUInt8(FChannelData& ChannelData, const int32 PixelIndex, const uint8 Value)
	{
		check(ChannelData.ValueType == EChannelValueType::UInt8);
		ChannelData.Values[PixelIndex] = Value;
	}

	void FChannelDataOps::SetRawUInt16(FChannelData& ChannelData, const int32 PixelIndex, const uint16 Value)
	{
		check(ChannelData.ValueType == EChannelValueType::UInt16);
		FMemory::Memcpy(ChannelData.Values.GetData() + PixelIndex * sizeof(uint16), &Value, sizeof(Value));
	}

	void FChannelDataOps::SetRawFloat16(FChannelData& ChannelData, const int32 PixelIndex, const FFloat16 Value)
	{
		check(ChannelData.ValueType == EChannelValueType::Float16);
		FMemory::Memcpy(ChannelData.Values.GetData() + PixelIndex * sizeof(FFloat16), &Value, sizeof(Value));
	}

	void FChannelDataOps::SetRawFloat32(FChannelData& ChannelData, const int32 PixelIndex, const float Value)
	{
		check(ChannelData.ValueType == EChannelValueType::Float32);
		FMemory::Memcpy(ChannelData.Values.GetData() + PixelIndex * sizeof(float), &Value, sizeof(Value));
	}

	double FChannelDataOps::GetValueAsUnitDouble(const FChannelData& ChannelData, const int32 PixelIndex)
	{
		const uint8* Source = ChannelData.Values.GetData() + PixelIndex * GetBytesPerSample(ChannelData.ValueType);
		switch (ChannelData.ValueType)
		{
			case EChannelValueType::UInt8:
			{
				return static_cast<double>(*Source) / 255.0;
			}
			case EChannelValueType::UInt16:
			{
				uint16 Value = 0;
				FMemory::Memcpy(&Value, Source, sizeof(Value));
				return static_cast<double>(Value) / 65535.0;
			}
			case EChannelValueType::Float16:
			{
				FFloat16 Value;
				FMemory::Memcpy(&Value, Source, sizeof(Value));
				return static_cast<float>(Value);
			}
			case EChannelValueType::Float32:
			{
				float Value = 0.0f;
				FMemory::Memcpy(&Value, Source, sizeof(Value));
				return Value;
			}
			default:
				return 0.0;
		}
	}

	int32 FChannelDataOps::GetSampleIndex(const FChannelData& ChannelData, const int32 TargetX, const int32 TargetY, const int32 TargetWidth, const int32 TargetHeight)
	{
		const int32 SourceX = FMath::Clamp(static_cast<int32>((static_cast<int64>(TargetX) * ChannelData.Width) / FMath::Max(TargetWidth, 1)), 0, ChannelData.Width - 1);
		const int32 SourceY = FMath::Clamp(static_cast<int32>((static_cast<int64>(TargetY) * ChannelData.Height) / FMath::Max(TargetHeight, 1)), 0, ChannelData.Height - 1);

		return SourceY * ChannelData.Width + SourceX;
	}

	double FChannelDataOps::GetOutputChannelValue(const FResolvedOutputChannel* Channel, const ETextureChannelPackerChannel OutputChannel, const int32 X, const int32 Y, const int32 Width, const int32 Height)
	{
		if (!Channel)
		{
			return OutputChannel == ETextureChannelPackerChannel::Alpha ? 1.0 : 0.0;
		}

		if (Channel->bUseConstant)
		{
			return static_cast<double>(Channel->ConstantValue) / 255.0;
		}

		if (!Channel->bHasData)
		{
			return OutputChannel == ETextureChannelPackerChannel::Alpha ? 1.0 : 0.0;
		}

		const int32 SourceIndex = GetSampleIndex(Channel->Data, X, Y, Width, Height);
		return GetValueAsUnitDouble(Channel->Data, SourceIndex);
	}

	EChannelValueType FChannelDataOps::GetMaxValueType(const TArray<FResolvedOutputChannel>& Channels)
	{
		EChannelValueType Result = EChannelValueType::UInt8;
		for (const FResolvedOutputChannel& Channel : Channels)
		{
			if (!Channel.bHasData)
			{
				continue;
			}

			if (Channel.Data.ValueType == EChannelValueType::Float32)
			{
				return EChannelValueType::Float32;
			}

			if (Channel.Data.ValueType == EChannelValueType::Float16)
			{
				Result = EChannelValueType::Float16;
			}
			else if (Channel.Data.ValueType == EChannelValueType::UInt16 && Result == EChannelValueType::UInt8)
			{
				Result = EChannelValueType::UInt16;
			}
		}

		return Result;
	}

	ETextureSourceFormat FChannelDataOps::ResolveAutoFormat(const TArray<FResolvedOutputChannel>& Channels, const bool bSingleChannel)
	{
		switch (GetMaxValueType(Channels))
		{
			case EChannelValueType::Float32:
			{
				return bSingleChannel ? TSF_R32F : TSF_RGBA32F;
			}
			case EChannelValueType::Float16:
			{
				return bSingleChannel ? TSF_R16F : TSF_RGBA16F;
			}
			case EChannelValueType::UInt16:
			{
				return bSingleChannel ? TSF_G16 : TSF_RGBA16;
			}
			case EChannelValueType::UInt8:
			default:
			{
				return bSingleChannel ? TSF_G8 : TSF_BGRA8;
			}
		}
	}

	ETextureSourceFormat FChannelDataOps::ResolveOutputFormat(const FTextureChannelPackerOutputSettings& Settings, const TArray<FResolvedOutputChannel>& Channels, const bool bSingleChannel)
	{
		switch (Settings.SourceFormat)
		{
			case ETextureChannelPackerOutputFormat::G8:
			{
				return TSF_G8;
			}
			case ETextureChannelPackerOutputFormat::G16:
			{
				return TSF_G16;
			}
			case ETextureChannelPackerOutputFormat::R16F:
			{
				return TSF_R16F;
			}
			case ETextureChannelPackerOutputFormat::R32F:
			{
				return TSF_R32F;
			}
			case ETextureChannelPackerOutputFormat::BGRA8:
			{
				return TSF_BGRA8;
			}
			case ETextureChannelPackerOutputFormat::RGBA16:
			{
				return TSF_RGBA16;
			}
			case ETextureChannelPackerOutputFormat::RGBA16F:
			{
				return TSF_RGBA16F;
			}
			case ETextureChannelPackerOutputFormat::RGBA32F:
			{
				return TSF_RGBA32F;
			}
			case ETextureChannelPackerOutputFormat::Auto:
			default:
			{
				return ResolveAutoFormat(Channels, bSingleChannel);
			}
		}
	}

	FIntPoint FChannelDataOps::ResolveTargetSize(const FTextureChannelPackerOutputSettings& Settings, const FIntPoint FirstSourceSize)
	{
		if (Settings.bUseFirstInputResolution && FirstSourceSize.X > 0 && FirstSourceSize.Y > 0)
		{
			return FirstSourceSize;
		}

		const int32 Resolution = FMath::Clamp(Settings.Resolution, 1, 16384);
		return FIntPoint(Resolution, Resolution);
	}
} // namespace TextureChannelPackerTexture

#undef LOCTEXT_NAMESPACE
