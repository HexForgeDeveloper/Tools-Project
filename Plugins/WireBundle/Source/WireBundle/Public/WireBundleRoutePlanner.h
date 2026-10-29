#pragma once

#include "WireBundleModel.h"
#include "WireBundleSettings.h"

class SGraphPanel;

namespace WireBundle
{
    struct FRouteObstacle
    {
        FGuid Node;
        FBox2D Bounds;
        bool operator==(const FRouteObstacle& O) const
        { return Node==O.Node && Bounds.Min==O.Bounds.Min && Bounds.Max==O.Bounds.Max && Bounds.bIsValid==O.Bounds.bIsValid; }
    };
    struct FRouteWireGeometry
    {
        FLink Link;
        FVector2D Source, Target;
        FGuid SourceNode, TargetNode;
        FRouteEndpointGeometry SourceEndpoint, TargetEndpoint;
        bool operator==(const FRouteWireGeometry& O) const
        { return Link.Output==O.Link.Output && Link.Input==O.Link.Input && Link.SourceY==O.Link.SourceY
            && Source==O.Source && Target==O.Target && SourceNode==O.SourceNode && TargetNode==O.TargetNode
            && SourceEndpoint==O.SourceEndpoint && TargetEndpoint==O.TargetEndpoint; }
    };
    // Geometry and spline math are supplied by the adapter; planning never edits a graph.
    WIREBUNDLE_API FBundleRoutePlan BuildObstacleRoutePlan(const TArray<FRouteWireGeometry>& Wires,
        const TArray<FRouteObstacle>& Obstacles, const FVector2D& Center,
        const FWireBundleResolvedSettings& Settings,
        TFunctionRef<FVector2D(const FVector2D&, const FVector2D&)> Tangent);
    WIREBUNDLE_API FBundleRoutePlan BuildGraphRoutePlan(const TSharedRef<SGraphPanel>& Panel,
        const TArray<FLink>& Links, const FVector2D& Center, const FWireBundleResolvedSettings& Settings);
	// No graph mutations, transactions or Slate geometry. The same result can be
	// painted by a preview and then consumed by the graph adapter when committed.
	WIREBUNDLE_API FBundleRoutePlan BuildParallelRoutePlan(const TArray<FLink>& Links,
		const FVector2D& RequestedCenter, const FWireBundleResolvedSettings& Settings);
} // namespace WireBundle
