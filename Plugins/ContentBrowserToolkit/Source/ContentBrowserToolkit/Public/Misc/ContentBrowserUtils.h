// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Misc/ContentBrowserTypes.h"
#include "Textures/SlateIcon.h"

namespace ContentBrowserToolkit
{
	/** @brief Creates a menu icon from a built-in editor style brush. */
	CONTENTBROWSERTOOLKIT_API FSlateIcon MenuIcon(FName BrushName);

	/** @brief Short labels for the result table; detailed evidence stays in the selection panel. */
	CONTENTBROWSERTOOLKIT_API FString UsageLabel(ECBTUsageStatus Status) noexcept;

} // namespace ContentBrowserToolkit
