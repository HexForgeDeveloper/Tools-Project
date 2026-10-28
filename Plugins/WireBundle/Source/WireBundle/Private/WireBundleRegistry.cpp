#include "WireBundleRegistry.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_Knot.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "MaterialGraph/MaterialGraphNode_Root.h"
#include "MaterialGraphNode_Knot.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"

namespace WireBundle
{
	static FGuid GraphIdentity(UEdGraph* Graph)
	{
		// Root material graphs are rebuilt on reopen. Their expression IDs persist.
		return Cast<UMaterialGraph>(Graph) ? FGuid(0x57425247, 0, 0, 1) : Graph->GraphGuid;
	}

	static UObject* FindStorage(UEdGraph* Graph, bool bCreate)
	{
		if (!Graph)
		{
			return nullptr;
		}
		if (UMaterialGraph* MaterialGraph = Cast<UMaterialGraph>(Graph))
		{
			// Function/composite lifetime is different and must be tested separately.
			if (MaterialGraph->MaterialFunction || Cast<UMaterialGraphNode>(Graph->GetOuter()))
			{
				return nullptr;
			}
			UMaterial* Material = MaterialGraph->Material;
			if (!Material)
			{
				return nullptr;
			}
			if (UAssetUserData* Existing = Material->GetAssetUserDataOfClass(UWireBundleMaterialData::StaticClass()))
			{
				return Existing;
			}
			if (!bCreate)
			{
				return nullptr;
			}
			Material->Modify();
			UWireBundleMaterialData* Data = NewObject<UWireBundleMaterialData>(Material, NAME_None, RF_Transactional);
			Material->AddAssetUserData(Data);
			return Data;
		}
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
		if (!Blueprint)
		{
			return nullptr;
		}
		for (UBlueprintExtension* Extension : Blueprint->GetExtensions())
		{
			if (Extension && Extension->IsA<UWireBundleBlueprintData>())
			{
				return Extension;
			}
		}
		if (!bCreate)
		{
			return nullptr;
		}
		Blueprint->Modify();
		UWireBundleBlueprintData* Data = NewObject<UWireBundleBlueprintData>(Blueprint, NAME_None, RF_Transactional);
		Blueprint->AddExtension(Data);
		return Data;
	}

	static TArray<FWireBundleRecord>* Records(UObject* Data)
	{
		if (UWireBundleBlueprintData* BP = Cast<UWireBundleBlueprintData>(Data))
		{
			return &BP->Bundles;
		}
		if (UWireBundleMaterialData* Material = Cast<UWireBundleMaterialData>(Data))
		{
			return &Material->Bundles;
		}
		return nullptr;
	}

	bool SupportsBundleStore(UEdGraph* Graph)
	{
		if (!Graph)
		{
			return false;
		}
		if (UMaterialGraph* MaterialGraph = Cast<UMaterialGraph>(Graph))
		{
			return MaterialGraph->Material && !MaterialGraph->MaterialFunction && !Cast<UMaterialGraphNode>(Graph->GetOuter());
		}
		return FBlueprintEditorUtils::FindBlueprintForGraph(Graph) != nullptr;
	}

	FGuid NodeIdentity(const UEdGraphNode* Node)
	{
		if (const UMaterialGraphNode* MaterialNode = Cast<UMaterialGraphNode>(Node))
		{
			return MaterialNode->MaterialExpression ? MaterialNode->MaterialExpression->MaterialExpressionGuid : FGuid();
		}
		return Node ? Node->NodeGuid : FGuid();
	}

	static UEdGraphNode* ResolveNode(UEdGraph* Graph, const FGuid& Id)
	{
		if (!Id.IsValid())
		{
			return nullptr;
		}
		UEdGraphNode* Found = nullptr;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && NodeIdentity(Node) == Id)
			{
				if (Found)
				{
					return nullptr;
				} // Ambiguous IDs must never select a node for deletion.
				Found = Node;
			}
		}
		return Found;
	}

	static FString TypeSignature(const UEdGraphPin* Pin)
	{
		return Pin->PinType.PinCategory.ToString() + TEXT("|") + Pin->PinType.PinSubCategory.ToString()
			+ TEXT("|") + GetPathNameSafe(Pin->PinType.PinSubCategoryObject.Get())
			+ TEXT("|") + FString::FromInt(int32(Pin->PinType.ContainerType));
	}

	FWireBundlePinIdentity IdentifyPin(const UEdGraphPin* Pin)
	{
		FWireBundlePinIdentity Result;
		if (!Pin)
		{
			return Result;
		}
		Result.Node = NodeIdentity(Pin->GetOwningNode());
		Result.Pin = Pin->PinId;
		Result.Name = Pin->PinName;
		Result.bOutput = Pin->Direction == EGPD_Output;
		Result.bMaterialRoot = Pin->GetOwningNode()->IsA<UMaterialGraphNode_Root>();
		Result.Type = TypeSignature(Pin);
		if (!Cast<UMaterialGraph>(Pin->GetOwningNode()->GetGraph()))
		{
			Result.BlueprintNode = Pin->GetOwningNode();
			Result.BlueprintType = Pin->PinType;
		}
		int32 Slot = 0;
		for (const UEdGraphPin* Candidate : Pin->GetOwningNode()->Pins)
		{
			if (Candidate && Candidate->Direction == Pin->Direction)
			{
				if (Candidate == Pin)
				{
					Result.Slot = Slot;
					break;
				}
				++Slot;
			}
		}
		return Result;
	}

	UEdGraphPin* ResolvePin(UEdGraph* Graph, const FWireBundlePinIdentity& Identity)
	{
		if (!Graph)
		{
			return nullptr;
		}
		UEdGraphNode* Node = nullptr;
		if (Identity.BlueprintNode && !Cast<UMaterialGraph>(Graph))
		{
			if (Identity.BlueprintNode->GetGraph() != Graph || !Graph->Nodes.Contains(Identity.BlueprintNode))
			{
				return nullptr;
			}
			Node = Identity.BlueprintNode;
		}
		else
		{
			Node = ResolveNode(Graph, Identity.Node);
		}
		if (Identity.bMaterialRoot)
		{
			Node = nullptr;
			for (UEdGraphNode* Candidate : Graph->Nodes)
			{
				if (Candidate && Candidate->IsA<UMaterialGraphNode_Root>())
				{
					if (Node)
					{
						return nullptr;
					}
					Node = Candidate;
				}
			}
		}
		if (!Node)
		{
			return nullptr;
		}
		int32		 Slot = 0;
		UEdGraphPin* Fallback = nullptr;
		int32		 MatchingSignatures = 0;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || (Pin->Direction == EGPD_Output) != Identity.bOutput)
			{
				continue;
			}
			const bool bExact = Pin->PinId == Identity.Pin;
			const bool bType = Identity.BlueprintNode ? Pin->PinType == Identity.BlueprintType : TypeSignature(Pin) == Identity.Type;
			const bool bSignature = !Pin->bOrphanedPin && Pin->PinName == Identity.Name && bType;
			if (bExact && bSignature)
			{
				return Pin;
			}
			// Both material reconstruction and BP asset duplication can assign new
			// PinIds. A fallback is allowed only on the exact stable node identity,
			// with a unique matching signature and unchanged directional slot.
			if (bSignature)
			{
				++MatchingSignatures;
				if (Slot == Identity.Slot)
				{
					Fallback = Pin;
				}
			}
			++Slot;
		}
		return MatchingSignatures == 1 ? Fallback : nullptr;
	}

	FWireBundleRecord NewRecord(UEdGraph* Graph)
	{
		FWireBundleRecord Result;
		Result.Id = FGuid::NewGuid();
		Result.Graph = GraphIdentity(Graph);
		return Result;
	}

	TArray<FWireBundleRecord> ReadBundles(UEdGraph* Graph)
	{
		TArray<FWireBundleRecord>		 Result;
		const TArray<FWireBundleRecord>* All = Records(FindStorage(Graph, false));
		if (All)
		{
			for (const FWireBundleRecord& Record : *All)
			{
				if (Record.Graph == GraphIdentity(Graph))
				{
					Result.Add(Record);
				}
			}
		}
		return Result;
	}

	bool RemoveBundleRecord(UEdGraph* Graph, const FGuid& Id)
	{
		UObject*				   Data = FindStorage(Graph, false);
		TArray<FWireBundleRecord>* All = Records(Data);
		if (!All || !All->ContainsByPredicate([&](const FWireBundleRecord& R) { return R.Id == Id && R.Graph == GraphIdentity(Graph); }))
		{
			return false;
		}
		Data->SetFlags(RF_Transactional);
		Data->Modify();
		All->RemoveAll([&](const FWireBundleRecord& R) { return R.Id == Id && R.Graph == GraphIdentity(Graph); });
		return true;
	}

	bool StoreBundle(UEdGraph* Graph, const FWireBundleRecord& Record)
	{
		if (!SupportsBundleStore(Graph))
		{
			return false;
		}
		if (!Record.Id.IsValid() || Record.Wires.IsEmpty() || Record.Graph != GraphIdentity(Graph))
		{
			return false;
		}
		if (ValidateBundle(Graph, Record) != EBundleHealth::Valid)
		{
			return false;
		}
		for (const FWireBundleRecord& Other : ReadBundles(Graph))
		{
			if (Other.Id == Record.Id)
			{
				continue;
			}
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node && BundleOwnsNode(Record, Node) && BundleOwnsNode(Other, Node))
				{
					return false;
				}
			}
		}
		UObject*				   Data = FindStorage(Graph, true);
		TArray<FWireBundleRecord>* All = Records(Data);
		if (!All)
		{
			return false;
		}
		Data->SetFlags(RF_Transactional);
		Data->Modify();
		if (FWireBundleRecord* Existing = All->FindByPredicate([&Record](const FWireBundleRecord& Item) { return Item.Id == Record.Id; }))
		{
			*Existing = Record;
		}
		else
		{
			All->Add(Record);
		}
		return true;
	}

	EBundleHealth ValidateBundle(UEdGraph* Graph, const FWireBundleRecord& Record)
	{
		if (Record.Version != 1)
		{
			return EBundleHealth::UnsupportedVersion;
		}
		if (!Graph || Record.Graph != GraphIdentity(Graph) || !Record.Id.IsValid() || Record.Wires.IsEmpty())
		{
			return EBundleHealth::Changed;
		}
		TSet<FGuid> Owned;
		for (const FWireBundleStoredWire& Wire : Record.Wires)
		{
			if (!Wire.Source.bOutput || Wire.Target.bOutput)
			{
				return EBundleHealth::Changed;
			}
			if (!Cast<UMaterialGraph>(Graph) && Wire.BlueprintReroutes.Num() != Wire.Reroutes.Num())
			{
				return EBundleHealth::Changed;
			}
			UEdGraphPin* Previous = ResolvePin(Graph, Wire.Source);
			UEdGraphPin* Target = ResolvePin(Graph, Wire.Target);
			if (!Previous || !Target)
			{
				return EBundleHealth::MissingEndpoints;
			}
			if (Wire.Reroutes.IsEmpty())
			{
				return EBundleHealth::Changed;
			}
			for (int32 Index = 0; Index < Wire.Reroutes.Num(); ++Index)
			{
				const FGuid&  Id = Wire.Reroutes[Index];
				UEdGraphNode* Node = Wire.BlueprintReroutes.IsValidIndex(Index)
					? ToRawPtr(Wire.BlueprintReroutes[Index])
					: ResolveNode(Graph, Id);
				if (!Node || Node->GetGraph() != Graph || !Graph->Nodes.Contains(Node))
				{
					return EBundleHealth::MissingReroutes;
				}
				if (Owned.Contains(Id) || (!Node->IsA<UK2Node_Knot>() && !Node->IsA<UMaterialGraphNode_Knot>()))
				{
					return EBundleHealth::Changed;
				}
				Owned.Add(Id);
				UEdGraphPin* In = nullptr;
				UEdGraphPin* Out = nullptr;
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (Pin && Pin->Direction == EGPD_Input)
					{
						In = Pin;
					}
					if (Pin && Pin->Direction == EGPD_Output)
					{
						Out = Pin;
					}
				}
				if (!In || !Out || In->bOrphanedPin || Out->bOrphanedPin || In->LinkedTo.Num() != 1 || In->LinkedTo[0] != Previous
					|| !Previous->LinkedTo.Contains(In) || Out->LinkedTo.Num() != 1)
				{
					return EBundleHealth::Changed;
				}
				Previous = Out;
			}
			if (Previous->LinkedTo.Num() != 1 || Previous->LinkedTo[0] != Target || !Target->LinkedTo.Contains(Previous))
			{
				return EBundleHealth::Changed;
			}
		}
		return EBundleHealth::Valid;
	}

	bool FindBundleForNode(UEdGraph* Graph, const UEdGraphNode* Node, FWireBundleRecord& OutRecord)
	{
		if (!Node || Node->GetGraph() != Graph)
		{
			return false;
		}
		bool bFound = false;
		for (const FWireBundleRecord& Record : ReadBundles(Graph))
		{
			if (BundleOwnsNode(Record, Node))
			{
				if (bFound && OutRecord.Id != Record.Id)
				{
					return false;
				}
				OutRecord = Record;
				bFound = true;
			}
		}
		return bFound;
	}

	bool BundleOwnsNode(const FWireBundleRecord& Record, const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return false;
		}
		for (const FWireBundleStoredWire& Wire : Record.Wires)
		{
			if (!Wire.BlueprintReroutes.IsEmpty())
			{
				if (Wire.BlueprintReroutes.Contains(Node))
				{
					return true;
				}
			}
			else if (Wire.Reroutes.Contains(NodeIdentity(Node)))
			{
				return true;
			}
		}
		return false;
	}

	void CaptureReroutes(FWireBundleStoredWire& Wire, const TArray<UEdGraphNode*>& Nodes)
	{
		Wire.Reroutes.Reset();
		Wire.BlueprintReroutes.Reset();
		for (UEdGraphNode* Node : Nodes)
		{
			Wire.Reroutes.Add(NodeIdentity(Node));
			if (!Cast<UMaterialGraph>(Node->GetGraph()))
			{
				Wire.BlueprintReroutes.Add(Node);
			}
		}
	}

	bool BuildAdoptionRecord(UEdGraph* Graph, const TArray<UEdGraphNode*>& Selected,
		FWireBundleRecord& OutRecord, FText& OutError)
	{
		auto Fail = [&OutError](const TCHAR* Message) { OutError = FText::FromString(Message); return false; };
		if (!SupportsBundleStore(Graph))
		{
			return Fail(TEXT("Persistent bundles are not supported in this graph yet."));
		}
		if (Selected.IsEmpty())
		{
			return Fail(TEXT("Select all reroutes of at least two complete wire chains."));
		}
		TSet<UEdGraphNode*> Nodes;
		for (UEdGraphNode* Node : Selected)
		{
			if (!Node || Node->GetGraph() != Graph || (!Node->IsA<UK2Node_Knot>() && !Node->IsA<UMaterialGraphNode_Knot>()))
			{
				return Fail(TEXT("Select reroute nodes only; leave the source and destination nodes unselected."));
			}
			FWireBundleRecord Existing;
			if (FindBundleForNode(Graph, Node, Existing))
			{
				return Fail(TEXT("A selected reroute already belongs to a registered bundle."));
			}
			Nodes.Add(Node);
		}
		FWireBundleRecord	  Record = NewRecord(Graph);
		TSet<UEdGraphNode*>	  Visited;
		TArray<UEdGraphNode*> Sorted = Selected;
		Sorted.StableSort([](const UEdGraphNode& A, const UEdGraphNode& B) { return A.NodePosY < B.NodePosY; });
		for (UEdGraphNode* Root : Sorted)
		{
			UEdGraphPin* Input = nullptr;
			for (UEdGraphPin* Pin : Root->Pins)
			{
				if (Pin && Pin->Direction == EGPD_Input)
				{
					Input = Pin;
				}
			}
			if (!Input || Input->LinkedTo.Num() != 1)
			{
				return Fail(TEXT("A selected reroute is disconnected or has extra input links."));
			}
			if (Nodes.Contains(Input->LinkedTo[0]->GetOwningNode()))
			{
				continue;
			}
			FWireBundleStoredWire Wire;
			TArray<UEdGraphNode*> Chain;
			Wire.Source = IdentifyPin(Input->LinkedTo[0]);
			UEdGraphNode* Current = Root;
			while (Current)
			{
				if (Visited.Contains(Current))
				{
					return Fail(TEXT("The selection contains a loop or shared reroute chain."));
				}
				Visited.Add(Current);
				Chain.Add(Current);
				UEdGraphPin* Output = nullptr;
				for (UEdGraphPin* Pin : Current->Pins)
				{
					if (Pin && Pin->Direction == EGPD_Output)
					{
						Output = Pin;
					}
				}
				if (!Output || Output->LinkedTo.Num() != 1)
				{
					return Fail(TEXT("A selected reroute has an incomplete chain or extra output links."));
				}
				UEdGraphPin* Next = Output->LinkedTo[0];
				if (!Nodes.Contains(Next->GetOwningNode()))
				{
					Wire.Target = IdentifyPin(Next);
					break;
				}
				Current = Next->GetOwningNode();
			}
			CaptureReroutes(Wire, Chain);
			Record.Wires.Add(MoveTemp(Wire));
		}
		if (Visited.Num() != Nodes.Num())
		{
			return Fail(TEXT("Some selected reroutes form a closed or disconnected chain."));
		}
		if (Record.Wires.Num() < 2)
		{
			return Fail(TEXT("At least two complete wire chains must be selected."));
		}
		if (ValidateBundle(Graph, Record) != EBundleHealth::Valid)
		{
			return Fail(TEXT("The selected topology cannot be safely registered."));
		}
		OutRecord = MoveTemp(Record);
		return true;
	}
} // namespace WireBundle
