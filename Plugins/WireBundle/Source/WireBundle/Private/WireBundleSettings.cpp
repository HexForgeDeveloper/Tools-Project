#include "WireBundleSettings.h"

void FWireBundleResolvedSettings::Sanitize()
{
    auto Safe = [](float Value, float Default, float Min, float Max)
    { return FMath::IsFinite(Value) ? FMath::Clamp(Value, Min, Max) : Default; };
    Movement.BundleWidth = Safe(Movement.BundleWidth, 160, 16, 4096);
    Movement.WireSpacing = Safe(Movement.WireSpacing, 12, 1, 256);
    Movement.GridSize = Safe(Movement.GridSize, 16, 1, 512);
    Movement.NudgeStep = Safe(Movement.NudgeStep, 16, 1, 1024);
    Movement.FastNudgeMultiplier = Safe(Movement.FastNudgeMultiplier, 4, 1, 32);
    if (Movement.KeyboardAxis != EWireBundleMovementAxis::Free
        && Movement.KeyboardAxis != EWireBundleMovementAxis::Horizontal
        && Movement.KeyboardAxis != EWireBundleMovementAxis::Vertical)
    { Movement.KeyboardAxis = EWireBundleMovementAxis::Free; }
    if (Movement.CenterOffset.ContainsNaN()) { Movement.CenterOffset = FVector2D::ZeroVector; }
    Gesture.MinimumDragDistance = Safe(Gesture.MinimumDragDistance, 8, 1, 128);
    Gesture.WireHitTolerance = Safe(Gesture.WireHitTolerance, 3, 0, 32);
    Gesture.MinimumWireCount = FMath::Clamp(Gesture.MinimumWireCount, 2, 256);
    Gesture.CurveSamples = FMath::Clamp(Gesture.CurveSamples, 16, 256);
    if (!Gesture.GestureButton.IsMouseButton()) { Gesture.GestureButton = EKeys::LeftMouseButton; }
    if (!Gesture.BundleKey.IsValid() || Gesture.BundleKey.IsMouseButton()) { Gesture.BundleKey = EKeys::B; }
    if (!Gesture.AdoptKey.IsValid() || Gesture.AdoptKey.IsMouseButton() || Gesture.AdoptKey == Gesture.BundleKey)
    { Gesture.AdoptKey = Gesture.BundleKey == EKeys::A ? EKeys::G : EKeys::A; }
    if (!Gesture.CancelKey.IsValid() || Gesture.CancelKey.IsMouseButton()) { Gesture.CancelKey = EKeys::Escape; }
    Appearance.GuideThickness = Safe(Appearance.GuideThickness, 2, 0.5f, 16);
    Appearance.HighlightThickness = Safe(Appearance.HighlightThickness, 3, 0.5f, 16);
    Appearance.RoutePreviewThickness = Safe(Appearance.RoutePreviewThickness, 2, 0.5f, 16);
    Appearance.MarkerRadius = Safe(Appearance.MarkerRadius, 5, 1, 64);
    Appearance.NotificationDuration = Safe(Appearance.NotificationDuration, 4, 0.5f, 30);
    Appearance.HintFontSize = FMath::Clamp(Appearance.HintFontSize, 8, 32);
    if (Appearance.HintOffset.ContainsNaN()) { Appearance.HintOffset = FVector2D(16, 12); }
}

FWireBundleResolvedSettings ResolveWireBundleSettings()
{
    FWireBundleResolvedSettings Result;
    const UWireBundleEditorSettings* Settings = GetDefault<UWireBundleEditorSettings>();
    Result.Movement = Settings->Movement;
    Result.Gesture = Settings->Gesture;
    Result.Appearance = Settings->Appearance;
    Result.bEnableBlueprint = Settings->bEnableBlueprint;
    Result.bEnableMaterial = Settings->bEnableMaterial;
    Result.Sanitize();
    return Result;
}
