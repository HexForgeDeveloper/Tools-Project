// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/SContentUsageAuditWidget.h"
#include "ContentBrowserModule.h"
#include "Engine/World.h"
#include "IContentBrowserSingleton.h"
#include "Misc/ContentBrowserUtils.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "Services/ContentBrowserUsageAudit.h"
#include "UI/SUsageRowWidget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"

void SContentUsageAuditWidget::Construct(const FArguments& Args)
{
	Folder = Args._Folder;
	Assets = Args._Assets;

	TArray<const TCHAR*> Labels = //
		{
			TEXT("All results"),		 //
			TEXT("Used in map"),		 //
			TEXT("Used elsewhere"),		 //
			TEXT("No references found"), //
			TEXT("Needs checking")		 //
		};

	for (const TCHAR* Label : Labels)
	{
		Filters.Add(MakeShared<FString>(Label));
	}

	ActiveFilter = Filters[0];
	const FString Scope = Assets.IsEmpty() ? TEXT("Folder: ") + Folder + TEXT(" (including subfolders)")
		: Assets.Num() == 1				   ? TEXT("Asset: ") + Assets[0].AssetName.ToString()
										   : FString::Printf(TEXT("Selected assets: %d"), Assets.Num());
	/* clang-format off */
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(12)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Scope))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(12, 0, 12, 12)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 8, 0)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Check against map:")))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNew(SObjectPropertyEntryBox)
				.AllowedClass(UWorld::StaticClass())
				.ObjectPath(this, &SContentUsageAuditWidget::MapPath)
				.OnObjectChanged(this, &SContentUsageAuditWidget::SelectMap)
				.AllowClear(true)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(12, 0, 0, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Scan")))
				.OnClicked(this, &SContentUsageAuditWidget::Scan)
				.IsEnabled_Lambda([this] { return SelectedMap.IsValid(); })
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8, 0, 0, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Export CSV")))
				.OnClicked(this, &SContentUsageAuditWidget::Export)
				.IsEnabled_Lambda([this] { return !Report.Rows.IsEmpty(); })
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(12, 0, 12, 10)
		[
			SAssignNew(Summary, STextBlock)
			.Text(FText::FromString(TEXT("1. Choose a map.   2. Click Scan.   3. Select a result to see its references.")))
			.AutoWrapText(true)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(12, 0, 12, 10)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNew(SSearchBox)
				.HintText(FText::FromString(TEXT("Find asset...")))
				.OnTextChanged_Lambda([this](const FText& Text)
					{
						Search = Text.ToString();
						Refresh();
					})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(12, 0, 0, 0)
			[
				SAssignNew(FilterBox, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&Filters)
				.InitiallySelectedItem(ActiveFilter)
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Label)
					{
						return SNew(STextBlock)
							.Text(FText::FromString(*Label));
					})
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Label, ESelectInfo::Type)
					{
						ActiveFilter = Label;
						Refresh();
					})
				[
					SNew(STextBlock)
					.Text_Lambda([this]
						{
							return FText::FromString(*ActiveFilter);
						})
				]
			]
		]

		+ SVerticalBox::Slot()
		.FillHeight(1)
		.Padding(12, 0, 12, 10)
		[
			SAssignNew(List, SListView<TSharedPtr<FCBTUsageRow>>)
			.ListItemsSource(&VisibleRows)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow_Lambda([](TSharedPtr<FCBTUsageRow> Item, const TSharedRef<STableViewBase>& Owner)
				{
					return SNew(SUsageRowWidget, Owner).Item(Item);
				})
			.OnSelectionChanged(this, &SContentUsageAuditWidget::SelectRow)
			.OnMouseButtonDoubleClick(this, &SContentUsageAuditWidget::Locate)
			.HeaderRow(GetHeaderRowWidget())
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(12, 0, 12, 4)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("References for selected asset")))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(12, 0, 12, 10)
		[
			SNew(SBox)
			.HeightOverride(120)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SAssignNew(Details, STextBlock)
					.Text(FText::FromString(TEXT("Select a result above. Double-click to " "locate it in Content Browser.")))
					.AutoWrapText(true)
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(12, 0, 12, 12)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Read-only. No references found = review candidate, not permission to delete.")))
			.ToolTipText_Lambda([this]
				{
					return FText::FromString(FString::Join(Report.Warnings, TEXT("\n")));
				})
		]
	];
	/* clang-format on */
}

FString SContentUsageAuditWidget::MapPath() const
{
	return SelectedMap.IsValid() ? SelectedMap.GetObjectPathString() : FString();
}

void SContentUsageAuditWidget::SelectMap(const FAssetData& Asset)
{
	SelectedMap = Asset;
	Report = FCBTUsageReport();
	Refresh();
	Summary->SetText(FText::FromString(TEXT("Click Scan to check the selection against this map.")));
}

void SContentUsageAuditWidget::Refresh()
{
	{
		VisibleRows.Reset();
		for (const auto& Item : Report.Rows)
		{
			bool bActiveFilterValid = (!ActiveFilter.IsValid()
									|| ActiveFilter == Filters[0]
									|| ContentBrowserToolkit::UsageLabel(Item->Status) == *ActiveFilter)
									&& (Search.IsEmpty()
									|| Item->Asset.GetObjectPathString().Contains(Search));

			if (bActiveFilterValid)
			{
				VisibleRows.Add(Item);
			}
		}
		if (List.IsValid())
		{
			List->ClearSelection();
			List->RequestListRefresh();
		}
		if (Details.IsValid())
		{
			Details->SetText(FText::FromString(TEXT("Select a result to inspect references.")));
		}
	}
}

void SContentUsageAuditWidget::SelectRow(TSharedPtr<FCBTUsageRow> Item, ESelectInfo::Type)
{
	if (Item.IsValid())
	{
		Details->SetText(FText::FromString(Item->Asset.GetObjectPathString() + TEXT("\n\n") + Item->Evidence));
	}
}

FReply SContentUsageAuditWidget::Export()
{
	const FString Filename = FContentBrowserUsageAudit::Export(Report);
	Details->SetText(FText::FromString(Filename.IsEmpty() ? TEXT("Export failed.") : TEXT("CSV saved: ") + Filename));
	return FReply::Handled();
}

void SContentUsageAuditWidget::Locate(TSharedPtr<FCBTUsageRow> Item)
{
	if (Item.IsValid())
	{
		FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().SyncBrowserToAssets({ Item->Asset });
	}
}

TSharedRef<SHeaderRow> SContentUsageAuditWidget::GetHeaderRowWidget() const
{
	return SNew(SHeaderRow)
		+ SHeaderRow::Column(TEXT("Asset"))
			  .DefaultLabel(FText::FromString(TEXT("Asset")))
			  .FillWidth(0.4f)

		+ SHeaderRow::Column(TEXT("Type"))
			  .DefaultLabel(FText::FromString(TEXT("Type")))
			  .FillWidth(0.2f)

		+ SHeaderRow::Column(TEXT("Status"))
			  .DefaultLabel(FText::FromString(TEXT("Usage")))
			  .FillWidth(0.28f)

		+ SHeaderRow::Column(TEXT("Size"))
			  .DefaultLabel(FText::FromString(TEXT("Size")))
			  .FillWidth(0.12f);
}

FReply SContentUsageAuditWidget::Scan()
{
	Report = FCBTUsageReport();
	TArray<FString> Folders;
	if (Assets.IsEmpty())
	{
		Folders.Add(Folder);
	}
	else
	{
		for (const FAssetData& Asset : Assets)
		{
			Folders.AddUnique(Asset.PackagePath.ToString());
		}
	}
	TSet<FSoftObjectPath> Requested;
	for (const FAssetData& Asset : Assets)
	{
		Requested.Add(Asset.ToSoftObjectPath());
	}
	TSet<FSoftObjectPath> Added;
	for (const FString& Path : Folders)
	{
		const FCBTUsageReport Partial = FContentBrowserUsageAudit::Scan(Path, SelectedMap);
		Report.MapPackage = Partial.MapPackage;
		Report.Folder = Assets.IsEmpty() ? Folder : TEXT("Selected assets");
		for (const FString& Warning : Partial.Warnings)
		{
			Report.Warnings.AddUnique(Warning);
		}

		for (const auto& Item : Partial.Rows)
		{
			const FSoftObjectPath Object = Item->Asset.ToSoftObjectPath();
			if ((Assets.IsEmpty() || Requested.Contains(Object)) && !Added.Contains(Object))
			{
				Report.Rows.Add(Item);
				Added.Add(Object);
			}
		}
		if (!Partial.Error.IsEmpty())
		{
			Report.Error = Partial.Error;
			break;
		}
		if (Partial.bCancelled)
		{
			Report.bCancelled = true;
			break;
		}
	}
	if (!Report.Error.IsEmpty() || Report.bCancelled)
	{
		for (auto& Item : Report.Rows)
		{
			if (Item->Status == ECBTUsageStatus::Review)
			{
				Item->Status = ECBTUsageStatus::Protected;
			}
		}
	}

	int32 Used = 0;
	int32 Elsewhere = 0;
	int32 Review = 0;
	int32 Protected = 0;

	for (const auto& Item : Report.Rows)
	{
		switch (Item->Status)
		{
			case ECBTUsageStatus::MapDependency:
				++Used;
				break;
			case ECBTUsageStatus::Referenced:
				++Elsewhere;
				break;
			case ECBTUsageStatus::Review:
				++Review;
				break;
			default:
				++Protected;
				break;
		}
	}

	Refresh();
	Summary->SetText(FText::FromString(
		!Report.Error.IsEmpty()
			? Report.Error
			: FString::Printf(
				  TEXT("%s   %d checked  |  %d used in map  |  %d used elsewhere  |  %d no references  |  %d need checking"),
				  Report.bCancelled ? TEXT("Cancelled:") : TEXT("Done:"), Report.Rows.Num(), Used, Elsewhere, Review, Protected)));
	return FReply::Handled();
}
