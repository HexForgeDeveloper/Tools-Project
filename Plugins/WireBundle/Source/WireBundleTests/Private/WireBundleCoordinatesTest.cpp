#if WITH_DEV_AUTOMATION_TESTS
	#include "WireBundleCoordinates.h"
	#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireBundleWindowCoordinatesTest, "WireBundle.Gesture.WindowCoordinates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWireBundleWindowCoordinatesTest::RunTest(const FString&)
{
	const TArray<FVector2D> Origins = { FVector2D::ZeroVector, FVector2D(384, 120), FVector2D(-1920, 80) };
	for (const FVector2D& Origin : Origins)
	{
		for (float Scale : { 1.0f, 1.25f, 1.5f, 2.0f })
		{
			const FGeometry PaintGeometry = FGeometry::MakeRoot(FVector2D(900, 600),
				FSlateLayoutTransform(Scale, FVector2D(37, 63)));
			const FVector2D LocalPress(120, 180);
			const FVector2D LocalCursor(340, 90);
			// Simulate desktop input into a scaled, translated child of a
			// floating window, including a window on a negative-coordinate monitor.
			const FVector2D DesktopPress = Origin + FVector2D(37, 63) + LocalPress * Scale;
			const FVector2D DesktopCursor = Origin + FVector2D(37, 63) + LocalCursor * Scale;
			TestTrue(TEXT("Press stays under its desktop position"),
				WireBundle::DesktopToPaintLocal(DesktopPress, Origin, PaintGeometry).Equals(LocalPress, 0.001));
			TestTrue(TEXT("Cursor end stays under the mouse"),
				WireBundle::DesktopToPaintLocal(DesktopCursor, Origin, PaintGeometry).Equals(LocalCursor, 0.001));
			const FSlateRect DesktopClip(Origin.X + 37, Origin.Y + 63, Origin.X + 937, Origin.Y + 663);
			const FSlateRect Clip = WireBundle::DesktopToPaintClip(DesktopClip, Origin);
			TestTrue(TEXT("Clipping uses the same window space"),
				FMath::IsNearlyEqual(Clip.Left, 37.0f) && FMath::IsNearlyEqual(Clip.Top, 63.0f)
					&& FMath::IsNearlyEqual(Clip.Right, 937.0f) && FMath::IsNearlyEqual(Clip.Bottom, 663.0f));
		}
	}
	return true;
}
#endif
