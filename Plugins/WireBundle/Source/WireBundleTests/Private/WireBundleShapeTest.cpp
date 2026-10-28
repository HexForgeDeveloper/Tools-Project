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
#include "Editor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireBundleShapePlanTest, "WireBundle.Shape.Planning",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWireBundleShapePlanTest::RunTest(const FString&)
{
    WireBundle::FShapeSnapshot S;
    S.Positions = {FVector2D(-21,-22),FVector2D(139,-22),FVector2D(-21,-2),FVector2D(139,-2)};
    S.Left=0; S.Right=160; S.Center=FVector2D(80,0); S.Spacing=20;
    FWireBundleMovementSettings Config;
    auto P = WireBundle::PlanShapeEdit(S, WireBundle::EShapeHandle::Move, FVector2D(10,25), Config);
    TestTrue(TEXT("Common translation preserves all offsets"), P[0].Equals(S.Positions[0]+FVector2D(10,25)) && P[3].Equals(S.Positions[3]+FVector2D(10,25)));
    Config.MouseAxis=EWireBundleMovementAxis::Horizontal;
    P=WireBundle::PlanShapeEdit(S,WireBundle::EShapeHandle::Move,FVector2D(10,25),Config);
    TestEqual(TEXT("Mouse axis constraint"),P[0].Y,S.Positions[0].Y);
    Config.MouseAxis=EWireBundleMovementAxis::Free; Config.bSnapToGrid=true; Config.GridSize=16;
    P=WireBundle::PlanShapeEdit(S,WireBundle::EShapeHandle::Move,FVector2D(10,25),Config);
    TestTrue(TEXT("Snap common center without compressing lanes"),P[0].Equals(S.Positions[0]+FVector2D(16,32)) && P[2].Y-P[0].Y==20);
    TestTrue(TEXT("Clicking handle never snaps or reshapes"),WireBundle::PlanShapeEdit(S,WireBundle::EShapeHandle::Entry,FVector2D::ZeroVector,Config)==S.Positions);
    Config.bSnapToGrid=false;
    P=WireBundle::PlanShapeEdit(S,WireBundle::EShapeHandle::Exit,FVector2D(80,99),Config);
    TestTrue(TEXT("Only exit column changes width"),P[0]==S.Positions[0] && P[1]==S.Positions[1]+FVector2D(80,0));
    P=WireBundle::PlanShapeEdit(S,WireBundle::EShapeHandle::Entry,FVector2D(999,0),Config);
    TestEqual(TEXT("Cannot invert columns"),P[1].X-P[0].X,16.0);
    P=WireBundle::PlanShapeEdit(S,WireBundle::EShapeHandle::Spacing,FVector2D(0,10),Config);
    TestEqual(TEXT("Spacing expands symmetrically"),P[2].Y-P[0].Y,40.0);
    TestEqual(TEXT("Spacing preserves center"),(P[0].Y+P[2].Y)*0.5,-12.0);
    TestEqual(TEXT("Preview never modifies baseline"),S.Positions[0].Y,-22.0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireBundleShapeCommitTest, "WireBundle.Shape.BlueprintUndoAndSafety",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWireBundleShapeCommitTest::RunTest(const FString&)
{
    UBlueprint* BP=FKismetEditorUtilities::CreateBlueprint(UObject::StaticClass(),GetTransientPackage(),
        MakeUniqueObjectName(GetTransientPackage(),UBlueprint::StaticClass(),TEXT("ShapeTest")),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
    UEdGraph* Graph=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("Test"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(BP,Graph);
    TArray<UK2Node_Knot*> Nodes;
    for(int32 I=0;I<4;++I) {FGraphNodeCreator<UK2Node_Knot> C(*Graph);Nodes.Add(C.CreateNode());C.Finalize();}
    const auto Panel=SNew(SGraphPanel).GraphObj(Graph);
    for(auto Node:Nodes){Panel->SelectionManager.SetNodeSelection(Node,true);}
    for(int32 I=0;I<2;++I){Nodes[I]->GetOutputPin()->MakeLinkTo(Nodes[I+2]->GetInputPin());}
    WireBundle::Bundle(Panel,FVector2D(300,100));
    const auto Records=WireBundle::ReadBundles(Graph);
    if(!TestEqual(TEXT("Bundle exists"),Records.Num(),1)){return false;}
    WireBundle::FShapeSnapshot S;
    if(!TestTrue(TEXT("Capture healthy two-column bundle"),WireBundle::CaptureShape(Graph,Records[0].Id,S))){return false;}
    Panel->SelectionManager.ClearSelectionSet(); Panel->SelectionManager.SetNodeSelection(S.Nodes[0].Get(),true);
    WireBundle::FShapeSnapshot Selection;
    TestFalse(TEXT("Single registered reroute must not expand into handles"),WireBundle::CaptureSelectedShape(Panel,Selection));
    Panel->SelectionManager.SetNodeSelection(S.Nodes[1].Get(),true);
    TestTrue(TEXT("Two selected reroutes enable handles"),WireBundle::CaptureSelectedShape(Panel,Selection));
    TestEqual(TEXT("Exactly two nodes captured"),Selection.Nodes.Num(),2);
    for (auto N:S.Nodes) { Panel->SelectionManager.SetNodeSelection(N.Get(),true); }
    const FWireBundleMovementSettings Config;
    TestFalse(TEXT("No motion creates no transaction"),WireBundle::CommitShapeEdit(Panel,S,WireBundle::EShapeHandle::Move,FVector2D::ZeroVector,Config));
    TestTrue(TEXT("Commit entire shape"),WireBundle::CommitShapeEdit(Panel,S,WireBundle::EShapeHandle::Move,FVector2D(32,48),Config));
    TestEqual(TEXT("No topology changes"),Graph->Nodes.Num(),8);
    TestTrue(TEXT("Registry remains healthy"),WireBundle::ValidateBundle(Graph,Records[0])==WireBundle::EBundleHealth::Valid);
    for(int32 I=0;I<S.Nodes.Num();++I)
    {TestEqual(TEXT("Every node moves"),S.Nodes[I]->NodePosX,int32(S.Positions[I].X+32));}
    GEditor->UndoTransaction();
    for(int32 I=0;I<S.Nodes.Num();++I){TestEqual(TEXT("Single undo restores all nodes"),S.Nodes[I]->NodePosX,int32(S.Positions[I].X));}
    GEditor->RedoTransaction();
    TestEqual(TEXT("Redo restores shape"),S.Nodes[0]->NodePosY,int32(S.Positions[0].Y+48));
    GEditor->UndoTransaction();
    S.Nodes[0]->NodePosX+=1;
    TestFalse(TEXT("Concurrent position change refuses stale drag"),WireBundle::CommitShapeEdit(Panel,S,WireBundle::EShapeHandle::Exit,FVector2D(64,0),Config));
    S.Nodes[0]->NodePosX-=1;
    TestTrue(TEXT("Width commit"),WireBundle::CommitShapeEdit(Panel,S,WireBundle::EShapeHandle::Exit,FVector2D(64,0),Config));
    TestEqual(TEXT("Entry column preserved"),S.Nodes[0]->NodePosX,int32(S.Positions[0].X));
    TestEqual(TEXT("Exit column widened"),S.Nodes[1]->NodePosX,int32(S.Positions[1].X+64));
    GEditor->UndoTransaction();
    Graph->bEditable=false;
    TestFalse(TEXT("Read-only graph refused"),WireBundle::CommitShapeEdit(Panel,S,WireBundle::EShapeHandle::Move,FVector2D(8,0),Config));
    Graph->bEditable=true;
    S.Nodes[0]->Pins[1]->BreakAllPinLinks();
    TestFalse(TEXT("Broken topology refused"),WireBundle::CommitShapeEdit(Panel,S,WireBundle::EShapeHandle::Move,FVector2D(8,0),Config));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireBundleSingleColumnTest, "WireBundle.Blueprint.SingleColumnLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWireBundleSingleColumnTest::RunTest(const FString&)
{
    UBlueprint* BP=FKismetEditorUtilities::CreateBlueprint(UObject::StaticClass(),GetTransientPackage(),
        MakeUniqueObjectName(GetTransientPackage(),UBlueprint::StaticClass(),TEXT("SingleColumnTest")),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
    UEdGraph* Graph=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("Test"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(BP,Graph);
    TArray<UK2Node_Knot*> Nodes;
    for(int32 I=0;I<4;++I) {FGraphNodeCreator<UK2Node_Knot> C(*Graph);Nodes.Add(C.CreateNode());C.Finalize();}
    const auto Panel=SNew(SGraphPanel).GraphObj(Graph);
    TArray<WireBundle::FLink> Links;
    for(int32 I=0;I<2;++I)
    {
        Nodes[I]->GetOutputPin()->MakeLinkTo(Nodes[I+2]->GetInputPin());
        Links.Add({FEdGraphPinReference(Nodes[I]->GetOutputPin()),FEdGraphPinReference(Nodes[I+2]->GetInputPin()),I*100});
    }
    auto Config=ResolveWireBundleSettings(); Config.Movement.RerouteColumns=1;
    WireBundle::Bundle(Panel,FVector2D(300,100),&Links,&Config);
    TestEqual(TEXT("Single native reroute per wire"),Graph->Nodes.Num(),6);
    auto Records=WireBundle::ReadBundles(Graph);
    if(!TestEqual(TEXT("Single-column descriptor stored"),Records.Num(),1)){return false;}
    for(const auto& Wire:Records[0].Wires){TestEqual(TEXT("Exactly one owned knot"),Wire.Reroutes.Num(),1);}
    const FGuid Id=Records[0].Id;
    GEditor->UndoTransaction(); TestEqual(TEXT("Creation undo"),Graph->Nodes.Num(),4);
    GEditor->RedoTransaction(); TestEqual(TEXT("Creation redo"),Graph->Nodes.Num(),6);
    WireBundle::FShapeSnapshot Shape;
    if(!TestTrue(TEXT("Capture compact shape"),WireBundle::CaptureShape(Graph,Id,Shape))){return false;}
    TestEqual(TEXT("Single-column shape"),Shape.Columns,1);
    TestTrue(TEXT("Single-column move"),WireBundle::CommitShapeEdit(Panel,Shape,WireBundle::EShapeHandle::Move,FVector2D(32,16),Config.Movement));
    GEditor->UndoTransaction();
    TestTrue(TEXT("Single-column spacing edit"),WireBundle::CommitShapeEdit(Panel,Shape,WireBundle::EShapeHandle::Spacing,FVector2D(0,12),Config.Movement));
    GEditor->UndoTransaction();
    TestFalse(TEXT("Compact bundle has no width handles"),WireBundle::CommitShapeEdit(Panel,Shape,WireBundle::EShapeHandle::Exit,FVector2D(32,0),Config.Movement));
    TestTrue(TEXT("Single-column unbundle"),WireBundle::Unbundle(Panel,Id));
    TestEqual(TEXT("Original nodes retained"),Graph->Nodes.Num(),4);
    for(int32 I=0;I<2;++I){TestTrue(TEXT("Original direct connection restored"),Nodes[I]->GetOutputPin()->LinkedTo.Contains(Nodes[I+2]->GetInputPin()));}
    GEditor->UndoTransaction();
    Records=WireBundle::ReadBundles(Graph);
    TestTrue(TEXT("Unbundle undo restores compact topology"),Records.Num()==1 && WireBundle::ValidateBundle(Graph,Records[0])==WireBundle::EBundleHealth::Valid);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireBundleMultiColumnTest, "WireBundle.Shape.MultipleRowsAndColumns",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWireBundleMultiColumnTest::RunTest(const FString&)
{
    for (const int32 Count : {3,5,8})
    {
        UBlueprint* BP=FKismetEditorUtilities::CreateBlueprint(UObject::StaticClass(),GetTransientPackage(),
            MakeUniqueObjectName(GetTransientPackage(),UBlueprint::StaticClass(),TEXT("MultiColumnTest")),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
        UEdGraph* Graph=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("Test"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
        FBlueprintEditorUtils::AddUbergraphPage(BP,Graph);
        const auto Panel=SNew(SGraphPanel).GraphObj(Graph);
        TArray<WireBundle::FLink> Links;
        for (int32 Row=0; Row<Count; ++Row)
        {
            FGraphNodeCreator<UK2Node_Knot> A(*Graph); auto Source=A.CreateNode(); A.Finalize();
            FGraphNodeCreator<UK2Node_Knot> B(*Graph); auto Target=B.CreateNode(); B.Finalize();
            Source->GetOutputPin()->MakeLinkTo(Target->GetInputPin());
            Links.Add({FEdGraphPinReference(Source->GetOutputPin()),FEdGraphPinReference(Target->GetInputPin()),Row*100});
        }
        auto Config=ResolveWireBundleSettings(); Config.Movement.RerouteColumns=Count;
        WireBundle::Bundle(Panel,FVector2D(400,200),&Links,&Config);
        const auto Records=WireBundle::ReadBundles(Graph);
        if (!TestEqual(TEXT("Multiple-column bundle stored"),Records.Num(),1)) { return false; }
        TestEqual(TEXT("Exact node count"),Graph->Nodes.Num(),Count*(Count+2));
        WireBundle::FShapeSnapshot Shape;
        if (!TestTrue(TEXT("Capture all rows and columns"),WireBundle::CaptureShape(Graph,Records[0].Id,Shape))) { return false; }
        TestEqual(TEXT("All wires captured"),Shape.NumRows(),Count);
        TestEqual(TEXT("All columns captured"),Shape.Columns,Count);
        const auto Plan=WireBundle::PlanShapeEdit(Shape,WireBundle::EShapeHandle::Exit,FVector2D(160,0),Config.Movement);
        TestTrue(TEXT("Intermediate node resizes proportionally"),FMath::IsNearlyEqual(Plan[1].X-Shape.Positions[1].X, FMath::RoundToDouble(160.0/(Count-1))));
        TestTrue(TEXT("Resize multi-column shape"),WireBundle::CommitShapeEdit(Panel,Shape,WireBundle::EShapeHandle::Exit,FVector2D(160,0),Config.Movement));
        TestTrue(TEXT("Connections survive resize"),WireBundle::ValidateBundle(Graph,Records[0])==WireBundle::EBundleHealth::Valid);
        GEditor->UndoTransaction();
        TestEqual(TEXT("Undo restores intermediate node"),Shape.Nodes[1]->NodePosX,int32(Shape.Positions[1].X));
        TestTrue(TEXT("Change multi-wire spacing"),WireBundle::CommitShapeEdit(Panel,Shape,WireBundle::EShapeHandle::Spacing,FVector2D(0,40),Config.Movement));
        GEditor->UndoTransaction();
        TestTrue(TEXT("Unbundle all columns"),WireBundle::Unbundle(Panel,Records[0].Id));
        TestEqual(TEXT("Only endpoints remain"),Graph->Nodes.Num(),Count*2);
        for (const auto& Link:Links) { TestTrue(TEXT("Original direct link restored"),Link.Output.Get()->LinkedTo.Contains(Link.Input.Get())); }
        GEditor->UndoTransaction();
        TestTrue(TEXT("Undo restores long topology"),WireBundle::ValidateBundle(Graph,Records[0])==WireBundle::EBundleHealth::Valid);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireBundleMixedChainsTest, "WireBundle.Shape.MixedLegacyChainLengths",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWireBundleMixedChainsTest::RunTest(const FString&)
{
    UBlueprint* BP=FKismetEditorUtilities::CreateBlueprint(UObject::StaticClass(),GetTransientPackage(),
        MakeUniqueObjectName(GetTransientPackage(),UBlueprint::StaticClass(),TEXT("MixedChainTest")),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
    UEdGraph* Graph=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("Test"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(BP,Graph);
    const auto Panel=SNew(SGraphPanel).GraphObj(Graph);
    auto NewKnot=[&]() { FGraphNodeCreator<UK2Node_Knot> C(*Graph); auto N=C.CreateNode(); C.Finalize(); return N; };
    int32 Row=0;
    for (int32 Count:{1,3,5})
    {
        auto Source=NewKnot(), Target=NewKnot(); UEdGraphPin* Previous=Source->GetOutputPin();
        for (int32 I=0;I<Count;++I)
        {
            auto Knot=NewKnot(); Knot->NodePosX=I*60; Knot->NodePosY=Row*100+I*4;
            Previous->MakeLinkTo(Knot->GetInputPin()); Previous=Knot->GetOutputPin();
            Panel->SelectionManager.SetNodeSelection(Knot,true);
        }
        Previous->MakeLinkTo(Target->GetInputPin()); ++Row;
    }
    TestTrue(TEXT("Register unequal legacy chains"),WireBundle::Adopt(Panel));
    const auto Records=WireBundle::ReadBundles(Graph);
    if (!TestEqual(TEXT("One registered bundle"),Records.Num(),1)) { return false; }
    WireBundle::FShapeSnapshot Shape;
    if (!TestTrue(TEXT("Capture mixed chain lengths"),WireBundle::CaptureShape(Graph,Records[0].Id,Shape))) { return false; }
    TestEqual(TEXT("Three row ranges"),Shape.NumRows(),3);
    TestEqual(TEXT("All nine reroutes captured"),Shape.Nodes.Num(),9);
    TestEqual(TEXT("First row has one node"),Shape.RowEnd(0)-Shape.RowBegin(0),1);
    TestEqual(TEXT("Last row has five nodes"),Shape.RowEnd(2)-Shape.RowBegin(2),5);
    const FWireBundleMovementSettings Settings;
    TestTrue(TEXT("Resize mixed lengths without flattening"),WireBundle::CommitShapeEdit(Panel,Shape,WireBundle::EShapeHandle::Exit,FVector2D(120,0),Settings));
    TestEqual(TEXT("No nodes inserted or deleted"),Graph->Nodes.Num(),15);
    TestTrue(TEXT("All mixed connections preserved"),WireBundle::ValidateBundle(Graph,Records[0])==WireBundle::EBundleHealth::Valid);
    GEditor->UndoTransaction();
    TestTrue(TEXT("Change spacing with ragged rows"),WireBundle::CommitShapeEdit(Panel,Shape,WireBundle::EShapeHandle::Spacing,FVector2D(0,20),Settings));
    TestEqual(TEXT("Internal slope retained"),Shape.Nodes[2]->NodePosY-Shape.Nodes[1]->NodePosY,int32(Shape.Positions[2].Y-Shape.Positions[1].Y));
    GEditor->UndoTransaction();
    TestEqual(TEXT("Undo restores all positions"),Shape.Nodes.Last()->NodePosY,int32(Shape.Positions.Last().Y));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireBundleNativeSelectionTest, "WireBundle.Shape.UnregisteredNativeSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWireBundleNativeSelectionTest::RunTest(const FString&)
{
    UBlueprint* BP=FKismetEditorUtilities::CreateBlueprint(UObject::StaticClass(),GetTransientPackage(),
        MakeUniqueObjectName(GetTransientPackage(),UBlueprint::StaticClass(),TEXT("NativeShapeTest")),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
    UEdGraph* Graph=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("Test"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(BP,Graph);
    const auto Panel=SNew(SGraphPanel).GraphObj(Graph);
    TArray<UK2Node_Knot*> Nodes;
    for (int32 I=0;I<4;++I)
    {
        FGraphNodeCreator<UK2Node_Knot> C(*Graph); auto N=C.CreateNode(); C.Finalize();
        N->NodePosX=I*100; N->NodePosY=I*60; Nodes.Add(N); Panel->SelectionManager.SetNodeSelection(N,true);
    }
    WireBundle::FShapeSnapshot Shape;
    if (!TestTrue(TEXT("Native disconnected reroutes need no registration"),WireBundle::CaptureSelectedShape(Panel,Shape))) { return false; }
    TestTrue(TEXT("Temporary selection mode"),Shape.bSelectionOnly);
    TestEqual(TEXT("All four independent rows"),Shape.NumRows(),4);
    const FWireBundleMovementSettings Settings;
    TestTrue(TEXT("Move manually created reroutes"),WireBundle::CommitShapeEdit(Panel,Shape,WireBundle::EShapeHandle::Move,FVector2D(32,16),Settings));
    TestEqual(TEXT("No persistent metadata created"),WireBundle::ReadBundles(Graph).Num(),0);
    for (int32 I=0;I<4;++I) { TestEqual(TEXT("All selected nodes moved"),Nodes[I]->NodePosX,I*100+32); }
    GEditor->UndoTransaction();
    for (int32 I=0;I<4;++I) { TestEqual(TEXT("Single undo restores original nodes"),Nodes[I]->NodePosX,I*100); }
    TestTrue(TEXT("Native selection spacing"),WireBundle::CommitShapeEdit(Panel,Shape,WireBundle::EShapeHandle::Spacing,FVector2D(0,30),Settings));
    GEditor->UndoTransaction();
    Nodes[0]->GetOutputPin()->MakeLinkTo(Nodes[1]->GetInputPin());
    if (!TestTrue(TEXT("Connected native chains supported"),WireBundle::CaptureSelectedShape(Panel,Shape))) { return false; }
    TestEqual(TEXT("Connected pair is one row"),Shape.NumRows(),3);
    TestTrue(TEXT("Native width edit"),WireBundle::CommitShapeEdit(Panel,Shape,WireBundle::EShapeHandle::Exit,FVector2D(120,0),Settings));
    TestTrue(TEXT("External native links preserved"),Nodes[0]->GetOutputPin()->LinkedTo.Contains(Nodes[1]->GetInputPin()));
    GEditor->UndoTransaction();
    Nodes[0]->GetOutputPin()->MakeLinkTo(Nodes[2]->GetInputPin());
    TestFalse(TEXT("Topology changed during drag is refused"),WireBundle::CommitShapeEdit(Panel,Shape,WireBundle::EShapeHandle::Move,FVector2D(32,0),Settings));
    TestEqual(TEXT("Refusal leaves positions intact"),Nodes[0]->NodePosX,0);
    Panel->SelectionManager.ClearSelectionSet(); Panel->SelectionManager.SetNodeSelection(Nodes[0],true);
    TestFalse(TEXT("Single native reroute uses standard Unreal movement"),WireBundle::CaptureSelectedShape(Panel,Shape));
    TestEqual(TEXT("Single selection is not moved by plugin"),Nodes[0]->NodePosX,0);
    TestEqual(TEXT("Unselected node stays still"),Nodes[1]->NodePosX,100);
    TestEqual(TEXT("All connections remain"),Nodes[0]->GetOutputPin()->LinkedTo.Num(),2);
    TestEqual(TEXT("No bundle registration at any stage"),WireBundle::ReadBundles(Graph).Num(),0);
    return true;
}
#endif
