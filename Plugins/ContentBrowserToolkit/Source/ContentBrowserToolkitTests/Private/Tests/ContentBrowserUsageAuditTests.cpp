#include "ContentBrowserUsageAudit.h"
#include "Misc/AutomationTest.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/FileHelper.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCBTUsageClosureTest, "ContentBrowserToolkit.Audit.TransitiveCycles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Requires transitive protection, handles cycles and leaves disconnected assets outside the map closure.
 * @param Parameters Unused.
 * @return True when the graph preserves the complete reachable chain.
 */
bool FCBTUsageClosureTest::RunTest(const FString& Parameters)
{
	TMap<FName, TArray<FName>> Edges;
	Edges.Add(TEXT("Map"), {TEXT("Blueprint")});
	Edges.Add(TEXT("Blueprint"), {TEXT("Material")});
	Edges.Add(TEXT("Material"), {TEXT("Texture"), TEXT("Blueprint")});
	TMap<FName, FName> Parents;
	TestTrue(TEXT("Complete graph"), FContentBrowserUsageAudit::Traverse(
										 {TEXT("Map")},
										 [&](FName Name)
										 {
											 return Edges.FindRef(Name);
										 },
										 []()
										 {
											 return false;
										 },
										 Parents));
	TestTrue(TEXT("Indirect texture is protected"), Parents.Contains(TEXT("Texture")));
	TestEqual(TEXT("Chain explains material dependency"), Parents.FindRef(TEXT("Texture")), FName(TEXT("Material")));
	TestFalse(TEXT("Unrelated asset is not falsely attributed to the map"), Parents.Contains(TEXT("Unrelated")));
	TestEqual(TEXT("Cycles terminate without losing reachable nodes"), Parents.Num(), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCBTUsageCancelTest, "ContentBrowserToolkit.Audit.Cancellation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Requires cancellation to be observable instead of reporting a complete no-reference scan.
 * @param Parameters Unused.
 * @return True when a partial graph is explicitly rejected as complete.
 */
bool FCBTUsageCancelTest::RunTest(const FString& Parameters)
{
	TMap<FName, FName> Parents;
	TestFalse(TEXT("Cancellation cannot produce complete coverage"), FContentBrowserUsageAudit::Traverse(
																		 {TEXT("Map")},
																		 [](FName)
																		 {
																			 return TArray<FName>{TEXT("Texture")};
																		 },
																		 []()
																		 {
																			 return true;
																		 },
																		 Parents));
	TestFalse(TEXT("Unvisited dependency is absent from partial graph"), Parents.Contains(TEXT("Texture")));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCBTUsageSavedMapTest, "ContentBrowserToolkit.Audit.SavedMap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Exercises registry refresh, real map loading, classification and CSV export.
 * @param Parameters Unused.
 * @return True when a saved engine map is protected and its evidence is exported.
 */
bool FCBTUsageSavedMapTest::RunTest(const FString& Parameters)
{
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.SearchAllAssets(true);
	const FAssetData Map = Registry.GetAssetByObjectPath(FSoftObjectPath(TEXT("/Engine/Maps/Entry.Entry")));
	if (!TestTrue(TEXT("Engine entry map exists"), Map.IsValid()))
	{
		return false;
	}
	const FCBTUsageReport Report = FContentBrowserUsageAudit::Scan(TEXT("/Engine/Maps"), Map);
	TestTrue(TEXT("Real scan completed"), Report.Error.IsEmpty() && !Report.bCancelled);
	const auto* MapRow = Report.Rows.FindByPredicate(
		[&](const TSharedPtr<FCBTUsageRow>& Row)
		{
			return Row->Asset.PackageName == Map.PackageName;
		});
	if (TestNotNull(TEXT("Selected map is in results"), MapRow))
	{
		TestTrue(TEXT("Selected map is protected as used"), (*MapRow)->Status == ECBTUsageStatus::MapDependency);
		TestTrue(TEXT("Saved package size is reported"), (*MapRow)->Bytes > 0);
	}
	FString Csv;
	const FString Filename = FContentBrowserUsageAudit::Export(Report);
	TestTrue(TEXT("CSV is readable"), !Filename.IsEmpty() && FFileHelper::LoadFileToString(Csv, *Filename));
	TestTrue(TEXT("CSV includes map evidence and limitations"), Csv.Contains(TEXT("/Engine/Maps/Entry")) && Csv.Contains(TEXT("NOT deletion approval")));
	const FCBTUsageReport Invalid = FContentBrowserUsageAudit::Scan(TEXT("not a mounted folder"), FAssetData());
	TestTrue(TEXT("Invalid selection returns no candidates"), !Invalid.Error.IsEmpty() && Invalid.Rows.IsEmpty());
	return true;
}
#endif
