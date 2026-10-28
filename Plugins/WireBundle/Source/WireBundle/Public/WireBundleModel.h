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
    int32 SourceY = 0;
};

struct FBundleRouteLane
{
    FLink Link;
    FVector2D Entry;
    FVector2D Exit;
    TArray<FVector2D> Reroutes;
};

// Logical positions at which the adapter inserts reroutes, in graph units.
// The native knot's top-left offset belongs to the adapter, not the planner.
struct FBundleRoutePlan
{
    int32 Columns = 2;
    FVector2D Center = FVector2D::ZeroVector;
    TArray<FBundleRouteLane> Lanes;
};
}
