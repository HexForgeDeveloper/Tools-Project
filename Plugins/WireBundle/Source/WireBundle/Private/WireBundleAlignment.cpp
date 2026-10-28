#include "WireBundleAlignment.h"
#include "WireBundleGesture.h"
#include "SGraphPanel.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_Knot.h"
#include "MaterialGraphNode_Knot.h"
#include "Materials/MaterialExpression.h"
#include "MaterialEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"

namespace WireBundle
{
static bool AlignmentNodes(const TSharedRef<SGraphPanel>& Panel,TArray<UEdGraphNode*>& Nodes)
{
    if (!CanEdit(Panel) || Panel->SelectionManager.GetSelectedNodes().Num()<2) { return false; }
    auto Graph=Panel->GetGraphObj();
    for (auto Object:Panel->SelectionManager.GetSelectedNodes())
    {
        auto Node=Cast<UEdGraphNode>(Object);
        if (!Node || Node->GetGraph()!=Graph || !Graph->Nodes.Contains(Node)
            || (!Node->IsA<UK2Node_Knot>() && !Node->IsA<UMaterialGraphNode_Knot>())) { return false; }
        if (const auto Knot=Cast<UMaterialGraphNode_Knot>(Node))
        { if (!Knot->MaterialExpression) { return false; } }
        Nodes.Add(Node);
    }
    return FBlueprintEditorUtils::FindBlueprintForGraph(Graph)
        || FMaterialEditorUtilities::GetIMaterialEditorForObject(Graph).IsValid();
}
bool CanAlignVertical(const TSharedRef<SGraphPanel>& Panel)
{ TArray<UEdGraphNode*> Nodes; return AlignmentNodes(Panel,Nodes); }
bool AlignVertical(const TSharedRef<SGraphPanel>& Panel)
{
    TArray<UEdGraphNode*> Nodes;
    if (!AlignmentNodes(Panel,Nodes)) { return false; }
    double X=0;
    for (auto Node:Nodes) { X+=Node->NodePosX+21.0; }
    X/=Nodes.Num();
    const auto Settings=ResolveWireBundleSettings().Movement;
    if (Settings.bSnapToGrid) { X=FMath::GridSnap(X,double(Settings.GridSize)); }
    X-=21;
    if (!FMath::IsFinite(X) || X<MIN_int32+1.0 || X>MAX_int32-1.0) { return false; }
    const int32 Target=FMath::RoundToInt32(X);
    if (!Nodes.ContainsByPredicate([Target](const auto Node) { return Node->NodePosX!=Target; })) { return false; }
    const FScopedTransaction Transaction(NSLOCTEXT("WireBundle","AlignVertical","Align reroutes vertically"));
    auto Graph=Panel->GetGraphObj(); Graph->Modify();
    for (auto Node:Nodes)
    {
        Node->Modify(); Node->NodePosX=Target;
        if (auto Knot=Cast<UMaterialGraphNode_Knot>(Node))
        {
            Knot->MaterialExpression->Modify();
            Knot->MaterialExpression->MaterialExpressionEditorX=Target;
        }
    }
    if (auto BP=FBlueprintEditorUtils::FindBlueprintForGraph(Graph)) { FBlueprintEditorUtils::MarkBlueprintAsModified(BP); }
    else { FMaterialEditorUtilities::UpdateMaterialAfterGraphChange(Graph); }
    Graph->NotifyGraphChanged(); return true;
}
}
