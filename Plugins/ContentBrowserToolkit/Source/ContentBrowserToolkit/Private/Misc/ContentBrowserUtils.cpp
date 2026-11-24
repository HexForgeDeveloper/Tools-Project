// Fill out your copyright notice in the Description page of Project Settings.


#include "Misc/ContentBrowserUtils.h"
#include "Styling/AppStyle.h"

/** @brief Creates a menu icon from a built-in editor style brush. */
FSlateIcon ContentBrowserToolkit::MenuIcon(FName BrushName)
{
	return FSlateIcon(FAppStyle::GetAppStyleSetName(), BrushName);
}

FString ContentBrowserToolkit::UsageLabel(ECBTUsageStatus Status) noexcept
{
	switch (Status)
	{
		case ECBTUsageStatus::MapDependency:
			return TEXT("Used in map");
		case ECBTUsageStatus::Referenced:
			return TEXT("Used elsewhere");
		case ECBTUsageStatus::Protected:
			return TEXT("Needs checking");
		default:
			return TEXT("No references found");
	}
}
