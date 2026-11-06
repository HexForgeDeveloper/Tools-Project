#include "ContentBrowserUsageAudit.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Internationalization/Regex.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

bool FContentBrowserUsageAudit::Traverse(const TArray<FName>& Roots, TFunctionRef<TArray<FName>(FName)> Dependencies, TFunctionRef<bool()> Cancel, TMap<FName, FName>& Parents)
{
	Parents.Reset();
	TArray<FName> Queue;
	for (FName Root : Roots)
	{
		if (!Root.IsNone() && !Parents.Contains(Root))
		{
			Parents.Add(Root, NAME_None);
			Queue.Add(Root);
		}
	}
	for (int32 Index = 0; Index < Queue.Num(); ++Index)
	{
		if (Cancel())
		{
			return false;
		}
		for (FName Next : Dependencies(Queue[Index]))
		{
			if (!Next.IsNone() && !Parents.Contains(Next))
			{
				Parents.Add(Next, Queue[Index]);
				Queue.Add(Next);
			}
		}
	}
	return true;
}

FString FContentBrowserUsageAudit::StatusText(ECBTUsageStatus Status)
{
	switch (Status)
	{
	case ECBTUsageStatus::MapDependency:
		return TEXT("Used by selected map");
	case ECBTUsageStatus::Referenced:
		return TEXT("Other references found");
	case ECBTUsageStatus::Protected:
		return TEXT("Protected / incomplete");
	default:
		return TEXT("No tracked references - REVIEW ONLY");
	}
}

FCBTUsageReport FContentBrowserUsageAudit::Scan(const FString& Folder, const FAssetData& Map)
{
	FCBTUsageReport Result;
	Result.Folder = Folder;
	Result.MapPackage = Map.PackageName.ToString();
	if (!FPackageName::IsValidLongPackageName(Folder / TEXT("__AuditValidation__")) || Map.AssetClassPath != UWorld::StaticClass()->GetClassPathName())
	{
		Result.Error = TEXT("Choose a mounted content folder and a World map asset.");
		return Result;
	}
	FScopedSlowTask Progress(5.f, FText::FromString(TEXT("Auditing asset usage (read-only)")));
	if (!IsRunningCommandlet())
	{
		Progress.MakeDialog(true);
	}
	auto Cancel = [&]()
	{
		return Progress.ShouldCancel();
	};
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Progress.EnterProgressFrame(1.f, FText::FromString(TEXT("Refreshing saved dependencies")));
	Registry.SearchAllAssets(true);
	Registry.ScanPathsSynchronous({Folder, FPackageName::GetLongPackagePath(Map.PackageName.ToString())}, true);
	TArray<FAssetData> All;
	Registry.GetAllAssets(All, false);
	TMap<FName, FPrimaryAssetId> PrimaryByPackage;
	TMap<FPrimaryAssetId, FName> PackageByPrimary;
	TSet<FName> WorldPackages;
	for (const FAssetData& Asset : All)
	{
		if (Asset.AssetClassPath == UWorld::StaticClass()->GetClassPathName())
		{
			WorldPackages.Add(Asset.PackageName);
		}
		const FPrimaryAssetId Id = Asset.GetPrimaryAssetId();
		if (Id.IsValid())
		{
			PrimaryByPackage.Add(Asset.PackageName, Id);
			PackageByPrimary.Add(Id, Asset.PackageName);
		}
	}
	TArray<FName> Roots{Map.PackageName};
	bool bIncomplete = false;
	Progress.EnterProgressFrame(1.f, FText::FromString(TEXT("Checking external actors and map object references")));
	TArray<FString> ExternalPaths = ULevel::GetExternalActorsPaths(Map.PackageName.ToString());
	ExternalPaths.Append(ULevel::GetExternalObjectsPaths(Map.PackageName.ToString()));
	for (const FString& Path : ExternalPaths)
	{
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *FPackageName::LongPackageNameToFilename(Path), TEXT("*.uasset"), true, false);
		if (!Files.IsEmpty())
		{
			Registry.ScanFilesSynchronous(Files, true);
		}
		for (const FString& File : Files)
		{
			FString Package;
			if (FPackageName::TryConvertFilenameToLongPackageName(File, Package))
			{
				Roots.AddUnique(FName(*Package));
			}
			else
			{
				bIncomplete = true;
			}
		}
	}
	TStrongObjectPtr<UWorld> World(Cast<UWorld>(Map.GetAsset()));
	if (!World.IsValid())
	{
		Result.Error = TEXT("Map failed to load. No unused-asset conclusion is available.");
		return Result;
	}
	if (World->GetOutermost()->IsDirty())
	{
		bIncomplete = true;
	}
	TSet<UObject*> Visited;
	TArray<UObject*> Objects{World.Get()};
	for (int32 Index = 0; Index < Objects.Num(); ++Index)
	{
		if (Cancel() || Objects.Num() > 200000)
		{
			bIncomplete = true;
			Result.bCancelled = Cancel();
			break;
		}
		UObject* Object = Objects[Index];
		if (!IsValid(Object) || Visited.Contains(Object))
		{
			continue;
		}
		Visited.Add(Object);
		const FName Package = Object->GetOutermost()->GetFName();
		const FString PackagePath = Package.ToString();
		if (PackagePath.StartsWith(TEXT("/Script/")) || PackagePath.StartsWith(TEXT("/Engine/")))
		{
			continue;
		}
		if (!PackagePath.StartsWith(TEXT("/Temp/")) && !PackagePath.StartsWith(TEXT("/Transient")))
		{
			Roots.AddUnique(Package);
		}
		TArray<UObject*> References;
		FReferenceFinder Finder(References, nullptr, false, false, false, false);
		Finder.FindReferences(Object);
		for (UObject* Reference : References)
		{
			if (Reference && !Visited.Contains(Reference))
			{
				Objects.Add(Reference);
			}
		}
	}
	Progress.EnterProgressFrame(1.f, FText::FromString(TEXT("Following hard, soft, searchable and management dependencies")));
	TMap<FName, FName> Parents;
	auto Dependencies = [&](FName Package)
	{
		TArray<FName> Packages;
		// Streaming levels and level instances can own external actors independently of the selected map.
		if (WorldPackages.Contains(Package) && Package != Map.PackageName)
		{
			TArray<FString> Paths = ULevel::GetExternalActorsPaths(Package.ToString());
			Paths.Append(ULevel::GetExternalObjectsPaths(Package.ToString()));
			for (const FString& Path : Paths)
			{
				TArray<FString> Files;
				IFileManager::Get().FindFilesRecursive(Files, *FPackageName::LongPackageNameToFilename(Path), TEXT("*.uasset"), true, false);
				if (!Files.IsEmpty())
				{
					Registry.ScanFilesSynchronous(Files, true);
				}
				for (const FString& File : Files)
				{
					FString ExternalPackage;
					if (FPackageName::TryConvertFilenameToLongPackageName(File, ExternalPackage))
					{
						Packages.AddUnique(FName(*ExternalPackage));
					}
					else
					{
						bIncomplete = true;
					}
				}
			}
		}
		TArray<FAssetIdentifier> Edges;
		Registry.GetDependencies(FAssetIdentifier(Package), Edges, UE::AssetRegistry::EDependencyCategory::All);
		if (const FPrimaryAssetId* Id = PrimaryByPackage.Find(Package))
		{
			TArray<FAssetIdentifier> Managed;
			Registry.GetDependencies(FAssetIdentifier(*Id), Managed, UE::AssetRegistry::EDependencyCategory::All);
			Edges.Append(Managed);
		}
		for (const FAssetIdentifier& Edge : Edges)
		{
			if (!Edge.PackageName.IsNone())
			{
				Packages.AddUnique(Edge.PackageName);
			}
			else if (const FName* Resolved = PackageByPrimary.Find(Edge.GetPrimaryAssetId()))
			{
				Packages.AddUnique(*Resolved);
			}
			else
			{
				bIncomplete = true;
			}
		}
		return Packages;
	};
	if (!Traverse(Roots, Dependencies, Cancel, Parents))
	{
		Result.bCancelled = true;
		bIncomplete = true;
	}
	for (FName Root : Roots)
	{
		if (Root != Map.PackageName && Parents.Contains(Root))
		{
			Parents[Root] = Map.PackageName;
		}
	}
	Progress.EnterProgressFrame(1.f, FText::FromString(TEXT("Checking literal project and plugin source/config references")));
	TMap<FName, FString> TextReferences;
	const FRegexPattern Pattern(TEXT("(/[A-Za-z][A-Za-z0-9_]*/[A-Za-z0-9_/\\-]+)"));
	for (const FString& Directory : {(FPaths::ProjectDir() / TEXT("Source")), FPaths::ProjectConfigDir(), FPaths::ProjectPluginsDir(), FPaths::ProjectDir() / TEXT("DevOps")})
	{
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *Directory, TEXT("*"), true, false);
		for (const FString& File : Files)
		{
			if (Cancel())
			{
				Result.bCancelled = true;
				bIncomplete = true;
				break;
			}
			if (File.Contains(TEXT("/Binaries/")) || File.Contains(TEXT("/Intermediate/")) || File.Contains(TEXT("/Saved/")))
			{
				continue;
			}
			const FString Extension = FPaths::GetExtension(File).ToLower();
			if (!TArray<FString>{TEXT("cpp"), TEXT("h"), TEXT("cs"), TEXT("ini"), TEXT("json"), TEXT("py"), TEXT("ps1"), TEXT("bat")}.Contains(Extension))
			{
				continue;
			}
			FString Text;
			if (!FFileHelper::LoadFileToString(Text, *File))
			{
				bIncomplete = true;
				continue;
			}
			FRegexMatcher Matcher(Pattern, Text);
			while (Matcher.FindNext())
			{
				FString Literal = Matcher.GetCaptureGroup(1);
				while (Literal.EndsWith(TEXT("/")))
				{
					Literal.LeftChopInline(1);
				}
				TextReferences.FindOrAdd(FName(*Literal)) = File;
			}
		}
	}
	Progress.EnterProgressFrame(1.f, FText::FromString(TEXT("Classifying assets and collecting evidence")));
	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(FName(*Folder), Assets, true, false);
	Result.Warnings.Add(TEXT("Read-only evidence report, NOT deletion approval. Dynamic strings, runtime downloads and external consumers cannot be proven absent."));
	Result.Warnings.Add(TEXT("Maps are loaded for inspection without switching the editor level or saving packages. Unsaved objects may affect results; rescan after saving changes."));
	if (bIncomplete)
	{
		Result.Warnings.Add(TEXT("Coverage is incomplete or cancelled. Unresolved assets are protected; no absence-of-usage conclusion is available."));
	}
	for (const FAssetData& Asset : Assets)
	{
		TSharedPtr<FCBTUsageRow> Row = MakeShared<FCBTUsageRow>();
		Row->Asset = Asset;
		const FString Package = Asset.PackageName.ToString();
		FString Filename;
		if (FPackageName::DoesPackageExist(Package, &Filename))
		{
			Row->Bytes = FMath::Max<int64>(0, IFileManager::Get().FileSize(*Filename));
			for (const FString& Ext : {FString(TEXT("uexp")), FString(TEXT("ubulk")), FString(TEXT("uptnl"))})
			{
				Row->Bytes += FMath::Max<int64>(0, IFileManager::Get().FileSize(*FPaths::ChangeExtension(Filename, Ext)));
			}
		}
		if (Parents.Contains(Asset.PackageName))
		{
			Row->Status = ECBTUsageStatus::MapDependency;
			TArray<FString> Chain;
			FName Current = Asset.PackageName;
			while (!Current.IsNone())
			{
				Chain.Insert(Current.ToString(), 0);
				Current = Parents.FindRef(Current);
			}
			Row->Evidence = FString::Join(Chain, TEXT(" -> "));
		}
		else
		{
			TArray<FAssetIdentifier> Referencers;
			Registry.GetReferencers(FAssetIdentifier(Asset.PackageName), Referencers, UE::AssetRegistry::EDependencyCategory::All);
			if (const FPrimaryAssetId* Id = PrimaryByPackage.Find(Asset.PackageName))
			{
				TArray<FAssetIdentifier> Managed;
				Registry.GetReferencers(FAssetIdentifier(*Id), Managed, UE::AssetRegistry::EDependencyCategory::All);
				Referencers.Append(Managed);
			}
			Referencers.RemoveAll(
				[&](const FAssetIdentifier& Id)
				{
					return Id.PackageName == Asset.PackageName;
				});
			FString TextEvidence;
			for (const auto& Pair : TextReferences)
			{
				const FString Literal = Pair.Key.ToString();
				if (Package == Literal || Package.StartsWith(Literal + TEXT("/")))
				{
					TextEvidence = Pair.Value;
					break;
				}
			}
			if (!Referencers.IsEmpty() || !TextEvidence.IsEmpty())
			{
				Row->Status = ECBTUsageStatus::Referenced;
				TArray<FString> Names;
				for (const auto& Id : Referencers)
				{
					Names.AddUnique(Id.ToString());
				}
				Names.Sort();
				Row->Evidence = FString::Join(Names, TEXT("; ")) + (TextEvidence.IsEmpty() ? FString() : TEXT(" Source/config: ") + TextEvidence);
			}
			else
			{
				const FString Kind = Asset.AssetClassPath.GetAssetName().ToString();
				UPackage* Loaded = FindPackage(nullptr, *Package);
				if (bIncomplete || Asset.IsRedirector() || PrimaryByPackage.Contains(Asset.PackageName) || Kind == TEXT("World") || Kind == TEXT("MapBuildDataRegistry") || Kind == TEXT("PrimaryAssetLabel") || Package.Contains(TEXT("/__External")) || (Loaded && Loaded->IsDirty()) ||
					Filename.IsEmpty())
				{
					Row->Status = ECBTUsageStatus::Protected;
					Row->Evidence = TEXT("Special package, unsaved data or incomplete scan; manual review required.");
				}
				else
				{
					Row->Evidence = TEXT("No references found by these checks. This is not proof that the asset is unused.");
				}
			}
		}
		Result.Rows.Add(Row);
	}
	Result.Rows.Sort(
		[](const TSharedPtr<FCBTUsageRow>& A, const TSharedPtr<FCBTUsageRow>& B)
		{
			return A->Bytes > B->Bytes;
		});
	return Result;
}

FString FContentBrowserUsageAudit::Export(const FCBTUsageReport& Report)
{
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("ContentBrowserToolkit/Audits");
	IFileManager::Get().MakeDirectory(*Directory, true);
	const FString Filename = FPaths::ConvertRelativePathToFull(Directory / (TEXT("Usage-") + FGuid::NewGuid().ToString() + TEXT(".csv")));
	auto Quote = [](FString Value)
	{
		return TEXT("\"") + Value.Replace(TEXT("\""), TEXT("\"\"")) + TEXT("\"");
	};
	FString Csv = TEXT("Map,Folder,Asset,Status,Bytes,Evidence,Limitations\n");
	const FString Limitations = FString::Join(Report.Warnings, TEXT(" | "));
	for (const auto& Row : Report.Rows)
	{
		Csv += Quote(Report.MapPackage) + TEXT(",") + Quote(Report.Folder) + TEXT(",") + Quote(Row->Asset.GetObjectPathString()) + TEXT(",") + Quote(StatusText(Row->Status)) + TEXT(",") + LexToString(Row->Bytes) + TEXT(",") + Quote(Row->Evidence) + TEXT(",") + Quote(Limitations) + TEXT("\n");
	}
	return FFileHelper::SaveStringToFile(Csv, *Filename, FFileHelper::EEncodingOptions::ForceUTF8) ? Filename : FString();
}
