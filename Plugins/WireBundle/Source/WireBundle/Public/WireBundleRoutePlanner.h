#pragma once

#include "WireBundleModel.h"
#include "WireBundleSettings.h"

namespace WireBundle
{
// No graph mutations, transactions or Slate geometry. The same result can be
// painted by a preview and then consumed by the graph adapter when committed.
WIREBUNDLE_API FBundleRoutePlan BuildParallelRoutePlan(const TArray<FLink>& Links,
    const FVector2D& RequestedCenter, const FWireBundleResolvedSettings& Settings);
}
