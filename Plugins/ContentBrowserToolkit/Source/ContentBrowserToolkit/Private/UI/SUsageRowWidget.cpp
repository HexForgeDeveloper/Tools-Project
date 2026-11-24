// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/SUsageRowWidget.h"

namespace ContentBrowserToolkit
{
	const FLinearColor SuccessColor = FLinearColor(0.35f, 0.85f, 0.45f);
	const FLinearColor WarningColor = FLinearColor(0.45f, 0.7f, 1.f);
	const FLinearColor ErrorColor = FLinearColor(1.f, 0.7f, 0.3f);

	/** @brief Short labels for the result table; detailed evidence stays in the selection panel. */
	FString UsageLabel(ECBTUsageStatus Status)
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
} // namespace ContentBrowserToolkit



void SUsageRowWidget::Construct(const FArguments& Args, const TSharedRef<STableViewBase>& Owner)
{
	Item = Args._Item;
	SMultiColumnTableRow::Construct(FSuperRowType::FArguments().Padding(5), Owner);
}

TSharedRef<SWidget> SUsageRowWidget::GenerateWidgetForColumn(const FName& Column)
{
	FString Value;
	FLinearColor Color = FLinearColor::White;

	if (Column == TEXT("Asset"))
	{
		Value = Item->Asset.AssetName.ToString();
	}
	else if (Column == TEXT("Type"))
	{
		Value = Item->Asset.AssetClassPath.GetAssetName().ToString();
	}
	else if (Column == TEXT("Size"))
	{
		Value = FString::Printf(TEXT("%.2f MiB"), Item->Bytes / (1024.0 * 1024.0));
	}
	else
	{
		Value = ContentBrowserToolkit::UsageLabel(Item->Status);
		Color = Item->Status == ECBTUsageStatus::MapDependency ? ContentBrowserToolkit::SuccessColor
			: Item->Status == ECBTUsageStatus::Referenced	   ? ContentBrowserToolkit::WarningColor
															   : ContentBrowserToolkit::ErrorColor;
	}

	return SNew(STextBlock)
		.Text(FText::FromString(Value))
		.ColorAndOpacity(Color)
		.ToolTipText(FText::FromString(Item->Asset.GetObjectPathString()));
}
