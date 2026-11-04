// Copyright 2025 DimAlek. All Rights Reserved.

#pragma once

#include "Processing/TextureChannelPackerChannelData.h"

namespace TextureChannelPackerTexture
{
	/** @brief Owns package creation, texture settings, registry notification and saving. */
	class TEXTURECHANNELPACKER_API FTextureAssetWriter final
	{
	public:
#if WITH_DEV_AUTOMATION_TESTS
		/** @brief Development-only failure injection; return true to reject a package save before disk I/O. */
		static TFunction<bool(const FString&)> ShouldFailSaveForTesting;
#endif
		/**
		 * @brief Removes an output owned by the current operation, deleting its file before unregistering it.
		 * @param Texture Newly created output; never pass a pre-existing user asset.
		 * @param bRegistered Whether AssetCreated has been sent for this object.
		 * @param OutError Receives the retained file path if disk cleanup fails.
		 * @return True when file and object cleanup succeeded; false leaves the asset available for recovery.
		 */
		static bool DiscardTexture(UTexture2D* Texture, bool bRegistered, FText& OutError);
		/**
		 * @brief Creates, registers and saves a uniquely named texture asset; call on the game thread.
		 * @param OutputPackagePath Validated destination folder under project Content.
		 * @param AssetName Requested base name, sanitized and made unique before package creation.
		 * @param Width Output width in pixels.
		 * @param Height Output height in pixels.
		 * @param OutputFormat Resolved editor source format used by the writer.
		 * @param Channels Resolved output mappings and their scalar data.
		 * @param Settings Requested output texture settings.
		 * @param OutError Receives a user-facing failure description; undefined after success.
		 * @return Saved and registered texture, or nullptr on failure; cleanup failures are appended to OutError.
		 * @note Call on the game thread; this function accesses editor texture objects.
		 */
		static UTexture2D* CreateTextureAsset(const FString& OutputPackagePath, const FString& AssetName, const int32 Width, const int32 Height, const ETextureSourceFormat OutputFormat,
			const TArray<FResolvedOutputChannel>& Channels, const FTextureChannelPackerOutputSettings& Settings, FText& OutError);

	private:
		/**
		 * @brief Applies output compression, sampling and streaming settings to a generated texture.
		 * @param Texture Texture object to read, populate or save; callers must retain its lifetime.
		 * @param Settings Requested output texture settings.
		 */
		static void ApplyTextureSettings(UTexture2D* Texture, const FTextureChannelPackerOutputSettings& Settings);

		/**
		 * @brief Persists a generated texture package and reports save failures.
		 * @param Texture Texture object to read, populate or save; callers must retain its lifetime.
		 * @param OutError Receives a user-facing failure description; undefined after success.
		 * @return True only when SavePackage reports success.
		 */
		static bool SaveTexturePackage(UTexture2D* Texture, FText& OutError);
	};
} // namespace TextureChannelPackerTexture
