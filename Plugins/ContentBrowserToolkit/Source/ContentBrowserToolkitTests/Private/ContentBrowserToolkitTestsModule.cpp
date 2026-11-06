#include "Modules/ModuleManager.h"

/** @brief Loads the standalone editor automation module without adding production dependencies. */
class FContentBrowserToolkitTestsModule : public IModuleInterface
{
};

IMPLEMENT_MODULE(FContentBrowserToolkitTestsModule, ContentBrowserToolkitTests)