// Copyright 2025 DimAlek. All Rights Reserved.

#include "Services/TextureChannelPackerPathService.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"

namespace TexturePathUtils
{
	/** @brief Tests a directory boundary rather than accepting similarly named sibling directories. */
	static bool IsWithin(const FString& Path, const FString& Root)
	{
		return Path.Equals(Root, ESearchCase::IgnoreCase) || Path.StartsWith(Root + TEXT("/"), ESearchCase::IgnoreCase);
	}
} // namespace TexturePathUtils

FString FTextureChannelPackerPathService::NormalizeOutputPath(const FString& InPath)
{
	FString Path = InPath.TrimStartAndEnd();
	Path.ReplaceInline(TEXT("\\"), TEXT("/"));
	FPaths::RemoveDuplicateSlashes(Path);
	FPaths::NormalizeDirectoryName(Path);
	if (Path.IsEmpty())
	{
		return FString();
	}

	// Reject traversal explicitly instead of silently changing the user's destination.
	TArray<FString> Segments;
	Path.ParseIntoArray(Segments, TEXT("/"), true);
	if (Segments.Contains(TEXT("..")) || Segments.Contains(TEXT(".")))
	{
		return FString();
	}

	FString PackagePath;
	if (TexturePathUtils::IsWithin(Path, TEXT("/Game")))
	{
		PackagePath = TEXT("/Game") + Path.Mid(5);
	}
	else
	{
		if (FPaths::IsRelative(Path))
		{
			return FString();
		}

		FString ContentRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
		FPaths::NormalizeDirectoryName(ContentRoot);
		if (!TexturePathUtils::IsWithin(Path, ContentRoot))
		{
			return FString();
		}
		PackagePath = TEXT("/Game") + Path.Mid(ContentRoot.Len());
	}

	return FPackageName::IsValidLongPackageName(PackagePath / TEXT("FolderValidation")) ? PackagePath : FString();
}
FString FTextureChannelPackerPathService::SanitizeAssetName(const FString& InAssetName)
{
	const FString TrimmedName = InAssetName.TrimStartAndEnd();
	const FString SanitizedName = ObjectTools::SanitizeObjectName(TrimmedName.IsEmpty() ? TEXT("T_Packed_Texture") : TrimmedName);

	return SanitizedName.IsEmpty() ? TEXT("T_Packed_Texture") : SanitizedName;
}

FString FTextureChannelPackerPathService::BuildPackageName(const FString& OutputPackagePath, const FString& AssetName)
{
	const FString Folder = NormalizeOutputPath(OutputPackagePath);
	return Folder.IsEmpty() ? FString() : Folder / SanitizeAssetName(AssetName);
}

bool FTextureChannelPackerPathService::MakeUniqueAssetPackageName(const FString& OutputPackagePath, const FString& AssetName, FString& OutPackageName, FString& OutAssetName)
{
	OutPackageName.Reset();
	OutAssetName.Reset();
	const FString BasePackageName = BuildPackageName(OutputPackagePath, AssetName);
	if (!FPackageName::IsValidLongPackageName(BasePackageName))
	{
		return false;
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(BasePackageName, TEXT(""), OutPackageName, OutAssetName);

	return !OutPackageName.IsEmpty() && !OutAssetName.IsEmpty();
}
