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
	/**
	 * @brief Checks Texture2D class, supported source format, dimensions and per-buffer processing limit.
	 * @param Texture Candidate input; nullptr is rejected.
	 * @param OutError Receives a description when the initial validation fails.
	 * @return True if source metadata meets processing limits; actual source locking can still fail later.
	 */
	static bool CanReadTexture(const UTexture* Texture, FText& OutError);
	/**
	 * @brief Validates the destination and executes a synchronous operation on the game thread.
	 * @param Request Input mappings, output naming rules and texture settings.
	 * @return Completion state and saved outputs; failed Unpack cleanup reports surviving outputs explicitly.
	 * @note Editor transactions do not provide atomic rollback of files already saved to disk.
	 */
	static FTextureChannelPackerResult Execute(const FTextureChannelPackerRequest& Request);
	/**
	 * @brief Builds a transient linear source-data preview on the game thread without registering assets.
	 * @param Request Inputs and output source format; the destination path is not used for preview.
	 * @param PreviewMode Composite RGB or a grayscale component view.
	 * @return Preview texture and dimensions, or an error. Retain the texture strongly while displaying it.
	 * @note Platform compression, mip generation and sRGB rendering are not simulated.
	 */
	static FTextureChannelPackerPreviewResult BuildPreview(const FTextureChannelPackerRequest& Request, ETextureChannelPackerPreviewMode PreviewMode);
	/** @brief Returns editor source dimensions, or zero when the texture has no supported source object. */
	static FIntPoint GetTextureSourceSize(const UTexture* Texture);
};
