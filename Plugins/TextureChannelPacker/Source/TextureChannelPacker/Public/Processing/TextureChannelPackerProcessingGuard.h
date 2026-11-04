#pragma once
#include "Models/TextureChannelPackerTypes.h"

namespace TextureChannelPackerTexture
{
	/** @brief Shared game-thread cancellation and overflow-safe limits applied before buffer allocation. */
	class TEXTURECHANNELPACKER_API FProcessingGuard final
	{
	public:
		/** @brief Identifies stable cooperative cancellation boundaries for deterministic regressions. */
		enum class EStage : uint8 
		{ 
			Checkpoint, 
			ReadBlock, 
			WriteRow, 
			PreviewRow 
		};

		/** Per-buffer cap, not a bound on total process memory or platform texture compilation. */
		static constexpr int64 MaxBufferBytes = 512ll * 1024 * 1024;
		
		/** @brief Validates dimensions, storage width and byte count before converting to int32-sized storage. */
		static bool ValidateBuffer(int32 Width, int32 Height, int32 BytesPerPixel, FText& OutError);
		
		/** @brief Validates a supported source layout and its maximum single-buffer allocation. */
		static bool ValidateTexture(int32 Width, int32 Height, ETextureSourceFormat Format, FText& OutError);
		
		/** @brief Returns true and sets an error when the user requests cancellation. */
		static bool IsCancelled(FText& OutError, EStage Stage = EStage::Checkpoint);

#if WITH_DEV_AUTOMATION_TESTS
		/** @brief Scoped test seam for deterministic cancellation at processing checkpoints. */
		static TFunction<bool(EStage)> ShouldCancelForTesting;
#endif
	
	};
} // namespace TextureChannelPackerTexture
