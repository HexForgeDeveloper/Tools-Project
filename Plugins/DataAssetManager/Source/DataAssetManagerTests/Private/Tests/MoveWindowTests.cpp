// Copyright (c) 2026 DimAlek. All Rights Reserved.

#include "Tests/DataAssetManagerTestTypes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "UI/DataAssetMoveWindow.h"
#include "Widgets/SWidget.h"
#include "Widgets/SWindow.h"

namespace
{
	/** @brief Finds an actual move/cancel button in the window content by its text label. */
	TSharedPtr<SButton> FindWorkflowButton(const TSharedRef<SWidget>& Widget, const FString& Label)
	{
		if (Widget->GetType() == FName(TEXT("SButton")))
		{
			TSharedRef<SButton> Button = StaticCastSharedRef<SButton>(Widget);
			TSharedRef<SWidget> Content = Button->GetContent();
			if (Content->GetType() == FName(TEXT("STextBlock")) && StaticCastSharedRef<STextBlock>(Content)->GetText().ToString() == Label)
			{
				return Button;
			}
		}
		FChildren* Children = Widget->GetChildren();
		for (int32 Index = 0; Index < Children->Num(); ++Index)
		{
			if (TSharedPtr<SButton> Button = FindWorkflowButton(Children->GetChildAt(Index), Label))
			{
				return Button;
			}
		}
		return nullptr;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMoveWindowLifetimeTest, "DataAssetManager.MoveWindow.Lifetime", DataAssetManagerFlags::Flags)

/** @brief Verifies that move callbacks do not retain the window or its contents through an ownership cycle. */
bool FMoveWindowLifetimeTest::RunTest(const FString& Parameters)
{
	for (int32 Iteration = 0; Iteration < 3; ++Iteration)
	{
		int32 Notifications = 0;
		TSharedPtr<SWindow> Window = FDataAssetMoveWindow::Create({}, [&Notifications](const TArray<FAssetData>&) { ++Notifications; });
		TSharedPtr<SButton> MoveButton = FindWorkflowButton(Window->GetContent(), TEXT("Move"));
		TSharedPtr<SButton> CancelButton = FindWorkflowButton(Window->GetContent(), TEXT("Cancel"));
		if (!MoveButton || !CancelButton)
		{
			AddError(TEXT("Workflow buttons unavailable"));
			return false;
		}
		TSharedPtr<SWidget> RetainedContent = Window->GetContent();
		const TWeakPtr<SWindow> WeakWindow = Window;
		const TWeakPtr<SWidget> WeakContent = RetainedContent;

		Window.Reset();
		TestFalse(TEXT("Move callbacks do not keep the window alive"), WeakWindow.IsValid());
		TestTrue(TEXT("Retained move content remains valid"), WeakContent.IsValid());

		MoveButton->SimulateClick();
		CancelButton->SimulateClick();
		TestEqual(TEXT("Expired-window buttons do not notify a move"), Notifications, 0);
		MoveButton.Reset();
		CancelButton.Reset();
		RetainedContent.Reset();
		TestFalse(TEXT("Move window content is released without ownership cycles"), WeakContent.IsValid());
	}

	return true;
}
