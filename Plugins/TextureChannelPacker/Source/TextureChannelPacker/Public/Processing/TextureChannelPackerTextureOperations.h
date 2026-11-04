// Copyright 2025 DimAlek. All Rights Reserved.

#pragma once

#include "Processing/TextureChannelPackerChannelData.h"

namespace TextureChannelPackerTexture
{
	/** @brief Coordinates request resolution and persistent Pack, Repack, Copy and Unpack operations. */
	class TEXTURECHANNELPACKER_API FTextureOperations final
	{
	public:
		/**
		 * @brief Reads request mappings and resolves constants, source data and the first input dimensions.
		 * @param Mappings Request mappings to resolve into source samples or constants.
		 * @param OutChannels Receives resolved channel records; may be partially populated on failure.
		 * @param OutFirstSourceSize Receives the first readable input dimensions, or zero for constants-only requests.
		 * @param OutError Receives a user-facing failure description; undefined after success.
		 * @return True if at least one source or explicit non-default constant can be used.
		 * @note Call on the game thread; this function accesses editor texture objects.
		 */
		static bool ResolveChannels(const TArray<FTextureChannelPackerMapping>& Mappings, TArray<FResolvedOutputChannel>& OutChannels, FIntPoint& OutFirstSourceSize, FText& OutError);

		/**
		 * @brief Executes Pack, Repack or Copy and reports the generated asset.
		 * @param Request Operation inputs, naming rules and output settings.
		 * @return Operation status and the saved output, if successful.
		 * @note Call on the game thread; this function accesses editor texture objects.
		 */
		static FTextureChannelPackerResult ExecutePackLike(const FTextureChannelPackerRequest& Request);

		/**
		 * @brief Creates enabled Unpack outputs, rolling back outputs owned by this request on failure.
		 * @param Request Operation inputs, naming rules and output settings.
		 * @return Operation status; failed rollback retains surviving outputs and reports their cleanup errors.
		 * @note Call on the game thread; this function accesses editor texture objects.
		 */
		static FTextureChannelPackerResult ExecuteUnpack(const FTextureChannelPackerRequest& Request);

	private:
		/** @brief Removes only outputs created by this request; retains and reports assets whose cleanup failed. */
		static void RollbackOutputs(FTextureChannelPackerResult& Result);
	};
} // namespace TextureChannelPackerTexture
