#include "WireBundleGesture.h"

#if WITH_DEV_AUTOMATION_TESTS
	#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireBundleIntersectionTest, "WireBundle.Gesture.Intersections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWireBundleIntersectionTest::RunTest(const FString&)
{
	const TArray<FVector2D> Wire = { FVector2D(0, 50), FVector2D(100, 50) };
	TestTrue(TEXT("Crossing"), WireBundle::CrossesWire(FVector2D(50, 0), FVector2D(50, 100), Wire));
	TestTrue(TEXT("Reverse gesture"), WireBundle::CrossesWire(FVector2D(50, 100), FVector2D(50, 0), Wire));
	TestFalse(TEXT("Nearby non-crossing wire"), WireBundle::CrossesWire(FVector2D(50, 0), FVector2D(50, 40), Wire));
	TestTrue(TEXT("Touching endpoint"), WireBundle::CrossesWire(FVector2D(50, 0), FVector2D(50, 50), Wire));
	TestTrue(TEXT("Tolerance"), WireBundle::CrossesWire(FVector2D(50, 0), FVector2D(50, 48), Wire));
	TestFalse(TEXT("Click is not a gesture"), WireBundle::CrossesWire(FVector2D(50, 50), FVector2D(50, 50), Wire));
	TestFalse(TEXT("Short drag is not a gesture"), WireBundle::CrossesWire(FVector2D(50, 48), FVector2D(50, 52), Wire));
	TestTrue(TEXT("Collinear overlap"), WireBundle::CrossesWire(FVector2D(20, 50), FVector2D(80, 50), Wire));
	TestFalse(TEXT("Collinear disjoint"), WireBundle::CrossesWire(FVector2D(120, 50), FVector2D(180, 50), Wire));
	const TArray<FVector2D> Curve = { FVector2D(0, 0), FVector2D(50, 100), FVector2D(100, 0) };
	TestTrue(TEXT("Curved wire crossing"), WireBundle::CrossesWire(FVector2D(0, 50), FVector2D(100, 50), Curve));
	WireBundle::FWireSnapshot Snapshot;
	Snapshot.Points = Curve;
	TestEqual(TEXT("Multiple crossings select one edge"), WireBundle::FindCrossedLinks({ Snapshot }, FVector2D(0, 50), FVector2D(100, 50)).Num(), 1);
	return true;
}
#endif
