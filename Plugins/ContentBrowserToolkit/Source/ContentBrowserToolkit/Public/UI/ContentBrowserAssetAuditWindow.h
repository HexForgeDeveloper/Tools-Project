#pragma once

#include "Services/ContentBrowserAssetSearch.h"

struct FAssetData;
class SWindow;

/** @brief Identifies the relationship inspected for a selected asset. */
enum class ECBTAssetAudit
{
	MaterialParents,
	BlueprintParent,
	AnimationSkeleton
};

/** @brief Creates a read-only report for a selection filtered to the requested audit type. */
CONTENTBROWSERTOOLKIT_API TSharedRef<SWindow> CreateContentBrowserAssetAuditWindow(const FText& Title, ECBTAssetAudit Audit, const TArray<FAssetData>& Assets);

/** @brief Displays report rows and explains an empty result. */
CONTENTBROWSERTOOLKIT_API TSharedRef<SWindow> CreateContentBrowserAssetReportWindow(const FText& Title, TArray<FAssetAuditRow> Rows, const FText& EmptyMessage);
