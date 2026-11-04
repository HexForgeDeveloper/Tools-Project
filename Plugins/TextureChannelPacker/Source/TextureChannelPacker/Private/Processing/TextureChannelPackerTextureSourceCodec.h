// Copyright 2025 DimAlek. All Rights Reserved.

#pragma once

#include "Processing/TextureChannelPackerChannelData.h"

namespace TextureChannelPackerTexture
{
	/** @brief Adapter between editor TextureSource data and resolved scalar channels. */
	class FTextureSourceCodec final
	{
	public:
		/**
		 * @brief Reads mip zero of an editor TextureSource into scalar channel storage; call on the game thread.
		 * @param Texture Texture object to read, populate or save; callers must retain its lifetime.
		 * @param Channel Logical source or destination component.
		 * @param OutData Receives decoded scalar samples on success.
		 * @param OutError Receives a user-facing failure description; undefined after success.
		 * @return True on complete decoding; false for an invalid texture, unsupported format or source lock failure.
		 * @note Call on the game thread; this function accesses editor texture objects.
		 */
		static bool ReadTextureChannel(UTexture* Texture, const ETextureChannelPackerChannel Channel, FChannelData& OutData, FText& OutError);

		/**
		 * @brief Writes resolved channels to mip zero in the selected source format; call on the game thread.
		 * @param Texture Texture object to read, populate or save; callers must retain its lifetime.
		 * @param Width Output width in pixels.
		 * @param Height Output height in pixels.
		 * @param OutputFormat Resolved editor source format used by the writer.
		 * @param Channels Resolved output mappings and their scalar data.
		 * @param OutError Receives a user-facing failure description; undefined after success.
		 * @return True when mip zero was populated; false if initialization or writing fails.
		 * @note Call on the game thread; this function accesses editor texture objects.
		 */
		static bool WriteTextureSource(UTexture2D* Texture, const int32 Width, const int32 Height, const ETextureSourceFormat OutputFormat, const TArray<FResolvedOutputChannel>& Channels, FText& OutError);
	};
} // namespace TextureChannelPackerTexture
