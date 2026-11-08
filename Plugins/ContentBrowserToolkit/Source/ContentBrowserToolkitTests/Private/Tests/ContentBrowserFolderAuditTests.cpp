#include "ContentBrowserFolderAudit.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/PrimaryAssetLabel.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "UObject/SavePackage.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCBTFolderReportTest, "ContentBrowserToolkit.FolderReports.SavedPackages", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Checks recursive inventory, folder boundaries, saved soft references and CSV output against real packages. */
bool FCBTFolderReportTest::RunTest(const FString& Parameters)
{
	const FString Root = TEXT("/Game/CBTFolderAudit_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString Folder = Root / TEXT("Props");
	const FString TargetName = Folder / TEXT("Nested/Target");
	const FString InternalName = Folder / TEXT("Internal");
	const FString ExternalName = Root / TEXT("PropsExtra/External");
	TArray<FString> Files;
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	ON_SCOPE_EXIT
	{
		for (const FString& File : Files)
		{
			IFileManager::Get().Delete(*File);
		}
	};

	const auto SaveLabel = [&Files](const FString& PackageName, UPrimaryAssetLabel* Reference) -> UPrimaryAssetLabel* {
		UPackage* Package = CreatePackage(*PackageName);
		const FString Name = FPackageName::GetLongPackageAssetName(PackageName);
		auto* Label = NewObject<UPrimaryAssetLabel>(Package, *Name, RF_Public | RF_Standalone);
		if (Reference)
		{
			Label->ExplicitAssets.Add(Reference);
		}
		const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		Files.Add(Filename);
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		return UPackage::SavePackage(Package, Label, *Filename, Args) ? Label : nullptr;
	};
	auto* Target = SaveLabel(TargetName, nullptr);
	if (!TestNotNull(TEXT("Target package saved"), Target)
		|| !TestNotNull(TEXT("Internal referencer saved"), SaveLabel(InternalName, Target))
		|| !TestNotNull(TEXT("Sibling prefix referencer saved"), SaveLabel(ExternalName, Target)))
	{
		return false;
	}
	Registry.SearchAllAssets(true);
	Registry.ScanFilesSynchronous(Files, true);

	const FCBTFolderAuditReport Inventory = FContentBrowserFolderAudit::Scan(Folder, false);
	TestTrue(TEXT("Inventory succeeded"), Inventory.Error.IsEmpty());
	TestEqual(TEXT("Inventory excludes the sibling with the same prefix"), Inventory.Rows.Num(), 2);
	TestEqual(TEXT("Only the nested child folder is summarized"), Inventory.Subfolders.Num(), 1);
	TestEqual(TEXT("Class summary counts both assets"), Inventory.Classes.IsEmpty() ? 0 : Inventory.Classes[0].AssetCount, 2);
	TestTrue(TEXT("Known package bytes are reported"), Inventory.TotalBytes > 0 && Inventory.UnavailablePackages == 0);

	const FCBTFolderAuditReport References = FContentBrowserFolderAudit::Scan(Folder, true);
	TestTrue(TEXT("Reference audit succeeded"), References.Error.IsEmpty());
	const auto* TargetRow = References.Rows.FindByPredicate([&TargetName](const TSharedPtr<FCBTFolderAuditRow>& Row) {
		return Row->Asset.PackageName == FName(*TargetName);
	});
	if (!TestNotNull(TEXT("Nested target is reported"), TargetRow))
	{
		return false;
	}
	TestTrue(TEXT("External sibling is included and internal referencer excluded"),
		(*TargetRow)->ExternalReferencers == TArray<FName>{ FName(*ExternalName) });

	const FString CsvPath = FContentBrowserFolderAudit::Export(References);
	FString Csv;
	TestTrue(TEXT("CSV contains external reference evidence"), !CsvPath.IsEmpty() && FFileHelper::LoadFileToString(Csv, *CsvPath) && Csv.Contains(ExternalName));
	if (!CsvPath.IsEmpty())
	{
		IFileManager::Get().Delete(*CsvPath);
	}
	return true;
}
#endif
