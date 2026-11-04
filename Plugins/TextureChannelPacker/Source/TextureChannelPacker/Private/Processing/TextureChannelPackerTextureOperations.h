// Copyright 2025 DimAlek. All Rights Reserved.

#pragma once

#include "Processing/TextureChannelPackerChannelData.h"

namespace TextureChannelPackerTexture
{
	/** @brief Coordinates request resolution and persistent Pack, Repack, Copy and Unpack operations. */
	class FTextureOperations final
	{
	public:
		/** @brief Reads request mappings and resolves constants, source data and the first input dimensions. */
		static bool ResolveChannels(const TArray<FTextureChannelPackerMapping>& Mappings, TArray<FResolvedOutputChannel>& OutChannels, FIntPoint& OutFirstSourceSize, FText& OutError);

		/** @brief Executes Pack, Repack or Copy and reports the generated asset. */
		static FTextureChannelPackerResult ExecutePackLike(const FTextureChannelPackerRequest& Request);

		/** @brief Creates enabled Unpack outputs and reports assets completed before any failure. */
		static FTextureChannelPackerResult ExecuteUnpack(const FTextureChannelPackerRequest& Request);
	};
} // namespace TextureChannelPackerTexture
