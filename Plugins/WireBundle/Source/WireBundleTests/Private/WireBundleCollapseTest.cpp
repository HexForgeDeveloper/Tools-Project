#if WITH_DEV_AUTOMATION_TESTS
#include "WireBundleCollapse.h"
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
#include "MaterialEditorModule.h"
#include "IMaterialEditor.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionReroute.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraphNode_Knot.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireBundleCollapseBlueprintTest,"WireBundle.Collapse.BlueprintChains",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWireBundleCollapseBlueprintTest::RunTest(const FString&)
{
    TGuardValue<FWireBundleMovementSettings> Guard(GetMutableDefault<UWireBundleEditorSettings>()->Movement,FWireBundleMovementSettings());
    for (int32 Count:{2,3,4})
    {
        auto BP=FKismetEditorUtilities::CreateBlueprint(UObject::StaticClass(),GetTransientPackage(),
            MakeUniqueObjectName(GetTransientPackage(),UBlueprint::StaticClass(),TEXT("CollapseTest")),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
        auto Graph=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("Test"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
        FBlueprintEditorUtils::AddUbergraphPage(BP,Graph);auto Panel=SNew(SGraphPanel).GraphObj(Graph);
        auto New=[&]() { FGraphNodeCreator<UK2Node_Knot> C(*Graph);auto N=C.CreateNode();C.Finalize();return N; };
        auto Source=New(),Target=New(),Branch=New();TArray<UK2Node_Knot*> Chain;
        auto Previous=Source->GetOutputPin();
        for(int32 I=0;I<Count;++I)
        {
            auto N=New();N->NodePosX=I*100;N->NodePosY=I*20;Chain.Add(N);
            Previous->MakeLinkTo(N->GetInputPin());Previous=N->GetOutputPin();Panel->SelectionManager.SetNodeSelection(N,true);
        }
        Previous->MakeLinkTo(Target->GetInputPin());Chain[0]->GetOutputPin()->MakeLinkTo(Branch->GetInputPin());
        TestTrue(TEXT("Linear native chain can collapse"),WireBundle::CanCollapseReroutes(Panel));
        TestTrue(TEXT("Collapse native chain"),WireBundle::CollapseReroutes(Panel));
        TestEqual(TEXT("One reroute plus original endpoints"),Graph->Nodes.Num(),4);
        TestTrue(TEXT("Input preserved"),Source->GetOutputPin()->LinkedTo.Contains(Chain[0]->GetInputPin()));
        TestTrue(TEXT("Original destination preserved"),Chain[0]->GetOutputPin()->LinkedTo.Contains(Target->GetInputPin()));
        TestTrue(TEXT("Output branch preserved"),Chain[0]->GetOutputPin()->LinkedTo.Contains(Branch->GetInputPin()));
        TestEqual(TEXT("Survivor centered horizontally"),Chain[0]->NodePosX,(Count-1)*50);
        TestEqual(TEXT("Survivor centered vertically"),Chain[0]->NodePosY,(Count-1)*10);
        TestEqual(TEXT("Only survivor selected"),Panel->SelectionManager.GetSelectedNodes().Num(),1);
        TestEqual(TEXT("No registration created"),WireBundle::ReadBundles(Graph).Num(),0);
        GEditor->UndoTransaction();
        TestEqual(TEXT("One undo restores every reroute"),Graph->Nodes.Num(),Count+3);
        for(int32 I=0;I<Count-1;++I){TestTrue(TEXT("Original internal links restored"),Chain[I]->GetOutputPin()->LinkedTo.Contains(Chain[I+1]->GetInputPin()));}
        GEditor->RedoTransaction();TestEqual(TEXT("Redo collapses again"),Graph->Nodes.Num(),4);
        GEditor->UndoTransaction();
        Panel->SelectionManager.ClearSelectionSet();for(auto N:Chain){Panel->SelectionManager.SetNodeSelection(N,true);}
        Chain.Last()->GetOutputPin()->MakeLinkTo(Branch->GetInputPin());
        TestFalse(TEXT("Distinct paths to the same target are not silently merged"),WireBundle::CollapseReroutes(Panel));
        Chain.Last()->GetOutputPin()->BreakLinkTo(Branch->GetInputPin());
        Panel->SelectionManager.ClearSelectionSet();Panel->SelectionManager.SetNodeSelection(Source,true);Panel->SelectionManager.SetNodeSelection(Branch,true);
        TestFalse(TEXT("Independent nodes cannot merge signals"),WireBundle::CollapseReroutes(Panel));
        TestEqual(TEXT("Rejected collapse leaves nodes intact"),Graph->Nodes.Num(),Count+3);
        Panel->SelectionManager.ClearSelectionSet();for(auto N:Chain){Panel->SelectionManager.SetNodeSelection(N,true);}
        Graph->bEditable=false;TestFalse(TEXT("Read-only graph refused"),WireBundle::CollapseReroutes(Panel));Graph->bEditable=true;
        // A cycle has no root and must be refused without modifying links.
        Source->GetOutputPin()->BreakLinkTo(Chain[0]->GetInputPin());Chain.Last()->GetOutputPin()->MakeLinkTo(Chain[0]->GetInputPin());
        TestFalse(TEXT("Cycle refused"),WireBundle::CollapseReroutes(Panel));
    }
    // Collapsing one wire in a registered bundle preserves the record and Unbundle.
    auto BP=FKismetEditorUtilities::CreateBlueprint(UObject::StaticClass(),GetTransientPackage(),
        MakeUniqueObjectName(GetTransientPackage(),UBlueprint::StaticClass(),TEXT("RegisteredCollapse")),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
    auto Graph=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("Test"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());FBlueprintEditorUtils::AddUbergraphPage(BP,Graph);
    auto Panel=SNew(SGraphPanel).GraphObj(Graph);TArray<WireBundle::FLink> Links;
    for(int32 I=0;I<2;++I)
    {
        FGraphNodeCreator<UK2Node_Knot> A(*Graph);auto S=A.CreateNode();A.Finalize();
        FGraphNodeCreator<UK2Node_Knot> B(*Graph);auto T=B.CreateNode();B.Finalize();S->GetOutputPin()->MakeLinkTo(T->GetInputPin());
        Links.Add({FEdGraphPinReference(S->GetOutputPin()),FEdGraphPinReference(T->GetInputPin()),I*100});
    }
    auto Config=ResolveWireBundleSettings();Config.Movement.RerouteColumns=3;
    WireBundle::Bundle(Panel,FVector2D(300,100),&Links,&Config);
    auto Records=WireBundle::ReadBundles(Graph);if(!TestEqual(TEXT("Bundle recorded"),Records.Num(),1)){return false;}
    const auto Id=Records[0].Id;Panel->SelectionManager.ClearSelectionSet();
    for(auto N:Records[0].Wires[0].BlueprintReroutes){Panel->SelectionManager.SetNodeSelection(N,true);}
    TestTrue(TEXT("Collapse registered chain"),WireBundle::CollapseReroutes(Panel));
    Records=WireBundle::ReadBundles(Graph);if(!TestEqual(TEXT("Registration retained"),Records.Num(),1)){return false;}
    TestTrue(TEXT("Bundle ID unchanged"),Records[0].Id==Id);
    TestEqual(TEXT("Selected wire has one owned node"),Records[0].Wires[0].Reroutes.Num(),1);
    TestEqual(TEXT("Unselected wire unchanged"),Records[0].Wires[1].Reroutes.Num(),3);
    TestTrue(TEXT("Shortened record valid"),WireBundle::ValidateBundle(Graph,Records[0])==WireBundle::EBundleHealth::Valid);
    GEditor->UndoTransaction();Records=WireBundle::ReadBundles(Graph);TestEqual(TEXT("Undo restores recorded chain"),Records[0].Wires[0].Reroutes.Num(),3);
    GEditor->RedoTransaction();TestTrue(TEXT("Unbundle shortened bundle"),WireBundle::Unbundle(Panel,Id));
    TestEqual(TEXT("Unbundle leaves original endpoints"),Graph->Nodes.Num(),4);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireBundleCollapseMaterialTest,"WireBundle.Collapse.MaterialChains",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWireBundleCollapseMaterialTest::RunTest(const FString&)
{
    auto& Module=FModuleManager::LoadModuleChecked<IMaterialEditorModule>("MaterialEditor");
    for(int32 Count:{2,3,4})
    {
        auto Original=NewObject<UMaterial>(GetTransientPackage(),MakeUniqueObjectName(GetTransientPackage(),UMaterial::StaticClass(),TEXT("CollapseMaterial")),RF_Transactional);
        auto Editor=Module.CreateMaterialEditor(EToolkitMode::Standalone,nullptr,Original);
        auto Material=CastChecked<UMaterial>(Editor->GetMaterialInterface());auto Graph=Material->MaterialGraph;
        auto New=[&](int32 X)
        {
            auto E=Editor->CreateNewMaterialExpression(UMaterialExpressionReroute::StaticClass(),FVector2D(X,0),false,false,Graph);
            return CastChecked<UMaterialGraphNode_Knot>(E->GraphNode);
        };
        auto Source=New(-100),Target=New(600),Branch=New(600);TArray<UMaterialGraphNode_Knot*> Chain;
        auto Previous=Source->GetOutputPin();auto Panel=SNew(SGraphPanel).GraphObj(Graph);
        for(int32 I=0;I<Count;++I)
        {
            auto N=New(I*100);Chain.Add(N);Graph->GetSchema()->TryCreateConnection(Previous,N->GetInputPin());
            Previous=N->GetOutputPin();Panel->SelectionManager.SetNodeSelection(N,true);
        }
        Graph->GetSchema()->TryCreateConnection(Previous,Target->GetInputPin());
        Graph->GetSchema()->TryCreateConnection(Chain[0]->GetOutputPin(),Branch->GetInputPin());
        const int32 Nodes=Graph->Nodes.Num(),Expressions=Material->GetExpressions().Num();
        TestTrue(TEXT("Collapse material chain"),WireBundle::CollapseReroutes(Panel));
        TestEqual(TEXT("Native graph nodes removed"),Graph->Nodes.Num(),Nodes-Count+1);
        TestEqual(TEXT("Native expressions removed"),Material->GetExpressions().Num(),Expressions-Count+1);
        TestTrue(TEXT("Material input preserved"),Source->GetOutputPin()->LinkedTo.Contains(Chain[0]->GetInputPin()));
        TestTrue(TEXT("Material destination preserved"),Chain[0]->GetOutputPin()->LinkedTo.Contains(Target->GetInputPin()));
        TestTrue(TEXT("Material branch preserved"),Chain[0]->GetOutputPin()->LinkedTo.Contains(Branch->GetInputPin()));
        TestEqual(TEXT("Survivor expression position synced"),Chain[0]->MaterialExpression->MaterialExpressionEditorX,Chain[0]->NodePosX);
        GEditor->UndoTransaction();TestEqual(TEXT("Material undo restores nodes"),Graph->Nodes.Num(),Nodes);TestEqual(TEXT("Material undo restores expressions"),Material->GetExpressions().Num(),Expressions);
        GEditor->RedoTransaction();TestEqual(TEXT("Material redo collapses nodes"),Graph->Nodes.Num(),Nodes-Count+1);
        Editor->CloseWindow(EAssetEditorCloseReason::AssetForceDeleted);
    }
    return true;
}
#endif
