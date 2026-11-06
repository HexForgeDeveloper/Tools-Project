// Copyright (c) 2026 DimAlek. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Types/DataAssetDiffTypes.h"
class SWindow;
class ITableRow;
class STableViewBase;

/** @brief Builds an independent asset comparison window with optional change notifications. */
class DATAASSETMANAGER_API FDataAssetDiffWindow final
{
public:
	/** @brief Creates an unshown window; callers must supply a comparable diff and avoid strong owner captures in OnChanged. */
	static TSharedRef<SWindow> Create(TSharedRef<FDataAssetDiffResult> DiffResult, FSimpleDelegate OnChanged = FSimpleDelegate());

private:
	/** @brief Builds a difference row and its copy controls. */
	static TSharedRef<ITableRow> GenerateDataAssetDiffRow(TSharedPtr<FDataAssetDiffEntry> Entry, const TSharedRef<STableViewBase>& OwnerTable, TSharedRef<FDataAssetDiffResult> DiffResult, TSharedRef<TFunction<void()>> RefreshDiff);

	/** @brief Copies the selected property through the asset service and refreshes the comparison. */
	static void CopyDataAssetDiffValue(TSharedPtr<FDataAssetDiffEntry> Entry, TSharedRef<FDataAssetDiffResult> DiffResult, bool bLeftToRight, TSharedRef<TFunction<void()>> RefreshDiff);
};
