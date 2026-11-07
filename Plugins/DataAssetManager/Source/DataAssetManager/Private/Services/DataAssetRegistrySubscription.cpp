// Copyright (c) 2026 DimAlek. All Rights Reserved.
#include "Services/DataAssetRegistrySubscription.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"

FDataAssetRegistrySubscription::FDataAssetRegistrySubscription(TFunction<void(const FAssetData&)> OnAdded, TFunction<void(const FAssetData&)> OnRemoved, TFunction<void(const FAssetData&, const FString&)> OnRenamed)
	: Added(MoveTemp(OnAdded)), Removed(MoveTemp(OnRemoved)), Renamed(MoveTemp(OnRenamed))
{
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	if (Registry.IsLoadingAssets())
	{
		LoadedHandle = Registry.OnFilesLoaded().AddRaw(this, &FDataAssetRegistrySubscription::Attach);
	}
	else
	{
		Attach();
	}
}

void FDataAssetRegistrySubscription::Attach()
{
	if (AddedHandle.IsValid())
	{
		return;
	}
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	AddedHandle = Registry.OnAssetAdded().AddLambda([this](const FAssetData& Asset) { if (Added) { Added(Asset); } });
	RemovedHandle = Registry.OnAssetRemoved().AddLambda([this](const FAssetData& Asset) { if (Removed) { Removed(Asset); } });
	RenamedHandle = Registry.OnAssetRenamed().AddLambda([this](const FAssetData& Asset, const FString& OldPath) { if (Renamed) { Renamed(Asset, OldPath); } });
}

FDataAssetRegistrySubscription::~FDataAssetRegistrySubscription()
{
	if (FAssetRegistryModule* Module = FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry"))
	{
		IAssetRegistry& Registry = Module->Get();
		Registry.OnAssetAdded().Remove(AddedHandle);
		Registry.OnAssetRemoved().Remove(RemovedHandle);
		Registry.OnAssetRenamed().Remove(RenamedHandle);
		Registry.OnFilesLoaded().Remove(LoadedHandle);
	}
}
