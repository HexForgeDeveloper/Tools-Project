#include "WireBundleGesture.h"
#include "SGraphPanel.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_Knot.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireBundleBlueprintTest, "WireBundle.Blueprint.BundleUndoRedo",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWireBundleBlueprintTest::RunTest(const FString&)
{
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(UObject::StaticClass(),
        GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("WireBundleTest")),
        BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, TEXT("TestGraph"),
        UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(Blueprint, Graph);
    TArray<UK2Node_Knot*> Nodes;
    for (int32 I = 0; I < 4; ++I)
    {
        FGraphNodeCreator<UK2Node_Knot> Creator(*Graph);
        UK2Node_Knot* Node = Creator.CreateNode();
        Creator.Finalize();
        Node->NodePosX = I < 2 ? 0 : 600;
        Node->NodePosY = (I % 2) * 120;
        Nodes.Add(Node);
    }
    Nodes[0]->GetOutputPin()->MakeLinkTo(Nodes[2]->GetInputPin());
    Nodes[1]->GetOutputPin()->MakeLinkTo(Nodes[3]->GetInputPin());
    const TSharedRef<SGraphPanel> Panel = SNew(SGraphPanel).GraphObj(Graph);
    for (UK2Node_Knot* Node : Nodes) { Panel->SelectionManager.SetNodeSelection(Node, true); }
    WireBundle::Bundle(Panel, FVector2D(300, 100));
    TestEqual(TEXT("Two reroutes for each wire"), Graph->Nodes.Num(), 8);
    TestEqual(TEXT("Created reroutes selected together"), Panel->SelectionManager.GetSelectedNodes().Num(), 4);
    for (int32 I = 0; I < 2; ++I)
    {
        UEdGraphPin* Pin = Nodes[I]->GetOutputPin();
        int32 Hops = 0;
        while (Pin && Pin->LinkedTo.Num() == 1 && Hops < 5)
        {
            UEdGraphNode* Next = Pin->LinkedTo[0]->GetOwningNode();
            ++Hops;
            if (Next == Nodes[I + 2]) { break; }
            UK2Node_Knot* Knot = Cast<UK2Node_Knot>(Next);
            Pin = Knot ? Knot->GetOutputPin() : nullptr;
        }
        TestEqual(TEXT("Original destination reachable through two reroutes"), Hops, 3);
    }
    GEditor->UndoTransaction();
    TestEqual(TEXT("One undo removes the entire bundle"), Graph->Nodes.Num(), 4);
    TestTrue(TEXT("Original first link restored"), Nodes[0]->GetOutputPin()->LinkedTo.Contains(Nodes[2]->GetInputPin()));
    TestTrue(TEXT("Original second link restored"), Nodes[1]->GetOutputPin()->LinkedTo.Contains(Nodes[3]->GetInputPin()));
    GEditor->RedoTransaction();
    TestEqual(TEXT("Redo restores the entire bundle"), Graph->Nodes.Num(), 8);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireBundleGestureScopeTest, "WireBundle.Blueprint.GestureScopeUndo",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWireBundleGestureScopeTest::RunTest(const FString&)
{
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(UObject::StaticClass(),
        GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("WireBundleScope")),
        BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, TEXT("TestGraph"),
        UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(Blueprint, Graph);
    TArray<UK2Node_Knot*> Nodes;
    for (int32 I = 0; I < 6; ++I)
    {
        FGraphNodeCreator<UK2Node_Knot> Creator(*Graph);
        Nodes.Add(Creator.CreateNode());
        Creator.Finalize();
    }
    TArray<WireBundle::FWireSnapshot> Wires;
    for (int32 I = 0; I < 3; ++I)
    {
        Nodes[I]->GetOutputPin()->MakeLinkTo(Nodes[I + 3]->GetInputPin());
        WireBundle::FWireSnapshot Wire;
        Wire.Link = {FEdGraphPinReference(Nodes[I]->GetOutputPin()), FEdGraphPinReference(Nodes[I + 3]->GetInputPin()), I * 100};
        Wire.Points = {FVector2D(0, I * 100), FVector2D(600, I * 100)};
        Wires.Add(Wire);
    }
    TArray<WireBundle::FLink> Links = WireBundle::FindCrossedLinks(Wires, FVector2D(300, -20), FVector2D(300, 120));
    TestEqual(TEXT("Gesture picks only crossed wires"), Links.Num(), 2);
    const WireBundle::FLink Duplicate = Links[0];
    Links.Add(Duplicate); // Defensive deduplication before modifying graph.
    Links.Add(WireBundle::FLink()); // Stale or unresolved references must be ignored.
    const TSharedRef<SGraphPanel> Panel = SNew(SGraphPanel).GraphObj(Graph);
    WireBundle::Bundle(Panel, FVector2D(300, 50), &Links);
    TestEqual(TEXT("No prior selection needed and duplicate ignored"), Graph->Nodes.Num(), 10);
    TestTrue(TEXT("Uncrossed connection untouched"), Nodes[2]->GetOutputPin()->LinkedTo.Contains(Nodes[5]->GetInputPin()));
    GEditor->UndoTransaction();
    TestEqual(TEXT("Gesture undo removes all reroutes"), Graph->Nodes.Num(), 6);
    for (int32 I = 0; I < 3; ++I)
    {
        TestTrue(TEXT("Every original connection restored"), Nodes[I]->GetOutputPin()->LinkedTo.Contains(Nodes[I + 3]->GetInputPin()));
    }
    Graph->bEditable = false;
    WireBundle::Bundle(Panel, FVector2D(300, 50), &Links);
    TestEqual(TEXT("Read-only graphs cannot be changed"), Graph->Nodes.Num(), 6);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireBundleSettingsMovementTest, "WireBundle.Settings.LayoutAndMovement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWireBundleSettingsMovementTest::RunTest(const FString&)
{
    UWireBundleEditorSettings* Settings = GetMutableDefault<UWireBundleEditorSettings>();
    TGuardValue<FWireBundleMovementSettings> Guard(Settings->Movement, FWireBundleMovementSettings());
    Settings->Movement.BundleWidth = 240;
    Settings->Movement.NudgeStep = 10;
    Settings->Movement.FastNudgeMultiplier = 3;
    Settings->Movement.KeyboardAxis = EWireBundleMovementAxis::Horizontal;
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(UObject::StaticClass(),
        GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("WireBundleMovement")),
        BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, TEXT("TestGraph"),
        UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(Blueprint, Graph);
    TArray<UK2Node_Knot*> Nodes;
    for (int32 I = 0; I < 4; ++I)
    {
        FGraphNodeCreator<UK2Node_Knot> Creator(*Graph);
        Nodes.Add(Creator.CreateNode());
        Creator.Finalize();
    }
    const TSharedRef<SGraphPanel> Panel = SNew(SGraphPanel).GraphObj(Graph);
    for (UK2Node_Knot* Node : Nodes) { Panel->SelectionManager.SetNodeSelection(Node, true); }
    for (int32 I = 0; I < 2; ++I) { Nodes[I]->GetOutputPin()->MakeLinkTo(Nodes[I + 2]->GetInputPin()); }
    WireBundle::Bundle(Panel, FVector2D(300, 100));
    TArray<UEdGraphNode*> Created;
    int32 MinX = MAX_int32, MaxX = MIN_int32;
    for (UObject* Object : Panel->SelectionManager.GetSelectedNodes())
    {
        UEdGraphNode* Node = CastChecked<UEdGraphNode>(Object);
        Created.Add(Node);
        MinX = FMath::Min(MinX, Node->NodePosX);
        MaxX = FMath::Max(MaxX, Node->NodePosX);
    }
    TestEqual(TEXT("Plugin settings control bundle width"), MaxX - MinX, 240);
    if (!TestEqual(TEXT("Four reroutes created"), Created.Num(), 4)) { return false; }
    const int32 X = Created[0]->NodePosX, Y = Created[0]->NodePosY;
    WireBundle::Nudge(Panel, FVector2D(1, 0), true);
    TestEqual(TEXT("Fast step uses configured multiplier"), Created[0]->NodePosX, X + 30);
    WireBundle::Nudge(Panel, FVector2D(0, 1), false);
    TestEqual(TEXT("Axis constraint prevents vertical movement"), Created[0]->NodePosY, Y);
    GEditor->UndoTransaction();
    TestEqual(TEXT("Movement undo restores position"), Created[0]->NodePosX, X);
    Settings->Movement.KeyboardAxis = EWireBundleMovementAxis::Free;
    Settings->Movement.bSnapToGrid = true;
    Settings->Movement.GridSize = 16;
    const int32 RelativeX = Created[1]->NodePosX - Created[0]->NodePosX;
    const int32 RelativeY = Created[1]->NodePosY - Created[0]->NodePosY;
    WireBundle::Nudge(Panel, FVector2D(1, 0), false);
    TestEqual(TEXT("Grid movement keeps bundle spacing X"), Created[1]->NodePosX - Created[0]->NodePosX, RelativeX);
    TestEqual(TEXT("Grid movement keeps bundle spacing Y"), Created[1]->NodePosY - Created[0]->NodePosY, RelativeY);
    return true;
}
#endif
