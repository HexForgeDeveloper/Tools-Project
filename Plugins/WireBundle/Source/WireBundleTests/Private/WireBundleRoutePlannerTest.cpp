#include "WireBundleRoutePlanner.h"

#if WITH_DEV_AUTOMATION_TESTS
	#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireBundleRoutePlanTest, "WireBundle.Routing.ParallelPlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWireBundleRoutePlanTest::RunTest(const FString&)
{
	FWireBundleResolvedSettings Settings;
	Settings.Movement.BundleWidth = 240;
	Settings.Movement.WireSpacing = 20;
	Settings.Movement.CenterOffset = FVector2D(5, 7);
	Settings.Movement.bSnapToGrid = true;
	Settings.Movement.GridSize = 16;
	WireBundle::FLink Upper, Lower;
	Upper.SourceY = 0;
	Lower.SourceY = 100;
	const TArray<WireBundle::FLink> Input = { Lower, Upper };
	const auto						Plan = WireBundle::BuildParallelRoutePlan(Input, FVector2D(300, 60), Settings);
	TestTrue(TEXT("Offset then grid snapping"), Plan.Center.Equals(FVector2D(304, 64)));
	TestEqual(TEXT("One lane per logical wire"), Plan.Lanes.Num(), 2);
	TestEqual(TEXT("Lane order follows source height"), Plan.Lanes[0].Link.SourceY, 0);
	TestEqual(TEXT("Input order is never modified"), Input[0].SourceY, 100);
	TestTrue(TEXT("Exact preview entry"), Plan.Lanes[0].Entry.Equals(FVector2D(184, 54)));
	TestTrue(TEXT("Exact preview exit"), Plan.Lanes[0].Exit.Equals(FVector2D(424, 54)));
	TestTrue(TEXT("Lane spacing"), Plan.Lanes[1].Entry.Equals(FVector2D(184, 74)));
	TestEqual(TEXT("Empty selection produces no lanes"),
		WireBundle::BuildParallelRoutePlan({}, FVector2D::ZeroVector, Settings).Lanes.Num(), 0);
	Settings.Movement.RerouteColumns = 1;
	const auto Single = WireBundle::BuildParallelRoutePlan(Input, FVector2D(300, 60), Settings);
	TestEqual(TEXT("Single-column plan"), Single.Columns, 1);
	TestTrue(TEXT("One insertion point per lane"), Single.Lanes[0].Entry == Single.Lanes[0].Exit);
	TestEqual(TEXT("Single column uses same planned center"), Single.Lanes[0].Entry.X, Single.Center.X);
	Settings.Movement.RerouteColumns = 5;
	const auto Multi = WireBundle::BuildParallelRoutePlan(Input, FVector2D(300, 60), Settings);
	TestEqual(TEXT("Five insertion points on each wire"), Multi.Lanes[0].Reroutes.Num(), 5);
	TestTrue(TEXT("Intermediate columns evenly distributed"), Multi.Lanes[0].Reroutes[2].Equals(FVector2D(304, 54)));
	return true;
}
#endif
