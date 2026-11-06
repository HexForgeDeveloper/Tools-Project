#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"

/** @brief Conservative evidence categories; no category authorizes deletion. */
enum class ECBTUsageStatus : uint8
{
	MapDependency,
	Referenced,
	Protected,
	Review
};

/** @brief One asset and the evidence found by a read-only scan. */
struct FCBTUsageRow
{
	FAssetData Asset;
	ECBTUsageStatus Status = ECBTUsageStatus::Review;
	FString Evidence;
	int64 Bytes = 0;
};

/** @brief Scan results with explicit coverage limitations and cancellation state. */
struct FCBTUsageReport
{
	FString Folder;
	FString MapPackage;
	TArray<TSharedPtr<FCBTUsageRow>> Rows;
	TArray<FString> Warnings;
	FString Error;
	bool bCancelled = false;
};

/** @brief Read-only dependency audit; never deletes, renames, saves or unloads assets. */
class CONTENTBROWSERTOOLKIT_API FContentBrowserUsageAudit
{
public:
	/** @brief Traverses dependency chains with cycle protection and optional cancellation.
	 * @param Roots Packages to protect.
	 * @param Dependencies Provider of all outgoing dependency edges.
	 * @param Cancel Returns true to stop traversal.
	 * @param Parents Output predecessor map; roots have NAME_None predecessors.
	 * @return False when cancelled; partial output must not be interpreted as complete.
	 */
	static bool Traverse(const TArray<FName>& Roots, TFunctionRef<TArray<FName>(FName)> Dependencies, TFunctionRef<bool()> Cancel, TMap<FName, FName>& Parents);

	/** @brief Audits a Content Browser folder against a selected map and other project references.
	 * @param Folder Mounted package folder, scanned recursively.
	 * @param Map World asset selected by the user.
	 * @return Evidence, coverage warnings and rows; absence of evidence is review-only.
	 */
	static FCBTUsageReport Scan(const FString& Folder, const FAssetData& Map);

	/** @brief Returns the display name of an evidence category.
	 * @param Status Category.
	 * @return Stable human-readable name.
	 */
	static FString StatusText(ECBTUsageStatus Status);

	/** @brief Saves a UTF-8 CSV report under Project/Saved/ContentBrowserToolkit/Audits.
	 * @param Report Completed or partial report, with limitations recorded in every row.
	 * @return Absolute filename, or empty on failure.
	 */
	static FString Export(const FCBTUsageReport& Report);
};
