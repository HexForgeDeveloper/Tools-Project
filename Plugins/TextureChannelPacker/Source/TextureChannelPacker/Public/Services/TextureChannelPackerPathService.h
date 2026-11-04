// Copyright 2025 DimAlek. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** @brief Validates project output directories and builds sanitized, unique asset package names. */
class TEXTURECHANNELPACKER_API FTextureChannelPackerPathService final
{
public:
	/**
	 * @brief Resolves a /Game folder or an absolute directory inside project Content.
	 * @param InPath Destination folder; whitespace and path separators are normalized.
	 * @return Canonical /Game path, or empty for unsupported roots, traversal or invalid package characters.
	 */
	static FString NormalizeOutputPath(const FString& InPath);
	/** @brief Sanitizes an object name, using T_Packed_Texture when the input is empty or unusable. */
	static FString SanitizeAssetName(const FString& InAssetName);
	/** @brief Builds a sanitized asset package name, returning empty when the destination is invalid. */
	static FString BuildPackageName(const FString& OutputPackagePath, const FString& AssetName);
	/** @brief Resolves a unique asset package; clears both output strings and returns false for invalid destinations. */
	static bool MakeUniqueAssetPackageName(const FString& OutputPackagePath, const FString& AssetName, FString& OutPackageName, FString& OutAssetName);
};
