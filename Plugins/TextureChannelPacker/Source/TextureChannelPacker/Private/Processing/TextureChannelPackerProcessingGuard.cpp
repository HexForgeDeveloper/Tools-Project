#include "Processing/TextureChannelPackerProcessingGuard.h"
#include "Misc/FeedbackContext.h"

#define LOCTEXT_NAMESPACE "TextureChannelPackerGuard"

namespace TextureChannelPackerTexture
{
#if WITH_DEV_AUTOMATION_TESTS
	TFunction<bool(FProcessingGuard::EStage)> FProcessingGuard::ShouldCancelForTesting;
#endif
	bool FProcessingGuard::ValidateBuffer(const int32 Width, const int32 Height, const int32 BytesPerPixel, FText& OutError)
	{
		if (Width <= 0 || Height <= 0 || Width > 16384 || Height > 16384 || BytesPerPixel <= 0 || BytesPerPixel > 16)
		{
			OutError = LOCTEXT("InvalidDimensions", "Texture dimensions must be between 1 and 16384 and use a supported pixel layout.");
			return false;
		}
		const int64 Bytes = static_cast<int64>(Width) * Height * BytesPerPixel;
		if (Bytes > MaxBufferBytes)
		{
			OutError = LOCTEXT("BufferLimit", "Texture buffer exceeds the 512 MiB processing limit. Reduce the output resolution or source precision.");
			return false;
		}
		return true;
	}

	bool FProcessingGuard::ValidateTexture(const int32 Width, const int32 Height, const ETextureSourceFormat Format, FText& OutError)
	{
		int32 Bytes = 0;
		switch (Format)
		{
			case TSF_G8:
				Bytes = 1;
				break;
			case TSF_G16:
			case TSF_R16F:
				Bytes = 2;
				break;
			case TSF_BGRA8:
			case TSF_R32F:
				Bytes = 4;
				break;
			case TSF_RGBA16:
			case TSF_RGBA16F:
				Bytes = 8;
				break;
			case TSF_RGBA32F:
				Bytes = 16;
				break;
			default:
				break;
		}
		return ValidateBuffer(Width, Height, Bytes, OutError);
	}

	bool FProcessingGuard::IsCancelled(FText& OutError, const EStage Stage)
	{
		bool bCancelled = GWarn && GWarn->ReceivedUserCancel();
#if WITH_DEV_AUTOMATION_TESTS
		bCancelled |= ShouldCancelForTesting && ShouldCancelForTesting(Stage);
#endif
		if (bCancelled)
		{
			OutError = LOCTEXT("Cancelled", "Texture channel operation was cancelled.");
		}
		return bCancelled;
	}
} // namespace TextureChannelPackerTexture

#undef LOCTEXT_NAMESPACE
