// Copyright (c) 2026 DimAlek. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FAssetData;
class SWindow;

/**
 * @brief Displays the destination selection workflow for moving Data Assets.
 */
class DATAASSETMANAGER_API FDataAssetMoveWindow final
{
public:
	/**
	 * @brief Builds an unshown move window with a snapshot of the assets to move.
	 * @param AssetsToMove Assets captured when the workflow is opened.
	 * @param OnAssetsMoved Callback receiving moved assets after a successful operation.
	 * @return Window whose callbacks do not strongly retain the window itself.
	 */
	static TSharedRef<SWindow> Create(
		TArray<TSharedPtr<FAssetData>> AssetsToMove,
		TFunction<void(const TArray<FAssetData>&)> OnAssetsMoved = {});

	/**
	 * @brief Shows the move workflow independently of the main manager widget.
	 * @param AssetsToMove Assets captured when the workflow is opened.
	 * @param OnAssetsMoved Callback receiving moved assets after a successful operation.
	 */
	static void Show(
		TArray<TSharedPtr<FAssetData>> AssetsToMove,
		TFunction<void(const TArray<FAssetData>&)> OnAssetsMoved = {});
};
