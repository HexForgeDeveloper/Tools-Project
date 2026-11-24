// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Services/ContentBrowserUsageAudit.h"
#include "Widgets/Views/STableRow.h"

/**
 * @brief Compact, fixed-height result with independent columns and a full-path tooltip.
 */
class CONTENTBROWSERTOOLKIT_API SUsageRowWidget : public SMultiColumnTableRow<TSharedPtr<FCBTUsageRow>>
{
public:
	SLATE_BEGIN_ARGS(SUsageRowWidget)
	{
	}
	SLATE_ARGUMENT(TSharedPtr<FCBTUsageRow>, Item)
	SLATE_END_ARGS()

	/** @brief Binds one result to its virtualized row. */
	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& Owner);

	/** @brief Generates one compact result cell. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& Column) override;

private:
	/** @brief Binds one result to its virtualized row. */
	TSharedPtr<FCBTUsageRow> Item;
};
