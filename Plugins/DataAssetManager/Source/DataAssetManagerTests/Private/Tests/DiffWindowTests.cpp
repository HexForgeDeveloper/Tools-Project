// Copyright (c) 2026 DimAlek. All Rights Reserved.
#include "Tests/DataAssetManagerTestHelpers.h"
#include "UI/DataAssetDiffWindow.h"
#include "UI/SDataAssetManagerWidget.h"
#include "Services/DataAssetManagerAssetService.h"
#include "Widgets/SWindow.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiffWindowLifetimeTest, "DataAssetManager.UI.DiffWindow.IndependentLifetime", DataAssetManagerFlags::Flags)

/** @brief Verifies the comparison window and its refresh callbacks do not retain the manager or form an ownership cycle. */
bool FDiffWindowLifetimeTest::RunTest(const FString& Parameters)
{
	using namespace DataAssetManagerTests;
	FScopedTestDataAsset Left(MakeUniqueAssetName(TEXT("DiffLeft")));
	FScopedTestDataAsset Right(MakeUniqueAssetName(TEXT("DiffRight")));
	if (!Left.GetAsset() || !Right.GetAsset()) { AddError(TEXT("Diff fixtures unavailable")); return false; }
	Left.GetAssetAs<UTestDataAsset>()->TestProperty = 42;
	TSharedPtr<SWindow> Window;
	TWeakPtr<SDataAssetManagerWidget> WeakManager;
	{
		TSharedRef<SDataAssetManagerWidget> Manager = SNew(SDataAssetManagerWidget);
		WeakManager = Manager;
		TSharedRef<FDataAssetDiffResult> Result = MakeShared<FDataAssetDiffResult>(FDataAssetManagerAssetService::DiffAssets(Left.MakeSharedAssetData(), Right.MakeSharedAssetData()));
		if (!TestTrue(TEXT("Fixtures are comparable"), Result->bComparable)) { return false; }
		Window = FDataAssetDiffWindow::Create(Result);
	}
	TestFalse(TEXT("Diff does not retain manager"), WeakManager.IsValid());
	TestTrue(TEXT("Diff remains valid after manager release"), Window.IsValid());
	TWeakPtr<SWindow> WeakWindow = Window;
	Window.Reset();
	TestFalse(TEXT("Diff and refresh state can be released"), WeakWindow.IsValid());
	return true;
}
