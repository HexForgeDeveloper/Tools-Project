// Copyright (c) 2026 DimAlek. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Types/DataAssetReferenceTypes.h"
class SWindow;
class SWidget;
class ITableRow;
class STableViewBase;

/** @brief Builds a reference inspector whose callbacks and data do not depend on the manager widget lifetime. */
class DATAASSETMANAGER_API FDataAssetReferenceInspectorWindow final
{
public:
	/** @brief Creates an unshown window holding its own reference inspection snapshot. */
	static TSharedRef<SWindow> Create(const TSharedPtr<FAssetData>& AssetData);
private:
	/** @brief Builds one dependency or referencer section and retains its list data. */
	static TSharedRef<SWidget> BuildReferenceListSection(FText Title, FText EmptyText, TSharedRef<FDataAssetReferenceInspectionResult> InspectionResult, bool bReferencedBy);
	/** @brief Creates an asset row with independent navigation callbacks. */
	static TSharedRef<ITableRow> GenerateReferenceEntryRow(TSharedPtr<FDataAssetReferenceEntry> Entry, const TSharedRef<STableViewBase>& OwnerTable);
	/** @brief Selects a resolved entry in Content Browser. */
	static void SyncReferenceEntryInContentBrowser(TSharedPtr<FDataAssetReferenceEntry> Entry);
	/** @brief Opens a resolved entry using the editor asset subsystem. */
	static void OpenReferenceEntryInEditor(TSharedPtr<FDataAssetReferenceEntry> Entry);
};
