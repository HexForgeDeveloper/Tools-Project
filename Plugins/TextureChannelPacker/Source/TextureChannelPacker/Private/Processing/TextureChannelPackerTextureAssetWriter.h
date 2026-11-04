// Copyright 2025 DimAlek. All Rights Reserved.

#pragma once

#include "Processing/TextureChannelPackerChannelData.h"

namespace TextureChannelPackerTexture
{
	/** @brief Owns package creation, texture settings, registry notification and saving. */
	class FTextureAssetWriter final
	{
	public:
		/** @brief Creates, registers and saves a uniquely named texture asset; call on the game thread. */
		static UTexture2D* CreateTextureAsset(const FString& OutputPackagePath, const FString& AssetName, const int32 Width, const int32 Height, const ETextureSourceFormat OutputFormat,
			const TArray<FResolvedOutputChannel>& Channels, const FTextureChannelPackerOutputSettings& Settings, FText& OutError);

	private:
		/** @brief Applies output compression, sampling and streaming settings to a generated texture. */
		static void ApplyTextureSettings(UTexture2D* Texture, const FTextureChannelPackerOutputSettings& Settings);

		/** @brief Persists a generated texture package and reports save failures. */
		static bool SaveTexturePackage(UTexture2D* Texture, FText& OutError);
	};
} // namespace TextureChannelPackerTexture
