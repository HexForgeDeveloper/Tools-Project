// Copyright 2025 DimAlek. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SWindow;

/** @brief Editor module interface for opening the modeless packing tool. */
class ITextureChannelPackerModule : public IModuleInterface
{
public:
	/** @brief Opens the tool or brings its existing window to the front; call on the game thread. */
	virtual void OpenPluginWindow() = 0;
};

/** @brief Registers editor menus and manages a single tool window owned by the main editor frame. */
class FTextureChannelPackerModule : public ITextureChannelPackerModule
{
public:
	/** @brief Registers the Tools menu entry. */
	virtual void StartupModule() override;
	/** @brief Removes owned menu entries and requests destruction of the tool window. */
	virtual void ShutdownModule() override;

	/** @brief Creates a native child window, with a standalone fallback when no main frame is available. */
	virtual void OpenPluginWindow() override;

private:
	/** Slate owns the window; this weak reference avoids extending its lifetime after closure. */
	TWeakPtr<SWindow> ToolWindow;
};
