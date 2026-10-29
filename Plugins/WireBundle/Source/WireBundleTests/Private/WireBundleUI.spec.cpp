#if WITH_DEV_AUTOMATION_TESTS

#include "Async/Async.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "IAutomationDriverModule.h"
#include "IAutomationDriver.h"
#include "IDriverSequence.h"
#include "LocateBy.h"
#include "DriverConfiguration.h"
#include "WireBundleGesture.h"
#include "WireBundleRegistry.h"
#include "WireBundleShape.h"
#include "WireBundleShapeToolbar.h"
#include "GraphEditor.h"
#include "SGraphPanel.h"
#include "SGraphNode.h"
#include "EdGraphUtilities.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Commands/UICommandList.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Editor.h"
#include "EditorUndoClient.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Knot.h"
#include "K2Node_CallFunction.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/MaterialExpressionAdd.h"
#include "WireBundleRoutePlanner.h"
#include "MaterialEditorModule.h"
#include "IMaterialEditor.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionReroute.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraphNode_Knot.h"
#include "UObject/GCObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ImageUtils.h"

namespace WireBundleUI
{
// Test-only sizing gives real native graph nodes repeatable staggered bounds.
class SRoutingWall final : public SGraphNode
{
public:
    SLATE_BEGIN_ARGS(SRoutingWall) {} SLATE_END_ARGS()
    void Construct(const FArguments&, UEdGraphNode* Node)
    {
        GraphNode=Node;
        GetOrAddSlot(ENodeZone::Center)[SNew(SBorder).BorderBackgroundColor(FLinearColor(0.2f,0.2f,0.2f))
            [SNew(SBox).WidthOverride(Node->NodeWidth).HeightOverride(Node->NodeHeight)]];
    }
    virtual void UpdateGraphNode() override {}
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(GraphNode->NodeWidth,GraphNode->NodeHeight); }
};
struct FRoutingWallFactory final : FGraphPanelNodeFactory
{
    virtual TSharedPtr<SGraphNode> CreateNode(UEdGraphNode* Node) const override
    { return Node->NodeComment==TEXT("WireBundle routing test wall") ? SNew(SRoutingWall,Node) : TSharedPtr<SGraphNode>(); }
};

// Driver sequences block their calling thread; all UObject/Slate inspection stays
// on the game thread while the spec body runs on a worker, as in Epic's driver spec.
template<typename F> auto OnGameThread(F&& Work)
{
    return Async(EAsyncExecution::TaskGraphMainThread, Forward<F>(Work)).Get();
}

struct FFixture : public FGCObject, public FEditorUndoClient
{
    TObjectPtr<UObject> Asset = nullptr;
    virtual void AddReferencedObjects(FReferenceCollector& Collector) override { Collector.AddReferencedObject(Asset); }
    virtual FString GetReferencerName() const override { return TEXT("WireBundleUIFixture"); }
    TSharedPtr<IMaterialEditor> MaterialEditor;
    UEdGraph* Graph = nullptr;
    TArray<UEdGraphNode*> Nodes;
    TSharedPtr<SGraphEditor> Editor;
    TSharedPtr<SWindow> Window;
    TSharedPtr<SWidget> Outside;
    TSharedPtr<FGraphPanelNodeFactory> RoutingWallFactory;
    FWireBundleResolvedSettings SavedSettings;
    TWeakPtr<SWidget> PreviousFocus;
    TWeakPtr<SWindow> PreviousWindow;
    bool bAllowedTooltips = true;

    // A standalone SGraphEditor does not receive a Blueprint editor's undo
    // refresh callbacks. Rebuild its pin widgets after restoring transactions.
    virtual void PostUndo(bool bSuccess) override { if (bSuccess && Editor) { Editor->NotifyGraphChanged(); } }
    virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }
    SGraphPanel& Panel() const { return *Editor->GetGraphPanel(); }
    static UEdGraphPin* Pin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
    {
        for (auto P : Node->Pins) { if (P->Direction == Direction) { return P; } }
        return nullptr;
    }
    void Create(bool bMaterial)
    {
        auto& App = FSlateApplication::Get();
        PreviousFocus = App.GetKeyboardFocusedWidget();
        PreviousWindow = App.GetActiveTopLevelWindow();
        bAllowedTooltips = App.GetAllowTooltips();
        App.SetAllowTooltips(false);
        App.CloseToolTip();
        auto Settings = GetMutableDefault<UWireBundleEditorSettings>();
        SavedSettings.Movement = Settings->Movement; SavedSettings.Gesture = Settings->Gesture;
        SavedSettings.Appearance = Settings->Appearance;
        SavedSettings.bEnableBlueprint = Settings->bEnableBlueprint; SavedSettings.bEnableMaterial = Settings->bEnableMaterial;
        Settings->Movement = FWireBundleMovementSettings();
        Settings->Gesture = FWireBundleGestureSettings();
        Settings->Appearance = FWireBundleAppearanceSettings();
        Settings->Appearance.bShowNotifications = false;
        Settings->bEnableBlueprint = Settings->bEnableMaterial = true;

        if (bMaterial)
        {
            auto Original = NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transactional);
            Asset = Original;
            MaterialEditor = FModuleManager::LoadModuleChecked<IMaterialEditorModule>("MaterialEditor")
                .CreateMaterialEditor(EToolkitMode::Standalone, nullptr, Original);
            Graph = CastChecked<UMaterial>(MaterialEditor->GetMaterialInterface())->MaterialGraph;
        }
        else
        {
            auto BP = FKismetEditorUtilities::CreateBlueprint(UObject::StaticClass(), GetTransientPackage(),
                MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("WireBundleUI")),
                BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
            Asset = BP;
            Graph = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("UI"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
            FBlueprintEditorUtils::AddUbergraphPage(BP, Graph);
        }
        // Two independent chains, with endpoints outside the four editable reroutes.
        for (int32 Row = 0; Row < 2; ++Row)
        {
            for (int32 X : {40, 280, 560, 800})
            {
                UEdGraphNode* Node;
                if (bMaterial)
                {
                    Node = MaterialEditor->CreateNewMaterialExpression(UMaterialExpressionReroute::StaticClass(),
                        FVector2D(X, 180 + Row * 200), false, false, Graph)->GraphNode;
                }
                else
                {
                    FGraphNodeCreator<UK2Node_Knot> Creator(*Graph);
                    Node = Creator.CreateNode(); Creator.Finalize();
                }
                Node->NodePosX = X; Node->NodePosY = 180 + Row * 200;
                Nodes.Add(Node);
            }
            for (int32 I = Row * 4; I < Row * 4 + 3; ++I)
            {
                Graph->GetSchema()->TryCreateConnection(Pin(Nodes[I], EGPD_Output), Pin(Nodes[I+1], EGPD_Input));
            }
        }
        auto Commands = MakeShared<FUICommandList>();
        Commands->MapAction(FGenericCommands::Get().Undo, FExecuteAction::CreateLambda([] { GEditor->UndoTransaction(); }));
        Commands->MapAction(FGenericCommands::Get().Redo, FExecuteAction::CreateLambda([] { GEditor->RedoTransaction(); }));
        Editor = SNew(SGraphEditor).GraphToEdit(Graph).AdditionalCommands(Commands);
        GEditor->RegisterForUndo(this);
        Outside = SNew(SBorder).Padding(0);
        Window = SNew(SWindow).Title(FText::FromString(TEXT("WireBundle UI automation")))
            .ClientSize(FVector2D(1000, 700)).ScreenPosition(FVector2D(80, 80)).AutoCenter(EAutoCenter::None)
            [ SNew(SVerticalBox)
                + SVerticalBox::Slot().FillHeight(1)[Editor.ToSharedRef()]
                + SVerticalBox::Slot().AutoHeight().MaxHeight(60)[SNew(SBox).HeightOverride(60)[Outside.ToSharedRef()]] ];
        App.AddWindow(Window.ToSharedRef());
        Window->BringToFront(true);
        Editor->SetViewLocation(FVector2D::ZeroVector, 1.0f);
        App.SetKeyboardFocus(Editor);
        Select({1,2,5,6});
        // Establish registered chains as fixture data, not as the action under test.
        WireBundle::Adopt(StaticCastSharedRef<SGraphPanel>(Panel().AsShared()));
        Select({});
        GEditor->ResetTransaction(FText::FromString(TEXT("WireBundle UI fixture ready")));
    }
    void Select(std::initializer_list<int32> Indices)
    {
        Panel().SelectionManager.ClearSelectionSet();
        for (int32 I : Indices) { Panel().SelectionManager.SetNodeSelection(Nodes[I], true); }
    }
    TArray<FVector2D> Positions() const
    {
        TArray<FVector2D> Result;
        for (auto Node : Nodes) { Result.Emplace(Node->NodePosX, Node->NodePosY); }
        return Result;
    }
    bool OriginalLinksIntact() const
    {
        for (int32 Row = 0; Row < 2; ++Row)
        { for (int32 I = Row*4; I < Row*4+3; ++I)
            { if (!Pin(Nodes[I], EGPD_Output)->LinkedTo.Contains(Pin(Nodes[I+1], EGPD_Input))) { return false; } } }
        return true;
    }
    // Offsets are relative to the driver element's desktop center, including DPI and zoom.
    FIntPoint Offset(FVector2D GraphPoint, FVector2D DesktopAdjustment = FVector2D::ZeroVector) const
    {
        const auto& G = Panel().GetCachedGeometry();
        const auto P = G.LocalToAbsolute((GraphPoint - Panel().GetViewOffset()) * Panel().GetZoomAmount()) + DesktopAdjustment;
        const auto& WindowGeometry = Window->GetCachedGeometry();
        const auto Center = WindowGeometry.LocalToAbsolute(WindowGeometry.GetLocalSize() * 0.5);
        return FIntPoint(FMath::RoundToInt(P.X-Center.X), FMath::RoundToInt(P.Y-Center.Y));
    }
    void Destroy()
    {
        GEditor->UnregisterForUndo(this);
        GEditor->ResetTransaction(FText::FromString(TEXT("WireBundle UI fixture cleanup")));
        auto Settings = GetMutableDefault<UWireBundleEditorSettings>();
        Settings->Movement = SavedSettings.Movement; Settings->Gesture = SavedSettings.Gesture;
        Settings->Appearance = SavedSettings.Appearance;
        Settings->bEnableBlueprint = SavedSettings.bEnableBlueprint; Settings->bEnableMaterial = SavedSettings.bEnableMaterial;
        if (Window) { Window->RequestDestroyWindow(); }
        if (MaterialEditor) { MaterialEditor->CloseWindow(EAssetEditorCloseReason::AssetForceDeleted); }
        if (auto W = PreviousWindow.Pin()) { W->BringToFront(true); }
        if (auto Focus = PreviousFocus.Pin()) { FSlateApplication::Get().SetKeyboardFocus(Focus); }
        FSlateApplication::Get().SetAllowTooltips(bAllowedTooltips);
        if(RoutingWallFactory) { FEdGraphUtilities::UnregisterVisualNodeFactory(RoutingWallFactory); RoutingWallFactory.Reset(); }
        Editor.Reset(); Window.Reset(); MaterialEditor.Reset(); Nodes.Reset(); Graph = nullptr; Asset = nullptr;
    }
};
}

BEGIN_DEFINE_SPEC(FWireBundleUISpec, "WireBundle.UI", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    TUniquePtr<WireBundleUI::FFixture> Fixture;
    FAutomationDriverPtr Driver;
    TSharedPtr<IElementLocator, ESPMode::ThreadSafe> GraphLocator;
    TSharedPtr<IElementLocator, ESPMode::ThreadSafe> PointerLocator;
    void Move(IActionSequence& Actions, FVector2D Point, FVector2D Adjustment = FVector2D::ZeroVector);
    void MoveToolbar(IActionSequence& Actions, int32 Handle);
    bool Perform(TFunctionRef<void(IActionSequence&)> Build);
    void CheckUnchanged(const TArray<FVector2D>& Before);
END_DEFINE_SPEC(FWireBundleUISpec)

void FWireBundleUISpec::Move(IActionSequence& Actions, FVector2D Point, FVector2D Adjustment)
{
    const auto Offset = WireBundleUI::OnGameThread([&] { return Fixture->Offset(Point, Adjustment); });
    Actions.MoveToElement(PointerLocator.ToSharedRef(), Offset.X, Offset.Y).Wait(FTimespan::FromMilliseconds(100));
}
void FWireBundleUISpec::MoveToolbar(IActionSequence& Actions, int32 Handle)
{
    const auto Offset = WireBundleUI::OnGameThread([&]
    {
        auto& Panel=Fixture->Panel();
        WireBundle::FShapeSnapshot Shape;
        WireBundle::CaptureSelectedShape(StaticCastSharedRef<SGraphPanel>(Panel.AsShared()),Shape);
        double Left=DBL_MAX,Right=-DBL_MAX,Top=DBL_MAX;
        for(const auto& P:Shape.Positions) { Left=FMath::Min(Left,P.X+21); Right=FMath::Max(Right,P.X+21); Top=FMath::Min(Top,P.Y+12); }
        const auto& G=Panel.GetCachedGeometry();
        const auto Anchor=G.LocalToAbsolute((FVector2D((Left+Right)*0.5,Top)-Panel.GetViewOffset())*Panel.GetZoomAmount());
        const auto H=WireBundle::ShapeToolbarHandles(Anchor,G.GetLayoutBoundingRect(),ResolveWireBundleSettings().Appearance.ShapeHandleRadius)[Handle];
        const auto Center=G.LocalToAbsolute(G.GetLocalSize()*0.5);
        return FIntPoint(FMath::RoundToInt(H.X-Center.X),FMath::RoundToInt(H.Y-Center.Y));
    });
    Actions.MoveToElement(GraphLocator.ToSharedRef(),Offset.X,Offset.Y).Wait(FTimespan::FromMilliseconds(100));
}
bool FWireBundleUISpec::Perform(TFunctionRef<void(IActionSequence&)> Build)
{
    auto Sequence = Driver->CreateSequence(); Build(Sequence->Actions());
    const bool bSuccess = Sequence->Perform();
    if (!bSuccess)
    {
        WireBundleUI::OnGameThread([&]
        {
            TArray<FColor> Pixels; FIntVector Size;
            if (FSlateApplication::Get().TakeScreenshot(Fixture->Window.ToSharedRef(), Pixels, Size))
            {
                TArray64<uint8> Png;
                FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
                FFileHelper::SaveArrayToFile(Png, *(FPaths::ProjectSavedDir()/TEXT("WireBundleUIFailure.png")));
            }
        });
    }
    return TestTrue(TEXT("Automation Driver input completed"), bSuccess);
}
void FWireBundleUISpec::CheckUnchanged(const TArray<FVector2D>& Before)
{
    WireBundleUI::OnGameThread([&]
    {
        TestTrue(TEXT("Cancelled input leaves every node unchanged"), Before == Fixture->Positions());
        TestTrue(TEXT("Connections remain intact"), Fixture->OriginalLinksIntact());
    });
}

void FWireBundleUISpec::Define()
{
    for (bool bMaterial : {false, true})
    {
        Describe(bMaterial ? TEXT("Material") : TEXT("Blueprint"), [this, bMaterial]
        {
            BeforeEach([this, bMaterial]
            {
                Fixture = MakeUnique<WireBundleUI::FFixture>(); Fixture->Create(bMaterial);
                IAutomationDriverModule::Get().Enable();
                auto Configuration = MakeShared<FDriverConfiguration, ESPMode::ThreadSafe>();
                Configuration->ImplicitWait = FTimespan::FromSeconds(5);
                Driver = IAutomationDriverModule::Get().CreateDriver(Configuration);
                GraphLocator = By::WidgetLambda([this](TArray<TSharedRef<SWidget>>& Widgets)
                { if (Fixture && Fixture->Editor) { Widgets.Add(Fixture->Panel().AsShared()); } });
                PointerLocator = By::WidgetLambda([this](TArray<TSharedRef<SWidget>>& Widgets)
                { if (Fixture && Fixture->Window) { Widgets.Add(Fixture->Window.ToSharedRef()); } });
            });
            BeforeEach(EAsyncExecution::ThreadPool, [this]
            {
                Perform([](IActionSequence& A) { A.Wait(FTimespan::FromMilliseconds(400)); });
                WireBundleUI::OnGameThread([&]
                {
                    Fixture->Editor->SetViewLocation(FVector2D::ZeroVector, 1.0f);
                    Fixture->Window->BringToFront(true);
                    FSlateApplication::Get().ProcessApplicationActivationEvent(true);
                    TestTrue(TEXT("Graph has a laid-out viewport"), Fixture->Panel().GetCachedGeometry().GetLocalSize().X > 800);
                });
                Perform([](IActionSequence& A) { A.Wait(FTimespan::FromMilliseconds(150)); });
            });
            AfterEach(EAsyncExecution::ThreadPool, [this]
            {
                Perform([](IActionSequence& A) { A.Release(EMouseButtons::Left).Release(EKeys::LeftAlt).Release(EKeys::LeftControl).Release(EKeys::LeftShift); });
                WireBundleUI::OnGameThread([&]
                {
                    Driver.Reset(); GraphLocator.Reset(); PointerLocator.Reset(); IAutomationDriverModule::Get().Disable();
                    FSlateApplication::Get().ProcessApplicationActivationEvent(true);
                    Fixture->Destroy(); Fixture.Reset();
                });
            });

            It("SingleNodeDragKeepsNeighbours", EAsyncExecution::ThreadPool, [this]
            {
                const auto Before = WireBundleUI::OnGameThread([&] { Fixture->Select({1}); return Fixture->Positions(); });
                const auto Offset=WireBundleUI::OnGameThread([&]
                {
                    const auto Node=Fixture->Panel().GetNodeWidgetFromGuid(Fixture->Nodes[1]->NodeGuid);
                    const auto& G=Fixture->Panel().GetCachedGeometry();
                    const auto P=Node->GetCachedGeometry().LocalToAbsolute(FVector2D(8,8));
                    const auto D=P-G.LocalToAbsolute(G.GetLocalSize()*0.5);
                    return FIntPoint(FMath::RoundToInt(D.X),FMath::RoundToInt(D.Y));
                });
                Perform([&](IActionSequence& A)
                {
                    A.MoveToElement(GraphLocator.ToSharedRef(),Offset.X,Offset.Y).Wait(FTimespan::FromMilliseconds(100));
                });
                // Automation Driver checks the center of the hovered spacer for
                // visibility, but that center is covered by the knot's pin. Send
                // the native button event at the actual grab point instead.
                WireBundleUI::OnGameThread([&] { FSlateApplication::Get().OnMouseDown(Fixture->Window->GetNativeWindow(),EMouseButtons::Left); });
                Perform([](IActionSequence& A) { A.MoveByOffset(8,8).MoveByOffset(40,24).Wait(FTimespan::FromMilliseconds(150)); });
                WireBundleUI::OnGameThread([] { FSlateApplication::Get().OnMouseUp(EMouseButtons::Left); });
                WireBundleUI::OnGameThread([&]
                {
                    const auto After = Fixture->Positions();
                    TestFalse(TEXT("Native reroute actually moved"), After[1] == Before[1]);
                    for (int32 I=0; I<Before.Num(); ++I) { if(I!=1) { TestTrue(TEXT("Unselected neighbour stays still"), After[I]==Before[I]); } }
                    TestEqual(TEXT("Single selection stays single"), Fixture->Panel().SelectionManager.GetSelectedNodes().Num(), 1);
                    TestTrue(TEXT("Native drag keeps links"), Fixture->OriginalLinksIntact());
                });
            });

            for (int32 Handle : {0,1,2,3})
            {
                const TCHAR* Names[] = {TEXT("MoveHandleUndoRedo"),TEXT("EntryWidthUndoRedo"),TEXT("ExitWidthUndoRedo"),TEXT("SpacingUndoRedo")};
                It(Names[Handle], EAsyncExecution::ThreadPool, [this, Handle]
                {
                    const auto Before = WireBundleUI::OnGameThread([&] { Fixture->Select({1,2,5}); return Fixture->Positions(); });
                    // Two selected chains, with the fourth registered reroute deliberately excluded.
                    Perform([&](IActionSequence& A)
                    { MoveToolbar(A,Handle); A.Press(EMouseButtons::Left).MoveByOffset(48,32).Wait(FTimespan::FromMilliseconds(150)).Release(EMouseButtons::Left); });
                    const auto After = WireBundleUI::OnGameThread([&]
                    {
                        const auto P=Fixture->Positions();
                        TestTrue(TEXT("Handle edit changes selected nodes"),P!=Before);
                        for(int32 I:{0,3,4,6,7}) { TestTrue(TEXT("Unselected nodes unchanged"),P[I]==Before[I]); }
                        TestEqual(TEXT("Selection does not expand"),Fixture->Panel().SelectionManager.GetSelectedNodes().Num(),3);
                        TestTrue(TEXT("Handle preserves topology"),Fixture->OriginalLinksIntact());
                        return P;
                    });
                    if (After == Before) { return; }
                    Perform([&](IActionSequence& A) { A.Focus(GraphLocator.ToSharedRef()).TypeChord(EKeys::LeftControl,EKeys::Z); });
                    CheckUnchanged(Before);
                    Perform([&](IActionSequence& A) { A.TypeChord(EKeys::LeftControl,EKeys::Y); });
                    WireBundleUI::OnGameThread([&] { TestTrue(TEXT("Redo restores exact edit"),Fixture->Positions()==After); });
                });
            }

            for (int32 CancelMode : {0,1,2})
            {
                const TCHAR* Names[] = {TEXT("EscapeCancelsDrag"),TEXT("ReleaseOutsideCancelsDrag"),TEXT("ApplicationDeactivationCancelsDrag")};
                It(Names[CancelMode], EAsyncExecution::ThreadPool, [this, CancelMode]
                {
                    const auto Before=WireBundleUI::OnGameThread([&] { Fixture->Select({1,2,5}); return Fixture->Positions(); });
                    Perform([&](IActionSequence& A) { MoveToolbar(A,0); A.Press(EMouseButtons::Left).MoveByOffset(48,32).Wait(FTimespan::FromMilliseconds(150)); });
                    CheckUnchanged(Before); // Dragging is preview-only until release.
                    if(CancelMode==2)
                    {
                        WireBundleUI::OnGameThread([] { FSlateApplication::Get().ProcessApplicationActivationEvent(false); });
                        Perform([](IActionSequence& A) { A.Wait(FTimespan::FromMilliseconds(200)); });
                        WireBundleUI::OnGameThread([] { FSlateApplication::Get().ProcessApplicationActivationEvent(true); });
                    }
                    Perform([&](IActionSequence& A)
                    {
                        if(CancelMode==0) { A.Type(EKeys::Escape); }
                        if(CancelMode==1) { A.MoveToElement(By::WidgetLambda([this](TArray<TSharedRef<SWidget>>& W) { W.Add(Fixture->Outside.ToSharedRef()); })); }
                        A.Release(EMouseButtons::Left);
                    });
                    CheckUnchanged(Before);
                });
            }

            It("ObstacleRoutingPreviewCommitUndoRedo", EAsyncExecution::ThreadPool, [this,bMaterial]
            {
                WireBundleUI::OnGameThread([&]
                {
                    for(const auto& R:WireBundle::ReadBundles(Fixture->Graph)) { WireBundle::RemoveBundleRecord(Fixture->Graph,R.Id); }
                    for(int32 I:{2,6}) { Fixture->Nodes[I]->NodePosX=920; }
                    for(int32 I:{3,7}) { Fixture->Nodes[I]->NodePosX=1200; }
                    UEdGraphNode* Obstacle;
                    if(bMaterial)
                    { Obstacle=Fixture->MaterialEditor->CreateNewMaterialExpression(UMaterialExpressionAdd::StaticClass(),FVector2D(500,220),false,false,Fixture->Graph)->GraphNode; }
                    else
                    {
                        FGraphNodeCreator<UK2Node_CallFunction> Creator(*Fixture->Graph);
                        auto Call=Creator.CreateNode();
                        Call->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString")));
                        Creator.Finalize(); Obstacle=Call;
                    }
                    Obstacle->NodePosX=500; Obstacle->NodePosY=220;
                    GetMutableDefault<UWireBundleEditorSettings>()->Movement.bAvoidNodes=true;
                    Fixture->Graph->NotifyGraphChanged();
                    GEditor->ResetTransaction(FText::FromString(TEXT("WireBundle obstacle fixture ready")));
                });
                Perform([](IActionSequence& A) { A.Wait(FTimespan::FromMilliseconds(500)); });
                const auto Expected=WireBundleUI::OnGameThread([&]
                {
                    TArray<WireBundle::FLink> Links;
                    for(int32 I:{1,5}) { Links.Add({FEdGraphPinReference(WireBundleUI::FFixture::Pin(Fixture->Nodes[I],EGPD_Output)),
                        FEdGraphPinReference(WireBundleUI::FFixture::Pin(Fixture->Nodes[I+1],EGPD_Input)),Fixture->Nodes[I]->NodePosY}); }
                    const auto P=WireBundle::BuildGraphRoutePlan(StaticCastSharedRef<SGraphPanel>(Fixture->Panel().AsShared()),Links,FVector2D(600,275),ResolveWireBundleSettings());
                    TestTrue(TEXT("Preview finds a corridor around a real node"),P.bAvoidedObstacles && !P.bBlocked);
                    return P;
                });
                if(Expected.bBlocked) { return; }
                const auto Before=WireBundleUI::OnGameThread([&] { return Fixture->Graph->Nodes.Num(); });
                auto Gesture=[&]
                {
                    Perform([&](IActionSequence& A)
                    {
                        Move(A,{600,120});
                        A.Press(EKeys::LeftControl).Press(EKeys::LeftAlt).Press(EMouseButtons::Left)
                            .MoveByOffset(0,310).Wait(FTimespan::FromMilliseconds(150))
                            .Release(EMouseButtons::Left).Release(EKeys::LeftAlt).Release(EKeys::LeftControl);
                    });
                };
                Gesture();
                WireBundleUI::OnGameThread([&]
                {
                    TestEqual(TEXT("Avoidance preserves two columns per wire"),Fixture->Graph->Nodes.Num(),Before+4);
                    const auto Records=WireBundle::ReadBundles(Fixture->Graph);
                    TestEqual(TEXT("Routed bundle is stored"),Records.Num(),1);
                    if(Records.Num()!=1) { return; }
                    TestTrue(TEXT("Routed links are healthy"),WireBundle::ValidateBundle(Fixture->Graph,Records[0])==WireBundle::EBundleHealth::Valid);
                    for(const auto& Lane:Expected.Lanes)
                    {
                        const auto W=Records[0].Wires.FindByPredicate([&](const FWireBundleStoredWire& Stored) { return WireBundle::ResolvePin(Fixture->Graph,Stored.Source)==Lane.Link.Output.Get(); });
                        if(!TestNotNull(TEXT("Preview lane was committed"),W)) { continue; }
                        TestEqual(TEXT("Preview column count matches commit"),W->Reroutes.Num(),Lane.Reroutes.Num());
                        for(int32 I=0;I<W->Reroutes.Num() && I<Lane.Reroutes.Num();++I)
                        {
                            const auto N=Fixture->Graph->Nodes.FindByPredicate([&](const UEdGraphNode* Node) { return WireBundle::NodeIdentity(Node)==W->Reroutes[I]; });
                            if(TestTrue(TEXT("Committed knot exists"),N!=nullptr))
                            { TestTrue(TEXT("Committed knot matches preview"),FVector2D((*N)->NodePosX+21,(*N)->NodePosY+12).Equals(Lane.Reroutes[I],0.51)); }
                        }
                    }
                });
                Perform([&](IActionSequence& A) { A.Focus(GraphLocator.ToSharedRef()).TypeChord(EKeys::LeftControl,EKeys::Z); });
                WireBundleUI::OnGameThread([&]
                {
                    TestEqual(TEXT("Undo removes detoured knots"),Fixture->Graph->Nodes.Num(),Before);
                    TestTrue(TEXT("Undo restores all original links"),Fixture->OriginalLinksIntact());
                    GetMutableDefault<UWireBundleEditorSettings>()->Movement.MaxRouteDetour=32;
                });
                Gesture();
                WireBundleUI::OnGameThread([&]
                {
                    TestEqual(TEXT("Blocked routing creates no nodes"),Fixture->Graph->Nodes.Num(),Before);
                    TestTrue(TEXT("Blocked routing preserves original links"),Fixture->OriginalLinksIntact());
                    TestTrue(TEXT("Blocked routing stores no metadata"),WireBundle::ReadBundles(Fixture->Graph).IsEmpty());
                });
                Perform([&](IActionSequence& A) { A.Focus(GraphLocator.ToSharedRef()).TypeChord(EKeys::LeftControl,EKeys::Y); });
                WireBundleUI::OnGameThread([&] { TestEqual(TEXT("Blocked gesture preserves redo history"),Fixture->Graph->Nodes.Num(),Before+4); });
            });

            It("MultiBendRoutingUndoRedo", EAsyncExecution::ThreadPool, [this,bMaterial]
            {
                WireBundleUI::OnGameThread([&]
                {
                    for(const auto& R:WireBundle::ReadBundles(Fixture->Graph)) { WireBundle::RemoveBundleRecord(Fixture->Graph,R.Id); }
                    for(int32 I:{2,6}) { Fixture->Nodes[I]->NodePosX=920; }
                    for(int32 I:{3,7}) { Fixture->Nodes[I]->NodePosX=1200; }
                    Fixture->RoutingWallFactory=MakeShared<WireBundleUI::FRoutingWallFactory>();
                    FEdGraphUtilities::RegisterVisualNodeFactory(Fixture->RoutingWallFactory);
                    for(int32 I=0;I<2;++I)
                    {
                        UEdGraphNode* Wall;
                        if(bMaterial)
                        { Wall=Fixture->MaterialEditor->CreateNewMaterialExpression(UMaterialExpressionAdd::StaticClass(),FVector2D::ZeroVector,false,false,Fixture->Graph)->GraphNode; }
                        else
                        {
                            FGraphNodeCreator<UK2Node_CallFunction> Creator(*Fixture->Graph);
                            auto Call=Creator.CreateNode(); Call->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString")));
                            Creator.Finalize(); Wall=Call;
                        }
                        Wall->NodePosX=I==0?420:740; Wall->NodePosY=I==0?-600:225;
                        Wall->NodeWidth=80; Wall->NodeHeight=925;
                        Wall->NodeComment=TEXT("WireBundle routing test wall");
                    }
                    auto& M=GetMutableDefault<UWireBundleEditorSettings>()->Movement;
                    M.bAvoidNodes=true; M.bAllowExtraBends=true; M.MaxRouteDetour=200; M.BundleWidth=520;
                    Fixture->Graph->NotifyGraphChanged();
                    GEditor->ResetTransaction(FText::FromString(TEXT("WireBundle multi-bend fixture")));
                });
                Perform([](IActionSequence& A) { A.Wait(FTimespan::FromMilliseconds(500)); });
                const auto Expected=WireBundleUI::OnGameThread([&]
                {
                    TArray<WireBundle::FLink> Links;
                    for(int32 I:{1,5}) { Links.Add({FEdGraphPinReference(WireBundleUI::FFixture::Pin(Fixture->Nodes[I],EGPD_Output)),
                        FEdGraphPinReference(WireBundleUI::FFixture::Pin(Fixture->Nodes[I+1],EGPD_Input)),Fixture->Nodes[I]->NodePosY}); }
                    auto Settings=ResolveWireBundleSettings(); Settings.Movement.bAllowExtraBends=false;
                    auto Panel=StaticCastSharedRef<SGraphPanel>(Fixture->Panel().AsShared());
                    TestTrue(TEXT("Simple corridor is blocked by staggered native nodes"),WireBundle::BuildGraphRoutePlan(Panel,Links,FVector2D(600,275),Settings).bBlocked);
                    const auto P=WireBundle::BuildGraphRoutePlan(Panel,Links,FVector2D(600,275),ResolveWireBundleSettings());
                    TestTrue(TEXT("Multi-bend preview uses extra native knots"),!P.bBlocked && P.ExtraBends>0);
                    return P;
                });
                if(Expected.bBlocked || Expected.ExtraBends<1) { return; }
                const auto Before=WireBundleUI::OnGameThread([&] { return Fixture->Graph->Nodes.Num(); });
                Perform([&](IActionSequence& A)
                {
                    Move(A,{600,120});
                    A.Press(EKeys::LeftControl).Press(EKeys::LeftAlt).Press(EMouseButtons::Left).MoveByOffset(0,310)
                        .Wait(FTimespan::FromMilliseconds(150)).Release(EMouseButtons::Left).Release(EKeys::LeftAlt).Release(EKeys::LeftControl);
                });
                const int32 Added=2*(Expected.Columns+Expected.ExtraBends);
                WireBundleUI::OnGameThread([&]
                {
                    TestEqual(TEXT("Every preview knot is created"),Fixture->Graph->Nodes.Num(),Before+Added);
                    const auto Records=WireBundle::ReadBundles(Fixture->Graph);
                    if(!TestEqual(TEXT("One routed record"),Records.Num(),1)) { return; }
                    TestTrue(TEXT("Multi-bend connections are healthy"),WireBundle::ValidateBundle(Fixture->Graph,Records[0])==WireBundle::EBundleHealth::Valid);
                    for(const auto& Lane:Expected.Lanes)
                    {
                        const auto W=Records[0].Wires.FindByPredicate([&](const FWireBundleStoredWire& R) { return WireBundle::ResolvePin(Fixture->Graph,R.Source)==Lane.Link.Output.Get(); });
                        if(!TestNotNull(TEXT("Preview lane exists"),W)) { continue; }
                        TestEqual(TEXT("Native chain has all bends"),W->Reroutes.Num(),Lane.Reroutes.Num());
                        for(int32 I=0;I<W->Reroutes.Num() && I<Lane.Reroutes.Num();++I)
                        {
                            const auto N=Fixture->Graph->Nodes.FindByPredicate([&](const UEdGraphNode* Node) { return WireBundle::NodeIdentity(Node)==W->Reroutes[I]; });
                            if(TestTrue(TEXT("Preview knot identity exists"),N!=nullptr))
                            { TestTrue(TEXT("Bend preview matches native position"),FVector2D((*N)->NodePosX+21,(*N)->NodePosY+12).Equals(Lane.Reroutes[I],0.51)); }
                        }
                    }
                });
                Perform([&](IActionSequence& A) { A.Focus(GraphLocator.ToSharedRef()).TypeChord(EKeys::LeftControl,EKeys::Z); });
                WireBundleUI::OnGameThread([&]
                {
                    TestEqual(TEXT("One undo removes all added bends"),Fixture->Graph->Nodes.Num(),Before);
                    TestTrue(TEXT("Undo restores original connections"),Fixture->OriginalLinksIntact());
                });
                Perform([](IActionSequence& A) { A.TypeChord(EKeys::LeftControl,EKeys::Y); });
                WireBundleUI::OnGameThread([&] { TestEqual(TEXT("Redo restores the complete routed chain"),Fixture->Graph->Nodes.Num(),Before+Added); });
            });

            It("GestureWheelCreatesThreeColumnsUndoRedo", EAsyncExecution::ThreadPool, [this]
            {
                const int32 Before=WireBundleUI::OnGameThread([&]
                {
                    // Creation needs ordinary wires: a gesture on registered nodes
                    // intentionally selects their existing bundle instead of nesting one.
                    for(const auto& Record:WireBundle::ReadBundles(Fixture->Graph))
                    { WireBundle::RemoveBundleRecord(Fixture->Graph,Record.Id); }
                    GEditor->ResetTransaction(FText::FromString(TEXT("WireBundle UI creation fixture")));
                    return Fixture->Graph->Nodes.Num();
                });
                Perform([&](IActionSequence& A)
                {
                    Move(A,{440,120});
                    A.Press(EKeys::LeftControl).Press(EKeys::LeftAlt).Press(EMouseButtons::Left)
                        .MoveByOffset(0,310).ScrollBy(1).Wait(FTimespan::FromMilliseconds(150))
                        .Release(EMouseButtons::Left).Release(EKeys::LeftAlt).Release(EKeys::LeftControl);
                });
                WireBundleUI::OnGameThread([&]
                {
                    TestEqual(TEXT("Two wires get three new reroutes each"),Fixture->Graph->Nodes.Num(),Before+6);
                    const auto Records=WireBundle::ReadBundles(Fixture->Graph);
                    bool bThree=false;
                    for(const auto& R:Records) { if(R.Wires.Num()==2 && R.Wires[0].Reroutes.Num()==3 && R.Wires[1].Reroutes.Num()==3)
                        { bThree=WireBundle::ValidateBundle(Fixture->Graph,R)==WireBundle::EBundleHealth::Valid; } }
                    TestTrue(TEXT("New three-column bundle has valid connections"),bThree);
                    TestEqual(TEXT("Wheel did not zoom the graph"),Fixture->Panel().GetZoomAmount(),1.0f);
                });
                if (!WireBundleUI::OnGameThread([&] { return Fixture->Graph->Nodes.Num()==Before+6; })) { return; }
                Perform([&](IActionSequence& A) { A.Focus(GraphLocator.ToSharedRef()).TypeChord(EKeys::LeftControl,EKeys::Z); });
                WireBundleUI::OnGameThread([&] { TestEqual(TEXT("Undo removes the new reroutes"),Fixture->Graph->Nodes.Num(),Before); TestTrue(TEXT("Undo reconnects original chains"),Fixture->OriginalLinksIntact()); });
                Perform([](IActionSequence& A) { A.TypeChord(EKeys::LeftControl,EKeys::Y); });
                WireBundleUI::OnGameThread([&] { TestEqual(TEXT("Redo restores new reroutes"),Fixture->Graph->Nodes.Num(),Before+6); });
            });
        });
    }
}
#endif

