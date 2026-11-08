// Copyright (c) 2026 DimAlek. All Rights Reserved.
#include "Tests/DataAssetManagerTestTypes.h"
#include "UI/DataAssetCreationWindow.h"
#include "Widgets/SWindow.h"
#include "UI/SDataAssetManagerWidget.h"
#include "Widgets/SWidget.h"

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FCreationWindowLifetimeTest, "DataAssetManager.CreationWindow.Lifetime", DataAssetManagerFlags::Flags)

/** @brief Enumerates destruction and retained-content regression scenarios. */
void FCreationWindowLifetimeTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	Names.Add(TEXT("ReleaseAndReopen"));
	Commands.Add(TEXT("ReleaseAndReopen"));
	Names.Add(TEXT("RetainedContentDoesNotRetainWindow"));
	Commands.Add(TEXT("RetainedContent"));
	Names.Add(TEXT("IndependentManagerLifetime"));
	Commands.Add(TEXT("ManagerLifetime"));
}

/** @brief Verifies real Slate callbacks do not form a window ownership cycle, including when its content outlives it. */
bool FCreationWindowLifetimeTest::RunTest(const FString& Parameters)
{
	for (int32 Iteration = 0; Iteration < 3; ++Iteration)
	{
		TSharedPtr<SWindow> Window;
		if (Parameters == TEXT("ManagerLifetime"))
		{
			TWeakPtr<SDataAssetManagerWidget> WeakManager;
			{
				TSharedRef<SDataAssetManagerWidget> Manager = SNew(SDataAssetManagerWidget);
				WeakManager = Manager;
				Window = FDataAssetCreationWindow::Create();
			}
			TestFalse(TEXT("Creation window does not retain manager"), WeakManager.IsValid());
		}
		else
		{
			Window = FDataAssetCreationWindow::Create();
		}
		TSharedPtr<SWidget> RetainedContent;
		if (Parameters == TEXT("RetainedContent"))
		{
			RetainedContent = Window->GetContent();
		}
		const TWeakPtr<SWindow> WeakWindow = Window;
		const TWeakPtr<SWidget> WeakContent = Window->GetContent();
		Window.Reset();
		TestFalse(TEXT("Button callbacks do not keep the window alive"), WeakWindow.IsValid());
		if (RetainedContent)
		{
			TestTrue(TEXT("Retained content is still valid"), WeakContent.IsValid());
		}
		RetainedContent.Reset();
		TestFalse(TEXT("Window content is released without ownership cycles"), WeakContent.IsValid());
	}
	return true;
}
