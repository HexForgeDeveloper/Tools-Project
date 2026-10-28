#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h"
#include "WireBundleSettings.h"
#include "WireBundleModel.h"

class SGraphPanel;
class IInputProcessor;

namespace WireBundle
{
struct FWireSnapshot
{
    FLink Link;
    TArray<FVector2D> Points;
};

WIREBUNDLE_API bool CanEdit(const TSharedRef<SGraphPanel>& Panel);
WIREBUNDLE_API void Bundle(const TSharedRef<SGraphPanel>& Panel, const FVector2D& Center,
    const TArray<FLink>* ExplicitLinks = nullptr, const FWireBundleResolvedSettings* Settings = nullptr);
WIREBUNDLE_API bool Nudge(const TSharedRef<SGraphPanel>& Panel, FVector2D Direction, bool bFast);
WIREBUNDLE_API bool Unbundle(const TSharedRef<SGraphPanel>& Panel, const FGuid& Id);
WIREBUNDLE_API bool SelectBundle(const TSharedRef<SGraphPanel>& Panel, const FGuid& Id);
WIREBUNDLE_API bool Adopt(const TSharedRef<SGraphPanel>& Panel);
WIREBUNDLE_API bool CrossesWire(const FVector2D& Start, const FVector2D& End,
    const TArray<FVector2D>& Wire, double Tolerance = 3.0, double MinimumDragDistance = 8.0);
WIREBUNDLE_API TArray<FLink> FindCrossedLinks(const TArray<FWireSnapshot>& Wires,
    const FVector2D& Start, const FVector2D& End,
    const FWireBundleGestureSettings& Settings = FWireBundleGestureSettings());
WIREBUNDLE_API TSharedRef<IInputProcessor> CreateInput();
}
