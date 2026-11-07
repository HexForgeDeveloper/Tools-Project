// Copyright (c) 2026 DimAlek. All Rights Reserved.

#include "Tests/DataAssetManagerTestTypes.h"
#include "UI/DataAssetMoveWindow.h"
#include "Widgets/SWidget.h"
#include "Widgets/SWindow.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMoveWindowLifetimeTest, "DataAssetManager.MoveWindow.Lifetime", DataAssetManagerFlags::Flags)

/** @brief Verifies that move callbacks do not retain the window or its contents through an ownership cycle. */
bool FMoveWindowLifetimeTest::RunTest(const FString& Parameters)
{
	for (int32 Iteration = 0; Iteration < 3; ++Iteration)
	{
		TSharedPtr<SWindow> Window = FDataAssetMoveWindow::Create({});
		TSharedPtr<SWidget> RetainedContent = Window->GetContent();
		const TWeakPtr<SWindow> WeakWindow = Window;
		const TWeakPtr<SWidget> WeakContent = RetainedContent;

		Window.Reset();
		TestFalse(TEXT("Move callbacks do not keep the window alive"), WeakWindow.IsValid());
		TestTrue(TEXT("Retained move content remains valid"), WeakContent.IsValid());

		RetainedContent.Reset();
		TestFalse(TEXT("Move window content is released without ownership cycles"), WeakContent.IsValid());
	}

	return true;
}
