// Copyright 2025 DimAlek. All Rights Reserved.
#pragma once

#include "Models/TextureChannelPackerTypes.h"

/** @brief Converts supported editor option values and stable Slate combo-box labels without owning widget state. */
class TEXTURECHANNELPACKER_API FTextureChannelPackerUIConversions final
{
public:
	/** @brief Converts operation settings to the label used by the Slate combo box. */
	static FString OperationToString(const ETextureChannelPackerOperation Operation);

	/** @brief Resolves a supported combo label, falling back to the default operation value. */
	static ETextureChannelPackerOperation StringToOperation(const FString& OperationName);

	/** @brief Converts channel settings to the label used by the Slate combo box. */
	static FString ChannelToString(const ETextureChannelPackerChannel Channel);

	/** @brief Resolves a supported combo label, falling back to the default channel value. */
	static ETextureChannelPackerChannel StringToChannel(const FString& ChannelName);

	/** @brief Converts compression settings to the label used by the Slate combo box. */
	static FString CompressionToString(const TextureCompressionSettings Compression);

	/** @brief Resolves a supported combo label, falling back to the default compression value. */
	static TextureCompressionSettings StringToCompression(const FString& Compression);

	/** @brief Converts mip settings to the label used by the Slate combo box. */
	static FString MipToString(const TextureMipGenSettings Mip);

	/** @brief Resolves a supported combo label, falling back to the default mip value. */
	static TextureMipGenSettings StringToMip(const FString& Mip);

	/** @brief Converts filter settings to the label used by the Slate combo box. */
	static FString FilterToString(const TextureFilter Filter);

	/** @brief Resolves a supported combo label, falling back to the default filter value. */
	static TextureFilter StringToFilter(const FString& Filter);

	/** @brief Converts LOD group settings to the label used by the Slate combo box. */
	static FString LODGroupToString(const TextureGroup LODGroup);

	/** @brief Resolves a supported combo label, falling back to the default LOD group value. */
	static TextureGroup StringToLODGroup(const FString& LODGroup);
};
