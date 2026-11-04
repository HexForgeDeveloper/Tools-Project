// Copyright 2025 DimAlek. All Rights Reserved.

#pragma once

#include "Processing/TextureChannelPackerChannelData.h"
#include "Math/Float16.h"

namespace TextureChannelPackerTexture
{
	/** @brief Scalar channel storage, sampling and output format policies; independent of asset persistence. */
	class FChannelDataOps final
	{
	public:
		/** @brief Converts a logical channel to its RGBA component index. */
		static int32 GetChannelIndex(const ETextureChannelPackerChannel Channel);

		/** @brief Allocates scalar channel storage for the requested dimensions and precision. */
		static void InitializeChannel(FChannelData& ChannelData, const int32 Width, const int32 Height, const EChannelValueType ValueType);

		/** @brief Stores an unconverted 8-bit sample. */
		static void SetRawUInt8(FChannelData& ChannelData, const int32 PixelIndex, const uint8 Value);

		/** @brief Stores an unconverted 16-bit unsigned sample. */
		static void SetRawUInt16(FChannelData& ChannelData, const int32 PixelIndex, const uint16 Value);

		/** @brief Stores an unconverted half-float sample. */
		static void SetRawFloat16(FChannelData& ChannelData, const int32 PixelIndex, const FFloat16 Value);

		/** @brief Stores an unconverted float sample. */
		static void SetRawFloat32(FChannelData& ChannelData, const int32 PixelIndex, const float Value);

		/** @brief Reads a scalar sample, normalizing integers while preserving floating-point range. */
		static double GetValueAsUnitDouble(const FChannelData& ChannelData, const int32 PixelIndex);

		/** @brief Resolves a sampled value, constant or missing-channel default. */
		static double GetOutputChannelValue(const FResolvedOutputChannel* Channel, const ETextureChannelPackerChannel OutputChannel, const int32 X, const int32 Y, const int32 Width, const int32 Height);

		/** @brief Resolves an explicit format or the automatic precision policy. */
		static ETextureSourceFormat ResolveOutputFormat(const FTextureChannelPackerOutputSettings& Settings, const TArray<FResolvedOutputChannel>& Channels, const bool bSingleChannel);

		/** @brief Uses the first input dimensions or the clamped manual square resolution. */
		static FIntPoint ResolveTargetSize(const FTextureChannelPackerOutputSettings& Settings, const FIntPoint FirstSourceSize);

	private:
		/** @brief Returns the storage width of one scalar channel sample. */
		static int32 GetBytesPerSample(const EChannelValueType ValueType);

		/** @brief Stores a normalized value using the channel precision and integer clamping rules. */
		static void SetValue(FChannelData& ChannelData, const int32 PixelIndex, const double Value);

		/** @brief Maps output coordinates to a nearest-neighbor source sample. */
		static int32 GetSampleIndex(const FChannelData& ChannelData, const int32 TargetX, const int32 TargetY, const int32 TargetWidth, const int32 TargetHeight);

		/** @brief Selects the highest precision present in resolved source channels. */
		static EChannelValueType GetMaxValueType(const TArray<FResolvedOutputChannel>& Channels);

		/** @brief Chooses a source format from channel precision and output channel count. */
		static ETextureSourceFormat ResolveAutoFormat(const TArray<FResolvedOutputChannel>& Channels, const bool bSingleChannel);
	};
} // namespace TextureChannelPackerTexture
