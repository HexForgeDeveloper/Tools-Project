#pragma once
#include "CoreMinimal.h"
#include "WireBundleRegistry.h"
#include "WireBundleSettings.h"

class SGraphPanel;
class IInputProcessor;
namespace WireBundle
{
enum class EShapeHandle : uint8 { Move, Entry, Exit, Spacing };
struct FShapePinLinks
{
    FEdGraphPinReference Pin;
    TArray<FEdGraphPinReference> Links;
    FEdGraphPinType Type;
    bool operator==(const FShapePinLinks& Other) const { return Pin==Other.Pin && Links==Other.Links && Type==Other.Type; }
};
struct FShapeSnapshot
{
    TWeakObjectPtr<UEdGraph> Graph;
    FWireBundleRecord Record;
    bool bSelectionOnly = false;
    TArray<FShapePinLinks> Topology;
    int32 Columns = 2;
    // Nodes grouped by wire, ordered by their current average height.
    TArray<TWeakObjectPtr<UEdGraphNode>> Nodes;
    TArray<FVector2D> Positions;
    // Start offsets and a final sentinel allow different chain lengths per wire.
    TArray<int32> RowOffsets;
    int32 NumRows() const { return RowOffsets.IsEmpty() ? (Columns > 0 ? Positions.Num()/Columns : 0) : RowOffsets.Num()-1; }
    int32 RowBegin(int32 Row) const { return RowOffsets.IsEmpty() ? Row*Columns : RowOffsets[Row]; }
    int32 RowEnd(int32 Row) const { return RowOffsets.IsEmpty() ? (Row+1)*Columns : RowOffsets[Row+1]; }
    bool HasWidth() const { return (bSelectionOnly ? Positions.Num()>1 : Columns>1) && Right-Left>=16; }
    FVector2D Center = FVector2D::ZeroVector;
    double Left = 0, Right = 0, Spacing = 0;
};
WIREBUNDLE_API bool CaptureSelectedShape(const TSharedRef<SGraphPanel>& Panel, FShapeSnapshot& Out);
WIREBUNDLE_API bool CaptureShape(UEdGraph* Graph, const FGuid& Id, FShapeSnapshot& Out);
WIREBUNDLE_API TArray<FVector2D> PlanShapeEdit(const FShapeSnapshot& Shape, EShapeHandle Handle,
    FVector2D Delta, const FWireBundleMovementSettings& Settings);
WIREBUNDLE_API bool CommitShapeEdit(const TSharedRef<SGraphPanel>& Panel, const FShapeSnapshot& Shape,
    EShapeHandle Handle, const FVector2D& Delta, const FWireBundleMovementSettings& Settings);
TSharedRef<IInputProcessor> CreateShapeInput();
}
