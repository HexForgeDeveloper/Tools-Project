#if WITH_DEV_AUTOMATION_TESTS
#include "WireBundleRegistry.h"
#include "WireBundleRoutePlanner.h"
#include "WireBundleGesture.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/LinkerInstancingContext.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "K2Node_Knot.h"
#include "SGraphPanel.h"
#include "Editor.h"
#include "MaterialEditorModule.h"
#include "MaterialEditorActions.h"
#include "IMaterialEditor.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionReroute.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraphNode_Knot.h"

namespace WireBundleTests
{
static UEdGraph* FindBundleGraph(UBlueprint* Blueprint)
{
    if (!Blueprint) { return nullptr; }
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    { if (Graph && Graph->GetFName() == TEXT("BundleGraph")) { return Graph; } }
    return nullptr;
}
static UPackage* NewPackage()
{
    return CreatePackage(*FString::Printf(TEXT("/Game/Editor/WireBundleTests/T_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
}

static UObject* SaveAndLoadFresh(UObject* Asset)
{
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("WireBundleTests");
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Filename = Directory / (FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".uasset"));
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    if (!UPackage::SavePackage(Asset->GetPackage(), Asset, *Filename, Args)) { return nullptr; }
    UPackage* Destination = NewPackage();
    FLinkerInstancingContext Context;
    Context.AddPackageMapping(Asset->GetPackage()->GetFName(), Destination->GetFName());
    UPackage* Loaded = LoadPackage(Destination, *Filename, LOAD_ForDiff, nullptr, &Context);
    return Loaded ? FindObject<UObject>(Loaded, *Asset->GetName()) : nullptr;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireBundleRegistryBlueprintTest, "WireBundle.Registry.BlueprintPersistence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWireBundleRegistryBlueprintTest::RunTest(const FString&)
{
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(UObject::StaticClass(),
        WireBundleTests::NewPackage(), TEXT("BundleBP"), BPTYPE_Normal,
        UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, TEXT("BundleGraph"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(Blueprint, Graph);
    TArray<UK2Node_Knot*> Nodes;
    for (int32 I = 0; I < 4; ++I)
    {
        FGraphNodeCreator<UK2Node_Knot> Creator(*Graph);
        Nodes.Add(Creator.CreateNode());
        Creator.Finalize();
    }
    TSharedRef<SGraphPanel> Panel = SNew(SGraphPanel).GraphObj(Graph);
    for (UK2Node_Knot* Node : Nodes) { Panel->SelectionManager.SetNodeSelection(Node, true); }
    for (int32 I = 0; I < 2; ++I) { Nodes[I]->GetOutputPin()->MakeLinkTo(Nodes[I + 2]->GetInputPin()); }
    WireBundle::Bundle(Panel, FVector2D(300, 100));
    TArray<FWireBundleRecord> Bundles = WireBundle::ReadBundles(Graph);
    if (!TestEqual(TEXT("Descriptor stored"), Bundles.Num(), 1)) { return false; }
    const FGuid BundleId = Bundles[0].Id;
    TestTrue(TEXT("New descriptor valid"), WireBundle::ValidateBundle(Graph, Bundles[0]) == WireBundle::EBundleHealth::Valid);
    WireBundle::Bundle(Panel, FVector2D(900, 100));
    TestEqual(TEXT("Repeated command recognizes bundle without inserting more nodes"), Graph->Nodes.Num(), 8);
    GEditor->UndoTransaction();
    TestEqual(TEXT("Undo removes descriptor and geometry together"), WireBundle::ReadBundles(Graph).Num(), 0);
    GEditor->RedoTransaction();
    Bundles = WireBundle::ReadBundles(Graph);
    if (!TestEqual(TEXT("Redo restores descriptor"), Bundles.Num(), 1)) { return false; }
    TestTrue(TEXT("Identity preserved by redo"), Bundles[0].Id == BundleId);
    UBlueprint* Duplicate = DuplicateObject<UBlueprint>(Blueprint, WireBundleTests::NewPackage(), TEXT("BundleBPCopy"));
    UEdGraph* DuplicatedGraph = WireBundleTests::FindBundleGraph(Duplicate);
    if (TestNotNull(TEXT("Duplicated graph present"), DuplicatedGraph))
    {
        const auto Copied = WireBundle::ReadBundles(DuplicatedGraph);
        TestEqual(TEXT("Duplicate retains its own descriptor"), Copied.Num(), 1);
        if (!Copied.IsEmpty())
        {
            const auto Health = WireBundle::ValidateBundle(DuplicatedGraph, Copied[0]);
            if (Health != WireBundle::EBundleHealth::Valid)
            {
                AddInfo(FString::Printf(TEXT("Duplicate health=%d graph=%s record=%s"), int32(Health), *DuplicatedGraph->GraphGuid.ToString(), *Copied[0].Graph.ToString()));
                for (const auto& Wire : Copied[0].Wires)
                {
                    AddInfo(FString::Printf(TEXT("Endpoints resolve source=%d target=%d"), WireBundle::ResolvePin(DuplicatedGraph, Wire.Source) != nullptr, WireBundle::ResolvePin(DuplicatedGraph, Wire.Target) != nullptr));
                }
                for (UEdGraphNode* Node : DuplicatedGraph->Nodes)
                {
                    for (UEdGraphPin* Pin : Node->Pins)
                    { AddInfo(FString::Printf(TEXT("%s node=%s pin=%s name=%s linked=%d"), *Node->GetName(), *Node->NodeGuid.ToString(), *Pin->PinId.ToString(), *Pin->PinName.ToString(), Pin->LinkedTo.Num())); }
                }
            }
            TestTrue(TEXT("Duplicated topology valid"), Health == WireBundle::EBundleHealth::Valid);
        }
    }
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    UBlueprint* Loaded = Cast<UBlueprint>(WireBundleTests::SaveAndLoadFresh(Blueprint));
    if (!TestNotNull(TEXT("Blueprint read from disk into a different package"), Loaded)) { return false; }
    TestTrue(TEXT("Fresh object, not cached original"), Loaded != Blueprint);
    UEdGraph* LoadedGraph = WireBundleTests::FindBundleGraph(Loaded);
    if (!TestNotNull(TEXT("Loaded graph available"), LoadedGraph)) { return false; }
    const auto Reloaded = WireBundle::ReadBundles(LoadedGraph);
    if (!TestEqual(TEXT("Descriptor survives disk reload"), Reloaded.Num(), 1)) { return false; }
    TestTrue(TEXT("Same bundle identity after load"), Reloaded[0].Id == BundleId);
    TestTrue(TEXT("Reloaded topology valid"), WireBundle::ValidateBundle(LoadedGraph, Reloaded[0]) == WireBundle::EBundleHealth::Valid);
    UEdGraphPin* Source = WireBundle::ResolvePin(LoadedGraph, Reloaded[0].Wires[0].Source);
    if (Source && Source->LinkedTo.Num())
    {
        Source->BreakLinkTo(Source->LinkedTo[0]);
        TestTrue(TEXT("Manual topology edits detected"), WireBundle::ValidateBundle(LoadedGraph, Reloaded[0]) == WireBundle::EBundleHealth::Changed);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireBundleRegistryMaterialTest, "WireBundle.Registry.MaterialApplyPersistence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWireBundleRegistryMaterialTest::RunTest(const FString&)
{
    UPackage* Package = WireBundleTests::NewPackage();
    UMaterial* Original = NewObject<UMaterial>(Package, TEXT("BundleMaterial"), RF_Public | RF_Standalone | RF_Transactional);
    IMaterialEditorModule& Module = FModuleManager::LoadModuleChecked<IMaterialEditorModule>("MaterialEditor");
    const TSharedRef<IMaterialEditor> Editor = Module.CreateMaterialEditor(EToolkitMode::Standalone, nullptr, Original);
    UMaterial* Preview = CastChecked<UMaterial>(Editor->GetMaterialInterface());
    UMaterialGraph* Graph = Preview->MaterialGraph;
    TArray<UMaterialGraphNode_Knot*> Nodes;
    for (int32 I = 0; I < 4; ++I)
    {
        UMaterialExpression* Expression = Editor->CreateNewMaterialExpression(UMaterialExpressionReroute::StaticClass(), FVector2D(I * 120, I * 50), false, false, Graph);
        Nodes.Add(CastChecked<UMaterialGraphNode_Knot>(Expression->GraphNode));
    }
    TArray<WireBundle::FLink> Links;
    for (int32 I = 0; I < 2; ++I)
    {
        Graph->GetSchema()->TryCreateConnection(Nodes[I]->GetOutputPin(), Nodes[I + 2]->GetInputPin());
        Links.Add({FEdGraphPinReference(Nodes[I]->GetOutputPin()), FEdGraphPinReference(Nodes[I + 2]->GetInputPin()), I * 50});
    }
    const TSharedRef<SGraphPanel> Panel = SNew(SGraphPanel).GraphObj(Graph);
    WireBundle::Bundle(Panel, FVector2D(300, 100), &Links);
    auto Records = WireBundle::ReadBundles(Graph);
    if (!TestEqual(TEXT("Preview descriptor stored"), Records.Num(), 1)) { Editor->CloseWindow(EAssetEditorCloseReason::AssetForceDeleted); return false; }
    const auto PreviewPlan = WireBundle::BuildParallelRoutePlan(Links, FVector2D(300, 100), ResolveWireBundleSettings());
    for (int32 I = 0; I < PreviewPlan.Lanes.Num(); ++I)
    {
        const auto& Lane = PreviewPlan.Lanes[I];
        const auto& Wire = Records[0].Wires[I];
        for (int32 Side = 0; Side < 2; ++Side)
        {
            UEdGraphNode* Knot = nullptr;
            for (UEdGraphNode* Node : Graph->Nodes)
            { if (WireBundle::NodeIdentity(Node) == Wire.Reroutes[Side]) { Knot = Node; break; } }
            if (!TestNotNull(TEXT("Planned material reroute exists"), Knot)) { continue; }
            const FVector2D Point = Side == 0 ? Lane.Entry : Lane.Exit;
            TestEqual(TEXT("Material preview X matches creation"), Knot->NodePosX, FMath::RoundToInt32(Point.X - 21));
            TestEqual(TEXT("Material preview Y matches creation"), Knot->NodePosY, FMath::RoundToInt32(Point.Y - 12));
        }
    }
    const FGuid BundleId = Records[0].Id;
    TestTrue(TEXT("Preview descriptor valid"), WireBundle::ValidateBundle(Graph, Records[0]) == WireBundle::EBundleHealth::Valid);
    GEditor->UndoTransaction();
    TestEqual(TEXT("Material undo removes descriptor"), WireBundle::ReadBundles(Graph).Num(), 0);
    GEditor->RedoTransaction();
    Records = WireBundle::ReadBundles(Graph);
    if (!TestEqual(TEXT("Material redo restores descriptor"), Records.Num(), 1))
    { Editor->CloseWindow(EAssetEditorCloseReason::AssetForceDeleted); return false; }
    TestTrue(TEXT("Material redo keeps bundle ID"), Records[0].Id == BundleId);
    TestTrue(TEXT("Material redo topology valid"), WireBundle::ValidateBundle(Graph, Records[0]) == WireBundle::EBundleHealth::Valid);
    TestTrue(TEXT("Native Apply command executed"), Editor->GetToolkitCommands()->ExecuteAction(FMaterialEditorCommands::Get().Apply.ToSharedRef()));
    UMaterial* Applied = FindObject<UMaterial>(Package, TEXT("BundleMaterial"));
    TestTrue(TEXT("Apply copied internal user data"), Applied && Applied->GetAssetUserDataOfClass(UWireBundleMaterialData::StaticClass()) != nullptr);
    Editor->CloseWindow(EAssetEditorCloseReason::AssetForceDeleted);
    UMaterial* Loaded = Cast<UMaterial>(WireBundleTests::SaveAndLoadFresh(Applied));
    if (!TestNotNull(TEXT("Material loaded from disk"), Loaded)) { return false; }
    TestTrue(TEXT("Fresh material object"), Loaded != Applied);
    const TSharedRef<IMaterialEditor> Reopened = Module.CreateMaterialEditor(EToolkitMode::Standalone, nullptr, Loaded);
    UMaterialGraph* Rebuilt = CastChecked<UMaterial>(Reopened->GetMaterialInterface())->MaterialGraph;
    Records = WireBundle::ReadBundles(Rebuilt);
    TestEqual(TEXT("Bundle survives Apply/save/reopen"), Records.Num(), 1);
    if (!Records.IsEmpty())
    {
        TestTrue(TEXT("Bundle identity stable"), Records[0].Id == BundleId);
        TestTrue(TEXT("Rebuilt pin identities resolve"), WireBundle::ValidateBundle(Rebuilt, Records[0]) == WireBundle::EBundleHealth::Valid);
    }
    if (!Records.IsEmpty())
    {
        TArray<FWireBundleStoredWire> OriginalWires = Records[0].Wires;
        const int32 Before = Rebuilt->Nodes.Num();
        const int32 ExpressionCount = CastChecked<UMaterial>(Reopened->GetMaterialInterface())->GetExpressions().Num();
        const auto UnbundlePanel = SNew(SGraphPanel).GraphObj(Rebuilt);
        TestTrue(TEXT("Unbundle rebuilt material"), WireBundle::Unbundle(UnbundlePanel, BundleId));
        for (const auto& Wire : OriginalWires)
        {
            UEdGraphPin* Source = WireBundle::ResolvePin(Rebuilt, Wire.Source);
            UEdGraphPin* Target = WireBundle::ResolvePin(Rebuilt, Wire.Target);
            TestTrue(TEXT("Material original direct link restored"), Source && Target && Source->LinkedTo.Contains(Target));
        }
        TestEqual(TEXT("Native reroute nodes removed"), Rebuilt->Nodes.Num(), Before - 4);
        TestEqual(TEXT("Native expression collection updated"), CastChecked<UMaterial>(Reopened->GetMaterialInterface())->GetExpressions().Num(), ExpressionCount - 4);
        TestEqual(TEXT("Material record removed"), WireBundle::ReadBundles(Rebuilt).Num(), 0);
        GEditor->UndoTransaction();
        const auto Restored = WireBundle::ReadBundles(Rebuilt);
        TestEqual(TEXT("One undo restores material record"), Restored.Num(), 1);
        TestEqual(TEXT("One undo restores native expressions"), CastChecked<UMaterial>(Reopened->GetMaterialInterface())->GetExpressions().Num(), ExpressionCount);
        if (!Restored.IsEmpty())
        { TestTrue(TEXT("Undo restores material topology"), WireBundle::ValidateBundle(Rebuilt, Restored[0]) == WireBundle::EBundleHealth::Valid); }
        GEditor->RedoTransaction();
        TestEqual(TEXT("Material redo removes record"), WireBundle::ReadBundles(Rebuilt).Num(), 0);
        TestEqual(TEXT("Material redo removes reroutes"), Rebuilt->Nodes.Num(), Before - 4);
        // Restore the saved bundle before the subsequent duplication check.
        GEditor->UndoTransaction();
    }
    Reopened->CloseWindow(EAssetEditorCloseReason::AssetForceDeleted);
    UMaterial* Duplicate = DuplicateObject<UMaterial>(Loaded, WireBundleTests::NewPackage(), TEXT("BundleMaterialCopy"));
    const TSharedRef<IMaterialEditor> CopyEditor = Module.CreateMaterialEditor(EToolkitMode::Standalone, nullptr, Duplicate);
    UMaterialGraph* CopyGraph = CastChecked<UMaterial>(CopyEditor->GetMaterialInterface())->MaterialGraph;
    const auto CopyRecords = WireBundle::ReadBundles(CopyGraph);
    TestEqual(TEXT("Material asset duplication retains records"), CopyRecords.Num(), 1);
    if (!CopyRecords.IsEmpty())
    { TestTrue(TEXT("Duplicated material resolves its own expressions"), WireBundle::ValidateBundle(CopyGraph, CopyRecords[0]) == WireBundle::EBundleHealth::Valid); }
    CopyEditor->CloseWindow(EAssetEditorCloseReason::AssetForceDeleted);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireBundleAdoptionTest, "WireBundle.Registry.AdoptLegacyChains",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWireBundleAdoptionTest::RunTest(const FString&)
{
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(UObject::StaticClass(),
        WireBundleTests::NewPackage(), TEXT("AdoptBP"), BPTYPE_Normal,
        UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, TEXT("AdoptGraph"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(Blueprint, Graph);
    TArray<UK2Node_Knot*> Nodes;
    for (int32 I = 0; I < 8; ++I)
    {
        FGraphNodeCreator<UK2Node_Knot> Creator(*Graph);
        Nodes.Add(Creator.CreateNode());
        Creator.Finalize();
        Nodes.Last()->NodePosX = I * 25;
        Nodes.Last()->NodePosY = I * 13;
    }
    for (int32 I = 0; I < 2; ++I)
    {
        Nodes[I]->GetOutputPin()->MakeLinkTo(Nodes[4 + I * 2]->GetInputPin());
        Nodes[4 + I * 2]->GetOutputPin()->MakeLinkTo(Nodes[5 + I * 2]->GetInputPin());
        Nodes[5 + I * 2]->GetOutputPin()->MakeLinkTo(Nodes[I + 2]->GetInputPin());
    }
    const TSharedRef<SGraphPanel> Panel = SNew(SGraphPanel).GraphObj(Graph);
    for (int32 I = 4; I < 8; ++I) { Panel->SelectionManager.SetNodeSelection(Nodes[I], true); }
    TestTrue(TEXT("Legacy chains can be explicitly adopted"), WireBundle::Adopt(Panel));
    TestEqual(TEXT("Adoption does not insert nodes"), Graph->Nodes.Num(), 8);
    TestEqual(TEXT("One stored bundle"), WireBundle::ReadBundles(Graph).Num(), 1);
    for (int32 I = 0; I < 8; ++I)
    {
        TestEqual(TEXT("Adoption preserves X"), Nodes[I]->NodePosX, I * 25);
        TestEqual(TEXT("Adoption preserves Y"), Nodes[I]->NodePosY, I * 13);
    }
    GEditor->UndoTransaction();
    TestEqual(TEXT("Undo unregisters ownership"), WireBundle::ReadBundles(Graph).Num(), 0);
    TestEqual(TEXT("Undo adoption preserves native nodes"), Graph->Nodes.Num(), 8);
    GEditor->RedoTransaction();
    const auto Records = WireBundle::ReadBundles(Graph);
    if (!TestEqual(TEXT("Redo restores ownership"), Records.Num(), 1)) { return false; }
    TestTrue(TEXT("Adopted topology valid"), WireBundle::ValidateBundle(Graph, Records[0]) == WireBundle::EBundleHealth::Valid);
    GEditor->UndoTransaction();
    Nodes[4]->GetOutputPin()->MakeLinkTo(Nodes[3]->GetInputPin());
    TArray<UEdGraphNode*> Selected = {Nodes[4], Nodes[5], Nodes[6], Nodes[7]};
    FWireBundleRecord Rejected;
    FText Reason;
    TestFalse(TEXT("Extra outgoing connection prevents adoption"), WireBundle::BuildAdoptionRecord(Graph, Selected, Rejected, Reason));
    TestEqual(TEXT("Failed validation never registers a bundle"), WireBundle::ReadBundles(Graph).Num(), 0);
    TestEqual(TEXT("Validation preserves extra connection"), Nodes[4]->GetOutputPin()->LinkedTo.Num(), 2);
    return true;
}
#endif
