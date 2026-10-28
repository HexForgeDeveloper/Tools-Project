#include "WireBundleGesture.h"
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
#include "WireBundleRoutePlanner.h"
#include "WireBundleRegistry.h"
#include "K2Node_Knot.h"
#include "MaterialGraphNode_Knot.h"
#include "Materials/MaterialExpression.h"
#include "MaterialEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace WireBundle
{
	static void Notify(const FString& Message)
	{
		const FWireBundleAppearanceSettings Settings = ResolveWireBundleSettings().Appearance;
		if (!Settings.bShowNotifications)
		{
			return;
		}
		FNotificationInfo Info(FText::FromString(Message));
		Info.ExpireDuration = Settings.NotificationDuration;
		FSlateNotificationManager::Get().AddNotification(Info);
	}

	// Use the same schema operation as double-clicking a wire. Graph nodes remain
	// native; persistent bundle descriptions are stored separately on the asset.
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
				// Native K2 spawning snaps to the editor grid even when plugin snapping
				// is disabled. Apply the planned center after schema construction so
				// preview, lane spacing and plugin grid settings agree with the result.
				Node->Modify();
				Node->NodePosX = FMath::RoundToInt32(Position.X - 21.0);
				Node->NodePosY = FMath::RoundToInt32(Position.Y - 12.0);
				if (const auto Knot = Cast<UMaterialGraphNode_Knot>(Node))
				{
					if (Knot->MaterialExpression)
					{
						Knot->MaterialExpression->Modify();
						Knot->MaterialExpression->MaterialExpressionEditorX = Node->NodePosX;
						Knot->MaterialExpression->MaterialExpressionEditorY = Node->NodePosY;
					}
				}
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
		const FWireBundleResolvedSettings Settings = ResolveWireBundleSettings();
		return Schema->GetClass() == UEdGraphSchema_K2::StaticClass()
			? Settings.bEnableBlueprint && !FBlueprintEditorUtils::IsGraphReadOnly(Graph)
			: Settings.bEnableMaterial;
	}

	bool Nudge(const TSharedRef<SGraphPanel>& Panel, FVector2D Direction, bool bFast)
	{
		if (!CanEdit(Panel))
		{
			return false;
		}
		const FWireBundleMovementSettings Settings = ResolveWireBundleSettings().Movement;
		if (Settings.KeyboardAxis == EWireBundleMovementAxis::Horizontal)
		{
			Direction.Y = 0;
		}
		if (Settings.KeyboardAxis == EWireBundleMovementAxis::Vertical)
		{
			Direction.X = 0;
		}
		if (Direction.IsNearlyZero())
		{
			return true;
		}
		const FVector2D		  Delta = Direction * Settings.NudgeStep * (bFast ? Settings.FastNudgeMultiplier : 1.0f);
		TArray<UEdGraphNode*> Nodes;
		for (UObject* Object : Panel->SelectionManager.GetSelectedNodes())
		{
			UEdGraphNode* Node = Cast<UEdGraphNode>(Object);
			if (Node && (Node->IsA<UK2Node_Knot>() || Node->IsA<UMaterialGraphNode_Knot>()))
			{
				Nodes.Add(Node);
			}
		}
		if (Nodes.IsEmpty())
		{
			return false;
		}
		const FScopedTransaction Transaction(NSLOCTEXT("WireBundle", "Nudge", "Move wire bundle reroutes"));
		UEdGraph*				 Graph = Panel->GetGraphObj();
		Graph->Modify();
		// Snap one common delta, keeping the relative spacing of the bundle intact.
		FVector2D AppliedDelta = Delta;
		if (Settings.bSnapToGrid)
		{
			const UEdGraphNode* Anchor = Nodes[0];
			AppliedDelta.X = Direction.X == 0 ? 0 : FMath::GridSnap(Anchor->NodePosX + Delta.X, double(Settings.GridSize)) - Anchor->NodePosX;
			AppliedDelta.Y = Direction.Y == 0 ? 0 : FMath::GridSnap(Anchor->NodePosY + Delta.Y, double(Settings.GridSize)) - Anchor->NodePosY;
		}
		for (UEdGraphNode* Node : Nodes)
		{
			Node->Modify();
			Node->NodePosX += FMath::RoundToInt32(AppliedDelta.X);
			Node->NodePosY += FMath::RoundToInt32(AppliedDelta.Y);
			if (UMaterialGraphNode_Knot* Knot = Cast<UMaterialGraphNode_Knot>(Node))
			{
				if (Knot->MaterialExpression)
				{
					Knot->MaterialExpression->Modify();
					Knot->MaterialExpression->MaterialExpressionEditorX = Node->NodePosX;
					Knot->MaterialExpression->MaterialExpressionEditorY = Node->NodePosY;
				}
			}
		}
		if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph))
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		}
		else
		{
			FMaterialEditorUtilities::UpdateMaterialAfterGraphChange(Graph);
		}
		Graph->NotifyGraphChanged();
		return true;
	}

	bool SelectBundle(const TSharedRef<SGraphPanel>& Panel, const FGuid& Id)
	{
		if (!CanEdit(Panel))
		{
			return false;
		}
		UEdGraph* Graph = Panel->GetGraphObj();
		for (const FWireBundleRecord& Record : ReadBundles(Graph))
		{
			if (Record.Id != Id || ValidateBundle(Graph, Record) != EBundleHealth::Valid)
			{
				continue;
			}
			Panel->SelectionManager.ClearSelectionSet();
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (BundleOwnsNode(Record, Node))
				{
					Panel->SelectionManager.SetNodeSelection(Node, true);
				}
			}
			return true;
		}
		return false;
	}

	bool Unbundle(const TSharedRef<SGraphPanel>& Panel, const FGuid& Id)
	{
		if (!CanEdit(Panel))
		{
			return false;
		}
		UEdGraph* Graph = Panel->GetGraphObj();
		if (!FBlueprintEditorUtils::FindBlueprintForGraph(Graph)
			&& !FMaterialEditorUtilities::GetIMaterialEditorForObject(Graph).IsValid())
		{
			return false;
		}
		const TArray<FWireBundleRecord> Records = ReadBundles(Graph);
		const FWireBundleRecord*		Record = Records.FindByPredicate([&](const FWireBundleRecord& R) { return R.Id == Id; });
		if (!Record || ValidateBundle(Graph, *Record) != EBundleHealth::Valid)
		{
			Notify(TEXT("Wire Bundle: connections changed; the bundle was left untouched."));
			return false;
		}
		TArray<UEdGraphNode*> Owned;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!BundleOwnsNode(*Record, Node))
			{
				continue;
			}
			if (!Node->CanUserDeleteNode())
			{
				return false;
			}
			for (const FWireBundleRecord& Other : Records)
			{
				if (Other.Id != Id && BundleOwnsNode(Other, Node))
				{
					return false;
				}
			}
			Owned.Add(Node);
		}
		TArray<FLink> Direct;
		for (const FWireBundleStoredWire& Wire : Record->Wires)
		{
			UEdGraphPin* Output = ResolvePin(Graph, Wire.Source);
			UEdGraphPin* Input = ResolvePin(Graph, Wire.Target);
			// Only existing native-compatible endpoints are reconnected. No conversions.
			const auto Response = Graph->GetSchema()->CanCreateConnection(Output, Input).Response;
			if (Response != CONNECT_RESPONSE_MAKE && Response != CONNECT_RESPONSE_BREAK_OTHERS_A
				&& Response != CONNECT_RESPONSE_BREAK_OTHERS_B && Response != CONNECT_RESPONSE_BREAK_OTHERS_AB)
			{
				Notify(TEXT("Wire Bundle: the original pins cannot be connected directly."));
				return false;
			}
			if (Owned.Contains(Output->GetOwningNode()) || Owned.Contains(Input->GetOwningNode()))
			{
				return false;
			}
			Direct.Add({ FEdGraphPinReference(Output), FEdGraphPinReference(Input), 0 });
		}
		FScopedTransaction Transaction(NSLOCTEXT("WireBundle", "Unbundle", "Unbundle graph wires"));
		Graph->Modify();
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			// Native deletion updates adjacent pins too. Capture all linked nodes for Undo.
			if (Owned.Contains(Node))
			{
				Node->Modify();
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (Pin)
					{
						for (UEdGraphPin* Other : Pin->LinkedTo)
						{
							Other->GetOwningNode()->Modify();
						}
					}
				}
			}
		}
		if (!RemoveBundleRecord(Graph, Id))
		{
			Transaction.Cancel();
			return false;
		}
		// Splice every chain before native deletion. MakeLinkTo deliberately preserves
		// unrelated fan-out/fan-in; TryCreateConnection may break those existing links.
		for (UEdGraphNode* Node : Owned)
		{
			Node->BreakAllNodeLinks();
		}
		for (const FLink& Link : Direct)
		{
			Link.Output.Get()->MakeLinkTo(Link.Input.Get());
		}
		if (FBlueprintEditorUtils::FindBlueprintForGraph(Graph))
		{
			for (UEdGraphNode* Node : Owned)
			{
				FBlueprintEditorUtils::RemoveNode(nullptr, Node, true);
			}
			for (const FLink& Link : Direct)
			{
				if (UEdGraphPin* Output = Link.Output.Get())
				{
					Output->GetOwningNode()->PinConnectionListChanged(Output);
				}
				if (UEdGraphPin* Input = Link.Input.Get())
				{
					Input->GetOwningNode()->PinConnectionListChanged(Input);
				}
			}
			FBlueprintEditorUtils::MarkBlueprintAsModified(FBlueprintEditorUtils::FindBlueprintForGraph(Graph));
		}
		else
		{
			FMaterialEditorUtilities::DeleteNodes(Graph, Owned);
			FMaterialEditorUtilities::UpdateMaterialAfterGraphChange(Graph);
		}
		Panel->SelectionManager.ClearSelectionSet();
		Graph->NotifyGraphChanged();
		Notify(TEXT("Wire Bundle: direct connections restored. Ctrl+Z to undo."));
		return true;
	}

	bool Adopt(const TSharedRef<SGraphPanel>& Panel)
	{
		if (!CanEdit(Panel))
		{
			return false;
		}
		TArray<UEdGraphNode*> Selected;
		for (UObject* Object : Panel->SelectionManager.GetSelectedNodes())
		{
			if (UEdGraphNode* Node = Cast<UEdGraphNode>(Object))
			{
				Selected.Add(Node);
			}
		}
		FWireBundleRecord Record;
		FText			  Error;
		UEdGraph*		  Graph = Panel->GetGraphObj();
		if (!BuildAdoptionRecord(Graph, Selected, Record, Error))
		{
			Notify(Error.ToString());
			return false;
		}
		const FScopedTransaction Transaction(NSLOCTEXT("WireBundle", "Adopt", "Register existing wire bundle"));
		Graph->Modify();
		if (!StoreBundle(Graph, Record))
		{
			return false;
		}
		if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph))
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		}
		else
		{
			FMaterialEditorUtilities::UpdateMaterialAfterGraphChange(Graph);
		}
		Notify(TEXT("Wire Bundle: existing reroute chains registered. Connections and layout were preserved."));
		return true;
	}

	void Bundle(const TSharedRef<SGraphPanel>& Panel, const FVector2D& RequestedCenter, const TArray<FLink>* ExplicitLinks, const FWireBundleResolvedSettings* OverrideSettings)
	{
		if (!CanEdit(Panel))
		{
			return;
		}
		UEdGraph*					Graph = Panel->GetGraphObj();
		FWireBundleResolvedSettings Config = OverrideSettings ? *OverrideSettings : ResolveWireBundleSettings();
		Config.Sanitize();

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
		// Recognition never guesses ownership from visual proximity. Existing
		// records are inspected before adding nodes, preventing nested re-bundling.
		TArray<UEdGraphNode*> Candidates = Selected;
		if (ExplicitLinks)
		{
			Candidates.Reset();
			for (const FLink& Link : Links)
			{
				Candidates.AddUnique(Link.Output.Get()->GetOwningNode());
				Candidates.AddUnique(Link.Input.Get()->GetOwningNode());
			}
		}
		FWireBundleRecord Existing;
		bool			  bRecognized = false;
		for (UEdGraphNode* Node : Candidates)
		{
			FWireBundleRecord Found;
			if (FindBundleForNode(Graph, Node, Found))
			{
				if (bRecognized && Existing.Id != Found.Id)
				{
					Notify(TEXT("Wire Bundle: selection contains multiple bundles. Select one bundle."));
					return;
				}
				Existing = Found;
				bRecognized = true;
			}
		}
		if (bRecognized)
		{
			if (ValidateBundle(Graph, Existing) != EBundleHealth::Valid)
			{
				Notify(TEXT("Wire Bundle: the existing bundle was changed manually; its connections were left untouched."));
				return;
			}
			Panel->SelectionManager.ClearSelectionSet();
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (!Node)
				{
					continue;
				}
				if (BundleOwnsNode(Existing, Node))
				{
					Panel->SelectionManager.SetNodeSelection(Node, true);
				}
			}
			Notify(TEXT("Wire Bundle: existing bundle recognized and selected. No new reroutes were added."));
			return;
		}
		if (Links.Num() < Config.Gesture.MinimumWireCount)
		{
			Notify(FString::Printf(TEXT("Wire Bundle: at least %d connected wires are required."), Config.Gesture.MinimumWireCount));
			return;
		}
		const FBundleRoutePlan Plan = BuildParallelRoutePlan(Links, RequestedCenter, Config);
		FScopedTransaction	   Transaction(NSLOCTEXT("WireBundle", "Bundle", "Bundle graph wires"));
		Graph->Modify();
		for (const FLink& Link : Links)
		{
			Link.Output.Get()->GetOwningNode()->Modify();
			Link.Input.Get()->GetOwningNode()->Modify();
		}
		TArray<UEdGraphNode*> Created;
		FWireBundleRecord	  Record = NewRecord(Graph);
		int32				  Completed = 0;
		for (const FBundleRouteLane& Lane : Plan.Lanes)
		{
			UEdGraphPin* Output = Lane.Link.Output.Get();
			UEdGraphPin* Input = Lane.Link.Input.Get();
			if (!Output || !Input || !Output->LinkedTo.Contains(Input))
			{
				continue;
			}
			FWireBundleStoredWire StoredWire;
			StoredWire.Source = IdentifyPin(Output);
			StoredWire.Target = IdentifyPin(Input);
			TArray<UEdGraphNode*> Chain;
			FEdGraphPinReference  Previous(Output);
			for (const FVector2D& Point : Lane.Reroutes)
			{
				// Resolve pins again: a native material edit can reconstruct them.
				Input = Lane.Link.Input.Get();
				Output = Previous.Get();
				if (!Input || !Output || !Output->LinkedTo.Contains(Input))
				{
					break;
				}
				Output->GetOwningNode()->Modify();
				UEdGraphNode* Knot = InsertKnot(Graph, Output, Input, Point);
				if (!Knot)
				{
					break;
				}
				Created.Add(Knot);
				Chain.Add(Knot);
				Input = Lane.Link.Input.Get();
				UEdGraphPin* Next = nullptr;
				for (UEdGraphPin* Pin : Knot->Pins)
				{
					if (Pin && Pin->Direction == EGPD_Output && Pin->LinkedTo.Contains(Input))
					{
						Next = Pin;
						break;
					}
				}
				if (!Next)
				{
					break;
				}
				Previous = FEdGraphPinReference(Next);
			}
			if (Chain.Num() == Lane.Reroutes.Num())
			{
				CaptureReroutes(StoredWire, Chain);
				Record.Wires.Add(MoveTemp(StoredWire));
				++Completed;
			}
		}
		if (Created.IsEmpty())
		{
			Transaction.Cancel();
			Notify(TEXT("Wire Bundle: no reroutes were created. Open the asset in its editor and try again."));
			return;
		}
		const bool bStored = !Record.Wires.IsEmpty() && SupportsBundleStore(Graph) && StoreBundle(Graph, Record);
		Panel->SelectionManager.ClearSelectionSet();
		for (UEdGraphNode* Node : (Config.Movement.bSelectCreatedReroutes ? Created : Selected))
		{
			Panel->SelectionManager.SetNodeSelection(Node, true);
		}
		Graph->NotifyGraphChanged();
		Notify(FString::Printf(TEXT("Wire Bundle: %d/%d wires bundled. Ctrl+Z to undo.%s"), Completed, Links.Num(),
			bStored ? TEXT("") : TEXT(" Persistent recognition is not available for this graph.")));
	}

} // namespace WireBundle
