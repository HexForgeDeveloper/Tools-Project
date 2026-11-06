// Copyright (c) 2026 DimAlek. All Rights Reserved.
#include "Models/DataAssetListModel.h"
#include "Tests/DataAssetManagerTestTypes.h"

namespace
{
	/** @brief Builds registry metadata without creating or saving project content. */
	TSharedPtr<FAssetData> MakeSelectionAsset(const TCHAR* Name)
	{
		return MakeShared<FAssetData>(FName(*(FString(TEXT("/Game/SelectionTests/")) + Name)), FName(TEXT("/Game/SelectionTests")), FName(Name), UTestDataAsset::StaticClass()->GetClassPathName(), FAssetDataTagMap(), TArrayView<const int32>(), 0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FListSelectionRestoreTest, "DataAssetManager.Unit.ListSelection.RestoreAfterRefresh", DataAssetManagerFlags::Flags)

/** @brief Checks multi-selection rebinding, removal, filtering and rename mapping without a mock Slate widget. */
bool FListSelectionRestoreTest::RunTest(const FString& Parameters)
{
	const auto OldA = MakeSelectionAsset(TEXT("A"));
	const auto OldB = MakeSelectionAsset(TEXT("B"));
	const auto NewA = MakeSelectionAsset(TEXT("A"));
	const auto NewB = MakeSelectionAsset(TEXT("B"));
	const auto Added = MakeSelectionAsset(TEXT("Added"));
	auto Result = FDataAssetListModel::RestoreSelection({OldA, OldB}, {NewA, NewB, Added});
	TestEqual(TEXT("Both selected assets survive refresh"), Result.Num(), 2);
	TestTrue(TEXT("Selection uses refreshed metadata pointers"), Result.Contains(NewA) && Result.Contains(NewB));
	TestFalse(TEXT("Unrelated added asset is not selected"), Result.Contains(Added));
	Result = FDataAssetListModel::RestoreSelection({OldA, OldB}, {NewB});
	TestTrue(TEXT("Deleted or filtered-out entries are dropped"), Result.Num() == 1 && Result[0] == NewB);
	TestTrue(TEXT("Empty visible list clears selection"), FDataAssetListModel::RestoreSelection({OldA}, {}).IsEmpty());
	TestTrue(TEXT("Empty selection remains empty"), FDataAssetListModel::RestoreSelection({}, {NewA}).IsEmpty());
	const auto Renamed = MakeSelectionAsset(TEXT("Renamed"));
	Result = FDataAssetListModel::RestoreSelection({OldA, OldB}, {Renamed, NewB}, {{OldA->GetSoftObjectPath().ToString(), Renamed->GetSoftObjectPath().ToString()}});
	TestTrue(TEXT("Rename preserves both selected assets"), Result.Num() == 2 && Result.Contains(Renamed) && Result.Contains(NewB));
	Result = FDataAssetListModel::RestoreSelection({nullptr, MakeShared<FAssetData>(), OldA, OldA}, {nullptr, NewA});
	TestTrue(TEXT("Invalid and duplicate selections are ignored"), Result.Num() == 1 && Result[0] == NewA);
	return true;
}
