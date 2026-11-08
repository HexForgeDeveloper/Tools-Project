#include "ContentBrowserFolderAudit.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

namespace
{
	/** @brief Counts each package once per group, including packages with several asset exports. */
	struct FFolderGroupAccumulator
	{
		FCBTFolderAuditGroup Group;
		TSet<FName> Packages;

		/** @brief Adds one asset and its known package size. */
		void Add(const FString& Name, const FCBTFolderAuditRow& Row)
		{
			Group.Name = Name;
			++Group.AssetCount;
			bool bAlreadyCounted = false;
			Packages.Add(Row.Asset.PackageName, &bAlreadyCounted);
			if (!bAlreadyCounted && Row.Bytes >= 0)
			{
				Group.Bytes += Row.Bytes;
			}
		}
	};

	/** @brief Returns on-disk package bytes, including separate export and bulk data files. */
	int64 PackageBytes(const FName Package)
	{
		FString Filename;
		if (!FPackageName::DoesPackageExist(Package.ToString(), &Filename))
		{
			return -1;
		}
		int64 Bytes = IFileManager::Get().FileSize(*Filename);
		if (Bytes < 0)
		{
			return -1;
		}
		for (const TCHAR* Extension : { TEXT("uexp"), TEXT("ubulk"), TEXT("uptnl") })
		{
			Bytes += FMath::Max<int64>(0, IFileManager::Get().FileSize(*FPaths::ChangeExtension(Filename, Extension)));
		}
		return Bytes;
	}

	/** @brief Quotes a CSV cell, including embedded double quotes. */
	FString CsvCell(const FString& Text)
	{
		return TEXT("\"") + Text.Replace(TEXT("\""), TEXT("\"\"")) + TEXT("\"");
	}
} // namespace

FCBTFolderAuditReport FContentBrowserFolderAudit::Scan(const FString& Folder, bool bFindReferencers)
{
	FCBTFolderAuditReport Report;
	Report.Folder = Folder;
	Report.Folder.RemoveFromEnd(TEXT("/"));
	Report.bReferenceAudit = bFindReferencers;
	FString DiskPath;
	if (!FPackageName::IsValidLongPackageName(Report.Folder / TEXT("FolderAuditProbe"), true)
		|| !FPackageName::TryConvertLongPackageNameToFilename(Report.Folder, DiskPath))
	{
		Report.Error = TEXT("Select a mounted content folder.");
		return Report;
	}
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	if (Registry.IsLoadingAssets())
	{
		Report.Error = TEXT("The Asset Registry is still discovering assets. Run the report again after discovery finishes.");
		return Report;
	}

	const FString Prefix = Report.Folder + TEXT("/");
	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(FName(*Report.Folder), Assets, true, true);
	TMap<FName, int64> Sizes;
	TMap<FName, TArray<FName>> Referencers;
	TMap<FString, FFolderGroupAccumulator> Classes;
	TMap<FString, FFolderGroupAccumulator> Subfolders;
	for (const FAssetData& Asset : Assets)
	{
		auto Row = MakeShared<FCBTFolderAuditRow>();
		Row->Asset = Asset;
		const FName Package = Asset.PackageName;
		if (!Sizes.Contains(Package))
		{
			const int64 Bytes = PackageBytes(Package);
			Sizes.Add(Package, Bytes);
			if (Bytes < 0)
			{
				++Report.UnavailablePackages;
			}
			else
			{
				Report.TotalBytes += Bytes;
			}
		}
		Row->Bytes = Sizes.FindChecked(Package);
		if (bFindReferencers)
		{
			if (!Referencers.Contains(Package))
			{
				TArray<FName>& External = Referencers.Add(Package);
				Registry.GetReferencers(Package, External, UE::AssetRegistry::EDependencyCategory::Package);
				External.RemoveAll([&Prefix](FName Name) { return Name.ToString().StartsWith(Prefix); });
				External.Sort(FNameLexicalLess());
			}
			Row->ExternalReferencers = Referencers.FindChecked(Package);
		}
		const FString Class = Asset.AssetClassPath.ToString();
		Classes.FindOrAdd(Class).Add(Class, *Row);
		FString Child = Asset.PackagePath.ToString();
		if (Child.RemoveFromStart(Prefix))
		{
			int32 Slash;
			if (Child.FindChar(TEXT('/'), Slash))
			{
				Child.LeftInline(Slash);
			}
			Subfolders.FindOrAdd(Child).Add(Child, *Row);
		}
		Report.Rows.Add(Row);
	}
	for (auto& Pair : Classes)
	{
		Report.Classes.Add(MoveTemp(Pair.Value.Group));
	}
	for (auto& Pair : Subfolders)
	{
		Report.Subfolders.Add(MoveTemp(Pair.Value.Group));
	}
	const auto LargestGroupFirst = [](const FCBTFolderAuditGroup& A, const FCBTFolderAuditGroup& B) {
		return A.Bytes == B.Bytes ? A.Name < B.Name : A.Bytes > B.Bytes;
	};
	Report.Classes.Sort(LargestGroupFirst);
	Report.Subfolders.Sort(LargestGroupFirst);
	Report.Rows.Sort([bFindReferencers](const TSharedPtr<FCBTFolderAuditRow>& A, const TSharedPtr<FCBTFolderAuditRow>& B) {
		if (bFindReferencers && A->ExternalReferencers.Num() != B->ExternalReferencers.Num())
		{
			return A->ExternalReferencers.Num() > B->ExternalReferencers.Num();
		}
		return A->Bytes == B->Bytes ? A->Asset.GetObjectPathString() < B->Asset.GetObjectPathString() : A->Bytes > B->Bytes;
	});
	return Report;
}

FString FContentBrowserFolderAudit::Export(const FCBTFolderAuditReport& Report)
{
	if (!Report.Error.IsEmpty())
	{
		return {};
	}
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("ContentBrowserToolkit/Audits");
	IFileManager::Get().MakeDirectory(*Directory, true);
	const TCHAR* Prefix = Report.bReferenceAudit ? TEXT("FolderReferences-") : TEXT("FolderInventory-");
	const FString Path = FPaths::ConvertRelativePathToFull(Directory / (Prefix + FGuid::NewGuid().ToString() + TEXT(".csv")));
	FString Csv = TEXT("Scope,Kind,Name,AssetClass,AssetCount,PackageBytes,ExternalReferencers\n");
	for (const auto& Row : Report.Rows)
	{
		const FString References = FString::JoinBy(Row->ExternalReferencers, TEXT("; "), [](FName Name) { return Name.ToString(); });
		Csv += CsvCell(Report.Folder) + TEXT(",Asset,") + CsvCell(Row->Asset.GetObjectPathString()) + TEXT(",")
			+ CsvCell(Row->Asset.AssetClassPath.ToString()) + TEXT(",1,")
			+ (Row->Bytes < 0 ? FString() : LexToString(Row->Bytes)) + TEXT(",") + CsvCell(References) + TEXT("\n");
	}
	const auto AppendGroup = [&Csv, &Report](const TCHAR* Kind, const FCBTFolderAuditGroup& Group) {
		Csv += CsvCell(Report.Folder) + TEXT(",") + Kind + TEXT(",") + CsvCell(Group.Name) + TEXT(",,")
			+ LexToString(Group.AssetCount) + TEXT(",") + LexToString(Group.Bytes) + TEXT(",\n");
	};
	AppendGroup(TEXT("Total"), { Report.Folder, Report.Rows.Num(), Report.TotalBytes });
	for (const auto& Group : Report.Classes)
	{
		AppendGroup(TEXT("Class"), Group);
	}
	for (const auto& Group : Report.Subfolders)
	{
		AppendGroup(TEXT("Subfolder"), Group);
	}
	return FFileHelper::SaveStringToFile(Csv, *Path, FFileHelper::EEncodingOptions::ForceUTF8) ? Path : FString();
}
