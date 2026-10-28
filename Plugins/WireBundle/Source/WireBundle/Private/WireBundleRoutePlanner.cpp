#include "WireBundleRoutePlanner.h"

namespace WireBundle
{
	FBundleRoutePlan BuildParallelRoutePlan(const TArray<FLink>& Links,
		const FVector2D& RequestedCenter, const FWireBundleResolvedSettings& Settings)
	{
		FWireBundleResolvedSettings Config = Settings;
		Config.Sanitize();
		FBundleRoutePlan Plan;
		Plan.Columns = Config.Movement.RerouteColumns;
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
			const double	 Y = Plan.Center.Y + (Index - (Sorted.Num() - 1) * 0.5) * Config.Movement.WireSpacing;
			FBundleRouteLane Lane;
			Lane.Link = Sorted[Index];
			for (int32 Column = 0; Column < Plan.Columns; ++Column)
			{
				const double X = Plan.Columns == 1 ? Plan.Center.X
												   : Plan.Center.X + Config.Movement.BundleWidth * (double(Column) / (Plan.Columns - 1) - 0.5);
				Lane.Reroutes.Add(FVector2D(X, Y));
			}
			Lane.Entry = Lane.Reroutes[0];
			Lane.Exit = Lane.Reroutes.Last();
			Plan.Lanes.Add(MoveTemp(Lane));
		}
		return Plan;
	}
} // namespace WireBundle
