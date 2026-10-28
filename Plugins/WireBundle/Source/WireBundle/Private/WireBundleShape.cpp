#include "WireBundleShape.h"
#include "WireBundleGesture.h"
#include "SGraphPanel.h"
#include "EdGraph/EdGraph.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MaterialGraphNode_Knot.h"
#include "Materials/MaterialExpression.h"
#include "MaterialEditorUtilities.h"
#include "ScopedTransaction.h"
#include "K2Node_Knot.h"

namespace WireBundle
{
static double ShapeRowY(const TArray<UEdGraphNode*>& Row)
{ double Y=0; for (const auto Node : Row) { Y+=Node->NodePosY; } return Y/Row.Num(); }
static bool FinishShape(UEdGraph* Graph, const TArray<TArray<UEdGraphNode*>>& Rows,
    const FWireBundleRecord* Record, FShapeSnapshot& Out)
{
    FShapeSnapshot Result;
    Result.Graph = Graph; Result.Columns = 1;
    Result.bSelectionOnly = !Record;
    if (Record) { Result.Record = *Record; }
    Result.Left = DBL_MAX; Result.Right = -DBL_MAX;
    for (const auto& Row : Rows)
    {
        Result.Columns = FMath::Max(Result.Columns, Row.Num());
        Result.RowOffsets.Add(Result.Nodes.Num());
        for (UEdGraphNode* Node : Row)
        {
            Result.Nodes.Add(Node); Result.Positions.Add(FVector2D(Node->NodePosX, Node->NodePosY));
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (!Pin) { continue; }
                FShapePinLinks Topology; Topology.Pin=FEdGraphPinReference(Pin); Topology.Type=Pin->PinType;
                for (UEdGraphPin* Linked : Pin->LinkedTo) { Topology.Links.Add(FEdGraphPinReference(Linked)); }
                Result.Topology.Add(MoveTemp(Topology));
            }
            Result.Left = FMath::Min(Result.Left, Node->NodePosX+21.0);
            Result.Right = FMath::Max(Result.Right, Node->NodePosX+21.0);
        }
        Result.Center.Y += ShapeRowY(Row) + 12;
    }
    Result.RowOffsets.Add(Result.Nodes.Num());
    Result.Center.Y /= Rows.Num();
    Result.Center.X = (Result.Left + Result.Right) * 0.5;
    Result.Spacing = Rows.Num()>1 ? (ShapeRowY(Rows.Last()) - ShapeRowY(Rows[0])) / (Rows.Num() - 1) : 0;
    Out = MoveTemp(Result);
    return true;
}

bool CaptureShape(UEdGraph* Graph, const FGuid& Id, FShapeSnapshot& Out)
{
    if (!Graph) { return false; }
    const auto Records = ReadBundles(Graph);
    const auto Record = Records.FindByPredicate([&](const FWireBundleRecord& R) { return R.Id == Id; });
    if (!Record || Record->Wires.Num() < 2 || ValidateBundle(Graph, *Record) != EBundleHealth::Valid) { return false; }
    TArray<TArray<UEdGraphNode*>> Rows;
    for (const auto& Wire : Record->Wires)
    {
        const int32 Count = Wire.Reroutes.Num();
        if (!Count) { return false; }
        TArray<UEdGraphNode*> Row;
        for (int32 Side = 0; Side < Count; ++Side)
        {
            UEdGraphNode* Found = nullptr;
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                const bool Match = Wire.BlueprintReroutes.Num() == Count
                    ? Node == Wire.BlueprintReroutes[Side] : NodeIdentity(Node) == Wire.Reroutes[Side];
                if (Match) { if (Found) { return false; } Found = Node; }
            }
            if (!Found) { return false; }
            Row.Add(Found);
        }
        Rows.Add(Row);
    }
    auto RowY = [](const TArray<UEdGraphNode*>& Row)
    { double Y=0; for (const auto Node : Row) { Y += Node->NodePosY; } return Y/Row.Num(); };
    Rows.StableSort([&](const auto& A, const auto& B) { return RowY(A) < RowY(B); });
    return FinishShape(Graph, Rows, Record, Out);
}

bool CaptureSelectedShape(const TSharedRef<SGraphPanel>& Panel, FShapeSnapshot& Out)
{
    // The snapshot is always the explicit selection, never the owning bundle.
    if (Panel->SelectionManager.GetSelectedNodes().Num() < 2 || !CanEdit(Panel)) { return false; }
    UEdGraph* Graph=Panel->GetGraphObj();
    TArray<UEdGraphNode*> Selected;
    for (UObject* Object : Panel->SelectionManager.GetSelectedNodes())
    {
        auto Node=Cast<UEdGraphNode>(Object);
        if (!Node || Node->GetGraph()!=Graph || !Graph->Nodes.Contains(Node)
            || (!Node->IsA<UK2Node_Knot>() && !Node->IsA<UMaterialGraphNode_Knot>())) { return false; }
        Selected.AddUnique(Node);

    }
    if (Selected.IsEmpty()) { return false; }
    // Native, unregistered and mixed selections are temporary groups only.
    // Components follow selected links; no ownership metadata is created.
    Selected.Sort([](const UEdGraphNode& A,const UEdGraphNode& B) { return A.NodeGuid.ToString()<B.NodeGuid.ToString(); });
    TSet<UEdGraphNode*> Remaining;
    for (auto Node:Selected) { Remaining.Add(Node); }
    TArray<TArray<UEdGraphNode*>> Rows;
    for (auto Root:Selected)
    {
        if (!Remaining.Remove(Root)) { continue; }
        TArray<UEdGraphNode*> Row={Root};
        for (int32 I=0; I<Row.Num(); ++I)
        {
            for (auto Pin:Row[I]->Pins)
            {
                if (!Pin) { continue; }
                for (auto Linked:Pin->LinkedTo)
                {
                    if (Linked && Remaining.Remove(Linked->GetOwningNode())) { Row.Add(Linked->GetOwningNode()); }
                }
            }
        }
        Row.Sort([](const UEdGraphNode& A,const UEdGraphNode& B)
        { if (A.NodePosX!=B.NodePosX) { return A.NodePosX<B.NodePosX; } return A.NodeGuid.ToString()<B.NodeGuid.ToString(); });
        Rows.Add(MoveTemp(Row));
    }
    Rows.StableSort([](const auto& A,const auto& B) { return ShapeRowY(A)<ShapeRowY(B); });
    return FinishShape(Graph,Rows,nullptr,Out);
}

bool IsShapeSelectionCurrent(const TSharedRef<SGraphPanel>& Panel, const FShapeSnapshot& Shape)
{
    const auto& Selection=Panel->SelectionManager.GetSelectedNodes();
    if (!Shape.Graph.IsValid() || Panel->GetGraphObj()!=Shape.Graph.Get()
        || Shape.Nodes.Num()<2 || Selection.Num()!=Shape.Nodes.Num()) { return false; }
    TSet<UEdGraphNode*> Unique;
    for (const auto Weak:Shape.Nodes)
    {
        auto Node=Weak.Get();
        if (!Node || !Selection.Contains(Node) || Node->GetGraph()!=Shape.Graph.Get()
            || !Shape.Graph->Nodes.Contains(Node) || Unique.Contains(Node)) { return false; }
        Unique.Add(Node);
    }
    return true;
}

TArray<FVector2D> PlanShapeEdit(const FShapeSnapshot& Shape, EShapeHandle Handle,
    FVector2D Delta, const FWireBundleMovementSettings& Settings)
{
    if (Delta.ContainsNaN() || Shape.Columns < 1 || Shape.NumRows() < 1) { return {}; }
    if (Shape.RowOffsets.IsEmpty() && Shape.Positions.Num() % Shape.Columns) { return {}; }
    if (!Shape.RowOffsets.IsEmpty() && (Shape.RowOffsets[0] != 0 || Shape.RowOffsets.Last() != Shape.Positions.Num())) { return {}; }
    for (int32 Row=0; Row<Shape.NumRows(); ++Row)
    { if (Shape.RowBegin(Row)<0 || Shape.RowEnd(Row)<=Shape.RowBegin(Row) || Shape.RowEnd(Row)>Shape.Positions.Num()) { return {}; } }
    FWireBundleResolvedSettings Config;
    Config.Movement = Settings; Config.Sanitize();
    const auto& S = Config.Movement;
    auto Snap = [&](double V) { return S.bSnapToGrid ? FMath::GridSnap(V, double(S.GridSize)) : V; };
    auto Result = Shape.Positions;
    if (Delta.IsNearlyZero() || ((Handle == EShapeHandle::Entry || Handle == EShapeHandle::Exit) && FMath::IsNearlyZero(Delta.X))
        || (Handle == EShapeHandle::Spacing && FMath::IsNearlyZero(Delta.Y))) { return Result; }
    if (Handle == EShapeHandle::Move)
    {
        if (S.MouseAxis == EWireBundleMovementAxis::Horizontal) { Delta.Y = 0; }
        if (S.MouseAxis == EWireBundleMovementAxis::Vertical) { Delta.X = 0; }
        Delta.X = Delta.X == 0 ? 0 : Snap(Shape.Center.X + Delta.X) - Shape.Center.X;
        Delta.Y = Delta.Y == 0 ? 0 : Snap(Shape.Center.Y + Delta.Y) - Shape.Center.Y;
        Delta.X = FMath::RoundToDouble(Delta.X); Delta.Y = FMath::RoundToDouble(Delta.Y);
        for (auto& P : Result) { P += Delta; }
    }
    else if (Handle == EShapeHandle::Entry || Handle == EShapeHandle::Exit)
    {
        if (!Shape.HasWidth()) { return Result; }
        const bool bLeft = Handle == EShapeHandle::Entry;
        const double Target = bLeft ? FMath::Clamp(Snap(Shape.Left + Delta.X), Shape.Right - 4096, Shape.Right - 16)
            : FMath::Clamp(Snap(Shape.Right + Delta.X), Shape.Left + 16, Shape.Left + 4096);
        const double Left = bLeft ? Target : Shape.Left, Right = bLeft ? Shape.Right : Target;
        for (auto& P : Result)
        {
            const double Fraction = (P.X+21-Shape.Left)/(Shape.Right-Shape.Left);
            P.X = FMath::RoundToDouble(Left + Fraction*(Right-Left)-21);
        }
    }
    else if (Handle == EShapeHandle::Spacing)
    {
        const int32 Rows = Shape.NumRows();
        if (Rows<2) { return Result; }
        const double Spacing = FMath::Clamp(Snap(Shape.Spacing + Delta.Y * 2 / (Rows - 1)), 1.0, 256.0);
        for (int32 Row = 0; Row < Rows; ++Row)
        {
            double Current = 0;
            for (int32 I=Shape.RowBegin(Row); I<Shape.RowEnd(Row); ++I) { Current += Result[I].Y; }
            Current = Current / (Shape.RowEnd(Row)-Shape.RowBegin(Row)) + 12;
            const double Shift = FMath::RoundToDouble(Shape.Center.Y + (Row - (Rows-1)*0.5) * Spacing - Current);
            for (int32 I=Shape.RowBegin(Row); I<Shape.RowEnd(Row); ++I) { Result[I].Y += Shift; }
        }
    }
    return Result;
}

bool CommitShapeEdit(const TSharedRef<SGraphPanel>& Panel, const FShapeSnapshot& Shape,
    EShapeHandle Handle, const FVector2D& Delta, const FWireBundleMovementSettings& Settings)
{
    if (!CanEdit(Panel) || !IsShapeSelectionCurrent(Panel,Shape)) { return false; }
    FShapeSnapshot Current;
    const bool bCaptured = Shape.bSelectionOnly ? CaptureSelectedShape(Panel,Current) : CaptureShape(Shape.Graph.Get(),Shape.Record.Id,Current);
    if (!bCaptured || Current.bSelectionOnly!=Shape.bSelectionOnly || Current.Topology!=Shape.Topology
        || Current.Nodes != Shape.Nodes || Current.Positions != Shape.Positions || Current.RowOffsets != Shape.RowOffsets
        || (!Shape.bSelectionOnly && ValidateBundle(Shape.Graph.Get(), Shape.Record) != EBundleHealth::Valid)) { return false; }
    const auto Positions = PlanShapeEdit(Shape, Handle, Delta, Settings);
    if (Positions.Num() != Shape.Nodes.Num() || Positions == Shape.Positions) { return false; }
    for (const auto& P : Positions)
    { if (P.ContainsNaN() || FMath::Abs(P.X) > MAX_int32 - 1.0 || FMath::Abs(P.Y) > MAX_int32 - 1.0) { return false; } }
    UEdGraph* Graph = Shape.Graph.Get();
    if (!FBlueprintEditorUtils::FindBlueprintForGraph(Graph)
        && !FMaterialEditorUtilities::GetIMaterialEditorForObject(Graph).IsValid()) { return false; }
    const FScopedTransaction Transaction(NSLOCTEXT("WireBundle", "Shape", "Change wire bundle shape"));
    Graph->Modify();
    for (int32 I = 0; I < Shape.Nodes.Num(); ++I)
    {
        UEdGraphNode* Node = Shape.Nodes[I].Get();
        Node->Modify();
        Node->NodePosX = FMath::RoundToInt32(Positions[I].X); Node->NodePosY = FMath::RoundToInt32(Positions[I].Y);
        if (auto Knot = Cast<UMaterialGraphNode_Knot>(Node))
        {
            Knot->MaterialExpression->Modify();
            Knot->MaterialExpression->MaterialExpressionEditorX = Node->NodePosX;
            Knot->MaterialExpression->MaterialExpressionEditorY = Node->NodePosY;
        }
    }
    if (auto BP = FBlueprintEditorUtils::FindBlueprintForGraph(Graph)) { FBlueprintEditorUtils::MarkBlueprintAsModified(BP); }
    else { FMaterialEditorUtilities::UpdateMaterialAfterGraphChange(Graph); }
    Graph->NotifyGraphChanged();
    return true;
}
}
