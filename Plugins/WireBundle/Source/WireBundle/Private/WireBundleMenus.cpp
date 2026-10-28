#include "WireBundleMenus.h"
#include "WireBundleGesture.h"
#include "WireBundleRegistry.h"
#include "GraphEditor.h"
#include "GraphEditorModule.h"
#include "SGraphPanel.h"
#include "EdGraph/EdGraph.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Modules/ModuleManager.h"

namespace WireBundle
{
static TSharedRef<FExtender> ExtendMenu(const TSharedRef<FUICommandList> Commands,
    const UEdGraph* Graph, const UEdGraphNode* Node, const UEdGraphPin* Pin, bool bReadOnly)
{
    TSharedRef<FExtender> Extender = MakeShared<FExtender>();
    const TSharedPtr<SGraphEditor> Editor = SGraphEditor::FindGraphEditorForGraph(Graph);
    if (bReadOnly || !Node || Pin || !Editor || !Editor->GetGraphPanel()) { return Extender; }
    const TSharedRef<SGraphPanel> Panel = StaticCastSharedRef<SGraphPanel>(Editor->GetGraphPanel()->AsShared());
    if (!CanEdit(Panel)) { return Extender; }
    const TWeakPtr<SGraphPanel> WeakPanel = Panel;
    FWireBundleRecord Record;
    const bool bKnown = FindBundleForNode(const_cast<UEdGraph*>(Graph), Node, Record);
    const FGuid Id = Record.Id;
    const bool bValid = bKnown && ValidateBundle(const_cast<UEdGraph*>(Graph), Record) == EBundleHealth::Valid;
    // Each native schema uses its own node action section.
    for (FName Hook : {FName("EdGraphSchemaNodeActions"), FName("MaterialSchemaNodeActions")})
    {
        Extender->AddMenuExtension(Hook, EExtensionHook::After, Commands,
            FMenuExtensionDelegate::CreateLambda([WeakPanel, Id, bKnown, bValid](FMenuBuilder& Menu)
        {
            Menu.BeginSection("WireBundle", NSLOCTEXT("WireBundle", "Menu", "Wire Bundle"));
            if (bKnown)
            {
                Menu.AddMenuEntry(NSLOCTEXT("WireBundle", "Select", "Select entire bundle"),
                    NSLOCTEXT("WireBundle", "SelectTip", "Select all reroutes belonging to this bundle."), FSlateIcon(),
                    FUIAction(FExecuteAction::CreateLambda([WeakPanel, Id]()
                    { if (auto P = WeakPanel.Pin()) { SelectBundle(P.ToSharedRef(), Id); } }),
                    FCanExecuteAction::CreateLambda([WeakPanel, bValid]() { return bValid && WeakPanel.IsValid(); })));
                Menu.AddMenuEntry(NSLOCTEXT("WireBundle", "UnbundleMenu", "Unbundle wires"),
                    bValid ? NSLOCTEXT("WireBundle", "UnbundleTip", "Restore direct connections and remove this bundle's reroutes. Supports Undo.")
                           : NSLOCTEXT("WireBundle", "ChangedTip", "Connections changed manually. Restore the original chain before unbundling."), FSlateIcon(),
                    FUIAction(FExecuteAction::CreateLambda([WeakPanel, Id]()
                    { if (auto P = WeakPanel.Pin()) { Unbundle(P.ToSharedRef(), Id); } }),
                    FCanExecuteAction::CreateLambda([WeakPanel, bValid]() { return bValid && WeakPanel.IsValid(); })));
            }
            else
            {
                Menu.AddMenuEntry(NSLOCTEXT("WireBundle", "BundleMenu", "Bundle selected connections"),
                    NSLOCTEXT("WireBundle", "BundleTip", "Create a bundle between selected connected nodes."), FSlateIcon(),
                    FUIAction(FExecuteAction::CreateLambda([WeakPanel]()
                    {
                        if (auto P = WeakPanel.Pin())
                        {
                            FVector2D Center = FVector2D::ZeroVector;
                            int32 Count = 0;
                            for (UObject* Object : P->SelectionManager.GetSelectedNodes())
                            { if (auto N = Cast<UEdGraphNode>(Object)) { Center += FVector2D(N->NodePosX, N->NodePosY); ++Count; } }
                            if (Count) { Bundle(P.ToSharedRef(), Center / Count); }
                        }
                    })));
                Menu.AddMenuEntry(NSLOCTEXT("WireBundle", "AdoptMenu", "Register selected reroute chains"),
                    NSLOCTEXT("WireBundle", "AdoptTip", "Register legacy reroute chains without changing their connections or positions."), FSlateIcon(),
                    FUIAction(FExecuteAction::CreateLambda([WeakPanel]()
                    { if (auto P = WeakPanel.Pin()) { Adopt(P.ToSharedRef()); } })));
            }
            Menu.EndSection();
        }));
    }
    return Extender;
}

FDelegateHandle RegisterMenus()
{
    auto Delegate = FGraphEditorModule::FGraphEditorMenuExtender_SelectedNode::CreateStatic(&ExtendMenu);
    FModuleManager::LoadModuleChecked<FGraphEditorModule>("GraphEditor").GetAllGraphEditorContextMenuExtender().Add(Delegate);
    return Delegate.GetHandle();
}
void UnregisterMenus(FDelegateHandle Handle)
{
    if (auto Module = FModuleManager::GetModulePtr<FGraphEditorModule>("GraphEditor"))
    { Module->GetAllGraphEditorContextMenuExtender().RemoveAll([Handle](const auto& D) { return D.GetHandle() == Handle; }); }
}
}
