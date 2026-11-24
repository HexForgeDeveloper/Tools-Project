// Copyright 2025 DimAlek. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "Modules/ModuleManager.h"

class SWindow;
struct FAssetAuditRow;

/** @brief Exposes Asset Audit to Map directly on Content Browser assets and folders. */
class CONTENTBROWSERTOOLKIT_API FContentBrowserToolkitModule : public IModuleInterface
{
public:
	/** @brief Registers menu extensions when ToolMenus is ready. */
	virtual void StartupModule() override;
	/** @brief Removes callbacks and module-owned windows before unloading. */
	virtual void ShutdownModule() override;

private:
	/** @brief Adds matching asset, folder and Window menu actions. */
	void RegisterMenus();
	/** @brief Opens an audit scoped to a captured folder or exact asset selection. */
	void Open(FString Folder, TArray<FAssetData> Assets = {});
	/** @brief Opens a read-only folder report without requiring a map. */
	void OpenFolderReport(const FString& Folder, bool bFindReferencers);
	/** @brief Displays inspection results in the shared asset report window. */
	void OpenInspection(const FText& Title, TArray<FAssetAuditRow> Rows);
	/** @brief Prompts for a destination before generating a migration preview. */
	void OpenMigrationPreview(const FString& Folder);
	TArray<TWeakPtr<SWindow>> Windows; ///< @brief Windows holding module callback code.
};
