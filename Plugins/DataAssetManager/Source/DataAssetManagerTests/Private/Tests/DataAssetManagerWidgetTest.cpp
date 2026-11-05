// Copyright (c) 2026 DimAlek. All Rights Reserved.

#include "Tests/DataAssetManagerTestTypes.h"
#include "UI/SDataAssetManagerWidget.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataAssetManagerWidgetLifecycleTest,
	"DataAssetManager.UI.Widget.ConstructReleaseAndReopen",
	DataAssetManagerFlags::Flags)

bool FDataAssetManagerWidgetLifecycleTest::RunTest(const FString& Parameters)
{
	// Releasing the only strong reference must release the real widget and its subscriptions.
	TWeakPtr<SDataAssetManagerWidget> ReleasedWidget;
	for (int32 Iteration = 0; Iteration < 2; ++Iteration)
	{
		{
			TSharedRef<SDataAssetManagerWidget> Widget = SNew(SDataAssetManagerWidget);
			ReleasedWidget = Widget;
		}
		TestFalse(TEXT("Manager can be destroyed before it is reopened"), ReleasedWidget.IsValid());
	}
	return true;
}

#endif
