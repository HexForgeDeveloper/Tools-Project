// Copyright (c) 2026 DimAlek. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class SWindow;

/**
 * @brief Displays the folder and class selection workflow for creating a Data Asset.
 */
class DATAASSETMANAGER_API FDataAssetCreationWindow final
{
public:
	/**
	 * @brief Builds an unshown creation window without creating an asset or opening the class picker.
	 * @return Window whose callbacks retain only weak references to the window itself.
	 */
	static TSharedRef<SWindow> Create();

	/**
	 * @brief Shows the creation workflow independently of the main manager widget.
	 */
	static void Show();
};
