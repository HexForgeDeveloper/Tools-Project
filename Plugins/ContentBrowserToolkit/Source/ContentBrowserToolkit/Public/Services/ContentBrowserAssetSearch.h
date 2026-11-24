#pragma once

#include "CoreMinimal.h"

struct FAssetData;

/** @brief Display values captured without retaining loaded assets. */
struct FAssetAuditRow
{
	FString Name;
	FString Path;
	FString Summary;
	FString Details;
};

/** @brief Searches saved registry relationships or checks textures in the selected folders. */
enum class ECBTAssetSearch
{
	MaterialInstances,
	ChildBlueprints,
	SkeletonAnimations,
	TextureSets
};

/** @brief Creates report rows for the supported assets in the current selection. */
CONTENTBROWSERTOOLKIT_API TArray<FAssetAuditRow> BuildContentBrowserAssetSearch(ECBTAssetSearch Search, const TArray<FAssetData>& Assets);

/** @brief Reports naming violations in the selected package path and its descendants. */
CONTENTBROWSERTOOLKIT_API TArray<FAssetAuditRow> BuildContentBrowserNamingAudit(const FString& Folder);
