// Copyright 2025 DimAlek. All Rights Reserved.

#pragma once

#include "Processing/TextureChannelPackerChannelData.h"
#include "Math/Float16.h"

namespace TextureChannelPackerTexture
{
	/** @brief Scalar channel storage, sampling and output format policies; independent of asset persistence. */
	class TEXTURECHANNELPACKER_API FChannelDataOps final
	{
	public:
		/**
		 * @brief Converts a logical channel to its RGBA component index.
		 * @param Channel Logical source or destination component.
		 * @return Component index in RGBA order.
		 */
		static int32 GetChannelIndex(const ETextureChannelPackerChannel Channel);

		/**
		 * @brief Allocates scalar channel storage after validating dimensions and the per-buffer byte limit.
		 * @param OutError Receives a validation failure description.
		 * @return True when storage was initialized; false leaves empty channel data.
		 * @param ChannelData Scalar storage containing the samples.
		 * @param Width Output width in pixels.
		 * @param Height Output height in pixels.
		 * @param ValueType Scalar storage precision.
		 */
		static bool InitializeChannel(FChannelData& ChannelData, const int32 Width, const int32 Height, const EChannelValueType ValueType, FText& OutError);

		/**
		 * @brief Stores an unconverted 8-bit sample.
		 * @pre ChannelData was initialized with EChannelValueType::UInt8.
		 * @param ChannelData Scalar storage containing the samples.
		 * @param PixelIndex Zero-based sample index; the caller must ensure it is in range.
		 * @param Value Sample value to store or convert.
		 */
		static void SetRawUInt8(FChannelData& ChannelData, const int32 PixelIndex, const uint8 Value);

		/**
		 * @brief Stores an unconverted 16-bit unsigned sample.
		 * @pre ChannelData was initialized with EChannelValueType::UInt16.
		 * @param ChannelData Scalar storage containing the samples.
		 * @param PixelIndex Zero-based sample index; the caller must ensure it is in range.
		 * @param Value Sample value to store or convert.
		 */
		static void SetRawUInt16(FChannelData& ChannelData, const int32 PixelIndex, const uint16 Value);

		/**
		 * @brief Stores an unconverted half-float sample.
		 * @pre ChannelData was initialized with EChannelValueType::Float16.
		 * @param ChannelData Scalar storage containing the samples.
		 * @param PixelIndex Zero-based sample index; the caller must ensure it is in range.
		 * @param Value Sample value to store or convert.
		 */
		static void SetRawFloat16(FChannelData& ChannelData, const int32 PixelIndex, const FFloat16 Value);

		/**
		 * @brief Stores an unconverted float sample.
		 * @pre ChannelData was initialized with EChannelValueType::Float32.
		 * @param ChannelData Scalar storage containing the samples.
		 * @param PixelIndex Zero-based sample index; the caller must ensure it is in range.
		 * @param Value Sample value to store or convert.
		 */
		static void SetRawFloat32(FChannelData& ChannelData, const int32 PixelIndex, const float Value);

		/**
		 * @brief Reads a scalar sample, normalizing integers while preserving floating-point range.
		 * @param ChannelData Scalar storage containing the samples.
		 * @param PixelIndex Zero-based sample index; the caller must ensure it is in range.
		 * @return Unsigned samples normalized to [0, 1], or an unclamped floating-point sample.
		 */
		static double GetValueAsUnitDouble(const FChannelData& ChannelData, const int32 PixelIndex);

		/**
		 * @brief Resolves a sampled value, constant or missing-channel default.
		 * @param Channel Resolved mapping and scalar samples, or nullptr for the missing-channel default.
		 * @param OutputChannel Destination component used for default-value selection.
		 * @param X Output-space horizontal coordinate.
		 * @param Y Output-space vertical coordinate.
		 * @param Width Output width in pixels.
		 * @param Height Output height in pixels.
		 * @return Sampled or constant value; missing RGB defaults to zero and missing alpha to one.
		 */
		static double GetOutputChannelValue(const FResolvedOutputChannel* Channel, const ETextureChannelPackerChannel OutputChannel, const int32 X, const int32 Y, const int32 Width, const int32 Height);

		/**
		 * @brief Resolves an explicit format or the automatic precision policy.
		 * @param Settings Requested output texture settings.
		 * @param Channels Resolved output mappings and their scalar data.
		 * @param bSingleChannel Whether automatic format selection should produce a single-component source.
		 * @return Explicit source format or the format selected by Auto.
		 */
		static ETextureSourceFormat ResolveOutputFormat(const FTextureChannelPackerOutputSettings& Settings, const TArray<FResolvedOutputChannel>& Channels, const bool bSingleChannel);

		/**
		 * @brief Uses the first input dimensions or the clamped manual square resolution.
		 * @param Settings Requested output texture settings.
		 * @param FirstSourceSize Dimensions of the first readable source, or zero when none is available.
		 * @return First source dimensions or a manual square size clamped to [1, 16384].
		 */
		static FIntPoint ResolveTargetSize(const FTextureChannelPackerOutputSettings& Settings, const FIntPoint FirstSourceSize);

	private:
		/**
		 * @brief Returns the storage width of one scalar channel sample.
		 * @param ValueType Scalar storage precision.
		 * @return Byte count required for one stored sample.
		 */
		static int32 GetBytesPerSample(const EChannelValueType ValueType);

		/**
		 * @brief Stores a normalized value using the channel precision and integer clamping rules.
		 * @param ChannelData Scalar storage containing the samples.
		 * @param PixelIndex Zero-based sample index; the caller must ensure it is in range.
		 * @param Value Sample value to store or convert.
		 */
		static void SetValue(FChannelData& ChannelData, const int32 PixelIndex, const double Value);

		/**
		 * @brief Maps output coordinates to a nearest-neighbor source sample.
		 * @param ChannelData Scalar storage containing the samples.
		 * @param TargetX Output-space horizontal coordinate.
		 * @param TargetY Output-space vertical coordinate.
		 * @param TargetWidth Output grid width.
		 * @param TargetHeight Output grid height.
		 * @return Nearest-neighbor sample offset in the source channel.
		 */
		static int32 GetSampleIndex(const FChannelData& ChannelData, const int32 TargetX, const int32 TargetY, const int32 TargetWidth, const int32 TargetHeight);

		/**
		 * @brief Selects storage using Float32 > Float16 > UInt16 > UInt8 precedence.
		 * @param Channels Resolved output mappings and their scalar data.
		 * @return Highest-priority storage type, defaulting to UInt8 when there is no source data.
		 * @note This policy can reduce UInt16 precision when mixed with Float16; it is not lossless promotion.
		 */
		static EChannelValueType GetMaxValueType(const TArray<FResolvedOutputChannel>& Channels);

		/**
		 * @brief Chooses a source format from channel precision and output channel count.
		 * @param Channels Resolved output mappings and their scalar data.
		 * @param bSingleChannel Whether automatic format selection should produce a single-component source.
		 * @return Format inferred from input precision and requested channel count.
		 */
		static ETextureSourceFormat ResolveAutoFormat(const TArray<FResolvedOutputChannel>& Channels, const bool bSingleChannel);
	};
} // namespace TextureChannelPackerTexture
