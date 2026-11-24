// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Misc/ContentBrowserTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Views/SListView.h"

class SHeaderRow;
class STextBlock;

/**
 * @brief Context-driven audit: selected assets/folder, map picker, compact table and evidence panel.
 */
class CONTENTBROWSERTOOLKIT_API SContentUsageAuditWidget final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SContentUsageAuditWidget) { }
	SLATE_ARGUMENT(FString, Folder)
	SLATE_ARGUMENT(TArray<FAssetData>, Assets)
	SLATE_END_ARGS()

	/** @brief Builds a selection-specific audit without requiring manual path entry. */
	void Construct(const FArguments& Args);

private:
	/** @brief Returns the picker path without loading the selected map. */
	FString MapPath() const;

	/** @brief Invalidates the previous snapshot when the target map changes. */
	void SelectMap(const FAssetData& Asset);

	/** @brief Filters the current snapshot without rescanning assets. */
	void Refresh();

	/** @brief Scans selected folders, retaining only explicitly selected assets in asset mode. */
	FReply Scan();

	/** @brief Displays evidence only for the selected row, keeping the table compact. */
	void SelectRow(TSharedPtr<FCBTUsageRow> Item, ESelectInfo::Type);

	/** @brief Exports all results, including rows hidden by the current UI filter. */
	FReply Export();

	/** @brief Selects an audited asset in Content Browser without editing it. */
	void Locate(TSharedPtr<FCBTUsageRow> Item);

	/** @brief Getter for header row FCBTUsageRow list */
	[[nodiscard]] TSharedRef<SHeaderRow> GetHeaderRowWidget() const;

	FString Folder;										  ///< @brief Context folder; no manual path entry required.
	FString Search;										  ///< @brief Asset name/path search.
	FAssetData SelectedMap;								  ///< @brief Target map.
	FCBTUsageReport Report;								  ///< @brief Full results, independent of filters.
	TArray<FAssetData> Assets;							  ///< @brief Exact asset selection captured when opening the context menu.
	TArray<TSharedPtr<FCBTUsageRow>> VisibleRows;		  ///< @brief Filtered view.
	TArray<TSharedPtr<FString>> Filters;				  ///< @brief Stable status options.
	TSharedPtr<FString> ActiveFilter;					  ///< @brief Selected status filter.
	TSharedPtr<STextBlock> Summary, Details;			  ///< @brief Counts and selected-row evidence.
	TSharedPtr<SComboBox<TSharedPtr<FString>>> FilterBox; ///< @brief Status selector.
	TSharedPtr<SListView<TSharedPtr<FCBTUsageRow>>> List; ///< @brief Compact results.
};
