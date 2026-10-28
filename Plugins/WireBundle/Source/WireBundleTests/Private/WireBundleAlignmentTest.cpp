#if WITH_DEV_AUTOMATION_TESTS
#include "WireBundleAlignment.h"
#include "WireBundleRegistry.h"
#include "WireBundleSettings.h"
#include "Misc/AutomationTest.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Knot.h"
#include "SGraphPanel.h"
#include "Editor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireBundleAlignmentTest,"WireBundle.Alignment.Vertical",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWireBundleAlignmentTest::RunTest(const FString&)
{
    auto Settings=GetMutableDefault<UWireBundleEditorSettings>();
    TGuardValue<FWireBundleMovementSettings> Guard(Settings->Movement,FWireBundleMovementSettings());
    auto BP=FKismetEditorUtilities::CreateBlueprint(UObject::StaticClass(),GetTransientPackage(),
        MakeUniqueObjectName(GetTransientPackage(),UBlueprint::StaticClass(),TEXT("AlignTest")),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
    auto Graph=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("Test"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(BP,Graph);
    auto Panel=SNew(SGraphPanel).GraphObj(Graph);
    TArray<UK2Node_Knot*> Nodes;
    for(int32 I=0;I<4;++I)
    {
        FGraphNodeCreator<UK2Node_Knot> C(*Graph);auto N=C.CreateNode();C.Finalize();
        N->NodePosX=I*60;N->NodePosY=I*40;Nodes.Add(N);
        if(I<3){Panel->SelectionManager.SetNodeSelection(N,true);}
    }
    Nodes[0]->GetOutputPin()->MakeLinkTo(Nodes[1]->GetInputPin());
    Nodes[0]->GetOutputPin()->MakeLinkTo(Nodes[3]->GetInputPin());
    TestTrue(TEXT("Alignment available for native reroutes"),WireBundle::CanAlignVertical(Panel));
    TestTrue(TEXT("Align selected group"),WireBundle::AlignVertical(Panel));
    for(int32 I=0;I<3;++I)
    {TestEqual(TEXT("Shared average X"),Nodes[I]->NodePosX,60);TestEqual(TEXT("Original height retained"),Nodes[I]->NodePosY,I*40);}
    TestEqual(TEXT("Unselected node unaffected"),Nodes[3]->NodePosX,180);
    TestEqual(TEXT("Source branching retained"),Nodes[0]->GetOutputPin()->LinkedTo.Num(),2);
    TestEqual(TEXT("No metadata created"),WireBundle::ReadBundles(Graph).Num(),0);
    TestFalse(TEXT("Already aligned is a no-op"),WireBundle::AlignVertical(Panel));
    GEditor->UndoTransaction();
    for(int32 I=0;I<3;++I){TestEqual(TEXT("One Undo restores entire group"),Nodes[I]->NodePosX,I*60);}
    GEditor->RedoTransaction();
    TestEqual(TEXT("Redo reapplies alignment"),Nodes[2]->NodePosX,60);
    GEditor->UndoTransaction();
    Settings->Movement.bSnapToGrid=true;Settings->Movement.GridSize=16;
    TestTrue(TEXT("Grid-aligned vertical line"),WireBundle::AlignVertical(Panel));
    TestEqual(TEXT("Center follows plugin grid"),(Nodes[0]->NodePosX+21)%16,0);
    GEditor->UndoTransaction();
    Graph->bEditable=false;
    TestFalse(TEXT("Read-only graph refused"),WireBundle::AlignVertical(Panel));Graph->bEditable=true;
    Panel->SelectionManager.ClearSelectionSet();Panel->SelectionManager.SetNodeSelection(Nodes[0],true);
    TestFalse(TEXT("One reroute retains standard behavior"),WireBundle::CanAlignVertical(Panel));
    TestFalse(TEXT("Single node is not moved"),WireBundle::AlignVertical(Panel));
    return true;
}
#endif
