// Copyright (c) 2026 DimAlek. All Rights Reserved.

#include "UI/DataAssetMoveWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Services/DataAssetManagerAssetService.h"
#include "UI/SFolderTreeWidget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "SDataAssetManagerWidget"

/* clang-format off */
TSharedRef<SWindow> FDataAssetMoveWindow::Create(
	TArray<TSharedPtr<FAssetData>> AssetsToMove,
	TFunction<void(const TArray<FAssetData>&)> OnAssetsMoved)
{
	TSharedRef<SWindow> Window =
		SNew(SWindow)
			.Title(LOCTEXT("MoveSelectedDataAssetsWindowTitle", "Select Destination Folder"))
			.ClientSize(FVector2D(500, 600))
			.SupportsMaximize(false)
			.SupportsMinimize(false);

	const TWeakPtr<SWindow> WeakWindow = Window;
	TSharedPtr<SFolderTreeWidget> FolderTreeWidget;
	Window->SetContent(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
			.FillHeight(1.f)
			.Padding(10.f)
				[SAssignNew(FolderTreeWidget, SFolderTreeWidget)]

		+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(10.f)
			.HAlign(HAlign_Right)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(5.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("MoveSelectedDataAssetsButton", "Move"))
					.OnClicked_Lambda(
						[AssetsToMove = MoveTemp(AssetsToMove), FolderTreeWidget, WeakWindow, OnAssetsMoved = MoveTemp(OnAssetsMoved)]()
						{
							if (const TSharedPtr<SWindow> PinnedWindow = WeakWindow.Pin())
							{
								const FString DestinationPath = FolderTreeWidget.IsValid() ? FolderTreeWidget->GetSelectedDirectory() : FString();
								FSlateApplication::Get().RequestDestroyWindow(PinnedWindow.ToSharedRef());
								if (!DestinationPath.IsEmpty())
								{
									TArray<FAssetData> MovedAssets;
									if (FDataAssetManagerAssetService::MoveAssets(AssetsToMove, DestinationPath, &MovedAssets) > 0 && OnAssetsMoved)
									{
										OnAssetsMoved(MovedAssets);
									}
								}
							}
							return FReply::Handled();
						})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(5.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("MoveSelectedDataAssetsCancelButton", "Cancel"))
					.OnClicked_Lambda([WeakWindow]()
						{
							if (const TSharedPtr<SWindow> PinnedWindow = WeakWindow.Pin())
							{
								FSlateApplication::Get().RequestDestroyWindow(PinnedWindow.ToSharedRef());
							}
							return FReply::Handled();
						})
				]
			]);

	return Window;
}
/* clang-format on */

void FDataAssetMoveWindow::Show(
	TArray<TSharedPtr<FAssetData>> AssetsToMove,
	TFunction<void(const TArray<FAssetData>&)> OnAssetsMoved)
{
	FSlateApplication::Get().AddWindow(Create(MoveTemp(AssetsToMove), MoveTemp(OnAssetsMoved)));
}

#undef LOCTEXT_NAMESPACE
