#pragma once

#include "CoreMinimal.h"

struct FAssetData;
class SWindow;

/** @brief Creates a read-only settings comparison for the selected texture assets. */
TSharedRef<SWindow> CreateContentBrowserTextureAuditWindow(const TArray<FAssetData>& Assets);
