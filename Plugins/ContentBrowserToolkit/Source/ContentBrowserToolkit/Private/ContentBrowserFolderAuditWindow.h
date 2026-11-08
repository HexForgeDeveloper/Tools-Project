#pragma once

#include "Templates/SharedPointer.h"

class FString;
class SWindow;

/** @brief Creates a report window for one mounted Content Browser folder. */
TSharedRef<SWindow> CreateContentBrowserFolderAuditWindow(const FString& Folder, bool bFindReferencers);
