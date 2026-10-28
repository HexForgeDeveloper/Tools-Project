#if WITH_DEV_AUTOMATION_TESTS
#include "WireBundleShape.h"
#include "WireBundleGesture.h"
#include "Misc/AutomationTest.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Knot.h"
#include "SGraphPanel.h"
#include "SGraphNodeKnot.h"
#include "Layout/WidgetPath.h"
#include "Editor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireBundleSelectionIsolationTest,"WireBundle.Shape.RegisteredSelectionIsolation",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWireBundleSelectionIsolationTest::RunTest(const FString&)
{
    auto BP=FKismetEditorUtilities::CreateBlueprint(UObject::StaticClass(),GetTransientPackage(),
        MakeUniqueObjectName(GetTransientPackage(),UBlueprint::StaticClass(),TEXT("SelectionTest")),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
    auto Graph=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("Test"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());FBlueprintEditorUtils::AddUbergraphPage(BP,Graph);
    auto Panel=SNew(SGraphPanel).GraphObj(Graph);TArray<WireBundle::FLink> Links;
    for(int32 I=0;I<4;++I)
    {
        FGraphNodeCreator<UK2Node_Knot> A(*Graph);auto S=A.CreateNode();A.Finalize();
        FGraphNodeCreator<UK2Node_Knot> B(*Graph);auto T=B.CreateNode();B.Finalize();S->GetOutputPin()->MakeLinkTo(T->GetInputPin());
        Links.Add({FEdGraphPinReference(S->GetOutputPin()),FEdGraphPinReference(T->GetInputPin()),I*100});
    }
    auto Config=ResolveWireBundleSettings();Config.Movement=FWireBundleMovementSettings();Config.Movement.RerouteColumns=1;
    WireBundle::Bundle(Panel,FVector2D(300,100),&Links,&Config);
    auto Records=WireBundle::ReadBundles(Graph);if(!TestEqual(TEXT("Bundle exists"),Records.Num(),1)){return false;}
    TArray<UEdGraphNode*> Owned;
    for(int32 I=0;I<4;++I)
    {
        auto Node=Records[0].Wires[I].BlueprintReroutes[0].Get();Owned.Add(Node);
        Node->NodePosX+=I*20; // Give width and spacing handles meaningful independent tests.
    }
    const auto Geometry=FGeometry::MakeRoot(FVector2D(42,24),FSlateLayoutTransform(1.0f));
    for (auto Node:Owned)
    {
        Panel->SelectionManager.ClearSelectionSet();Panel->SelectionManager.SetNodeSelection(Node,true);
        WireBundle::FShapeSnapshot Single;
        TestFalse(TEXT("Top, middle and bottom single selections show no group handles"),WireBundle::CaptureSelectedShape(Panel,Single));
        TestEqual(TEXT("Capturing never selects extra nodes"),Panel->SelectionManager.GetSelectedNodes().Num(),1);
        FWidgetPath Path;Path.Widgets.AddWidget(FArrangedWidget(Panel,Geometry));
        TestFalse(TEXT("Empty graph canvas permits handles"),WireBundle::ShouldDeferShapePointerToGraph(Path));
        const auto Widget=SNew(SGraphNodeKnot,Node);
        Path.Widgets.AddWidget(FArrangedWidget(Widget,Geometry));
        TestTrue(TEXT("Native reroute clicks bypass overlapping handles"),WireBundle::ShouldDeferShapePointerToGraph(Path));
    }
    Panel->SelectionManager.ClearSelectionSet();
    for(int32 I=0;I<3;++I){Panel->SelectionManager.SetNodeSelection(Owned[I],true);}
    WireBundle::FShapeSnapshot Shape;
    if(!TestTrue(TEXT("Capture actual three-node selection"),WireBundle::CaptureSelectedShape(Panel,Shape))){return false;}
    TestEqual(TEXT("Owning bundle does not expand snapshot"),Shape.Nodes.Num(),3);
    TestTrue(TEXT("Preview uses explicit selection mode"),Shape.bSelectionOnly);
    const FVector2D Fourth(Owned[3]->NodePosX,Owned[3]->NodePosY);
    for(const auto Handle:{WireBundle::EShapeHandle::Move,WireBundle::EShapeHandle::Entry,WireBundle::EShapeHandle::Exit,WireBundle::EShapeHandle::Spacing})
    {
        TestTrue(TEXT("Edit selected subset"),WireBundle::CommitShapeEdit(Panel,Shape,Handle,FVector2D(32,24),Config.Movement));
        TestTrue(TEXT("Unselected fourth node never moves"),Fourth.Equals(FVector2D(Owned[3]->NodePosX,Owned[3]->NodePosY)));
        TestEqual(TEXT("Editing never changes selection count"),Panel->SelectionManager.GetSelectedNodes().Num(),3);
        GEditor->UndoTransaction();
        for(int32 I=0;I<Shape.Nodes.Num();++I){TestTrue(TEXT("Undo restores selected subset"),Shape.Positions[I].Equals(FVector2D(Shape.Nodes[I]->NodePosX,Shape.Nodes[I]->NodePosY)));}
    }
    Panel->SelectionManager.ClearSelectionSet();
    for(int32 I=1;I<4;++I){Panel->SelectionManager.SetNodeSelection(Owned[I],true);}
    TestFalse(TEXT("Same-size replacement selection invalidates stale handles"),WireBundle::IsShapeSelectionCurrent(Panel,Shape));
    TestFalse(TEXT("Stale drag cannot move old selection"),WireBundle::CommitShapeEdit(Panel,Shape,WireBundle::EShapeHandle::Move,FVector2D(32,24),Config.Movement));
    Panel->SelectionManager.ClearSelectionSet();Panel->SelectionManager.SetNodeSelection(Owned[0],true);
    TestFalse(TEXT("Single selection invalidates cached overlay"),WireBundle::IsShapeSelectionCurrent(Panel,Shape));
    WireBundle::FShapeSnapshot Full;
    TestTrue(TEXT("Bundle can still be inspected explicitly"),WireBundle::CaptureShape(Graph,Records[0].Id,Full));
    TestFalse(TEXT("Full-bundle snapshot cannot bypass actual selection"),WireBundle::CommitShapeEdit(Panel,Full,WireBundle::EShapeHandle::Move,FVector2D(32,24),Config.Movement));
    TestTrue(TEXT("Select entire bundle remains an explicit command"),WireBundle::SelectBundle(Panel,Records[0].Id));
    TestEqual(TEXT("Explicit command selects four reroutes"),Panel->SelectionManager.GetSelectedNodes().Num(),4);
    TestTrue(TEXT("Fresh explicit full selection is valid"),WireBundle::IsShapeSelectionCurrent(Panel,Full));
    return true;
}
#endif
