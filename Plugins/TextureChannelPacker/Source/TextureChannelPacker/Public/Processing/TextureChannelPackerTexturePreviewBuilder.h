// Copyright 2025 DimAlek. All Rights Reserved.

#pragma once

#include "Processing/TextureChannelPackerChannelData.h"

namespace TextureChannelPackerTexture
{
	/** @brief Builds transient display textures without creating persistent assets. */
	class TEXTURECHANNELPACKER_API FTexturePreviewBuilder final
	{
	public:
		/**
		 * @brief Resolves Pack, Repack or Copy channels and builds a transient preview.
		 * @param Request Operation inputs, naming rules and output settings.
		 * @param PreviewMode Composite display or one isolated component.
		 * @return Transient preview and status; no persistent package is created.
		 * @note Call on the game thread; this function accesses editor texture objects.
		 */
		static FTextureChannelPackerPreviewResult BuildPackLikePreview(const FTextureChannelPackerRequest& Request, const ETextureChannelPackerPreviewMode PreviewMode);

		/**
		 * @brief Builds a preview of the first enabled Unpack output.
		 * @param Request Operation inputs, naming rules and output settings.
		 * @param PreviewMode Composite display or one isolated component.
		 * @return Preview of the first enabled output, or a failure description.
		 * @note Call on the game thread; this function accesses editor texture objects.
		 */
		static FTextureChannelPackerPreviewResult BuildUnpackPreview(const FTextureChannelPackerRequest& Request, const ETextureChannelPackerPreviewMode PreviewMode);

	private:
		/**
		 * @brief Fits the output aspect ratio within the preview dimension limit.
		 * @param TargetSize Full output dimensions before preview downscaling.
		 * @return Aspect-preserving display size bounded to 512 pixels, or zero for invalid input.
		 */
		static FIntPoint ResolvePreviewSize(const FIntPoint TargetSize);

		/**
		 * @brief Clamps a normalized display sample and rounds it to an unsigned byte.
		 * @param Value Sample value to store or convert.
		 * @return Clamped and rounded 8-bit display value.
		 */
		static uint8 UnitDoubleToByte(const double Value);

		/**
		 * @brief Selects the component displayed by an isolated channel view.
		 * @param PreviewMode Composite display or one isolated component.
		 * @param R Red component in normalized units.
		 * @param G Green component in normalized units.
		 * @param B Blue component in normalized units.
		 * @param A Alpha component in normalized units.
		 * @return The requested component, with red as the composite fallback.
		 */
		static double SelectPreviewChannelValue(const ETextureChannelPackerPreviewMode PreviewMode, const double R, const double G, const double B, const double A);

		/**
		 * @brief Applies output source precision before converting to display bytes.
		 * @param Value Sample value to store or convert.
		 * @param Format Resolved output source precision.
		 * @return Value after source-format quantization, before final display clamping.
		 */
		static double QuantizePreviewValue(const double Value, const ETextureSourceFormat Format);

		/**
		 * @brief Creates a transient linear preview of output source data, without GPU compression or sRGB simulation.
		 * @param Channels Resolved output mappings and their scalar data.
		 * @param TargetSize Full output dimensions before preview downscaling.
		 * @param PreviewMode Composite display or one isolated component.
		 * @param OutputFormat Resolved editor source format used by the writer.
		 * @param OutError Receives a user-facing failure description; undefined after success.
		 * @return Transient opaque BGRA8 texture, or nullptr on failure; retain strongly after return.
		 * @note Call on the game thread; this function accesses editor texture objects.
		 */
		static UTexture2D* CreatePreviewTexture(const TArray<FResolvedOutputChannel>& Channels, const FIntPoint TargetSize, const ETextureChannelPackerPreviewMode PreviewMode, const ETextureSourceFormat OutputFormat, FText& OutError);
	};
} // namespace TextureChannelPackerTexture
