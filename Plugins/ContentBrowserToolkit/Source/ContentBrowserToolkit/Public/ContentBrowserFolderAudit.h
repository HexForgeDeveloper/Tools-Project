#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"

/** @brief One asset in a folder report. Disk size is -1 when its package is unavailable. */
struct FCBTFolderAuditRow
{
	FAssetData Asset;
	int64 Bytes = -1;
	TArray<FName> ExternalReferencers;
};

/** @brief Asset count and deduplicated package bytes for one class or child folder. */
struct FCBTFolderAuditGroup
{
	FString Name;
	int32 AssetCount = 0;
	int64 Bytes = 0;
};

/** @brief Read-only snapshot of a mounted content folder and its descendants. */
struct FCBTFolderAuditReport
{
	FString Folder;
	TArray<TSharedPtr<FCBTFolderAuditRow>> Rows;
	TArray<FCBTFolderAuditGroup> Classes;
	TArray<FCBTFolderAuditGroup> Subfolders;
	FString Error;
	int64 TotalBytes = 0;
	int32 UnavailablePackages = 0;
	bool bReferenceAudit = false;
};

/** @brief Inspects saved package sizes and direct package referencers through the Asset Registry. */
class CONTENTBROWSERTOOLKIT_API FContentBrowserFolderAudit
{
public:
	/** @brief Recursively inventories a folder or collects its incoming package references.
	 * @param Folder Mounted Unreal package path, such as /Game/Props.
	 * @param bFindReferencers Include direct hard and soft package references from outside the folder.
	 * @return Report with an error when the path is invalid or the registry is still discovering assets.
	 */
	static FCBTFolderAuditReport Scan(const FString& Folder, bool bFindReferencers);

	/** @brief Writes all rows and evidence to a new UTF-8 CSV file under Project/Saved. */
	static FString Export(const FCBTFolderAuditReport& Report);
};
