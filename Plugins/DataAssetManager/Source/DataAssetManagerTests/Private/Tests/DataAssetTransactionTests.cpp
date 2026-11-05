// Copyright (c) 2026 DimAlek. All Rights Reserved.

#include "Tests/DataAssetManagerTestHelpers.h"
#include "FunctionLibrary/DataAssetManagerFunctionLibrary.h"
#include "Services/DataAssetManagerAssetService.h"
#include "Editor.h"
#include "IDetailRootObjectCustomization.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDataAssetTransactionTest,
	"DataAssetManager.Integration.Transactions.ImportCopyResetUndoRedo",
	DataAssetManagerFlags::Flags)

/** @brief Verifies copy, reset and JSON import transactions against real editor-owned Data Assets. */
bool FDataAssetTransactionTest::RunTest(const FString& Parameters)
{
	using namespace DataAssetManagerTests;
	if (!TestNotNull(TEXT("Editor transaction system is available"), GEditor))
	{
		return false;
	}
	FScopedTestDataAsset Source(MakeUniqueAssetName(TEXT("TransactionSource")));
	FScopedTestDataAsset Target(MakeUniqueAssetName(TEXT("TransactionTarget")));
	UTestDataAsset* SourceAsset = Source.GetAssetAs<UTestDataAsset>();
	UTestDataAsset* TargetAsset = Target.GetAssetAs<UTestDataAsset>();
	if (!TestNotNull(TEXT("Source created"), SourceAsset) || !TestNotNull(TEXT("Target created"), TargetAsset))
	{
		return false;
	}
	SourceAsset->SetFlags(RF_Transactional);
	TargetAsset->SetFlags(RF_Transactional);
	SourceAsset->TestProperty = 42;
	TargetAsset->TestProperty = 7;

	// Each undo immediately follows the operation being verified; never clear the user's history.
	const auto VerifyUndoRedo = [this, TargetAsset](int32 Before, int32 After) {
		TestEqual(TEXT("Operation applies the expected value"), TargetAsset->TestProperty, After);
		if (!TestTrue(TEXT("Operation can be undone"), GEditor->UndoTransaction())) { return false; }
		TestEqual(TEXT("Undo restores the previous value"), TargetAsset->TestProperty, Before);
		if (!TestTrue(TEXT("Operation can be redone"), GEditor->RedoTransaction())) { return false; }
		TestEqual(TEXT("Redo restores the applied value"), TargetAsset->TestProperty, After);
		return true;
	};
	if (!TestTrue(TEXT("Copy succeeds"), FDataAssetManagerAssetService::CopyDiffPropertyValue(
		Source.MakeSharedAssetData(), Target.MakeSharedAssetData(), GET_MEMBER_NAME_CHECKED(UTestDataAsset, TestProperty)))
		|| !VerifyUndoRedo(7, 42)) { return false; }

	FDetailsObjectSet RootObjects;
	RootObjects.RootObjects.Add(TargetAsset);
	RootObjects.CommonBaseClass = UTestDataAsset::StaticClass();
	DataAssetManager::ResetToCDO(RootObjects);
	const int32 DefaultValue = GetDefault<UTestDataAsset>()->TestProperty;
	if (!VerifyUndoRedo(42, DefaultValue)) { return false; }

	const FString JsonFile = FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("DataAssetTransaction"), TEXT(".json"));
	ON_SCOPE_EXIT { IFileManager::Get().Delete(*JsonFile); };
	if (!TestTrue(TEXT("Export succeeds"), DataAssetManager::SaveDataAssetToJsonFile(SourceAsset, JsonFile))
		|| !TestTrue(TEXT("Import succeeds"), DataAssetManager::LoadDataAssetFromJsonFile(TargetAsset, JsonFile))) { return false; }
	VerifyUndoRedo(DefaultValue, 42);
	return true;
}

#endif
