// Copyright (c) 2026 DimAlek. All Rights Reserved.

#include "UI/DataAssetReferenceInspectorWindow.h"
#include "Services/DataAssetManagerAssetService.h"
#include "Types/DataAssetManagerConstants.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Widgets/SWindow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SDataAssetManagerWidget"

TSharedRef<SWindow> FDataAssetReferenceInspectorWindow::Create(const TSharedPtr<FAssetData>& AssetData)
{
	TSharedRef<FDataAssetReferenceInspectionResult> InspectionResult =
		MakeShared<FDataAssetReferenceInspectionResult>(FDataAssetManagerAssetService::InspectReferences(AssetData));

	const FText AssetName = InspectionResult->IsValidSourceAsset()
		? FText::FromName(InspectionResult->SourceAsset->AssetName)
		: LOCTEXT("ReferenceInspectorUnknownAsset", "Unknown Asset");

	const FText AssetPath = InspectionResult->IsValidSourceAsset()
		? FText::FromName(InspectionResult->SourceAsset->PackageName)
		: FText::GetEmpty();

	TSharedRef<SWindow> Window =
		SNew(SWindow)
			.Title(FText::Format(LOCTEXT("ReferenceInspectorWindowTitle", "Reference Inspector: {0}"), AssetName))
			.ClientSize(FVector2D(980.0f, 640.0f))
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
									.FillWidth(1.0f)
									.VAlign(VAlign_Center)
										[SNew(SVerticalBox)
											+ SVerticalBox::Slot()
												.AutoHeight()
													[SNew(STextBlock)
															.Text(AssetName)
															.Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))]

											+ SVerticalBox::Slot()
												.AutoHeight()
												.Padding(0.0f, 2.0f, 0.0f, 0.0f)
													[SNew(STextBlock)
															.Text(AssetPath)
															.ColorAndOpacity(FSlateColor::UseSubduedForeground())]]

								+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign(VAlign_Center)
									.Padding(12.0f, 0.0f)
										[SNew(STextBlock)
												.Text_Lambda([InspectionResult]() {
													return InspectionResult->IsPotentiallyUnused()
														? LOCTEXT("ReferenceInspectorPotentiallyUnused", "Potentially unused")
														: LOCTEXT("ReferenceInspectorHasReferencers", "Has referencers");
												})
												.ColorAndOpacity_Lambda([InspectionResult]() {
													return InspectionResult->IsPotentiallyUnused()
														? FSlateColor(FLinearColor(1.0f, 0.55f, 0.15f))
														: FSlateColor(FLinearColor(0.35f, 0.8f, 0.45f));
												})]

								+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign(VAlign_Center)
									.Padding(12.0f, 0.0f, 0.0f, 0.0f)
										[SNew(STextBlock)
												.Text_Lambda([InspectionResult]() {
													return FText::Format(
														LOCTEXT("ReferenceInspectorReferenceCount", "References: {0}"),
														FText::AsNumber(InspectionResult->References.Num()));
												})]

								+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign(VAlign_Center)
									.Padding(12.0f, 0.0f, 0.0f, 0.0f)
										[SNew(STextBlock)
												.Text_Lambda([InspectionResult]() {
													return FText::Format(
														LOCTEXT("ReferenceInspectorReferencedByCount", "Referenced by: {0}"),
														FText::AsNumber(InspectionResult->ReferencedBy.Num()));
												})]]]

		+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0.0f, 6.0f)
				[SNew(SSplitter)
						.Orientation(EOrientation::Orient_Horizontal)
					+ SSplitter::Slot()
						.Value(0.5f)
							[BuildReferenceListSection(
								LOCTEXT("ReferenceInspectorReferencesTitle", "References"),
								LOCTEXT("ReferenceInspectorNoReferences", "No referenced assets found"),
								InspectionResult,
								false)]

					+ SSplitter::Slot()
						.Value(0.5f)
							[BuildReferenceListSection(
								LOCTEXT("ReferenceInspectorReferencedByTitle", "Referenced By"),
								LOCTEXT("ReferenceInspectorNoReferencers", "No assets reference this Data Asset"),
								InspectionResult,
								true)]]

		+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f, 0.0f, 8.0f, 8.0f)
				[SNew(STextBlock)
						.Text_Lambda([InspectionResult]() {
							return FText::Format(
								LOCTEXT("ReferenceInspectorUnresolvedPackages", "Unresolved package references: {0}"),
								FText::AsNumber(InspectionResult->UnresolvedPackages.Num()));
						})
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.Visibility_Lambda([InspectionResult]() {
							return InspectionResult->UnresolvedPackages.Num() > 0
								? EVisibility::Visible
								: EVisibility::Collapsed;
						})]);

	return Window;
}


TSharedRef<SWidget> FDataAssetReferenceInspectorWindow::BuildReferenceListSection(FText Title, FText EmptyText, TSharedRef<FDataAssetReferenceInspectionResult> InspectionResult, bool bReferencedBy)
{
	const TArray<TSharedPtr<FDataAssetReferenceEntry>>* EntriesSource = bReferencedBy
		? &InspectionResult->ReferencedBy
		: &InspectionResult->References;

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(6.0f))
			[SNew(SVerticalBox)
				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 6.0f)
						[SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.VAlign(VAlign_Center)
									[SNew(STextBlock)
											.Text(Title)
											.Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))]

							+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
									[SNew(STextBlock)
											.Text_Lambda([InspectionResult, bReferencedBy]() {
												const int32 EntryCount = bReferencedBy
													? InspectionResult->ReferencedBy.Num()
													: InspectionResult->References.Num();
												return FText::AsNumber(EntryCount);
											})
											.ColorAndOpacity(FSlateColor::UseSubduedForeground())]]

				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(4.0f, 0.0f, 52.0f, 4.0f)
						[SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
								.FillWidth(0.32f)
									[SNew(STextBlock)
											.Text(LOCTEXT("ReferenceInspectorAssetColumn", "Asset"))
											.ColorAndOpacity(FSlateColor::UseSubduedForeground())]

							+ SHorizontalBox::Slot()
								.FillWidth(0.20f)
									[SNew(STextBlock)
											.Text(LOCTEXT("ReferenceInspectorClassColumn", "Class"))
											.ColorAndOpacity(FSlateColor::UseSubduedForeground())]

							+ SHorizontalBox::Slot()
								.FillWidth(0.48f)
									[SNew(STextBlock)
											.Text(LOCTEXT("ReferenceInspectorPathColumn", "Path"))
											.ColorAndOpacity(FSlateColor::UseSubduedForeground())]]

				+ SVerticalBox::Slot()
					.FillHeight(1.0f)
						[SNew(SOverlay)
							+ SOverlay::Slot()
								[SNew(SListView<TSharedPtr<FDataAssetReferenceEntry>>)
										.ListItemsSource(EntriesSource)
										.SelectionMode(ESelectionMode::Single)
										.OnGenerateRow_Lambda([InspectionResult](TSharedPtr<FDataAssetReferenceEntry> Entry, const TSharedRef<STableViewBase>& OwnerTable) {
											return GenerateReferenceEntryRow(Entry, OwnerTable);
										})
										.OnMouseButtonDoubleClick_Lambda([InspectionResult](TSharedPtr<FDataAssetReferenceEntry> Entry) {
											OpenReferenceEntryInEditor(Entry);
										})]

							+ SOverlay::Slot()
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
									[SNew(STextBlock)
											.Text(EmptyText)
											.ColorAndOpacity(FSlateColor::UseSubduedForeground())
											.Visibility_Lambda([InspectionResult, bReferencedBy]() {
												const int32 EntryCount = bReferencedBy
													? InspectionResult->ReferencedBy.Num()
													: InspectionResult->References.Num();
												return EntryCount == 0
													? EVisibility::HitTestInvisible
													: EVisibility::Collapsed;
											})]]];
}


TSharedRef<ITableRow> FDataAssetReferenceInspectorWindow::GenerateReferenceEntryRow(TSharedPtr<FDataAssetReferenceEntry> Entry, const TSharedRef<STableViewBase>& OwnerTable)
{
	const FText DisplayName = Entry.IsValid() ? Entry->GetDisplayName() : FText::GetEmpty();
	const FText ClassName = Entry.IsValid() ? Entry->GetClassDisplayName() : FText::GetEmpty();
	const FText PathText = Entry.IsValid() ? Entry->GetPathText() : FText::GetEmpty();
	const FText TooltipText = Entry.IsValid() ? FText::FromString(Entry->GetSortKey()) : FText::GetEmpty();

	return SNew(STableRow<TSharedPtr<FDataAssetReferenceEntry>>, OwnerTable)
		.Padding(FMargin(2.0f))
			[SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
					.FillWidth(0.32f)
					.VAlign(VAlign_Center)
					.Padding(2.0f, 0.0f)
						[SNew(STextBlock)
								.Text(DisplayName)
								.ToolTipText(TooltipText)]

				+ SHorizontalBox::Slot()
					.FillWidth(0.20f)
					.VAlign(VAlign_Center)
					.Padding(2.0f, 0.0f)
						[SNew(STextBlock)
								.Text(ClassName)
								.ColorAndOpacity(FSlateColor::UseSubduedForeground())
								.ToolTipText(TooltipText)]

				+ SHorizontalBox::Slot()
					.FillWidth(0.48f)
					.VAlign(VAlign_Center)
					.Padding(2.0f, 0.0f)
						[SNew(STextBlock)
								.Text(PathText)
								.ColorAndOpacity(FSlateColor::UseSubduedForeground())
								.ToolTipText(TooltipText)]

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
						[SNew(SButton)
								.ButtonStyle(FAppStyle::Get(), "SimpleButton")
								.ContentPadding(FMargin(2.0f))
								.ToolTipText(LOCTEXT("ReferenceInspectorSyncTooltip", "Find in Content Browser"))
								.IsEnabled_Lambda([Entry]() {
									return Entry.IsValid() && Entry->bAssetDataResolved;
								})
								.OnClicked_Lambda([Entry]() {
									SyncReferenceEntryInContentBrowser(Entry);
									return FReply::Handled();
								})
									[SNew(SImage)
											.Image(FAppStyle::GetBrush("Icons.Search"))]]

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(2.0f, 0.0f, 0.0f, 0.0f)
						[SNew(SButton)
								.ButtonStyle(FAppStyle::Get(), "SimpleButton")
								.ContentPadding(FMargin(2.0f))
								.ToolTipText(LOCTEXT("ReferenceInspectorOpenTooltip", "Open asset"))
								.IsEnabled_Lambda([Entry]() {
									return Entry.IsValid() && Entry->bAssetDataResolved;
								})
								.OnClicked_Lambda([Entry]() {
									OpenReferenceEntryInEditor(Entry);
									return FReply::Handled();
								})
									[SNew(SImage)
											.Image(FAppStyle::GetBrush("Icons.Edit"))]]];
}


void FDataAssetReferenceInspectorWindow::SyncReferenceEntryInContentBrowser(TSharedPtr<FDataAssetReferenceEntry> Entry)
{
	if (!Entry.IsValid() || !Entry->bAssetDataResolved)
	{
		return;
	}

	const FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(DataAssetManager::ModuleName::ContentBrowser);
	ContentBrowserModule.Get().SyncBrowserToAssets({ Entry->AssetData });
}


void FDataAssetReferenceInspectorWindow::OpenReferenceEntryInEditor(TSharedPtr<FDataAssetReferenceEntry> Entry)
{
	if (!Entry.IsValid() || !Entry->bAssetDataResolved)
	{
		return;
	}

	UObject* Asset = Entry->AssetData.GetAsset();
	if (IsValid(Asset))
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Asset);
	}
}


#undef LOCTEXT_NAMESPACE
