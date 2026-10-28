#include "WireBundleCollapse.h"
#include "WireBundleGesture.h"
#include "WireBundleRegistry.h"
#include "SGraphPanel.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "K2Node_Knot.h"
#include "MaterialGraphNode_Knot.h"
#include "Materials/MaterialExpression.h"
#include "MaterialEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

namespace WireBundle
{
struct FCollapsePlan
{
    TArray<UEdGraphNode*> Chain;
    UEdGraphPin* Source=nullptr;
    TArray<UEdGraphPin*> Targets;
    FVector2D Position=FVector2D::ZeroVector;
};
static UEdGraphPin* KnotPin(UEdGraphNode* Node,EEdGraphPinDirection Direction)
{
    UEdGraphPin* Found=nullptr;
    for (auto Pin:Node->Pins)
    {
        if (!Pin || Pin->bOrphanedPin) { return nullptr; }
        if (Pin->Direction==Direction) { if (Found) { return nullptr; } Found=Pin; }
    }
    return Found;
}
static bool DirectlyCompatible(UEdGraph* Graph,UEdGraphPin* Output,UEdGraphPin* Input)
{
    const auto R=Graph->GetSchema()->CanCreateConnection(Output,Input).Response;
    return R==CONNECT_RESPONSE_MAKE || R==CONNECT_RESPONSE_BREAK_OTHERS_A
        || R==CONNECT_RESPONSE_BREAK_OTHERS_B || R==CONNECT_RESPONSE_BREAK_OTHERS_AB;
}
static bool BuildCollapsePlan(const TSharedRef<SGraphPanel>& Panel,FCollapsePlan& Plan)
{
    if (!CanEdit(Panel) || Panel->SelectionManager.GetSelectedNodes().Num()<2) { return false; }
    auto Graph=Panel->GetGraphObj();
    if (!FBlueprintEditorUtils::FindBlueprintForGraph(Graph)
        && !FMaterialEditorUtilities::GetIMaterialEditorForObject(Graph).IsValid()) { return false; }
    TSet<UEdGraphNode*> Selected;
    UEdGraphNode* Root=nullptr;
    for (auto Object:Panel->SelectionManager.GetSelectedNodes())
    {
        auto Node=Cast<UEdGraphNode>(Object);
        if (!Node || Node->GetGraph()!=Graph || !Graph->Nodes.Contains(Node) || !Node->CanUserDeleteNode()
            || (!Node->IsA<UK2Node_Knot>() && !Node->IsA<UMaterialGraphNode_Knot>())) { return false; }
        auto Input=KnotPin(Node,EGPD_Input), Output=KnotPin(Node,EGPD_Output);
        if (!Input || !Output || Input->LinkedTo.Num()>1) { return false; }
        if (auto Knot=Cast<UMaterialGraphNode_Knot>(Node)) { if (!Knot->MaterialExpression) { return false; } }
        Selected.Add(Node);
    }
    for (auto Node:Selected)
    {
        const auto Input=KnotPin(Node,EGPD_Input);
        if (Input->LinkedTo.IsEmpty() || !Selected.Contains(Input->LinkedTo[0]->GetOwningNode()))
        { if (Root) { return false; } Root=Node; }
    }
    if (!Root) { return false; }
    const auto RootInput=KnotPin(Root,EGPD_Input), RootOutput=KnotPin(Root,EGPD_Output);
    if (RootInput->LinkedTo.Num()) { Plan.Source=RootInput->LinkedTo[0]; }
    auto Current=Root;
    TSet<UEdGraphNode*> Visited;
    while (Current)
    {
        if (Visited.Contains(Current)) { return false; }
        Visited.Add(Current); Plan.Chain.Add(Current);
        Plan.Position+=FVector2D(Current->NodePosX+21.0,Current->NodePosY+12.0);
        const auto Input=KnotPin(Current,EGPD_Input), Output=KnotPin(Current,EGPD_Output);
        if (Input->PinType!=RootInput->PinType || Output->PinType!=RootOutput->PinType) { return false; }
        UEdGraphNode* Next=nullptr;
        for (auto Other:Output->LinkedTo)
        {
            if (!Other || Other->bOrphanedPin || Other->Direction!=EGPD_Input || !Other->LinkedTo.Contains(Output)) { return false; }
            if (Selected.Contains(Other->GetOwningNode()))
            { if (Next || Other!=KnotPin(Other->GetOwningNode(),EGPD_Input)) { return false; } Next=Other->GetOwningNode(); }
            else
            {
                // Two existing paths into the same input cannot be represented
                // by a single pin link without changing their multiplicity.
                if (Plan.Targets.Contains(Other)) { return false; }
                Plan.Targets.Add(Other);
            }
        }
        Current=Next;
    }
    if (Visited.Num()!=Selected.Num()) { return false; }
    if (Plan.Source && (!Plan.Source->LinkedTo.Contains(RootInput) || !DirectlyCompatible(Graph,Plan.Source,RootInput))) { return false; }
    for (auto Target:Plan.Targets) { if (!DirectlyCompatible(Graph,RootOutput,Target)) { return false; } }
    Plan.Position/=Plan.Chain.Num();
    const auto Settings=ResolveWireBundleSettings().Movement;
    if (Settings.bSnapToGrid)
    {
        Plan.Position.X=FMath::GridSnap(Plan.Position.X,double(Settings.GridSize));
        Plan.Position.Y=FMath::GridSnap(Plan.Position.Y,double(Settings.GridSize));
    }
    Plan.Position-=FVector2D(21,12);
    return !Plan.Position.ContainsNaN() && FMath::Abs(Plan.Position.X)<MAX_int32-1.0 && FMath::Abs(Plan.Position.Y)<MAX_int32-1.0;
}
bool CanCollapseReroutes(const TSharedRef<SGraphPanel>& Panel)
{ FCollapsePlan Plan; return BuildCollapsePlan(Panel,Plan); }

bool CollapseReroutes(const TSharedRef<SGraphPanel>& Panel)
{
    FCollapsePlan Plan;
    if (!BuildCollapsePlan(Panel,Plan)) { return false; }
    auto Graph=Panel->GetGraphObj(); auto Survivor=Plan.Chain[0];
    TArray<UEdGraphNode*> Removed=Plan.Chain; Removed.RemoveAt(0);
    // Prepare metadata before deleting nodes. A normal bundle wire keeps its ID
    // and stores the shortened chain. Unrepresentable affected records are removed.
    TArray<FWireBundleRecord> Updated;
    TArray<FGuid> Affected;
    for (auto Record:ReadBundles(Graph))
    {
        bool bAffected=false, bCanKeep=ValidateBundle(Graph,Record)==EBundleHealth::Valid;
        for (auto& Wire:Record.Wires)
        {
            auto Source=ResolvePin(Graph,Wire.Source), Target=ResolvePin(Graph,Wire.Target);
            if ((Source && Removed.Contains(Source->GetOwningNode())) || (Target && Removed.Contains(Target->GetOwningNode())))
            { bAffected=true; bCanKeep=false; }
            TArray<UEdGraphNode*> Chain;
            bool bWireAffected=false;
            for (int32 I=0;I<Wire.Reroutes.Num();++I)
            {
                UEdGraphNode* Node=Wire.BlueprintReroutes.IsValidIndex(I) ? ToRawPtr(Wire.BlueprintReroutes[I]) : nullptr;
                if (!Node)
                { for (auto N:Graph->Nodes) { if (N && NodeIdentity(N)==Wire.Reroutes[I]) { Node=N; break; } } }
                if (Removed.Contains(Node)) { bAffected=true; bWireAffected=true; }
                else if (Node) { Chain.Add(Node); }
            }
            if (bWireAffected)
            {
                if (!Chain.Contains(Survivor)) { bCanKeep=false; }
                CaptureReroutes(Wire,Chain);
            }
        }
        if (bAffected) { Affected.Add(Record.Id); if (bCanKeep) { Updated.Add(MoveTemp(Record)); } }
    }
    const FScopedTransaction Transaction(NSLOCTEXT("WireBundle","Collapse","Collapse reroute chain"));
    Graph->Modify();
    for (auto Node:Plan.Chain)
    {
        Node->Modify();
        for (auto Pin:Node->Pins) { for (auto Other:Pin->LinkedTo) { Other->GetOwningNode()->Modify(); } }
    }
    for (const auto Id:Affected) { RemoveBundleRecord(Graph,Id); }
    for (auto Node:Plan.Chain) { Node->BreakAllNodeLinks(); }
    auto Input=KnotPin(Survivor,EGPD_Input), Output=KnotPin(Survivor,EGPD_Output);
    if (Plan.Source) { Plan.Source->MakeLinkTo(Input); }
    for (auto Target:Plan.Targets) { Output->MakeLinkTo(Target); }
    Survivor->NodePosX=FMath::RoundToInt32(Plan.Position.X); Survivor->NodePosY=FMath::RoundToInt32(Plan.Position.Y);
    if (auto Knot=Cast<UMaterialGraphNode_Knot>(Survivor))
    {
        Knot->MaterialExpression->Modify();
        Knot->MaterialExpression->MaterialExpressionEditorX=Survivor->NodePosX;
        Knot->MaterialExpression->MaterialExpressionEditorY=Survivor->NodePosY;
        FMaterialEditorUtilities::DeleteNodes(Graph,Removed);
        FMaterialEditorUtilities::UpdateMaterialAfterGraphChange(Graph);
    }
    else
    {
        for (auto Node:Removed) { FBlueprintEditorUtils::RemoveNode(nullptr,Node,true); }
        Survivor->PinConnectionListChanged(Input); Survivor->PinConnectionListChanged(Output);
        FBlueprintEditorUtils::MarkBlueprintAsModified(FBlueprintEditorUtils::FindBlueprintForGraph(Graph));
    }
    int32 Kept=0;
    for (const auto& Record:Updated) { if (StoreBundle(Graph,Record)) { ++Kept; } }
    if (Kept<Affected.Num() && ResolveWireBundleSettings().Appearance.bShowNotifications)
    {
        FNotificationInfo Info(NSLOCTEXT("WireBundle","CollapseRegistration","Reroutes collapsed. Affected bundle registrations that no longer match the graph were removed."));
        Info.ExpireDuration=ResolveWireBundleSettings().Appearance.NotificationDuration;
        FSlateNotificationManager::Get().AddNotification(Info);
    }
    Panel->SelectionManager.ClearSelectionSet(); Panel->SelectionManager.SetNodeSelection(Survivor,true);
    Graph->NotifyGraphChanged(); return true;
}
}
