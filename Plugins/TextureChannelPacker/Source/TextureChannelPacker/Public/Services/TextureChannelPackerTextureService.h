// Copyright 2025 DimAlek. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Models/TextureChannelPackerTypes.h"

class UTexture;
class UTexture2D;

/** @brief Game-thread facade for texture validation, persistent operations and transient previews. */
class TEXTURECHANNELPACKER_API FTextureChannelPackerTextureService final
{
public:
	/** @brief Checks texture class and editor source availability, reporting a readable failure reason. */
	static bool CanReadTexture(const UTexture* Texture, FText& OutError);
	/** @brief Validates the destination and dispatches a transactional packing operation on the game thread. */
	static FTextureChannelPackerResult Execute(const FTextureChannelPackerRequest& Request);
	/** @brief Builds a transient preview on the game thread without saving or registering assets. */
	static FTextureChannelPackerPreviewResult BuildPreview(const FTextureChannelPackerRequest& Request, ETextureChannelPackerPreviewMode PreviewMode);
	/** @brief Returns editor source dimensions, or zero when the texture has no supported source object. */
	static FIntPoint GetTextureSourceSize(const UTexture* Texture);
};
