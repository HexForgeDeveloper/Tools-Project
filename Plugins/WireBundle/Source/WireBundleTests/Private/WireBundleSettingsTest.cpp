#include "WireBundleSettings.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireBundleSettingsTest, "WireBundle.Settings.ResolveAndClamp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWireBundleSettingsTest::RunTest(const FString&)
{
    UWireBundleEditorSettings* Settings = GetMutableDefault<UWireBundleEditorSettings>();
    TGuardValue<FWireBundleMovementSettings> MovementGuard(Settings->Movement, FWireBundleMovementSettings());
    TGuardValue<FWireBundleGestureSettings> GestureGuard(Settings->Gesture, FWireBundleGestureSettings());
    TGuardValue<FWireBundleAppearanceSettings> AppearanceGuard(Settings->Appearance, FWireBundleAppearanceSettings());
    TGuardValue<bool> EditorGuard(Settings->bEnableMaterial, false);
    TestEqual(TEXT("Default width"), ResolveWireBundleSettings().Movement.BundleWidth, 160.0f);
    Settings->Movement.BundleWidth = -25;
    Settings->Movement.NudgeStep = 33;
    Settings->Gesture.MinimumWireCount = 0;
    Settings->Gesture.MinimumDragDistance = 24;
    Settings->Gesture.GestureButton = EKeys::A;
    Settings->Appearance.HintFontSize = 1000;
    Settings->Appearance.RoutePreviewThickness = -10;
    const FWireBundleResolvedSettings Config = ResolveWireBundleSettings();
    TestEqual(TEXT("Invalid width safely clamped"), Config.Movement.BundleWidth, 16.0f);
    TestEqual(TEXT("Custom step resolved"), Config.Movement.NudgeStep, 33.0f);
    TestEqual(TEXT("At least two wires required"), Config.Gesture.MinimumWireCount, 2);
    TestEqual(TEXT("Custom drag distance resolved"), Config.Gesture.MinimumDragDistance, 24.0f);
    TestTrue(TEXT("Keyboard key cannot be used as mouse button"), Config.Gesture.GestureButton == EKeys::LeftMouseButton);
    TestEqual(TEXT("Font size clamped"), Config.Appearance.HintFontSize, 32);
    TestEqual(TEXT("Route preview thickness clamped"), Config.Appearance.RoutePreviewThickness, 0.5f);
    TestTrue(TEXT("Route preview enabled by default"), Config.Appearance.bShowRoutePreview);
    TestFalse(TEXT("Editor filtering resolved"), Config.bEnableMaterial);
    Settings->Movement.NudgeStep = 17;
    TestEqual(TEXT("Edits apply on next resolution"), ResolveWireBundleSettings().Movement.NudgeStep, 17.0f);
    return true;
}
#endif
