#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h"

class SGraphPanel;
class IInputProcessor;

namespace WireBundle
{
struct FLink
{
    FEdGraphPinReference Output;
    FEdGraphPinReference Input;
    int32 SourceY = 0;
};

struct FWireSnapshot
{
    FLink Link;
    TArray<FVector2D> Points;
};

bool CanEdit(const TSharedRef<SGraphPanel>& Panel);
void Bundle(const TSharedRef<SGraphPanel>& Panel, const FVector2D& Center,
    const TArray<FLink>* ExplicitLinks = nullptr);
bool CrossesWire(const FVector2D& Start, const FVector2D& End,
    const TArray<FVector2D>& Wire, double Tolerance = 3.0);
TArray<FLink> FindCrossedLinks(const TArray<FWireSnapshot>& Wires,
    const FVector2D& Start, const FVector2D& End);
TSharedRef<IInputProcessor> CreateInput();
}
