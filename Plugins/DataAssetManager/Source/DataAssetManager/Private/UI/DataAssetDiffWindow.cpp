// Copyright (c) 2026 DimAlek. All Rights Reserved.
#include "UI/DataAssetDiffWindow.h"
#include "Services/DataAssetManagerAssetService.h"
#include "Styling/AppStyle.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SWindow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SDataAssetManagerWidget"

TSharedRef<SWindow> FDataAssetDiffWindow::Create(TSharedRef<FDataAssetDiffResult> DiffResult, FSimpleDelegate OnChanged)
{
	const FText LeftName = DiffResult->IsValidLeftAsset()
		? FText::FromName(DiffResult->LeftAsset->AssetName)
		: LOCTEXT("DataAssetDiffUnknownLeft", "Unknown");
	const FText RightName = DiffResult->IsValidRightAsset()
		? FText::FromName(DiffResult->RightAsset->AssetName)
		: LOCTEXT("DataAssetDiffUnknownRight", "Unknown");
	const FText LeftPath = DiffResult->IsValidLeftAsset()
		? FText::FromName(DiffResult->LeftAsset->PackageName)
		: FText::GetEmpty();
	const FText RightPath = DiffResult->IsValidRightAsset()
		? FText::FromName(DiffResult->RightAsset->PackageName)
		: FText::GetEmpty();

	TSharedPtr<SListView<TSharedPtr<FDataAssetDiffEntry>>> DiffListView;
	TSharedRef<TWeakPtr<SListView<TSharedPtr<FDataAssetDiffEntry>>>> DiffListViewWeak =
		MakeShared<TWeakPtr<SListView<TSharedPtr<FDataAssetDiffEntry>>>>();
	TSharedRef<TFunction<void()>> RefreshDiff = MakeShared<TFunction<void()>>();
	*RefreshDiff = [DiffResult, DiffListViewWeak, OnChanged]() {
		FDataAssetDiffResult UpdatedDiff = FDataAssetManagerAssetService::DiffAssets(DiffResult->LeftAsset, DiffResult->RightAsset);
		DiffResult->LeftAsset = UpdatedDiff.LeftAsset;
		DiffResult->RightAsset = UpdatedDiff.RightAsset;
		DiffResult->Entries = MoveTemp(UpdatedDiff.Entries);
		DiffResult->bComparable = UpdatedDiff.bComparable;
		DiffResult->ErrorText = UpdatedDiff.ErrorText;

		OnChanged.ExecuteIfBound();
		if (const TSharedPtr<SListView<TSharedPtr<FDataAssetDiffEntry>>> PinnedListView = DiffListViewWeak->Pin())
		{
			PinnedListView->RequestListRefresh();
		}
	};

	TSharedRef<SListView<TSharedPtr<FDataAssetDiffEntry>>> DiffListWidget =
		SAssignNew(DiffListView, SListView<TSharedPtr<FDataAssetDiffEntry>>)
			.ListItemsSource(&DiffResult->Entries)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow_Lambda([DiffResult, RefreshDiff](TSharedPtr<FDataAssetDiffEntry> Entry, const TSharedRef<STableViewBase>& OwnerTable) {
				return GenerateDataAssetDiffRow(Entry, OwnerTable, DiffResult, RefreshDiff);
			});
	*DiffListViewWeak = DiffListView;

	TSharedRef<SWindow> Window =
		SNew(SWindow)
			.Title(FText::Format(LOCTEXT("DataAssetDiffWindowTitle", "DataAsset Diff: {0} vs {1}"), LeftName, RightName))
			.ClientSize(FVector2D(1080.0f, 680.0f))
			.SupportsMaximize(true)
			.SupportsMinimize(false);

	Window->SetContent(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
			.AutoHeight()
				[SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
						.Padding(FMargin(8.0f, 6.0f))
							[SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
									.FillWidth(0.5f)
									.VAlign(VAlign_Center)
										[SNew(SVerticalBox)
											+ SVerticalBox::Slot()
												.AutoHeight()
													[SNew(STextBlock)
															.Text(LeftName)
															.Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))]

											+ SVerticalBox::Slot()
												.AutoHeight()
												.Padding(0.0f, 2.0f, 8.0f, 0.0f)
													[SNew(STextBlock)
															.Text(LeftPath)
															.ColorAndOpacity(FSlateColor::UseSubduedForeground())]]

								+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign(VAlign_Center)
									.Padding(10.0f, 0.0f)
										[SNew(STextBlock)
												.Text_Lambda([DiffResult]() {
													return FText::Format(
														LOCTEXT("DataAssetDiffCount", "{0} difference(s)"),
														FText::AsNumber(DiffResult->Entries.Num()));
												})]

								+ SHorizontalBox::Slot()
									.FillWidth(0.5f)
									.VAlign(VAlign_Center)
										[SNew(SVerticalBox)
											+ SVerticalBox::Slot()
												.AutoHeight()
													[SNew(STextBlock)
															.Text(RightName)
															.Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))]

											+ SVerticalBox::Slot()
												.AutoHeight()
												.Padding(8.0f, 2.0f, 0.0f, 0.0f)
													[SNew(STextBlock)
															.Text(RightPath)
															.ColorAndOpacity(FSlateColor::UseSubduedForeground())]]]]

		+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f, 8.0f, 8.0f, 4.0f)
				[SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
						.FillWidth(0.22f)
							[SNew(STextBlock)
									.Text(LOCTEXT("DataAssetDiffPropertyColumn", "Property"))
									.ColorAndOpacity(FSlateColor::UseSubduedForeground())]

					+ SHorizontalBox::Slot()
						.FillWidth(0.34f)
						.Padding(8.0f, 0.0f)
							[SNew(STextBlock)
									.Text(LOCTEXT("DataAssetDiffLeftColumn", "Left"))
									.ColorAndOpacity(FSlateColor::UseSubduedForeground())]

					+ SHorizontalBox::Slot()
						.FillWidth(0.34f)
						.Padding(8.0f, 0.0f, 0.0f, 0.0f)
							[SNew(STextBlock)
									.Text(LOCTEXT("DataAssetDiffRightColumn", "Right"))
									.ColorAndOpacity(FSlateColor::UseSubduedForeground())]

					+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(8.0f, 0.0f, 0.0f, 0.0f)
							[SNew(STextBlock)
									.Text(LOCTEXT("DataAssetDiffCopyColumn", "Copy"))
									.ColorAndOpacity(FSlateColor::UseSubduedForeground())]]

		+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(8.0f, 0.0f, 8.0f, 8.0f)
				[SNew(SOverlay)
					+ SOverlay::Slot()
						[DiffListWidget]

					+ SOverlay::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
							[SNew(STextBlock)
									.Text(LOCTEXT("DataAssetDiffNoDifferences", "No editable property differences found"))
									.ColorAndOpacity(FSlateColor::UseSubduedForeground())
									.Visibility_Lambda([DiffResult]() {
										return DiffResult->HasDifferences()
											? EVisibility::Collapsed
											: EVisibility::HitTestInvisible;
									})]]);

	return Window;
}


TSharedRef<ITableRow> FDataAssetDiffWindow::GenerateDataAssetDiffRow(TSharedPtr<FDataAssetDiffEntry> Entry, const TSharedRef<STableViewBase>& OwnerTable, TSharedRef<FDataAssetDiffResult> DiffResult, TSharedRef<TFunction<void()>> RefreshDiff)
{
	const FText PropertyName = Entry.IsValid() ? Entry->DisplayName : FText::GetEmpty();
	const FText PropertyDetails = Entry.IsValid()
		? FText::FromString(Entry->Category.IsEmpty()
				  ? Entry->PropertyName.ToString()
				  : FString::Printf(TEXT("%s / %s"), *Entry->Category, *Entry->PropertyName.ToString()))
		: FText::GetEmpty();
	const FText LeftValue = Entry.IsValid() && !Entry->LeftValue.IsEmpty()
		? FText::FromString(Entry->LeftValue)
		: LOCTEXT("DataAssetDiffEmptyLeftValue", "<Empty>");
	const FText RightValue = Entry.IsValid() && !Entry->RightValue.IsEmpty()
		? FText::FromString(Entry->RightValue)
		: LOCTEXT("DataAssetDiffEmptyRightValue", "<Empty>");

	return SNew(STableRow<TSharedPtr<FDataAssetDiffEntry>>, OwnerTable)
		.Padding(FMargin(2.0f, 4.0f))
			[SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
					.FillWidth(0.22f)
					.VAlign(VAlign_Top)
					.Padding(2.0f, 0.0f, 8.0f, 0.0f)
						[SNew(SVerticalBox)
							+ SVerticalBox::Slot()
								.AutoHeight()
									[SNew(STextBlock)
											.Text(PropertyName)]

							+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0.0f, 2.0f, 0.0f, 0.0f)
									[SNew(STextBlock)
											.Text(PropertyDetails)
											.ColorAndOpacity(FSlateColor::UseSubduedForeground())]]

				+ SHorizontalBox::Slot()
					.FillWidth(0.34f)
					.VAlign(VAlign_Top)
					.Padding(8.0f, 0.0f)
						[SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
								.Padding(FMargin(6.0f, 4.0f))
									[SNew(STextBlock)
											.Text(LeftValue)
											.AutoWrapText(true)
											.ToolTipText(LeftValue)]]

				+ SHorizontalBox::Slot()
					.FillWidth(0.34f)
					.VAlign(VAlign_Top)
					.Padding(8.0f, 0.0f, 0.0f, 0.0f)
						[SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
								.Padding(FMargin(6.0f, 4.0f))
									[SNew(STextBlock)
											.Text(RightValue)
											.AutoWrapText(true)
											.ToolTipText(RightValue)]]

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Top)
					.Padding(8.0f, 0.0f, 0.0f, 0.0f)
						[SNew(SVerticalBox)
							+ SVerticalBox::Slot()
								.AutoHeight()
									[SNew(SButton)
											.ButtonStyle(FAppStyle::Get(), "SimpleButton")
											.ContentPadding(FMargin(6.0f, 2.0f))
											.ToolTipText(LOCTEXT("DataAssetDiffCopyLeftToRightTooltip", "Copy the left value to the right Data Asset"))
											.OnClicked_Lambda([Entry, DiffResult, RefreshDiff]() {
												CopyDataAssetDiffValue(Entry, DiffResult, true, RefreshDiff);
												return FReply::Handled();
											})
												[SNew(STextBlock)
														.Text(LOCTEXT("DataAssetDiffCopyLeftToRight", "L -> R"))]]

							+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0.0f, 2.0f, 0.0f, 0.0f)
									[SNew(SButton)
											.ButtonStyle(FAppStyle::Get(), "SimpleButton")
											.ContentPadding(FMargin(6.0f, 2.0f))
											.ToolTipText(LOCTEXT("DataAssetDiffCopyRightToLeftTooltip", "Copy the right value to the left Data Asset"))
											.OnClicked_Lambda([Entry, DiffResult, RefreshDiff]() {
												CopyDataAssetDiffValue(Entry, DiffResult, false, RefreshDiff);
												return FReply::Handled();
											})
												[SNew(STextBlock)
														.Text(LOCTEXT("DataAssetDiffCopyRightToLeft", "L <- R"))]]]];
}


void FDataAssetDiffWindow::CopyDataAssetDiffValue(TSharedPtr<FDataAssetDiffEntry> Entry, TSharedRef<FDataAssetDiffResult> DiffResult, bool bLeftToRight, TSharedRef<TFunction<void()>> RefreshDiff)
{
	if (!Entry.IsValid())
	{
		return;
	}

	FText ErrorText;
	const TSharedPtr<FAssetData>& SourceAsset = bLeftToRight ? DiffResult->LeftAsset : DiffResult->RightAsset;
	const TSharedPtr<FAssetData>& TargetAsset = bLeftToRight ? DiffResult->RightAsset : DiffResult->LeftAsset;
	if (!FDataAssetManagerAssetService::CopyDiffPropertyValue(SourceAsset, TargetAsset, Entry->PropertyName, &ErrorText))
	{
		FNotificationInfo Info(ErrorText.IsEmpty()
				? LOCTEXT("DataAssetDiffCopyFailed", "Failed to copy the property value.")
				: ErrorText);
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}

	(*RefreshDiff)();


}

#undef LOCTEXT_NAMESPACE
