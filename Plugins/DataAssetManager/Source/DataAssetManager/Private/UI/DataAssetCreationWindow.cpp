// Copyright (c) 2026 DimAlek. All Rights Reserved.

#include "UI/DataAssetCreationWindow.h"
#include "ClassViewerFilter.h"
#include "Engine/DataAsset.h"
#include "Framework/Application/SlateApplication.h"
#include "FunctionLibrary/DataAssetManagerFunctionLibrary.h"
#include "Kismet2/SClassPickerDialog.h"
#include "Logging/DataAssetManagerLog.h"
#include "UI/SFolderTreeWidget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "DataAssetCreationWindow"

namespace
{
	/** @brief Restricts the class picker to concrete Data Asset classes. */
	class FDataAssetClassParentFilter final : public IClassViewerFilter
	{
	public:
		EClassFlags DisallowedClassFlags = CLASS_None;
		TSet<const UClass*> AllowedChildrenOfClasses;

		virtual bool IsClassAllowed(
			const FClassViewerInitializationOptions&,
			const UClass* InClass,
			TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
		{
			return InClass
				&& !InClass->HasAnyClassFlags(DisallowedClassFlags)
				&& InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InClass) != EFilterReturn::Failed;
		}

		virtual bool IsUnloadedClassAllowed(
			const FClassViewerInitializationOptions&,
			const TSharedRef<const IUnloadedBlueprintData>,
			TSharedRef<FClassViewerFilterFuncs>) override
		{
			return false;
		}
	};

	/** @brief Prompts for a Data Asset class and returns null when the picker is cancelled. */
	UClass* PickDataAssetClass()
	{
		FClassViewerInitializationOptions Options;
		Options.Mode = EClassViewerMode::ClassPicker;
		Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::DisplayName;

		TSharedRef<FDataAssetClassParentFilter> Filter = MakeShared<FDataAssetClassParentFilter>();
		Options.ClassFilters.Add(Filter);
		Filter->DisallowedClassFlags = CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_HideDropDown;
		Filter->AllowedChildrenOfClasses.Add(UDataAsset::StaticClass());

		const FText TitleText = LOCTEXT("CreateDataAssetOptions", "Pick Class For Data Asset Instance");
		UClass* ChosenClass = nullptr;
		if (!SClassPickerDialog::PickClass(TitleText, Options, ChosenClass, UDataAsset::StaticClass()))
		{
			return nullptr;
		}

		return ChosenClass;
	}

	/** @brief Creates a Data Asset of the selected class in the supplied directory. */
	void CreateDataAsset(const FString& Directory)
	{
		UClass* DataAssetClass = PickDataAssetClass();
		if (!DataAssetClass)
		{
			return;
		}

		if (CVarDebugDataAssetManager.GetValueOnAnyThread())
		{
			UE_LOG(SDataAssetManagerWidgetLog, Log, TEXT("Selected Data Asset Class: %s"), *DataAssetClass->GetName());
		}

		DataAssetManager::CreateNewDataAsset(DataAssetClass, Directory);
	}
} // namespace

/* clang-format off */
void FDataAssetCreationWindow::Show()
{
	TSharedRef<SWindow> Window =
		SNew(SWindow)
			.Title(FText::FromString(TEXT("Select Folder")))
			.ClientSize(FVector2D(500, 600))
			.SupportsMaximize(false)
			.SupportsMinimize(false);

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
					.Text(FText::FromString(TEXT("Create")))
					.OnClicked_Lambda([FolderTreeWidget, Window]()
						{
							FSlateApplication::Get().RequestDestroyWindow(Window);
							CreateDataAsset(FolderTreeWidget->GetSelectedDirectory());
							return FReply::Handled();
						})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(5.f, 0.f)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Cancel")))
					.OnClicked_Lambda([Window]()
						{
							FSlateApplication::Get().RequestDestroyWindow(Window);
							return FReply::Handled();
						})
				]
			]);

	FSlateApplication::Get().AddWindow(Window);
}
/* clang-format on */

#undef LOCTEXT_NAMESPACE
