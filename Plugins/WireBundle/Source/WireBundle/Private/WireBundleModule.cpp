#include "Modules/ModuleManager.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "SGraphPanel.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "MaterialGraph/MaterialGraphSchema.h"
#include "ScopedTransaction.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Editor.h"
#include "Runtime/Launch/Resources/Version.h"
#include "WireBundleGesture.h"

namespace WireBundle
{
	static void Notify(const FString& Message)
	{
		FNotificationInfo Info(FText::FromString(Message));
		Info.ExpireDuration = 4.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}

	// Use the same schema operation as double-clicking a wire. No custom graph
	// objects are serialized, so assets remain usable without this plugin.
	static UEdGraphNode* InsertKnot(UEdGraph* Graph, UEdGraphPin* A,
		UEdGraphPin* B, const FVector2D& Position)
	{
		TSet<UEdGraphNode*> Before;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			Before.Add(Node);
		}
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8
		Graph->GetSchema()->OnPinConnectionDoubleCicked(A, B, FVector2f(Position));
#else
		Graph->GetSchema()->OnPinConnectionDoubleCicked(A, B, Position);
#endif
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && !Before.Contains(Node))
			{
				return Node;
			}
		}
		return nullptr;
	}

	bool CanEdit(const TSharedRef<SGraphPanel>& Panel)
	{
		UEdGraph* Graph = Panel->GetGraphObj();
		if (!Graph || !Graph->bEditable || !Panel->IsGraphEditable() || (GEditor && GEditor->PlayWorld))
		{
			return false;
		}
		const UEdGraphSchema* Schema = Graph->GetSchema();
		// Exact schema classes avoid intercepting specialized K2-derived graphs.
		if (!Schema || (Schema->GetClass() != UEdGraphSchema_K2::StaticClass() && Schema->GetClass() != UMaterialGraphSchema::StaticClass()))
		{
			return false;
		}
		return true;
	}

	void Bundle(const TSharedRef<SGraphPanel>& Panel, const FVector2D& Center, const TArray<FLink>* ExplicitLinks)
	{
		if (!CanEdit(Panel))
		{
			return;
		}
		UEdGraph* Graph = Panel->GetGraphObj();

		TArray<UEdGraphNode*> Selected;
		for (UObject* Object : Panel->SelectionManager.GetSelectedNodes())
		{
			if (UEdGraphNode* Node = Cast<UEdGraphNode>(Object))
			{
				Selected.Add(Node);
			}
		}
		TSet<UEdGraphNode*> SelectedSet;
		for (UEdGraphNode* Node : Selected)
		{
			SelectedSet.Add(Node);
		}
		TArray<FLink> Links;
		for (UEdGraphNode* Node : Selected)
		{
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Output || Pin->bOrphanedPin)
				{
					continue;
				}
				for (UEdGraphPin* Other : Pin->LinkedTo)
				{
					if (Other && !Other->bOrphanedPin && SelectedSet.Contains(Other->GetOwningNode()))
					{
						Links.Add({ FEdGraphPinReference(Pin), FEdGraphPinReference(Other), Node->NodePosY });
					}
				}
			}
		}
		if (ExplicitLinks)
		{
			Links.Reset();
			for (const FLink& Link : *ExplicitLinks)
			{
				UEdGraphPin* Output = Link.Output.Get();
				UEdGraphPin* Input = Link.Input.Get();
				if (!Output || !Input || Output->bOrphanedPin || Input->bOrphanedPin
					|| Output->Direction != EGPD_Output || Input->Direction != EGPD_Input
					|| Output->GetOwningNode()->GetGraph() != Graph || Input->GetOwningNode()->GetGraph() != Graph
					|| !Output->LinkedTo.Contains(Input))
				{
					continue;
				}
				if (!Links.ContainsByPredicate([Output, Input](const FLink& Existing) { return Existing.Output.Get() == Output && Existing.Input.Get() == Input; }))
				{
					Links.Add(Link);
				}
			}
		}
		if (Links.Num() < 2)
		{
			Notify(TEXT("Wire Bundle: select nodes containing at least two connections, then press Ctrl+Alt+B over the graph."));
			return;
		}
		Links.StableSort([](const FLink& A, const FLink& B) { return A.SourceY < B.SourceY; });
		FScopedTransaction Transaction(NSLOCTEXT("WireBundle", "Bundle", "Bundle graph wires"));
		Graph->Modify();
		for (const FLink& Link : Links)
		{
			Link.Output.Get()->GetOwningNode()->Modify();
			Link.Input.Get()->GetOwningNode()->Modify();
		}
		TArray<UEdGraphNode*> Created;
		int32				  Completed = 0;
		for (int32 Index = 0; Index < Links.Num(); ++Index)
		{
			UEdGraphPin* Output = Links[Index].Output.Get();
			UEdGraphPin* Input = Links[Index].Input.Get();
			if (!Output || !Input || !Output->LinkedTo.Contains(Input))
			{
				continue;
			}
			const double  Y = Center.Y + (Index - (Links.Num() - 1) * 0.5) * 12.0;
			UEdGraphNode* Left = InsertKnot(Graph, Output, Input, FVector2D(Center.X - 80.0, Y));
			if (!Left)
			{
				continue;
			}
			Created.Add(Left);
			// Material edits may reconstruct pins; resolve the original input again.
			Input = Links[Index].Input.Get();
			if (!Input)
			{
				continue;
			}
			UEdGraphPin* LeftOutput = nullptr;
			for (UEdGraphPin* Pin : Left->Pins)
			{
				if (Pin && Pin->Direction == EGPD_Output && Pin->LinkedTo.Contains(Input))
				{
					LeftOutput = Pin;
					break;
				}
			}
			if (LeftOutput)
			{
				Left->Modify();
				if (UEdGraphNode* Right = InsertKnot(Graph, LeftOutput, Input, FVector2D(Center.X + 80.0, Y)))
				{
					Created.Add(Right);
					++Completed;
				}
			}
		}
		if (Created.IsEmpty())
		{
			Transaction.Cancel();
			Notify(TEXT("Wire Bundle: no reroutes were created. Open the asset in its editor and try again."));
			return;
		}
		Panel->SelectionManager.ClearSelectionSet();
		for (UEdGraphNode* Node : Created)
		{
			Panel->SelectionManager.SetNodeSelection(Node, true);
		}
		Graph->NotifyGraphChanged();
		Notify(FString::Printf(TEXT("Wire Bundle: %d/%d wires bundled. Drag selected reroutes to move the bundle. Ctrl+Z to undo."), Completed, Links.Num()));
	}

} // namespace WireBundle

class FWireBundleModule final : public IModuleInterface
{
	TSharedPtr<IInputProcessor> Input;

public:
	virtual void StartupModule() override
	{
		if (FSlateApplication::IsInitialized())
		{
			Input = WireBundle::CreateInput();
			FSlateApplication::Get().RegisterInputPreProcessor(Input);
		}
	}
	virtual void ShutdownModule() override
	{
		if (Input && FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(Input);
		}
		Input.Reset();
	}
};
IMPLEMENT_MODULE(FWireBundleModule, WireBundle)

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
	UEdGraph*	Graph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, TEXT("TestGraph"),
		  UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	FBlueprintEditorUtils::AddUbergraphPage(Blueprint, Graph);
	TArray<UK2Node_Knot*> Nodes;
	for (int32 I = 0; I < 4; ++I)
	{
		FGraphNodeCreator<UK2Node_Knot> Creator(*Graph);
		UK2Node_Knot*					Node = Creator.CreateNode();
		Creator.Finalize();
		Node->NodePosX = I < 2 ? 0 : 600;
		Node->NodePosY = (I % 2) * 120;
		Nodes.Add(Node);
	}
	Nodes[0]->GetOutputPin()->MakeLinkTo(Nodes[2]->GetInputPin());
	Nodes[1]->GetOutputPin()->MakeLinkTo(Nodes[3]->GetInputPin());
	const TSharedRef<SGraphPanel> Panel = SNew(SGraphPanel).GraphObj(Graph);
	for (UK2Node_Knot* Node : Nodes)
	{
		Panel->SelectionManager.SetNodeSelection(Node, true);
	}
	WireBundle::Bundle(Panel, FVector2D(300, 100));
	TestEqual(TEXT("Two reroutes for each wire"), Graph->Nodes.Num(), 8);
	TestEqual(TEXT("Created reroutes selected together"), Panel->SelectionManager.GetSelectedNodes().Num(), 4);
	for (int32 I = 0; I < 2; ++I)
	{
		UEdGraphPin* Pin = Nodes[I]->GetOutputPin();
		int32		 Hops = 0;
		while (Pin && Pin->LinkedTo.Num() == 1 && Hops < 5)
		{
			UEdGraphNode* Next = Pin->LinkedTo[0]->GetOwningNode();
			++Hops;
			if (Next == Nodes[I + 2])
			{
				break;
			}
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
	UEdGraph*	Graph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, TEXT("TestGraph"),
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
		Wire.Link = { FEdGraphPinReference(Nodes[I]->GetOutputPin()), FEdGraphPinReference(Nodes[I + 3]->GetInputPin()), I * 100 };
		Wire.Points = { FVector2D(0, I * 100), FVector2D(600, I * 100) };
		Wires.Add(Wire);
	}
	TArray<WireBundle::FLink> Links = WireBundle::FindCrossedLinks(Wires, FVector2D(300, -20), FVector2D(300, 120));
	TestEqual(TEXT("Gesture picks only crossed wires"), Links.Num(), 2);
	const WireBundle::FLink Duplicate = Links[0];
	Links.Add(Duplicate);			// Defensive deduplication before modifying graph.
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
#endif
