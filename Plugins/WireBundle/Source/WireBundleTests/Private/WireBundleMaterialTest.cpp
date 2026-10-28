#if WITH_DEV_AUTOMATION_TESTS
	#include "WireBundleGesture.h"
	#include "Misc/AutomationTest.h"
	#include "MaterialEditorModule.h"
	#include "IMaterialEditor.h"
	#include "Materials/Material.h"
	#include "Materials/MaterialExpressionReroute.h"
	#include "MaterialGraph/MaterialGraph.h"
	#include "MaterialGraphNode_Knot.h"
	#include "SGraphPanel.h"
	#include "Editor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireBundleMaterialTest, "WireBundle.Material.BundleUndoRedo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWireBundleMaterialTest::RunTest(const FString&)
{
	UMaterial*						  Original = NewObject<UMaterial>(GetTransientPackage(),
							   MakeUniqueObjectName(GetTransientPackage(), UMaterial::StaticClass(), TEXT("WireBundleMaterialTest")), RF_Transactional);
	IMaterialEditorModule&			  Module = FModuleManager::LoadModuleChecked<IMaterialEditorModule>("MaterialEditor");
	const TSharedRef<IMaterialEditor> Editor = Module.CreateMaterialEditor(EToolkitMode::Standalone, nullptr, Original);
	UMaterial*						  Material = Cast<UMaterial>(Editor->GetMaterialInterface());
	if (!TestNotNull(TEXT("Preview material"), Material))
	{
		Editor->CloseWindow(EAssetEditorCloseReason::AssetForceDeleted);
		return false;
	}
	UMaterialGraph* Graph = Material->MaterialGraph;
	if (!TestNotNull(TEXT("Material graph"), Graph))
	{
		Editor->CloseWindow(EAssetEditorCloseReason::AssetForceDeleted);
		return false;
	}
	TArray<UMaterialGraphNode_Knot*> Nodes;
	for (int32 I = 0; I < 4; ++I)
	{
		UMaterialExpression*	 Expression = Editor->CreateNewMaterialExpression(UMaterialExpressionReroute::StaticClass(),
				FVector2D(I < 2 ? 0 : 600, (I % 2) * 120), false, false, Graph);
		UMaterialGraphNode_Knot* Knot = Expression ? Cast<UMaterialGraphNode_Knot>(Expression->GraphNode) : nullptr;
		if (!TestNotNull(TEXT("Material reroute"), Knot))
		{
			Editor->CloseWindow(EAssetEditorCloseReason::AssetForceDeleted);
			return false;
		}
		Nodes.Add(Knot);
	}
	TArray<WireBundle::FLink> Links;
	for (int32 I = 0; I < 2; ++I)
	{
		Graph->GetSchema()->TryCreateConnection(Nodes[I]->GetOutputPin(), Nodes[I + 2]->GetInputPin());
		Links.Add({ FEdGraphPinReference(Nodes[I]->GetOutputPin()), FEdGraphPinReference(Nodes[I + 2]->GetInputPin()), I * 120 });
	}
	const int32					  Before = Graph->Nodes.Num();
	const TSharedRef<SGraphPanel> Panel = SNew(SGraphPanel).GraphObj(Graph);
	WireBundle::Bundle(Panel, FVector2D(300, 60), &Links);
	TestEqual(TEXT("Material bundle inserts four reroutes"), Graph->Nodes.Num(), Before + 4);
	TestEqual(TEXT("Material reroutes selected"), Panel->SelectionManager.GetSelectedNodes().Num(), 4);
	for (int32 I = 0; I < 2; ++I)
	{
		UEdGraphPin* Pin = Nodes[I]->GetOutputPin();
		int32		 Hops = 0;
		bool		 bReached = false;
		while (Pin && Pin->LinkedTo.Num() == 1 && Hops < 5)
		{
			UEdGraphNode* Next = Pin->LinkedTo[0]->GetOwningNode();
			++Hops;
			if (Next == Nodes[I + 2])
			{
				bReached = true;
				break;
			}
			UMaterialGraphNode_Knot* Knot = Cast<UMaterialGraphNode_Knot>(Next);
			Pin = Knot ? Knot->GetOutputPin() : nullptr;
		}
		TestTrue(TEXT("Material destination preserved"), bReached);
		TestEqual(TEXT("Material path through two new reroutes"), Hops, 3);
	}
	GEditor->UndoTransaction();
	TestEqual(TEXT("Material bundle undone atomically"), Graph->Nodes.Num(), Before);
	for (int32 I = 0; I < 2; ++I)
	{
		TestTrue(TEXT("Material original wire restored"), Nodes[I]->GetOutputPin()->LinkedTo.Contains(Nodes[I + 2]->GetInputPin()));
	}
	GEditor->RedoTransaction();
	TestEqual(TEXT("Material bundle redone"), Graph->Nodes.Num(), Before + 4);
	TArray<UMaterialGraphNode_Knot*> Reroutes;
	// Material Editor clears selection during undo/redo. Select the restored
	// reroutes explicitly, just as the user does before a movement command.
	Panel->SelectionManager.ClearSelectionSet();
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UMaterialGraphNode_Knot* Knot = Cast<UMaterialGraphNode_Knot>(Node))
		{
			if (!Nodes.Contains(Knot))
			{
				Reroutes.Add(Knot);
				Panel->SelectionManager.SetNodeSelection(Knot, true);
			}
		}
	}
	if (TestTrue(TEXT("Material reroutes available for movement"), !Reroutes.IsEmpty()))
	{
		const int32 X = Reroutes[0]->NodePosX;
		WireBundle::Nudge(Panel, FVector2D(1, 0), false);
		TestEqual(TEXT("Material keyboard movement"), Reroutes[0]->NodePosX, X + 16);
		TestEqual(TEXT("Expression position synchronized"), Reroutes[0]->MaterialExpression->MaterialExpressionEditorX, X + 16);
		GEditor->UndoTransaction();
		TestEqual(TEXT("Material movement undo"), Reroutes[0]->NodePosX, X);
		TestEqual(TEXT("Expression movement undo"), Reroutes[0]->MaterialExpression->MaterialExpressionEditorX, X);
	}
	// Discard only this transient test editor; do not display an apply dialog.
	Editor->CloseWindow(EAssetEditorCloseReason::AssetForceDeleted);
	return true;
}
#endif
