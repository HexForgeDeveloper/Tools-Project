// Copyright 2025 DimAlek. All Rights Reserved.

#pragma once

#include "Processing/TextureChannelPackerChannelData.h"

namespace TextureChannelPackerTexture
{
	/** @brief Adapter between editor TextureSource data and resolved scalar channels. */
	class FTextureSourceCodec final
	{
	public:
		/** @brief Reads mip zero of an editor TextureSource into scalar channel storage; call on the game thread. */
		static bool ReadTextureChannel(UTexture* Texture, const ETextureChannelPackerChannel Channel, FChannelData& OutData, FText& OutError);

		/** @brief Writes resolved channels to mip zero in the selected source format; call on the game thread. */
		static bool WriteTextureSource(UTexture2D* Texture, const int32 Width, const int32 Height, const ETextureSourceFormat OutputFormat, const TArray<FResolvedOutputChannel>& Channels, FText& OutError);
	};
} // namespace TextureChannelPackerTexture
