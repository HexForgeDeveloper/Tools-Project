#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h"

namespace WireBundle
{
	// Live references are scoped to an editor operation. Persistent bundle storage
	// will use graph/node/pin GUIDs through a separate repository, never Slate widgets.
	struct FLink
	{
		FEdGraphPinReference Output;
		FEdGraphPinReference Input;
		int32				 SourceY = 0;
	};

    // Other endpoint neighbours, excluding the edge being replaced. Predict the
    // same automatic knot orientation Unreal will use after inserting the route.
    struct FRouteEndpointGeometry
    {
        bool bIsKnot = false;
        double CenterX = 0, IncomingX = 0, OutgoingX = 0;
        int32 IncomingCount = 0, OutgoingCount = 0;
        int32 FixedDirection = 1;
        bool IsReversed(double NewNeighborX, bool bOutput) const
        {
            if(!bIsKnot) { return FixedDirection<0; }
            const int32 In=IncomingCount+(bOutput?0:1), Out=OutgoingCount+(bOutput?1:0);
            const double Left=(IncomingX+(bOutput?0:NewNeighborX))/FMath::Max(1,In);
            const double Right=(OutgoingX+(bOutput?NewNeighborX:0))/FMath::Max(1,Out);
            return In && Out ? Right<Left : In ? CenterX<Left : Right<CenterX;
        }
        bool operator==(const FRouteEndpointGeometry& O) const
        { return bIsKnot==O.bIsKnot && CenterX==O.CenterX && IncomingX==O.IncomingX && OutgoingX==O.OutgoingX
            && IncomingCount==O.IncomingCount && OutgoingCount==O.OutgoingCount && FixedDirection==O.FixedDirection; }
    };
	struct FBundleRouteLane
	{
		FLink			  Link;
		FVector2D		  Entry;
		FVector2D		  Exit;
		TArray<FVector2D> Reroutes;
        bool bSourceReversed = false, bTargetReversed = false;
        int32 Direction = 1;
        TArray<int32> KnotDirections;
	};

	// Logical positions at which the adapter inserts reroutes, in graph units.
	// The native knot's top-left offset belongs to the adapter, not the planner.
	struct FBundleRoutePlan
	{
		int32					 Columns = 2;
        bool bBlocked = false;
        bool bAvoidedObstacles = false;
        int32 ExtraBends = 0;
		FVector2D				 Center = FVector2D::ZeroVector;
		TArray<FBundleRouteLane> Lanes;
	};
} // namespace WireBundle
