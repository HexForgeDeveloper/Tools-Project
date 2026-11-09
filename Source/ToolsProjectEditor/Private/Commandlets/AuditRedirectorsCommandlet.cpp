#include "Commandlets/AuditRedirectorsCommandlet.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Modules/ModuleManager.h"
#include "UObject/ObjectRedirector.h"

DEFINE_LOG_CATEGORY_STATIC(LogAuditRedirectors, Log, All);

UAuditRedirectorsCommandlet::UAuditRedirectorsCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpDescription = TEXT("List redirectors under /Game and the saved packages that reference them. See the Unreal log for the report.");
}

int32 UAuditRedirectorsCommandlet::Main(const FString& Params)
{
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.SearchAllAssets(true);

	FARFilter Filter;
	Filter.PackagePaths.Add(TEXT("/Game"));
	Filter.ClassPaths.Add(UObjectRedirector::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;
	Filter.bIncludeOnlyOnDiskAssets = true;

	TArray<FAssetData> Redirectors;
	if (!Registry.GetAssets(Filter, Redirectors))
	{
		UE_LOG(LogAuditRedirectors, Error, TEXT("Failed to query redirectors under /Game."));
		return 1;
	}

	Redirectors.Sort([](const FAssetData& Left, const FAssetData& Right) {
		return Left.GetSoftObjectPath().ToString() < Right.GetSoftObjectPath().ToString();
	});

	int32 WithoutReferencers = 0;
	for (const FAssetData& Redirector : Redirectors)
	{
		TArray<FName> Referencers;
		Registry.GetReferencers(Redirector.PackageName, Referencers, UE::AssetRegistry::EDependencyCategory::Package);
		Referencers.Remove(Redirector.PackageName);
		Referencers.Sort(FNameLexicalLess());
		WithoutReferencers += Referencers.IsEmpty() ? 1 : 0;

		UE_LOG(LogAuditRedirectors, Display, TEXT("%s (%d saved package referencers)"),
			*Redirector.GetSoftObjectPath().ToString(), Referencers.Num());
		for (const FName Referencer : Referencers)
		{
			UE_LOG(LogAuditRedirectors, Display, TEXT("  %s"), *Referencer.ToString());
		}
	}

	UE_LOG(LogAuditRedirectors, Display, TEXT("/Game audit complete: %d redirectors, %d without saved package referencers."),
		Redirectors.Num(), WithoutReferencers);
	return 0;
}
