#if WITH_DEV_AUTOMATION_TESTS
#include "WireBundleGesture.h"
#include "WireBundleRegistry.h"
#include "Misc/AutomationTest.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Knot.h"
#include "SGraphPanel.h"
#include "Editor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireBundleUnbundleTest, "WireBundle.Blueprint.UnbundleAndSafety",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWireBundleUnbundleTest::RunTest(const FString&)
{
    UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(UObject::StaticClass(), GetTransientPackage(),
        MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("UnbundleTest")),
        BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("Test"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(BP, Graph);
    TArray<UK2Node_Knot*> Nodes;
    for (int32 I = 0; I < 5; ++I)
    { FGraphNodeCreator<UK2Node_Knot> C(*Graph); Nodes.Add(C.CreateNode()); C.Finalize(); }
    for (int32 I = 0; I < 2; ++I) { Nodes[I]->GetOutputPin()->MakeLinkTo(Nodes[I + 2]->GetInputPin()); }
    // An external source fan-out must survive unbundling and every undo/redo.
    Nodes[0]->GetOutputPin()->MakeLinkTo(Nodes[4]->GetInputPin());
    const TSharedRef<SGraphPanel> Panel = SNew(SGraphPanel).GraphObj(Graph);
    for (int32 I = 0; I < 4; ++I) { Panel->SelectionManager.SetNodeSelection(Nodes[I], true); }
    WireBundle::Bundle(Panel, FVector2D(300, 100));
    auto Records = WireBundle::ReadBundles(Graph);
    if (!TestEqual(TEXT("One bundle"), Records.Num(), 1)) { return false; }
    const FGuid Id = Records[0].Id;
    TestTrue(TEXT("Select whole bundle"), WireBundle::SelectBundle(Panel, Id));
    TestEqual(TEXT("Four reroutes selected"), Panel->SelectionManager.GetSelectedNodes().Num(), 4);
    TestTrue(TEXT("Unbundle succeeds"), WireBundle::Unbundle(Panel, Id));
    TestEqual(TEXT("Only original nodes remain"), Graph->Nodes.Num(), 5);
    TestEqual(TEXT("Record removed"), WireBundle::ReadBundles(Graph).Num(), 0);
    for (int32 I = 0; I < 2; ++I)
    { TestTrue(TEXT("Direct links restored"), Nodes[I]->GetOutputPin()->LinkedTo.Contains(Nodes[I + 2]->GetInputPin())); }
    TestTrue(TEXT("Source fan-out preserved"), Nodes[0]->GetOutputPin()->LinkedTo.Contains(Nodes[4]->GetInputPin()));
    GEditor->UndoTransaction();
    Records = WireBundle::ReadBundles(Graph);
    if (!TestEqual(TEXT("Undo restores record"), Records.Num(), 1)) { return false; }
    TestEqual(TEXT("Undo restores all reroutes"), Graph->Nodes.Num(), 9);
    TestTrue(TEXT("Undo restores valid topology"), WireBundle::ValidateBundle(Graph, Records[0]) == WireBundle::EBundleHealth::Valid);
    GEditor->RedoTransaction();
    TestEqual(TEXT("Redo removes reroutes"), Graph->Nodes.Num(), 5);
    TestEqual(TEXT("Redo removes record"), WireBundle::ReadBundles(Graph).Num(), 0);
    GEditor->UndoTransaction();
    Records = WireBundle::ReadBundles(Graph);
    UK2Node_Knot* Owned = CastChecked<UK2Node_Knot>(Records[0].Wires[0].BlueprintReroutes[0]);
    Owned->GetOutputPin()->MakeLinkTo(Nodes[4]->GetInputPin());
    const int32 Count = Graph->Nodes.Num();
    TestFalse(TEXT("Extra branch blocks destructive command"), WireBundle::Unbundle(Panel, Id));
    TestEqual(TEXT("No nodes removed on rejection"), Graph->Nodes.Num(), Count);
    TestEqual(TEXT("Record retained on rejection"), WireBundle::ReadBundles(Graph).Num(), 1);
    TestTrue(TEXT("Manual branch untouched"), Owned->GetOutputPin()->LinkedTo.Contains(Nodes[4]->GetInputPin()));
    Owned->GetOutputPin()->BreakLinkTo(Nodes[4]->GetInputPin());
    Graph->bEditable = false;
    TestFalse(TEXT("Read-only graph refused"), WireBundle::Unbundle(Panel, Id));
    Graph->bEditable = true;
    TestFalse(TEXT("Unknown identity refused"), WireBundle::Unbundle(Panel, FGuid::NewGuid()));
    return true;
}
#endif
