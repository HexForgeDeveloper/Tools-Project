// Copyright 2025 DimAlek. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Models/TextureChannelPackerTypes.h"

class UTexture;

/**
 * @class FTextureChannelPackerPatternService
 * @brief Utility class for matching texture names against preset patterns and generating packing requests.
 *
 * Provides helper functions for identifying textures based on naming patterns,
 * generating output asset names, and populating channel packing requests from
 * preset definitions.
 */
class TEXTURECHANNELPACKER_API FTextureChannelPackerPatternService final
{
public:
	/**
	 * @brief Captures reusable request settings without retaining asset references or output paths.
	 * @param Request Current packing or unpacking configuration.
	 * @param Name Display name of the new preset.
	 * @param OutputSuffix Active naming suffix, including an intentionally empty suffix.
	 * @return Preset preserving mappings, output settings and Unpack enabled states.
	 */
	static FTextureChannelPackerPreset CreatePreset(const FTextureChannelPackerRequest& Request, const FString& Name, const FString& OutputSuffix);

	/**
	 * @brief Determines whether a texture name matches one or more search patterns.
	 *
	 * Supports wildcard expressions as well as case-insensitive substring and suffix matching,
	 * Multiple patterns can be separated by ',' or ';'.
	 *
	 * @param TextureName Name of the texture to test.
	 * @param PatternList List of patterns separated by ',' or ';'.
	 *
	 * @return True if the texture name matches at least one pattern.
	 */
	static bool DoesNameMatchPattern(const FString& TextureName, const FString& PatternList);

	/**
	 * @brief Generates a common output base name from a collection of textures.
	 *
	 * Attempts to extract the longest common prefix from all texture names.
	 * Uses the first texture name when the trimmed common prefix has fewer than three characters.
	 * Uses FallbackName only when no non-null texture is supplied, and adds T_ to names derived from textures.
	 * The resulting name is sanitized for use as a valid asset name.
	 *
	 * @param Textures Source textures.
	 * @param FallbackName Name used when no common prefix can be generated.
	 *
	 * @return Sanitized base asset name.
	 */
	static FString BuildBaseNameFromTextures(const TArray<UTexture*>& Textures, const FString& FallbackName);

	/**
	 * @brief Applies a preset to a packing request by matching textures against
	 *			channel patterns.
	 *
	 * Copies preset settings into the request, resolves source textures using
	 * pattern matching, initializes channel mappings, and generates the output
	 * asset name.
	 *
	 * @param Preset Packing preset describing channel mappings.
	 * @param Textures Available source textures.
	 * @param Request  Request object to populate.
	 */
	static void ApplyPresetPatterns(const FTextureChannelPackerPreset& Preset, const TArray<UTexture*>& Textures, FTextureChannelPackerRequest& Request);
};
