// Copyright (c) 2026 DimAlek. All Rights Reserved.
#include "Tests/DataAssetManagerTestHelpers.h"
#include "FunctionLibrary/DataAssetManagerFunctionLibrary.h"
#include "IDetailRootObjectCustomization.h"
#include "Editor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FResetInstancedUndoTest, "DataAssetManager.Integration.Reset.InstancedUndoRedo", DataAssetManagerFlags::Flags)

/** @brief Verifies pointer identity, values and CDO isolation across reset, undo and redo. */
bool FResetInstancedUndoTest::RunTest(const FString& Parameters)
{
	using namespace DataAssetManagerTests;
	FScopedTestDataAsset Fixture(MakeUniqueAssetName(TEXT("ResetInstance")), TestRootPath, UTestDataAssetWithInstancedObject::StaticClass());
	auto* Asset = Fixture.GetAssetAs<UTestDataAssetWithInstancedObject>();
	if (!Asset || !Asset->InstancedObject || !GEditor)
	{
		AddError(TEXT("Reset fixture unavailable"));
		return false;
	}
	Asset->SetFlags(RF_Transactional);
	UTestDataAssetSubobject* Original = Asset->InstancedObject;
	Original->TestProperty = 123;
	const auto* Defaults = GetDefault<UTestDataAssetWithInstancedObject>();
	const int32 DefaultValue = Defaults->InstancedObject->TestProperty;
	FDetailsObjectSet Objects;
	Objects.RootObjects.Add(Asset);
	Objects.CommonBaseClass = Asset->GetClass();
	DataAssetManager::ResetToCDO(Objects);
	UTestDataAssetSubobject* ResetObject = Asset->InstancedObject;
	TestEqual(TEXT("Reset restores child value"), ResetObject->TestProperty, DefaultValue);
	TestTrue(TEXT("Reset child belongs to target"), ResetObject->GetOuter() == Asset);
	TestTrue(TEXT("Reset child is separate from CDO"), ResetObject != Defaults->InstancedObject);
	if (!TestTrue(TEXT("Reset undo succeeds"), GEditor->UndoTransaction()))
	{
		return false;
	}
	TestTrue(TEXT("Undo restores original child pointer"), Asset->InstancedObject == Original);
	TestEqual(TEXT("Undo restores original child value"), Asset->InstancedObject->TestProperty, 123);
	if (!TestTrue(TEXT("Reset redo succeeds"), GEditor->RedoTransaction()))
	{
		return false;
	}
	TestTrue(TEXT("Redo restores reset child pointer"), Asset->InstancedObject == ResetObject);
	TestEqual(TEXT("Redo restores default child value"), Asset->InstancedObject->TestProperty, DefaultValue);
	TestEqual(TEXT("CDO remains unchanged"), Defaults->InstancedObject->TestProperty, DefaultValue);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FResetNestedInstancedTest, "DataAssetManager.Integration.Reset.RejectNestedInstancedAtomically", DataAssetManagerFlags::Flags)

/** @brief Verifies unsupported nested references leave the whole asset and CDO unchanged. */
bool FResetNestedInstancedTest::RunTest(const FString& Parameters)
{
	using namespace DataAssetManagerTests;
	FScopedTestDataAsset Fixture(MakeUniqueAssetName(TEXT("ResetArray")), TestRootPath, UTestDataAssetWithInstancedArray::StaticClass());
	auto* Asset = Fixture.GetAssetAs<UTestDataAssetWithInstancedArray>();
	if (!Asset || Asset->Items.IsEmpty())
	{
		AddError(TEXT("Array fixture unavailable"));
		return false;
	}
	Asset->Value = 99;
	Asset->Items[0]->TestProperty = 123;
	UTestDataAssetSubobject* Original = Asset->Items[0];
	FDetailsObjectSet Objects;
	Objects.RootObjects.Add(Asset);
	Objects.CommonBaseClass = Asset->GetClass();
	DataAssetManager::ResetToCDO(Objects);
	TestEqual(TEXT("Scalar remains unchanged"), Asset->Value, 99);
	TestTrue(TEXT("Nested reference remains unchanged"), Asset->Items.Num() == 1 && Asset->Items[0] == Original);
	TestEqual(TEXT("Nested value remains unchanged"), Original->TestProperty, 123);
	TestEqual(TEXT("Default nested value remains unchanged"), GetDefault<UTestDataAssetWithInstancedArray>()->Items[0]->TestProperty, 7);
	return true;
}
