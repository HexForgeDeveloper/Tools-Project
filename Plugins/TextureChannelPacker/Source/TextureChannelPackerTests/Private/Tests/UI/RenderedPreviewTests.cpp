#include "UI/STextureChannelPackerWidget.h"

#if WITH_DEV_AUTOMATION_TESTS
	#include "Misc/AutomationTest.h"
	#include "Framework/Application/SlateApplication.h"
	#include "Widgets/Images/SImage.h"
	#include "Widgets/SWindow.h"
	#include "RHI.h"
	#include "RenderingThread.h"
	#include "ImageUtils.h"
	#include "Misc/FileHelper.h"
	#include "Misc/Paths.h"

/** @brief Executes a game-thread callback on successive automation frames. */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FTexturePackerWaitForRender, TFunction<bool()>, Callback);
bool FTexturePackerWaitForRender::Update()
{
	return Callback();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextureChannelPackerRenderedPreviewTest, "TextureChannelPacker.UI.RenderedPreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Verifies actual Slate screenshot pixels after the Preview button handler runs. @return True when queued. */
bool FTextureChannelPackerRenderedPreviewTest::RunTest(const FString&)
{
	if (GUsingNullRHI)
	{
		AddError(TEXT("RenderedPreview requires a real RHI; run the UI group without -NullRHI."));
		return false;
	}
	const auto Widget = SNew(STextureChannelPackerWidget);
	const auto Window = SNew(SWindow).Title(FText::FromString(TEXT("Texture Packer Preview Regression"))).ClientSize(FVector2D(960, 580))[Widget];
	FSlateApplication::Get().AddWindow(Window);
	Widget->OutputSettings.bUseFirstInputResolution = false;
	Widget->OutputSettings.Resolution = 2048;
	Widget->OutputSettings.SourceFormat = ETextureChannelPackerOutputFormat::BGRA8;
	const uint8 Values[] = { 240, 20, 80, 255 };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Widget->ChannelRows[Index].bUseConstant = true;
		Widget->ChannelRows[Index].ConstantValue = Values[Index];
	}
	const uint64 ReadyFrame = GFrameCounter + 10;
	ADD_LATENT_AUTOMATION_COMMAND(FTexturePackerWaitForRender([this, Widget, Window, TargetFrame = ReadyFrame, Phase = 0]() mutable {
		if (GFrameCounter < TargetFrame)
		{
			return false;
		}
		if (Phase == 0)
		{
			// Reproduce a real click after the empty preview has already been arranged and cached.
			Widget->OnPreviewClicked();
			TargetFrame = GFrameCounter + 10;
			Phase = 1;
			return false;
		}
		FlushRenderingCommands();
		TestTrue(TEXT("Generated texture is retained"), Widget->PreviewTexture.IsValid());
		TestTrue(TEXT("Image has nonzero arranged geometry"), Widget->PreviewImage->GetCachedGeometry().GetLocalSize().GetMin() > 0);
		TArray<FColor> Pixels;
		FIntVector Size;
		TestTrue(TEXT("Slate screenshot succeeds"), FSlateApplication::Get().TakeScreenshot(Widget->PreviewImage.ToSharedRef(), Pixels, Size));
		if (Size.X > 0 && Size.Y > 0 && Pixels.Num() == Size.X * Size.Y)
		{
			TArray64<uint8> Png;
			FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
			const FString Screenshot = FPaths::ProjectSavedDir() / TEXT("Automation/TextureChannelPacker/RenderedPreview.png");
			TestTrue(TEXT("Rendered evidence screenshot is saved"), FFileHelper::SaveArrayToFile(Png, *Screenshot));
		}
		int32 MatchingPixels = 0;
		for (const FColor Pixel : Pixels)
		{
			if (Pixel.R > 180 && Pixel.G < 110 && Pixel.B > 30 && Pixel.B < 180)
			{
				++MatchingPixels;
			}
		}
		TestTrue(TEXT("Rendered preview contains the generated color, not an empty background"), MatchingPixels > Pixels.Num() / 2 && MatchingPixels > 0);
		Window->RequestDestroyWindow();
		return true;
	}));
	return true;
}
#endif
