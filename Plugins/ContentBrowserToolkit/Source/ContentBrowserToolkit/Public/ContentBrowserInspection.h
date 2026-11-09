#pragma once

#include "ContentBrowserAssetSearch.h"

/** @brief Identifies the inspection to run against the current selection. */
enum class ECBTInspection
{
	UsageChains,
	ReplacementCandidates,
	TextureDuplicates,
	BlueprintDependencies,
	MaterialOverrides,
	AnimationSet,
	StaticMeshes,
	SkeletalMeshes,
	AudioImport
};

/** @brief Returns evidence rows for the selected inspection without editing assets. */
CONTENTBROWSERTOOLKIT_API TArray<FAssetAuditRow> BuildCBTInspection(ECBTInspection Inspection, const TArray<FAssetData>& Assets);

/** @brief Reports external dependencies, inbound references and proposed destination conflicts. */
CONTENTBROWSERTOOLKIT_API TArray<FAssetAuditRow> BuildCBTMigrationPreview(const FString& Folder, const FString& Destination);

/** @brief Reports cleanup candidates from saved metadata and empty package paths. */
CONTENTBROWSERTOOLKIT_API TArray<FAssetAuditRow> BuildCBTCleanupCandidates(const FString& Folder);
