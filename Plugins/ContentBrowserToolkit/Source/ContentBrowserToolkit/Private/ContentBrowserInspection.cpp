#include "ContentBrowserInspection.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"

/** @brief Inspects loaded assets for class-specific settings and comparison evidence. */
TArray<FAssetAuditRow> BuildCBTTypeInspection(ECBTInspection Inspection, const TArray<FAssetData>& Assets);

namespace
{
	using namespace UE::AssetRegistry;

	/** @brief Queries saved package edges without loading packages. */
	IAssetRegistry& InspectionRegistry()
	{
		return FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	}

	/** @brief Tests a package path against a folder boundary. */
	bool IsBelowFolder(FName Path, const FString& Folder)
	{
		const FString Name = Path.ToString();
		return Name.Equals(Folder, ESearchCase::IgnoreCase) || Name.StartsWith(Folder + TEXT("/"), ESearchCase::IgnoreCase);
	}

	/** @brief Returns whether any saved asset in this package represents a map. */
	bool IsMapPackage(IAssetRegistry& Registry, FName Package)
	{
		TArray<FAssetData> Assets;
		Registry.GetAssetsByPackageName(Package, Assets, true);
		return Assets.ContainsByPredicate([](const FAssetData& Asset) { return Asset.IsInstanceOf<UWorld>(); });
	}

	/** @brief Walks reverse package edges breadth first, preserving one shortest path per map and selection. */
	TArray<FAssetAuditRow> FindUsageChains(const TArray<FAssetData>& Assets)
	{
		IAssetRegistry& Registry = InspectionRegistry();
		TArray<FAssetAuditRow> Rows;
		for (const FAssetData& Asset : Assets)
		{
			const int32 FirstRow = Rows.Num();
			TArray<FName> Queue{ Asset.PackageName };
			TMap<FName, FName> TowardSelection;
			TowardSelection.Add(Asset.PackageName, NAME_None);
			for (int32 Index = 0; Index < Queue.Num(); ++Index)
			{
				const FName Package = Queue[Index];
				if (IsMapPackage(Registry, Package))
				{
					TArray<FString> Chain;
					for (FName Step = Package; !Step.IsNone(); Step = TowardSelection.FindChecked(Step))
					{
						Chain.Add(Step.ToString());
					}
					Rows.Add({ Asset.AssetName.ToString(), Asset.GetObjectPathString(), TEXT("Map usage"), FString::Join(Chain, TEXT(" -> ")) });
				}

				TArray<FName> Referencers;
				Registry.GetReferencers(Package, Referencers, EDependencyCategory::Package);
				Referencers.Sort(FNameLexicalLess());
				for (FName Referencer : Referencers)
				{
					if (!TowardSelection.Contains(Referencer))
					{
						TowardSelection.Add(Referencer, Package);
						Queue.Add(Referencer);
					}
				}
			}
			if (Rows.Num() == FirstRow)
			{
				Rows.Add({ Asset.AssetName.ToString(), Asset.GetObjectPathString(), TEXT("No saved map chain found"),
					TEXT("The report uses package edges known to the Asset Registry.") });
			}
		}
		return Rows;
	}

	/** @brief Collects a package closure with each package processed once, including cycles. */
	TArray<FName> CollectDependencyClosure(IAssetRegistry& Registry, const TArray<FName>& Roots, TMap<FName, FName>* Parents = nullptr)
	{
		TArray<FName> Queue = Roots;
		TSet<FName> Visited(Roots);
		for (int32 Index = 0; Index < Queue.Num(); ++Index)
		{
			TArray<FName> Dependencies;
			Registry.GetDependencies(Queue[Index], Dependencies, EDependencyCategory::Package);
			for (FName Dependency : Dependencies)
			{
				if (!Visited.Contains(Dependency) && !Dependency.ToString().StartsWith(TEXT("/Script/")))
				{
					Visited.Add(Dependency);
					if (Parents)
					{
						Parents->Add(Dependency, Queue[Index]);
					}
					Queue.Add(Dependency);
				}
			}
		}
		return Queue;
	}

	/** @brief Reads the saved package file size when the package is available on disk. */
	int64 ReadPackageBytes(FName Package)
	{
		FString File;
		return FPackageName::DoesPackageExist(Package.ToString(), &File) ? IFileManager::Get().FileSize(*File) : -1;
	}

	/** @brief Reports direct hard/soft edges and ranks transitive dependencies by saved package bytes. */
	TArray<FAssetAuditRow> InspectBlueprintDependencies(const TArray<FAssetData>& Assets)
	{
		IAssetRegistry& Registry = InspectionRegistry();
		TArray<FAssetAuditRow> Rows;
		for (const FAssetData& Asset : Assets)
		{
			TArray<FName> Hard;
			TArray<FName> Soft;
			Registry.GetDependencies(Asset.PackageName, Hard, EDependencyCategory::Package, EDependencyQuery::Hard);
			Registry.GetDependencies(Asset.PackageName, Soft, EDependencyCategory::Package, EDependencyQuery::Soft);
			TArray<FString> Details;
			for (const auto& Edges : { TPair<const TCHAR*, const TArray<FName>*>(TEXT("Hard"), &Hard), { TEXT("Soft"), &Soft } })
			{
				for (FName Package : *Edges.Value)
				{
					const FString Path = Package.ToString();
					const FString Root = FPackageName::GetPackageMountPoint(Path).ToString();
					Details.Add(FString::Printf(TEXT("%s: %s%s"), Edges.Key, *Path,
						Root == FPackageName::GetPackageMountPoint(Asset.PackageName.ToString()).ToString() || Root == TEXT("Engine") || Root == TEXT("Script") ? TEXT("") : TEXT(" [external mount]")));
				}
			}
			Details.Sort();
			TArray<TPair<FName, int64>> Sizes;
			TMap<FName, FName> Parents;
			for (FName Package : CollectDependencyClosure(Registry, { Asset.PackageName }, &Parents))
			{
				if (Package != Asset.PackageName)
				{
					Sizes.Emplace(Package, ReadPackageBytes(Package));
				}
			}
			Sizes.Sort([](const auto& A, const auto& B) { return A.Value > B.Value; });
			Details.Add(TEXT("\nLargest reachable package files:"));
			for (int32 Index = 0; Index < FMath::Min(Sizes.Num(), 10) && Sizes[Index].Value >= 0; ++Index)
			{
				TArray<FString> Chain;
				for (FName Step = Sizes[Index].Key; !Step.IsNone(); Step = Parents.FindRef(Step))
				{
					Chain.Insert(Step.ToString(), 0);
				}
				Details.Add(FString::Printf(TEXT("%lld bytes: %s"), Sizes[Index].Value, *FString::Join(Chain, TEXT(" -> "))));
			}
			Rows.Add({ Asset.AssetName.ToString(), Asset.GetObjectPathString(),
				FString::Printf(TEXT("%d hard / %d soft direct references"), Hard.Num(), Soft.Num()), FString::Join(Details, TEXT("\n")) });
		}
		return Rows;
	}

	/** @brief Marks incomplete registry snapshots in the report. */
	void AddDiscoveryStatus(TArray<FAssetAuditRow>& Rows)
	{
		if (InspectionRegistry().IsLoadingAssets())
		{
			Rows.Add({ TEXT("Asset Registry"), {}, TEXT("Scan in progress"), TEXT("Results may be incomplete. Run again after discovery finishes.") });
		}
	}
} // namespace

TArray<FAssetAuditRow> BuildCBTInspection(ECBTInspection Inspection, const TArray<FAssetData>& Assets)
{
	TArray<FAssetAuditRow> Rows;
	switch (Inspection)
	{
		case ECBTInspection::UsageChains:
			Rows = FindUsageChains(Assets);
			break;
		case ECBTInspection::BlueprintDependencies:
			Rows = InspectBlueprintDependencies(Assets);
			break;
		default:
			Rows = BuildCBTTypeInspection(Inspection, Assets);
			break;
	}
	AddDiscoveryStatus(Rows);
	return Rows;
}

TArray<FAssetAuditRow> BuildCBTMigrationPreview(const FString& Folder, const FString& Destination)
{
	TArray<FAssetAuditRow> Rows;
	if (Destination.IsEmpty() || IsBelowFolder(FName(*Destination), Folder))
	{
		Rows.Add({ TEXT("Destination"), Destination, TEXT("Choose a folder outside the source"), {} });
		return Rows;
	}

	IAssetRegistry& Registry = InspectionRegistry();
	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(FName(*Folder), Assets, true, true);
	TSet<FName> SourcePackages;
	for (const FAssetData& Asset : Assets)
	{
		SourcePackages.Add(Asset.PackageName);
	}
	for (FName Package : SourcePackages)
	{
		const FString Path = Package.ToString();
		const FString Target = Destination + Path.RightChop(Folder.Len());
		if (Registry.GetAssetPackageDataCopy(FName(*Target)).IsSet() || FPackageName::DoesPackageExist(Target))
		{
			Rows.Add({ FPackageName::GetLongPackageAssetName(Path), Path, TEXT("Destination conflict"), Target });
		}

		TArray<FName> Referencers;
		Registry.GetReferencers(Package, Referencers, EDependencyCategory::Package);
		for (FName Referencer : Referencers)
		{
			if (!IsBelowFolder(Referencer, Folder))
			{
				Rows.Add({ FPackageName::GetLongPackageAssetName(Path), Path,
					IsMapPackage(Registry, Referencer) ? TEXT("External map referencer") : TEXT("External referencer"), Referencer.ToString() });
			}
		}
	}
	for (FName Dependency : CollectDependencyClosure(Registry, SourcePackages.Array()))
	{
		if (!IsBelowFolder(Dependency, Folder))
		{
			const FString Path = Dependency.ToString();
			Rows.Add({ FPackageName::GetLongPackageAssetName(Path), Path, TEXT("External dependency"), TEXT("Reachable from the source folder.") });
		}
	}
	AddDiscoveryStatus(Rows);
	return Rows;
}

TArray<FAssetAuditRow> BuildCBTCleanupCandidates(const FString& Folder)
{
	IAssetRegistry& Registry = InspectionRegistry();
	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(FName(*Folder), Assets, true, true);
	TArray<FAssetAuditRow> Rows;
	TMap<FName, bool> HasReferencers;
	for (const FAssetData& Asset : Assets)
	{
		if (Asset.IsRedirector())
		{
			Rows.Add({ Asset.AssetName.ToString(), Asset.GetObjectPathString(), TEXT("Redirector"), TEXT("Review references before fixing up the redirector.") });
		}
		else if (!Asset.IsInstanceOf<UWorld>())
		{
			bool* bReferenced = HasReferencers.Find(Asset.PackageName);
			if (!bReferenced)
			{
				TArray<FAssetIdentifier> Referencers;
				Registry.GetReferencers(FAssetIdentifier(Asset.PackageName), Referencers, EDependencyCategory::All);
				bReferenced = &HasReferencers.Add(Asset.PackageName, !Referencers.IsEmpty());
			}
			if (!*bReferenced)
			{
				Rows.Add({ Asset.AssetName.ToString(), Asset.GetObjectPathString(), TEXT("No saved referencers found"),
					TEXT("Check code, config, Primary Asset rules and dynamic loading before removing this asset.") });
			}
		}
	}
	TArray<FString> Paths;
	Registry.GetSubPaths(Folder, Paths, true);
	Paths.AddUnique(Folder);
	TSet<FString> Occupied;
	for (const FAssetData& Asset : Assets)
	{
		FString Path = Asset.PackagePath.ToString();
		while (Path.Len() >= Folder.Len() && !Occupied.Contains(Path))
		{
			Occupied.Add(Path);
			Path = FPaths::GetPath(Path);
		}
	}
	for (const FString& Path : Paths)
	{
		if (!Occupied.Contains(Path))
		{
			Rows.Add({ FPaths::GetCleanFilename(Path), Path, TEXT("No saved assets in folder tree"), {} });
		}
	}
	AddDiscoveryStatus(Rows);
	return Rows;
}
