#pragma once

#include "Templates/SharedPointer.h"

class FString;
class SWindow;

/** @brief Creates a report window for one mounted Content Browser folder. */
CONTENTBROWSERTOOLKIT_API TSharedRef<SWindow> CreateContentBrowserFolderAuditWindow(const FString& Folder, bool bFindReferencers);
