// Copyright (c) 2026 DimAlek. All Rights Reserved.

#include "Tests/DataAssetManagerTestHelpers.h"

#include "Editor.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "FunctionLibrary/DataAssetManagerFunctionLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataAssetJsonRejectedArrayTest,
	"DataAssetManager.Integration.JsonImport.RejectsInvalidArrayElement",
	DataAssetManagerFlags::Flags)

/** @brief Verifies that a conversion failure inside an array leaves a saved asset clean and unchanged. */
bool FDataAssetJsonRejectedArrayTest::RunTest(const FString& Parameters)
{
	using namespace DataAssetManagerTests;
	FScopedTestDataAsset Fixture(MakeUniqueAssetName(TEXT("JsonRejectedArray")));
	UTestDataAsset* Asset = Fixture.GetAssetAs<UTestDataAsset>();
	if (!TestNotNull(TEXT("Target asset exists"), Asset))
	{
		return false;
	}

	Asset->TestProperty = 7;
	const TArray<FLinearColor> OriginalColors{ FLinearColor::Red, FLinearColor::Green, FLinearColor::Blue };
	Asset->Colors = OriginalColors;
	if (!TestTrue(TEXT("Baseline asset is saved"), Fixture.Save())
		|| !TestFalse(TEXT("Baseline package is clean"), Asset->GetOutermost()->IsDirty()))
	{
		return false;
	}

	const FString JsonFile = FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("DataAssetRejectedArray"), TEXT(".json"));
	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*JsonFile);
	};
	const FString Json = TEXT(R"({"testProperty":99,"colors":[{"r":0.25,"g":0.5,"b":0.75,"a":1},false]})");
	if (!TestTrue(TEXT("Invalid array fixture is written"), FFileHelper::SaveStringToFile(Json, *JsonFile)))
	{
		return false;
	}

	AddExpectedError(TEXT("JSON value that is neither string nor object"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("Unable to import Array element 1 for property Colors"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("Unable to import JSON value into property Colors"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Import rejects the second array element"), DataAssetManager::LoadDataAssetFromJsonFile(Asset, JsonFile));
	TestEqual(TEXT("Rejected import preserves the earlier scalar"), Asset->TestProperty, 7);
	TestTrue(TEXT("Rejected import preserves every array element and its length"), Asset->Colors == OriginalColors);
	TestFalse(TEXT("Rejected import leaves the package clean"), Asset->GetOutermost()->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataAssetJsonPartialImportTest,
	"DataAssetManager.Integration.JsonImport.PartialUpdateUndoRedo",
	DataAssetManagerFlags::Flags)

/** @brief Checks that a partial JSON import preserves omitted properties and is one undoable editor operation. */
bool FDataAssetJsonPartialImportTest::RunTest(const FString& Parameters)
{
	using namespace DataAssetManagerTests;
	FScopedTestDataAsset Fixture(MakeUniqueAssetName(TEXT("JsonPartialUpdate")));
	UTestDataAsset* Asset = Fixture.GetAssetAs<UTestDataAsset>();
	if (!TestNotNull(TEXT("Editor transaction system is available"), GEditor)
		|| !TestNotNull(TEXT("Target asset exists"), Asset))
	{
		return false;
	}

	Asset->SetFlags(RF_Transactional);
	Asset->TestProperty = 7;
	Asset->Color = FLinearColor::Green;
	Asset->Colors = { FLinearColor::Red, FLinearColor::Blue };
	const FString JsonFile = FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("DataAssetPartialUpdate"), TEXT(".json"));
	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*JsonFile);
	};
	if (!TestTrue(TEXT("Partial JSON fixture is written"), FFileHelper::SaveStringToFile(TEXT(R"({"testProperty":42,"colors":[]})"), *JsonFile))
		|| !TestTrue(TEXT("Partial import succeeds"), DataAssetManager::LoadDataAssetFromJsonFile(Asset, JsonFile)))
	{
		return false;
	}

	TestEqual(TEXT("Import updates the scalar"), Asset->TestProperty, 42);
	TestTrue(TEXT("Explicit empty array clears the previous contents"), Asset->Colors.IsEmpty());
	TestEqual(TEXT("Import preserves the omitted color"), Asset->Color, FLinearColor::Green);

	if (!TestTrue(TEXT("Partial import can be undone"), GEditor->UndoTransaction()))
	{
		return false;
	}
	TestEqual(TEXT("Undo restores the scalar"), Asset->TestProperty, 7);
	TestTrue(TEXT("Undo restores both original array elements"), Asset->Colors == TArray<FLinearColor>{ FLinearColor::Red, FLinearColor::Blue });
	TestEqual(TEXT("Undo preserves the omitted color"), Asset->Color, FLinearColor::Green);

	if (!TestTrue(TEXT("Partial import can be redone"), GEditor->RedoTransaction()))
	{
		return false;
	}
	TestEqual(TEXT("Redo reapplies the scalar"), Asset->TestProperty, 42);
	TestTrue(TEXT("Redo clears the array again"), Asset->Colors.IsEmpty());
	TestEqual(TEXT("Redo preserves the omitted color"), Asset->Color, FLinearColor::Green);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataAssetJsonDerivedSubobjectTest,
	"DataAssetManager.Integration.JsonImport.PreservesDerivedSubobject",
	DataAssetManagerFlags::Flags)

/** @brief Verifies that import retains the exported subobject class, values and ownership. */
bool FDataAssetJsonDerivedSubobjectTest::RunTest(const FString& Parameters)
{
	using namespace DataAssetManagerTests;
	FScopedTestDataAsset Source(MakeUniqueAssetName(TEXT("JsonDerivedSource")), TestRootPath, UTestDataAssetWithInstancedObject::StaticClass());
	FScopedTestDataAsset Target(MakeUniqueAssetName(TEXT("JsonDerivedTarget")), TestRootPath, UTestDataAssetWithInstancedObject::StaticClass());
	auto* From = Source.GetAssetAs<UTestDataAssetWithInstancedObject>();
	auto* To = Target.GetAssetAs<UTestDataAssetWithInstancedObject>();
	if (!TestNotNull(TEXT("Source exists"), From) || !TestNotNull(TEXT("Target exists"), To))
	{
		return false;
	}

	auto* Subobject = NewObject<UTestDerivedDataAssetSubobject>(From);
	Subobject->TestProperty = 23;
	Subobject->DerivedValue = 71;
	From->InstancedObject = Subobject;
	const FString JsonFile = FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("DataAssetDerived"), TEXT(".json"));
	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*JsonFile);
	};
	if (!TestTrue(TEXT("Derived subobject exports"), DataAssetManager::SaveDataAssetToJsonFile(From, JsonFile))
		|| !TestTrue(TEXT("Derived subobject imports"), DataAssetManager::LoadDataAssetFromJsonFile(To, JsonFile)))
	{
		return false;
	}

	const auto* Imported = Cast<UTestDerivedDataAssetSubobject>(To->InstancedObject.Get());
	if (!TestNotNull(TEXT("Imported subobject retains its derived class"), Imported))
	{
		return false;
	}
	TestTrue(TEXT("Imported subobject is owned by the target"), Imported->GetOuter() == To);
	TestEqual(TEXT("Inherited value is imported"), Imported->TestProperty, 23);
	TestEqual(TEXT("Derived value is imported"), Imported->DerivedValue, 71);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataAssetJsonBlueprintPropertyTest,
	"DataAssetManager.Integration.JsonImport.BlueprintPropertyUndoRedo",
	DataAssetManagerFlags::Flags)

/** @brief Imports a Blueprint-declared property into an asset instance and verifies Undo/Redo. */
bool FDataAssetJsonBlueprintPropertyTest::RunTest(const FString& Parameters)
{
	using namespace DataAssetManagerTests;
	if (!TestNotNull(TEXT("Editor transaction system is available"), GEditor))
	{
		return false;
	}

	const FName BlueprintName(*MakeUniqueAssetName(TEXT("JsonBlueprint")));
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		UPrimaryDataAsset::StaticClass(), GetTransientPackage(), BlueprintName, BPTYPE_Normal,
		UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
	const FName PropertyName(TEXT("ReleaseTestValue"));
	FEdGraphPinType PinType;
	PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	if (!TestNotNull(TEXT("Blueprint exists"), Blueprint)
		|| !TestTrue(TEXT("Blueprint integer variable is declared"), FBlueprintEditorUtils::AddMemberVariable(Blueprint, PropertyName, PinType)))
	{
		return false;
	}
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	if (!TestNotNull(TEXT("Blueprint class is compiled"), Blueprint->GeneratedClass.Get()))
	{
		return false;
	}
	const FIntProperty* Property = FindFProperty<FIntProperty>(Blueprint->GeneratedClass, PropertyName);
	if (!TestNotNull(TEXT("Generated class owns the integer property"), Property))
	{
		return false;
	}

	FScopedTestDataAsset Fixture(MakeUniqueAssetName(TEXT("JsonBlueprintInstance")), TestRootPath, Blueprint->GeneratedClass.Get());
	UDataAsset* Asset = Fixture.GetAssetAs<UDataAsset>();
	if (!TestNotNull(TEXT("Blueprint asset instance exists"), Asset))
	{
		return false;
	}
	Asset->SetFlags(RF_Transactional);
	Property->SetPropertyValue_InContainer(Asset, 7);

	const FString JsonFile = FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("DataAssetBlueprint"), TEXT(".json"));
	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*JsonFile);
	};
	if (!TestTrue(TEXT("Blueprint JSON fixture is written"), FFileHelper::SaveStringToFile(TEXT(R"({"releaseTestValue":42})"), *JsonFile))
		|| !TestTrue(TEXT("Blueprint property imports"), DataAssetManager::LoadDataAssetFromJsonFile(Asset, JsonFile)))
	{
		return false;
	}
	TestEqual(TEXT("Import updates the Blueprint property"), Property->GetPropertyValue_InContainer(Asset), 42);
	if (!TestTrue(TEXT("Blueprint import can be undone"), GEditor->UndoTransaction()))
	{
		return false;
	}
	TestEqual(TEXT("Undo restores the Blueprint property"), Property->GetPropertyValue_InContainer(Asset), 7);
	if (!TestTrue(TEXT("Blueprint import can be redone"), GEditor->RedoTransaction()))
	{
		return false;
	}
	TestEqual(TEXT("Redo reapplies the Blueprint property"), Property->GetPropertyValue_InContainer(Asset), 42);
	return true;
}

#endif
