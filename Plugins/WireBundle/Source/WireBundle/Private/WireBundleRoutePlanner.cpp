#include "WireBundleRoutePlanner.h"

namespace WireBundle
{
FBundleRoutePlan BuildParallelRoutePlan(const TArray<FLink>& Links,
    const FVector2D& RequestedCenter, const FWireBundleResolvedSettings& Settings)
{
    FWireBundleResolvedSettings Config = Settings;
    Config.Sanitize();
    FBundleRoutePlan Plan;
    Plan.Center = RequestedCenter + Config.Movement.CenterOffset;
    if (Config.Movement.bSnapToGrid)
    {
        Plan.Center.X = FMath::GridSnap(Plan.Center.X, double(Config.Movement.GridSize));
        Plan.Center.Y = FMath::GridSnap(Plan.Center.Y, double(Config.Movement.GridSize));
    }
    TArray<FLink> Sorted = Links;
    Sorted.StableSort([](const FLink& A, const FLink& B) { return A.SourceY < B.SourceY; });
    Plan.Lanes.Reserve(Sorted.Num());
    for (int32 Index = 0; Index < Sorted.Num(); ++Index)
    {
        const double Y = Plan.Center.Y + (Index - (Sorted.Num() - 1) * 0.5) * Config.Movement.WireSpacing;
        Plan.Lanes.Add({Sorted[Index],
            FVector2D(Plan.Center.X - Config.Movement.BundleWidth * 0.5, Y),
            FVector2D(Plan.Center.X + Config.Movement.BundleWidth * 0.5, Y)});
    }
    return Plan;
}
}
