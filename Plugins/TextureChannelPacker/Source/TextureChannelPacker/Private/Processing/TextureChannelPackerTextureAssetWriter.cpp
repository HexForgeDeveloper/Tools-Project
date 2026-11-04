// Copyright 2025 DimAlek. All Rights Reserved.

#include "Processing/TextureChannelPackerTextureAssetWriter.h"
#include "Processing/TextureChannelPackerTextureSourceCodec.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture2D.h"
#include "Misc/PackageName.h"
#include "Services/TextureChannelPackerPathService.h"
#include "UObject/SavePackage.h"
#include "HAL/FileManager.h"

#define LOCTEXT_NAMESPACE "TextureChannelPackerTextureService"

namespace TextureChannelPackerTexture
{
#if WITH_DEV_AUTOMATION_TESTS
	TFunction<bool(const FString&)> FTextureAssetWriter::ShouldFailSaveForTesting;
#endif

	bool FTextureAssetWriter::DiscardTexture(UTexture2D* Texture, const bool bRegistered, FText& OutError)
	{
		if (!Texture)
		{
			return true;
		}
		UPackage* Package = Texture->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		if (IFileManager::Get().FileExists(*Filename) && !IFileManager::Get().Delete(*Filename, false, false))
		{
			OutError = FText::Format(LOCTEXT("RollbackDeleteFailed", "Could not remove generated file: {0}"), FText::FromString(Filename));
			return false;
		}
		if (bRegistered)
		{
			FAssetRegistryModule::AssetDeleted(Texture);
		}
		Texture->ClearFlags(RF_Public | RF_Standalone | RF_Transactional);
		Texture->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
		Package->SetDirtyFlag(false);
		return true;
	}
	void FTextureAssetWriter::ApplyTextureSettings(UTexture2D* Texture, const FTextureChannelPackerOutputSettings& Settings)
	{
		Texture->CompressionSettings = Settings.CompressionSettings;
		Texture->MipGenSettings = Settings.MipGenSettings;
		Texture->Filter = Settings.Filter;
		Texture->LODGroup = Settings.LODGroup;
		Texture->SRGB = Settings.bSRGB;
		Texture->VirtualTextureStreaming = Settings.bVirtualTextureStreaming;
	}

	bool FTextureAssetWriter::SaveTexturePackage(UTexture2D* Texture, FText& OutError)
	{
		if (!Texture)
		{
			OutError = LOCTEXT("SaveMissingTexture", "Could not save the generated texture because the texture object is invalid.");
			return false;
		}

		UPackage* Package = Texture->GetOutermost();
		if (!Package)
		{
			OutError = FText::Format(LOCTEXT("SaveMissingPackage", "Could not resolve the package for texture '{0}'."), FText::FromString(Texture->GetName()));
			return false;
		}

		const FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
#if WITH_DEV_AUTOMATION_TESTS
		if (ShouldFailSaveForTesting && ShouldFailSaveForTesting(Package->GetName()))
		{
			OutError = LOCTEXT("InjectedSaveFailure", "Injected texture package save failure.");
			return false;
		}
#endif

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = EObjectFlags::RF_Public | EObjectFlags::RF_Standalone;
		SaveArgs.Error = GError;
		SaveArgs.SaveFlags = SAVE_NoError;
		SaveArgs.bWarnOfLongFilename = false;

		if (!UPackage::SavePackage(Package, Texture, *PackageFileName, SaveArgs))
		{
			OutError = FText::Format(LOCTEXT("SavePackageFailed", "Could not save generated texture package: {0}"), FText::FromString(PackageFileName));
			return false;
		}

		return true;
	}

	UTexture2D* FTextureAssetWriter::CreateTextureAsset(const FString& OutputPackagePath, const FString& AssetName, const int32 Width, const int32 Height, const ETextureSourceFormat OutputFormat,
		const TArray<FResolvedOutputChannel>& Channels, const FTextureChannelPackerOutputSettings& Settings, FText& OutError)
	{
		FString PackageName;
		FString UniqueAssetName;
		if (!FTextureChannelPackerPathService::MakeUniqueAssetPackageName(OutputPackagePath, AssetName, PackageName, UniqueAssetName))
		{
			OutError = FText::Format(LOCTEXT("InvalidPackageName", "Invalid output path or asset name: {0}"), FText::FromString(OutputPackagePath / AssetName));
			return nullptr;
		}

		UPackage* Package = CreatePackage(*PackageName);
		if (!Package)
		{
			OutError = FText::Format(LOCTEXT("CreatePackageFailed", "Could not create package: {0}"), FText::FromString(PackageName));
			return nullptr;
		}

		Package->FullyLoad();
		Package->Modify();

		UTexture2D* Texture = NewObject<UTexture2D>(Package, FName(*UniqueAssetName), RF_Public | RF_Standalone | RF_Transactional);
		if (!Texture)
		{
			OutError = FText::Format(LOCTEXT("CreateTextureFailed", "Could not create texture asset: {0}"), FText::FromString(UniqueAssetName));
			return nullptr;
		}

		Texture->Modify();
		if (!FTextureSourceCodec::WriteTextureSource(Texture, Width, Height, OutputFormat, Channels, OutError))
		{
			FText CleanupError;
			if (!DiscardTexture(Texture, false, CleanupError))
			{
				OutError = FText::Format(LOCTEXT("SourceCleanupFailed", "{0}\n{1}"), OutError, CleanupError);
			}
			return nullptr;
		}

		ApplyTextureSettings(Texture, Settings);
		Texture->PostEditChange();
		Texture->UpdateResource();

		Texture->MarkPackageDirty();
		Package->MarkPackageDirty();
		if (!SaveTexturePackage(Texture, OutError))
		{
			FText CleanupError;
			if (!DiscardTexture(Texture, false, CleanupError))
			{
				OutError = FText::Format(LOCTEXT("SaveCleanupFailed", "{0}\n{1}"), OutError, CleanupError);
			}
			return nullptr;
		}
		FAssetRegistryModule::AssetCreated(Texture);

		return Texture;
	}
} // namespace TextureChannelPackerTexture

#undef LOCTEXT_NAMESPACE
