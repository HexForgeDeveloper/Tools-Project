// Copyright (c) 2026 DimAlek. All Rights Reserved.
#include "Tests/DataAssetManagerTestHelpers.h"
#include "UI/DataAssetReferenceInspectorWindow.h"
#include "UI/SDataAssetManagerWidget.h"
#include "Widgets/SWindow.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReferenceInspectorLifetimeTest, "DataAssetManager.UI.ReferenceInspector.IndependentLifetime", DataAssetManagerFlags::Flags)

/** @brief Verifies the inspector owns its data without retaining the manager or itself. */
bool FReferenceInspectorLifetimeTest::RunTest(const FString& Parameters)
{
	using namespace DataAssetManagerTests;
	FScopedTestDataAsset Fixture(MakeUniqueAssetName(TEXT("ReferenceInspector")));
	TSharedPtr<SWindow> Inspector;
	TWeakPtr<SDataAssetManagerWidget> ReleasedManager;
	{
		TSharedRef<SDataAssetManagerWidget> Manager = SNew(SDataAssetManagerWidget);
		ReleasedManager = Manager;
		Inspector = FDataAssetReferenceInspectorWindow::Create(Fixture.MakeSharedAssetData());
	}
	TestFalse(TEXT("Inspector does not retain manager"), ReleasedManager.IsValid());
	TestTrue(TEXT("Inspector remains available after manager destruction"), Inspector.IsValid());
	TWeakPtr<SWindow> ReleasedInspector = Inspector;
	Inspector.Reset();
	TestFalse(TEXT("Inspector can release its contents and data"), ReleasedInspector.IsValid());
	return true;
}
