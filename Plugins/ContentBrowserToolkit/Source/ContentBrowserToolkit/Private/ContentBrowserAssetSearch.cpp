#include "ContentBrowserAssetSearch.h"

#include "Animation/AnimationAsset.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/BlueprintSupport.h"
#include "Engine/Blueprint.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstance.h"
#include "Misc/PackageName.h"
#include "Settings/ContentBrowserToolkitSettings.h"

namespace
{
	/** @brief Reads saved metadata without loading candidate assets. */
	IAssetRegistry& GetRegistry()
	{
		return FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	}

	/** @brief Normalizes both export-text references and plain object paths to the same key. */
	FSoftObjectPath ReadReference(const FAssetData& Asset, FName Tag)
	{
		FString Value;
		if (!Asset.GetTagValue(Tag, Value) || Value.IsEmpty() || Value == TEXT("None"))
		{
			return {};
		}
		return FSoftObjectPath(FPackageName::ExportTextPathToObjectPath(Value));
	}

	/** @brief Adds a result row without loading or retaining the reported asset. */
	void AddAssetRow(TArray<FAssetAuditRow>& Rows, const FAssetData& Asset, FString Summary, FString Details)
	{
		Rows.Add({ Asset.AssetName.ToString(), Asset.GetObjectPathString(), MoveTemp(Summary), MoveTemp(Details) });
	}

	/** @brief Finds direct and indirect descendants, visiting each key at most once per selected root. */
	TArray<FAssetAuditRow> FindRelatedAssets(ECBTAssetSearch Search, const TArray<FAssetData>& Selection)
	{
		const bool bBlueprint = Search == ECBTAssetSearch::ChildBlueprints;
		const bool bAnimation = Search == ECBTAssetSearch::SkeletonAnimations;
		const UClass* CandidateClass = bBlueprint ? UBlueprint::StaticClass()
			: bAnimation						  ? UAnimationAsset::StaticClass()
												  : UMaterialInstance::StaticClass();
		const FName ParentTag = bBlueprint ? FBlueprintTags::ParentClassPath : FName(bAnimation ? TEXT("Skeleton") : TEXT("Parent"));

		FARFilter Filter;
		Filter.ClassPaths.Add(CandidateClass->GetClassPathName());
		Filter.bRecursiveClasses = true;
		Filter.bIncludeOnlyOnDiskAssets = true;
		TArray<FAssetData> Candidates;
		GetRegistry().GetAssets(Filter, Candidates);

		TMap<FSoftObjectPath, TArray<int32>> Children;
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			const FSoftObjectPath Parent = ReadReference(Candidates[Index], ParentTag);
			if (!Parent.IsNull())
			{
				Children.FindOrAdd(Parent).Add(Index);
			}
		}

		TArray<FAssetAuditRow> Rows;
		TSet<FSoftObjectPath> ReportedRoots;
		for (const FAssetData& Asset : Selection)
		{
			const FSoftObjectPath Root = bBlueprint				 ? ReadReference(Asset, FBlueprintTags::GeneratedClassPath)
				: bAnimation && !Asset.IsInstanceOf<USkeleton>() ? ReadReference(Asset, ParentTag)
																 : Asset.GetSoftObjectPath();
			if (Root.IsNull())
			{
				AddAssetRow(Rows, Asset, TEXT("Reference unavailable"), TEXT("No usable registry reference was found. Save the asset and try again."));
				continue;
			}
			if (ReportedRoots.Contains(Root))
			{
				continue;
			}
			ReportedRoots.Add(Root);
			TArray<FSoftObjectPath> Queue{ Root };
			TSet<FSoftObjectPath> Visited{ Root };
			bool bFound = false;
			for (int32 Next = 0; Next < Queue.Num(); ++Next)
			{
				const FSoftObjectPath Current = Queue[Next];
				if (const TArray<int32>* Matches = Children.Find(Current))
				{
					for (const int32 Index : *Matches)
					{
						const FAssetData& Match = Candidates[Index];
						const FSoftObjectPath Key = bBlueprint ? ReadReference(Match, FBlueprintTags::GeneratedClassPath) : Match.GetSoftObjectPath();
						if (!Key.IsNull() && Visited.Contains(Key))
						{
							continue;
						}
						if (!Key.IsNull())
						{
							Visited.Add(Key);
							if (!bAnimation)
							{
								Queue.Add(Key);
							}
						}
						bFound = true;
						AddAssetRow(Rows, Match, bAnimation ? TEXT("Skeleton match") : Next == 0 ? TEXT("Direct match")
																								 : TEXT("Indirect match"),
							FString::Printf(TEXT("Search root: %s\nMatched reference: %s"), *Root.ToString(), *Current.ToString()));
					}
				}
			}
			if (!bFound)
			{
				AddAssetRow(Rows, Asset, TEXT("No saved matches"), Root.ToString());
			}
		}
		return Rows;
	}

	/** @brief Recognizes conventional suffixes without treating unrelated textures as a set. */
	int32 ParseTextureRole(const FString& Name, FString& Stem)
	{
		static const TPair<const TCHAR*, int32> Suffixes[] = {
			{ TEXT("_BaseColor"), 0 }, { TEXT("_Albedo"), 0 }, { TEXT("_Normal"), 1 }, { TEXT("_N"), 1 }, { TEXT("_ORM"), 2 }
		};
		for (const auto& Suffix : Suffixes)
		{
			if (Name.EndsWith(Suffix.Key))
			{
				Stem = Name.LeftChop(FCString::Strlen(Suffix.Key));
				return Stem.IsEmpty() ? INDEX_NONE : Suffix.Value;
			}
		}
		return INDEX_NONE;
	}

	/** @brief Checks only sets represented in the selection, looking for siblings in the same folder. */
	TArray<FAssetAuditRow> CheckTextureSets(const TArray<FAssetData>& Selection)
	{
		TArray<FAssetAuditRow> Rows;
		TMap<FName, TSet<FName>> SetsByFolder;
		for (const FAssetData& Asset : Selection)
		{
			FString Stem;
			if (ParseTextureRole(Asset.AssetName.ToString(), Stem) == INDEX_NONE)
			{
				AddAssetRow(Rows, Asset, TEXT("Unrecognized suffix"), TEXT("Expected _BaseColor, _Albedo, _Normal, _N or _ORM."));
				continue;
			}
			SetsByFolder.FindOrAdd(Asset.PackagePath).Add(FName(*Stem));
		}

		for (const auto& Folder : SetsByFolder)
		{
			TArray<FAssetData> Siblings;
			GetRegistry().GetAssetsByPath(Folder.Key, Siblings);
			TMap<FName, TArray<TPair<FAssetData, int32>>> Members;
			for (const FAssetData& Asset : Siblings)
			{
				FString Stem;
				const int32 Role = ParseTextureRole(Asset.AssetName.ToString(), Stem);
				if (Role != INDEX_NONE && Asset.IsInstanceOf<UTexture2D>() && Folder.Value.Contains(FName(*Stem)))
				{
					Members.FindOrAdd(FName(*Stem)).Emplace(Asset, Role);
				}
			}
			for (FName Stem : Folder.Value)
			{
				static const TCHAR* RoleNames[] = { TEXT("Base Color"), TEXT("Normal"), TEXT("ORM") };
				int32 Counts[UE_ARRAY_COUNT(RoleNames)] = {};
				TSet<FIntPoint> Sizes;
				bool bUnknownSize = false;
				TArray<FString> Details;
				for (const auto& Member : Members.FindOrAdd(Stem))
				{
					++Counts[Member.Value];
					const auto* Texture = Cast<UTexture2D>(Member.Key.GetAsset());
					FString Size = TEXT("source size unavailable");
					if (Texture && Texture->Source.IsValid())
					{
						const FIntPoint Dimensions(Texture->Source.GetSizeX(), Texture->Source.GetSizeY());
						Sizes.Add(Dimensions);
						Size = FString::Printf(TEXT("%d x %d"), Dimensions.X, Dimensions.Y);
					}
					else
					{
						bUnknownSize = true;
					}
					Details.Add(FString::Printf(TEXT("%s: %s (%s)"), RoleNames[Member.Value], *Member.Key.GetObjectPathString(), *Size));
				}
				TArray<FString> Issues;
				for (int32 Role = 0; Role < UE_ARRAY_COUNT(RoleNames); ++Role)
				{
					if (Counts[Role] != 1)
					{
						Issues.Add(FString::Printf(TEXT("%s %s"), Counts[Role] == 0 ? TEXT("Missing") : TEXT("Duplicate"), RoleNames[Role]));
					}
				}
				if (Sizes.Num() > 1)
				{
					Issues.Add(TEXT("Mismatched source sizes"));
				}
				if (bUnknownSize)
				{
					Issues.Add(TEXT("Source size unavailable"));
				}
				Rows.Add({ Stem.ToString(), Folder.Key.ToString(), Issues.IsEmpty() ? TEXT("Complete") : FString::Join(Issues, TEXT("; ")),
					FString::Join(Details, TEXT("\n")) });
			}
		}
		return Rows;
	}

	/** @brief Warns when a registry scan can omit results from a report. */
	void AddScanStatus(TArray<FAssetAuditRow>& Rows)
	{
		if (GetRegistry().IsLoadingAssets())
		{
			Rows.Add({ TEXT("Asset Registry"), FString(), TEXT("Scan in progress"), TEXT("Results may be incomplete. Run the report again once scanning finishes.") });
		}
	}
} // namespace

TArray<FAssetAuditRow> BuildContentBrowserAssetSearch(ECBTAssetSearch Search, const TArray<FAssetData>& Assets)
{
	TArray<FAssetAuditRow> Rows = Search == ECBTAssetSearch::TextureSets ? CheckTextureSets(Assets) : FindRelatedAssets(Search, Assets);
	AddScanStatus(Rows);
	return Rows;
}

TArray<FAssetAuditRow> BuildContentBrowserNamingAudit(const FString& Folder)
{
	TArray<FAssetData> Assets;
	GetRegistry().GetAssetsByPath(FName(*Folder), Assets, true);
	const auto& Prefixes = UContentBrowserToolkitSettings::Get()->NamingPrefixes;
	TArray<FAssetAuditRow> Rows;
	for (const FAssetData& Asset : Assets)
	{
		const FString* Prefix = Prefixes.Find(Asset.AssetClassPath.ToString());
		for (const UClass* Class = Prefix ? nullptr : Asset.GetClass(); Class && !Prefix; Class = Class->GetSuperClass())
		{
			Prefix = Prefixes.Find(Class->GetClassPathName().ToString());
		}
		if (Prefix && !Prefix->IsEmpty() && !Asset.AssetName.ToString().StartsWith(*Prefix, ESearchCase::CaseSensitive))
		{
			AddAssetRow(Rows, Asset, TEXT("Expected ") + *Prefix, TEXT("Asset class: ") + Asset.AssetClassPath.ToString());
		}
	}
	AddScanStatus(Rows);
	return Rows;
}
