// Copyright 2025 DimAlek. All Rights Reserved.

#pragma once

#include "Processing/TextureChannelPackerChannelData.h"

namespace TextureChannelPackerTexture
{
	/** @brief Builds transient display textures without creating persistent assets. */
	class FTexturePreviewBuilder final
	{
	public:
		/** @brief Resolves Pack, Repack or Copy channels and builds a transient preview. */
		static FTextureChannelPackerPreviewResult BuildPackLikePreview(const FTextureChannelPackerRequest& Request, const ETextureChannelPackerPreviewMode PreviewMode);

		/** @brief Builds a preview of the first enabled Unpack output. */
		static FTextureChannelPackerPreviewResult BuildUnpackPreview(const FTextureChannelPackerRequest& Request, const ETextureChannelPackerPreviewMode PreviewMode);

	private:
		/** @brief Fits the output aspect ratio within the preview dimension limit. */
		static FIntPoint ResolvePreviewSize(const FIntPoint TargetSize);

		/** @brief Clamps a normalized display sample and rounds it to an unsigned byte. */
		static uint8 UnitDoubleToByte(const double Value);

		/** @brief Selects the component displayed by an isolated channel view. */
		static double SelectPreviewChannelValue(const ETextureChannelPackerPreviewMode PreviewMode, const double R, const double G, const double B, const double A);

		/** @brief Applies output source precision before converting to display bytes. */
		static double QuantizePreviewValue(const double Value, const ETextureSourceFormat Format);

		/** @brief Creates a transient linear preview of output source data, without GPU compression or sRGB simulation. */
		static UTexture2D* CreatePreviewTexture(const TArray<FResolvedOutputChannel>& Channels, const FIntPoint TargetSize, const ETextureChannelPackerPreviewMode PreviewMode, const ETextureSourceFormat OutputFormat,
			FText& OutError);
	};
} // namespace TextureChannelPackerTexture
