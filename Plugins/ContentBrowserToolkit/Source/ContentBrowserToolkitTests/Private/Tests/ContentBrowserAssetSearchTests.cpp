#include "ContentBrowserAssetSearch.h"
#include "ContentBrowserInspection.h"
#include "Engine/PrimaryAssetLabel.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Settings/ContentBrowserToolkitSettings.h"
#include "UObject/SavePackage.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	/** @brief Isolates saved fixtures under a unique package path and removes their files and registry entries. */
	class FAssetSearchFixture
	{
	public:
		const FString Root = TEXT("/Game/CBTAssetSearch_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);

		/** @brief Removes only the packages created by this test. */
		~FAssetSearchFixture()
		{
			for (const auto& Entry : Packages)
			{
				IFileManager::Get().Delete(*Entry.Value);
				Registry.PackageDeleted(Entry.Key);
				Entry.Key->SetDirtyFlag(false);
			}
		}

		/** @brief Allocates an asset in its own package below the test root. */
		template <typename T>
		T* Create(const FString& RelativePath)
		{
			UPackage* Package = CreatePackage(*(Root / RelativePath));
			return NewObject<T>(Package, *FPackageName::GetLongPackageAssetName(RelativePath), RF_Public | RF_Standalone);
		}

		/** @brief Saves a real package so the query exercises tags read by the Asset Registry. */
		bool Save(UObject* Asset)
		{
			UPackage* Package = Asset->GetOutermost();
			const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
			Packages.Emplace(Package, File);
			FSavePackageArgs Args;
			Args.TopLevelFlags = RF_Public | RF_Standalone;
			return UPackage::SavePackage(Package, Asset, *File, Args);
		}

		/** @brief Waits for discovery and forces the registry to ingest the saved test files. */
		void Scan()
		{
			TArray<FString> Files;
			for (const auto& Entry : Packages)
			{
				Files.Add(Entry.Value);
			}
			Registry.SearchAllAssets(true);
			Registry.ScanFilesSynchronous(Files, true);
		}

		/** @brief Gets selection metadata from disk, matching the saved candidates in the search. */
		FAssetData GetSavedData(const UObject* Asset) const
		{
			return Registry.GetAssetByObjectPath(FSoftObjectPath(Asset), true);
		}

	private:
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		TArray<TPair<UPackage*, FString>> Packages;
	};

	/** @brief Finds evidence for the exact asset rather than relying on report ordering. */
	const FAssetAuditRow* FindRow(const TArray<FAssetAuditRow>& Rows, const UObject* Asset)
	{
		return Rows.FindByPredicate([Asset](const FAssetAuditRow& Row) { return Row.Path == Asset->GetPathName(); });
	}

	/** @brief Checks that the selected root has both direct and indirect matches, but no unrelated matches. */
	void CheckDescendants(FAutomationTestBase& Test, const TArray<FAssetAuditRow>& Rows, const UObject* Direct, const UObject* Indirect)
	{
		Test.TestEqual(TEXT("Only the two descendants are reported"), Rows.Num(), 2);
		const FAssetAuditRow* DirectRow = FindRow(Rows, Direct);
		const FAssetAuditRow* IndirectRow = FindRow(Rows, Indirect);
		Test.TestTrue(TEXT("Direct relationship is identified"), DirectRow && DirectRow->Summary == TEXT("Direct match"));
		Test.TestTrue(TEXT("Indirect relationship includes its parent"),
			IndirectRow && IndirectRow->Summary == TEXT("Indirect match") && IndirectRow->Details.Contains(Direct->GetPathName()));
	}

	/** @brief Creates and saves a normal Blueprint with an explicit parent class. */
	UBlueprint* SaveBlueprint(FAssetSearchFixture& Fixture, FName Name, UClass* Parent)
	{
		UPackage* Package = CreatePackage(*(Fixture.Root / Name.ToString()));
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(Parent, Package, Name, BPTYPE_Normal);
		return Blueprint && Blueprint->GeneratedClass && Fixture.Save(Blueprint) ? Blueprint : nullptr;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCBTMaterialSearchTest, "ContentBrowserToolkit.AssetSearch.MaterialInstances", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Finds descendants across two saved material instance links and excludes another material tree. */
bool FCBTMaterialSearchTest::RunTest(const FString& Parameters)
{
	FAssetSearchFixture Fixture;
	auto* Base = Fixture.Create<UMaterial>(TEXT("M_Base"));
	auto* Other = Fixture.Create<UMaterial>(TEXT("M_Other"));
	auto* Direct = Fixture.Create<UMaterialInstanceConstant>(TEXT("MI_Direct"));
	auto* Indirect = Fixture.Create<UMaterialInstanceConstant>(TEXT("MI_Indirect"));
	auto* Unrelated = Fixture.Create<UMaterialInstanceConstant>(TEXT("MI_Unrelated"));
	Direct->SetParentEditorOnly(Base, false);
	Indirect->SetParentEditorOnly(Direct, false);
	Unrelated->SetParentEditorOnly(Other, false);
	if (!TestTrue(TEXT("Material fixtures saved"), Fixture.Save(Base) && Fixture.Save(Other) && Fixture.Save(Direct) && Fixture.Save(Indirect) && Fixture.Save(Unrelated)))
	{
		return false;
	}
	Fixture.Scan();
	CheckDescendants(*this, BuildContentBrowserAssetSearch(ECBTAssetSearch::MaterialInstances, { Fixture.GetSavedData(Base) }), Direct, Indirect);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCBTBlueprintSearchTest, "ContentBrowserToolkit.AssetSearch.ChildBlueprints", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Verifies that Blueprint matching follows generated class paths rather than asset object paths. */
bool FCBTBlueprintSearchTest::RunTest(const FString& Parameters)
{
	FAssetSearchFixture Fixture;
	const UBlueprint* Base = SaveBlueprint(Fixture, TEXT("BP_Base"), AActor::StaticClass());
	const UBlueprint* Direct = Base ? SaveBlueprint(Fixture, TEXT("BP_Direct"), Base->GeneratedClass) : nullptr;
	const UBlueprint* Indirect = Direct ? SaveBlueprint(Fixture, TEXT("BP_Indirect"), Direct->GeneratedClass) : nullptr;
	const UBlueprint* Unrelated = SaveBlueprint(Fixture, TEXT("BP_Unrelated"), AActor::StaticClass());
	if (!TestTrue(TEXT("Blueprint fixtures saved"), Base && Direct && Indirect && Unrelated))
	{
		return false;
	}
	Fixture.Scan();
	const auto Rows = BuildContentBrowserAssetSearch(ECBTAssetSearch::ChildBlueprints, { Fixture.GetSavedData(Base) });
	TestEqual(TEXT("Only the two descendant Blueprints are reported"), Rows.Num(), 2);
	const auto* DirectRow = FindRow(Rows, Direct);
	const auto* IndirectRow = FindRow(Rows, Indirect);
	TestTrue(TEXT("Direct Blueprint match"), DirectRow && DirectRow->Summary == TEXT("Direct match"));
	TestTrue(TEXT("Indirect Blueprint match identifies the generated parent class"), IndirectRow && IndirectRow->Summary == TEXT("Indirect match") && IndirectRow->Details.Contains(Direct->GeneratedClass->GetPathName()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCBTSkeletonSearchTest, "ContentBrowserToolkit.AssetSearch.SkeletonAnimations", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Matches exact saved skeleton references and deduplicates a skeleton selected through two assets. */
bool FCBTSkeletonSearchTest::RunTest(const FString& Parameters)
{
	FAssetSearchFixture Fixture;
	auto* Skeleton = Fixture.Create<USkeleton>(TEXT("SKEL_Used"));
	auto* Other = Fixture.Create<USkeleton>(TEXT("SKEL_Other"));
	auto* First = Fixture.Create<UAnimSequence>(TEXT("AS_First"));
	auto* Second = Fixture.Create<UAnimSequence>(TEXT("AS_Second"));
	auto* Unrelated = Fixture.Create<UAnimSequence>(TEXT("AS_Unrelated"));
	First->SetSkeleton(Skeleton);
	Second->SetSkeleton(Skeleton);
	Unrelated->SetSkeleton(Other);
	if (!TestTrue(TEXT("Skeleton and animation fixtures saved"), Fixture.Save(Skeleton) && Fixture.Save(Other) && Fixture.Save(First) && Fixture.Save(Second) && Fixture.Save(Unrelated)))
	{
		return false;
	}
	Fixture.Scan();
	for (const TArray<FAssetData>& Selection : { TArray<FAssetData>{ Fixture.GetSavedData(First) },
			 TArray<FAssetData>{ Fixture.GetSavedData(Skeleton), Fixture.GetSavedData(First) } })
	{
		const auto Rows = BuildContentBrowserAssetSearch(ECBTAssetSearch::SkeletonAnimations, Selection);
		TestEqual(TEXT("Exact skeleton yields two rows without duplicates"), Rows.Num(), 2);
		TestNotNull(TEXT("First animation matches"), FindRow(Rows, First));
		TestNotNull(TEXT("Second animation matches"), FindRow(Rows, Second));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCBTTextureSetSearchTest, "ContentBrowserToolkit.AssetSearch.TextureSets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Detects missing, duplicate and differently sized roles while excluding a sibling folder. */
bool FCBTTextureSetSearchTest::RunTest(const FString& Parameters)
{
	FAssetSearchFixture Fixture;
	auto* Base = Fixture.Create<UTexture2D>(TEXT("Props/T_Rock_BaseColor"));
	auto* Alias = Fixture.Create<UTexture2D>(TEXT("Props/T_Rock_Albedo"));
	auto* Normal = Fixture.Create<UTexture2D>(TEXT("Props/T_Rock_N"));
	auto* OtherFolder = Fixture.Create<UTexture2D>(TEXT("Other/T_Rock_ORM"));
	for (UTexture2D* Texture : { Base, Alias, Normal, OtherFolder })
	{
		Texture->Source.Init(Texture == Normal ? 8 : 4, 4, 1, 1, TSF_BGRA8);
		if (!TestTrue(TEXT("Texture fixture saved"), Fixture.Save(Texture)))
		{
			return false;
		}
	}
	Fixture.Scan();
	const auto Rows = BuildContentBrowserAssetSearch(ECBTAssetSearch::TextureSets, { Fixture.GetSavedData(Base), Fixture.GetSavedData(Normal) });
	if (!TestEqual(TEXT("Multiple selected members produce one set"), Rows.Num(), 1))
	{
		return false;
	}
	TestTrue(TEXT("Reports all three issues"), Rows[0].Summary.Contains(TEXT("Duplicate Base Color")) && Rows[0].Summary.Contains(TEXT("Missing ORM")) && Rows[0].Summary.Contains(TEXT("Mismatched source sizes")));
	TestTrue(TEXT("Includes unselected sibling but excludes the other folder"),
		Rows[0].Details.Contains(Alias->GetPathName()) && !Rows[0].Details.Contains(OtherFolder->GetPathName()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCBTNamingAuditTest, "ContentBrowserToolkit.AssetSearch.FolderNaming", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Applies a configured inherited prefix recursively, with case and package boundaries respected. */
bool FCBTNamingAuditTest::RunTest(const FString& Parameters)
{
	FAssetSearchFixture Fixture;
	auto* Good = Fixture.Create<UTexture2D>(TEXT("Props/Tex_Valid"));
	auto* Bad = Fixture.Create<UTexture2D>(TEXT("Props/Nested/tex_Wrong"));
	auto* Outside = Fixture.Create<UTexture2D>(TEXT("PropsExtra/Bad"));
	for (UTexture2D* Texture : { Good, Bad, Outside })
	{
		Texture->Source.Init(4, 4, 1, 1, TSF_BGRA8);
		if (!TestTrue(TEXT("Naming fixture saved"), Fixture.Save(Texture)))
		{
			return false;
		}
	}
	Fixture.Scan();
	const TMap<FString, FString> Rules{ { TEXT("/Script/Engine.Texture"), TEXT("Tex_") } };
	TGuardValue<TMap<FString, FString>> RestoreRules(UContentBrowserToolkitSettings::GetMutable()->NamingPrefixes, Rules);
	const auto Rows = BuildContentBrowserNamingAudit(Fixture.Root / TEXT("Props"));
	TestEqual(TEXT("Only the nested violation is reported"), Rows.Num(), 1);
	const auto* Row = FindRow(Rows, Bad);
	TestTrue(TEXT("Report includes the configured prefix"), Row && Row->Summary == TEXT("Expected Tex_"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCBTMigrationPreviewTest, "ContentBrowserToolkit.Inspection.MigrationPreview", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Verifies a relative destination conflict and transitive dependencies on saved packages. */
bool FCBTMigrationPreviewTest::RunTest(const FString& Parameters)
{
	FAssetSearchFixture Fixture;
	auto* Source = Fixture.Create<UPrimaryAssetLabel>(TEXT("Source/Nested/Item"));
	auto* Dependency = Fixture.Create<UPrimaryAssetLabel>(TEXT("Other/Dependency"));
	auto* Indirect = Fixture.Create<UPrimaryAssetLabel>(TEXT("Other/Indirect"));
	auto* Inbound = Fixture.Create<UPrimaryAssetLabel>(TEXT("SourceExtra/Inbound"));
	auto* Conflict = Fixture.Create<UTexture2D>(TEXT("Destination/Nested/Item"));
	Conflict->Source.Init(4, 4, 1, 1, TSF_BGRA8);
	Source->ExplicitAssets.Add(Dependency);
	Dependency->ExplicitAssets.Add(Indirect);
	Inbound->ExplicitAssets.Add(Source);
	for (UObject* Asset : TArray<UObject*>{ Source, Dependency, Indirect, Inbound, Conflict })
	{
		if (!TestTrue(TEXT("Saved migration fixture"), Fixture.Save(Asset)))
		{
			return false;
		}
	}
	Fixture.Scan();
	const FString SourceFolder = Fixture.Root / TEXT("Source");
	const auto Rows = BuildCBTMigrationPreview(SourceFolder, Fixture.Root / TEXT("Destination"));
	TestTrue(TEXT("Finds the conflict in the relative nested folder"), Rows.ContainsByPredicate([Conflict](const FAssetAuditRow& Row) {
		return Row.Summary == TEXT("Destination conflict") && Row.Details == Conflict->GetOutermost()->GetName();
	}));
	TestTrue(TEXT("Finds inbound references from sibling folders with a similar prefix"), Rows.ContainsByPredicate([Inbound](const FAssetAuditRow& Row) {
		return Row.Summary == TEXT("External referencer") && Row.Details == Inbound->GetOutermost()->GetName();
	}));
	TestTrue(TEXT("Includes dependencies beyond one hop"), Rows.ContainsByPredicate([Indirect](const FAssetAuditRow& Row) {
		return Row.Summary == TEXT("External dependency") && Row.Path == Indirect->GetOutermost()->GetName();
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCBTCleanupCandidatesTest, "ContentBrowserToolkit.Inspection.CleanupCandidates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Distinguishes a package with a saved soft referencer from one with no observed referencers. */
bool FCBTCleanupCandidatesTest::RunTest(const FString& Parameters)
{
	FAssetSearchFixture Fixture;
	auto* Used = Fixture.Create<UPrimaryAssetLabel>(TEXT("Assets/Used"));
	auto* Candidate = Fixture.Create<UPrimaryAssetLabel>(TEXT("Assets/Candidate"));
	auto* Referencer = Fixture.Create<UPrimaryAssetLabel>(TEXT("External/Referencer"));
	Referencer->ExplicitAssets.Add(Used);
	if (!TestTrue(TEXT("Saved cleanup fixtures"), Fixture.Save(Used) && Fixture.Save(Candidate) && Fixture.Save(Referencer)))
	{
		return false;
	}
	Fixture.Scan();
	const auto Rows = BuildCBTCleanupCandidates(Fixture.Root / TEXT("Assets"));
	TestNull(TEXT("Asset with a saved soft referencer is excluded"), FindRow(Rows, Used));
	const auto* Row = FindRow(Rows, Candidate);
	TestTrue(TEXT("Unreferenced candidate includes its reason"), Row && Row->Summary == TEXT("No saved referencers found"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCBTTextureDuplicatesTest, "ContentBrowserToolkit.Inspection.TextureDuplicates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Distinguishes identical source pixels from different pixels and excludes other folders. */
bool FCBTTextureDuplicatesTest::RunTest(const FString& Parameters)
{
	FAssetSearchFixture Fixture;
	auto* Base = Fixture.Create<UTexture2D>(TEXT("Textures/T_Base"));
	auto* Same = Fixture.Create<UTexture2D>(TEXT("Textures/T_Same"));
	auto* Different = Fixture.Create<UTexture2D>(TEXT("Textures/T_Different"));
	auto* OtherFolder = Fixture.Create<UTexture2D>(TEXT("Other/T_Same"));
	uint8 Pixels[4 * 4 * 4] = {};
	for (UTexture2D* Texture : { Base, Same, OtherFolder, Different })
	{
		Pixels[0] = Texture == Different ? 1 : 0;
		Texture->Source.Init(4, 4, 1, 1, TSF_BGRA8, Pixels);
		Texture->SRGB = Texture != Same;
		if (!TestTrue(TEXT("Saved texture fixture"), Fixture.Save(Texture)))
		{
			return false;
		}
	}
	Fixture.Scan();
	const auto Rows = BuildCBTInspection(ECBTInspection::TextureDuplicates, { Fixture.GetSavedData(Base) });
	TestEqual(TEXT("Only the identical sibling is reported"), Rows.Num(), 1);
	const auto* Row = FindRow(Rows, Same);
	TestTrue(TEXT("Matching pixels and differing sRGB are reported"), Row && Row->Summary == TEXT("Matching source pixels") && Row->Details.Contains(TEXT("sRGB: 1 / 0")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCBTUsageChainInspectionTest, "ContentBrowserToolkit.Inspection.UsageChains", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Verifies that a saved map is a valid zero-hop usage chain for itself. */
bool FCBTUsageChainInspectionTest::RunTest(const FString& Parameters)
{
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.SearchAllAssets(true);
	const FAssetData Map = Registry.GetAssetByObjectPath(FSoftObjectPath(TEXT("/Engine/Maps/Entry.Entry")), true);
	if (!TestTrue(TEXT("Engine entry map is available"), Map.IsValid()))
	{
		return false;
	}
	const auto Rows = BuildCBTInspection(ECBTInspection::UsageChains, { Map });
	TestTrue(TEXT("Includes the zero-hop map chain"), Rows.ContainsByPredicate([&Map](const FAssetAuditRow& Row) {
		return Row.Summary == TEXT("Map usage") && Row.Details == Map.PackageName.ToString();
	}));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCBTAnimationSetInspectionTest, "ContentBrowserToolkit.Inspection.AnimationSet", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Matches the notify name visible in the editor, without requiring the internal event prefix. */
bool FCBTAnimationSetInspectionTest::RunTest(const FString& Parameters)
{
	FAssetSearchFixture Fixture;
	auto* Sequence = Fixture.Create<UAnimSequence>(TEXT("AS_Attack"));
	Sequence->Notifies.AddDefaulted_GetRef().NotifyName = TEXT("HitWindow");
	auto* Settings = UContentBrowserToolkitSettings::GetMutable();
	const TGuardValue<TArray<FName>> RestoreNotifies(Settings->RequiredAnimationNotifies, { TEXT("HitWindow"), TEXT("Recover") });
	const TGuardValue<TArray<FName>> RestoreCurves(Settings->RequiredAnimationCurves, { TEXT("Weight") });
	const auto Rows = BuildCBTInspection(ECBTInspection::AnimationSet, { FAssetData(Sequence) });
	const auto* Row = FindRow(Rows, Sequence);
	TestTrue(TEXT("Reports the missing notify and float curve but accepts HitWindow"),
		Row && Row->Details.Contains(TEXT("Missing notify: Recover")) && Row->Details.Contains(TEXT("Missing float curve: Weight"))
			&& !Row->Details.Contains(TEXT("Missing notify: HitWindow")));
	return true;
}
#endif
