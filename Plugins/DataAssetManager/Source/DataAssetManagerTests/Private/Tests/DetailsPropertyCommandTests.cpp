// Copyright (c) 2026 DimAlek. All Rights Reserved.
#include "Tests/DataAssetManagerTestHelpers.h"
#include "Customization/DataAssetDetailsExtensionHandler.h"
#include "IPropertyRowGenerator.h"
#include "IDetailTreeNode.h"
#include "PropertyHandle.h"
#include "PropertyEditorModule.h"
#include "Editor.h"

namespace
{
	/** @brief Locates a real editor property handle without mocking the Details panel. */
	TSharedPtr<IPropertyHandle> FindHandle(const TArray<TSharedRef<IDetailTreeNode>>& Nodes, FName Name)
	{
		for (const TSharedRef<IDetailTreeNode>& Node : Nodes) {
			TSharedPtr<IPropertyHandle> Handle = Node->CreatePropertyHandle();
			if (Handle && Handle->GetProperty() && Handle->GetProperty()->GetFName() == Name) { return Handle; }
			TArray<TSharedRef<IDetailTreeNode>> Children;
			Node->GetChildren(Children);
			if (TSharedPtr<IPropertyHandle> Found = FindHandle(Children, Name)) { return Found; }
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDetailsPropertyCommandTest, "DataAssetManager.Integration.Details.CommandsUndoRedo", DataAssetManagerFlags::Flags)

/** @brief Executes the same property commands used by Slate against real property handles. */
bool FDetailsPropertyCommandTest::RunTest(const FString& Parameters)
{
	using namespace DataAssetManagerTests;
	FScopedTestDataAsset Fixture(MakeUniqueAssetName(TEXT("DetailsCommands")));
	UTestDataAsset* Asset = Fixture.GetAssetAs<UTestDataAsset>();
	if (!Asset || !GEditor) { AddError(TEXT("Editor fixture unavailable")); return false; }
	Asset->SetFlags(RF_Transactional);
	const FLinearColor Before(-1, -1, -1, -1);
	Asset->Color = Before;
	Asset->Colors = {FLinearColor::Red, FLinearColor::Green, FLinearColor::Blue};
	FPropertyEditorModule& Module = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	TSharedRef<IPropertyRowGenerator> Generator = Module.CreatePropertyRowGenerator(FPropertyRowGeneratorArgs());
	Generator->SetObjects({Asset});
	const TSharedPtr<IPropertyHandle> Color = FindHandle(Generator->GetRootTreeNodes(), TEXT("Color"));
	const TSharedPtr<IPropertyHandle> Colors = FindHandle(Generator->GetRootTreeNodes(), TEXT("Colors"));
	if (!Color || !Colors || !Colors->AsArray()) { AddError(TEXT("Real property handles unavailable")); return false; }
	FDataAssetDetailsExtensionHandler Handler;
	Handler.RandomizeProperty(Color, {});
	const FLinearColor Applied = Asset->Color;
	TestNotEqual(TEXT("Randomization changes the color"), Applied, Before);
	if (!TestTrue(TEXT("Randomize undo succeeds"), GEditor->UndoTransaction())) { return false; }
	TestEqual(TEXT("Randomize undo restores value"), Asset->Color, Before);
	if (!TestTrue(TEXT("Randomize redo succeeds"), GEditor->RedoTransaction())) { return false; }
	TestEqual(TEXT("Randomize redo restores generated value"), Asset->Color, Applied);
	const TSharedPtr<IPropertyHandle> Middle = Colors->AsArray()->GetElement(1);
	FDataAssetDetailsExtensionHandler::MoveArrayElement(Middle, {}, -1);
	TestTrue(TEXT("Move preserves all elements in the expected order"), Asset->Colors == TArray<FLinearColor>{FLinearColor::Green, FLinearColor::Red, FLinearColor::Blue});
	if (!TestTrue(TEXT("Move undo succeeds"), GEditor->UndoTransaction())) { return false; }
	TestTrue(TEXT("Move undo restores original order"), Asset->Colors == TArray<FLinearColor>{FLinearColor::Red, FLinearColor::Green, FLinearColor::Blue});
	if (!TestTrue(TEXT("Move redo succeeds"), GEditor->RedoTransaction())) { return false; }
	TestTrue(TEXT("Move redo restores new order"), Asset->Colors == TArray<FLinearColor>{FLinearColor::Green, FLinearColor::Red, FLinearColor::Blue});
	return true;
}
