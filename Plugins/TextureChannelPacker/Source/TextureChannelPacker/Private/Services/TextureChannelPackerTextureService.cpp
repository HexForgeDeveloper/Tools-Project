// Copyright 2025 DimAlek. All Rights Reserved.

#include "Services/TextureChannelPackerTextureService.h"
#include "Processing/TextureChannelPackerTextureOperations.h"
#include "Processing/TextureChannelPackerTexturePreviewBuilder.h"
#include "Engine/Texture2D.h"
#include "Misc/ScopedSlowTask.h"
#include "ScopedTransaction.h"
#include "Services/TextureChannelPackerPathService.h"

#define LOCTEXT_NAMESPACE "TextureChannelPackerTextureService"

FTextureChannelPackerResult FTextureChannelPackerTextureService::Execute(const FTextureChannelPackerRequest& Request)
{
	check(IsInGameThread());
	if (FTextureChannelPackerPathService::NormalizeOutputPath(Request.OutputPackagePath).IsEmpty())
	{
		FTextureChannelPackerResult Result;
		Result.Message = FText::Format(LOCTEXT("InvalidOutputDirectory", "Invalid output folder: {0}. Choose /Game or a folder inside the project's Content directory."), FText::FromString(Request.OutputPackagePath));
		return Result;
	}

	const FScopedTransaction Transaction(LOCTEXT("TextureChannelPackerTransaction", "Texture Channel Packer Operation"));
	FScopedSlowTask SlowTask(1.0f, LOCTEXT("ProcessingTextures", "Processing texture channels..."));
	SlowTask.MakeDialog(true);

	if (SlowTask.ShouldCancel())
	{
		FTextureChannelPackerResult Result;
		Result.Message = LOCTEXT("OperationCancelled", "Texture channel operation was cancelled.");
		return Result;
	}

	switch (Request.Operation)
	{
		case ETextureChannelPackerOperation::Unpack:
			return TextureChannelPackerTexture::FTextureOperations::ExecuteUnpack(Request);
		case ETextureChannelPackerOperation::Pack:
		case ETextureChannelPackerOperation::Repack:
		case ETextureChannelPackerOperation::Copy:
		default:
			return TextureChannelPackerTexture::FTextureOperations::ExecutePackLike(Request);
	}
}

FTextureChannelPackerPreviewResult FTextureChannelPackerTextureService::BuildPreview(const FTextureChannelPackerRequest& Request, const ETextureChannelPackerPreviewMode PreviewMode)
{
	check(IsInGameThread());

	switch (Request.Operation)
	{
		case ETextureChannelPackerOperation::Unpack:
			return TextureChannelPackerTexture::FTexturePreviewBuilder::BuildUnpackPreview(Request, PreviewMode);
		case ETextureChannelPackerOperation::Pack:
		case ETextureChannelPackerOperation::Repack:
		case ETextureChannelPackerOperation::Copy:
		default:
			return TextureChannelPackerTexture::FTexturePreviewBuilder::BuildPackLikePreview(Request, PreviewMode);
	}
}

bool FTextureChannelPackerTextureService::CanReadTexture(const UTexture* Texture, FText& OutError)
{
	const UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
	if (!Texture2D)
	{
		OutError = LOCTEXT("CanReadUnsupportedClass", "Only Texture2D assets can be read by this tool.");
		return false;
	}

#if WITH_EDITORONLY_DATA
	if (Texture2D->Source.GetSizeX() <= 0 || Texture2D->Source.GetSizeY() <= 0 || Texture2D->Source.GetFormat() == TSF_Invalid)
	{
		OutError = FText::Format(LOCTEXT("CanReadMissingSource", "Texture '{0}' has no readable source data."), FText::FromString(Texture2D->GetName()));
		return false;
	}

	return true;
#else
	OutError = LOCTEXT("CanReadNeedsEditorData", "Editor-only texture source data is required.");
	return false;
#endif
}

FIntPoint FTextureChannelPackerTextureService::GetTextureSourceSize(const UTexture* Texture)
{
	const UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
	if (!Texture2D)
	{
		return FIntPoint::ZeroValue;
	}

#if WITH_EDITORONLY_DATA
	return FIntPoint(Texture2D->Source.GetSizeX(), Texture2D->Source.GetSizeY());
#else
	return FIntPoint::ZeroValue;
#endif
}

#undef LOCTEXT_NAMESPACE
