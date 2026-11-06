// Copyright (c) 2026 DimAlek. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"

/**
 * @brief Owns Asset Registry event bindings and removes them before releasing their callbacks.
 * @note Construct and destroy on the editor thread; this class does not marshal registry callbacks between threads.
 * @note Captured callback owners must outlive this subscription or use weak captures.
 *       Do not destroy the subscription from inside one of its callbacks.
 */
class DATAASSETMANAGER_API FDataAssetRegistrySubscription final
{
public:
	/**
	 * @brief Stores callbacks and subscribes after the initial registry scan, or immediately if the scan is complete.
	 * @param OnAdded Receives metadata for an added asset. An empty callback is allowed.
	 * @param OnRemoved Receives metadata for a removed asset. An empty callback is allowed.
	 * @param OnRenamed Receives updated asset metadata and the previous object path. An empty callback is allowed.
	 * @note Loads the AssetRegistry module. Events occurring before attachment are not replayed;
	 *       callers must obtain their initial asset list separately.
	 */
	FDataAssetRegistrySubscription(TFunction<void(const FAssetData&)> OnAdded, TFunction<void(const FAssetData&)> OnRemoved, TFunction<void(const FAssetData&, const FString&)> OnRenamed);
	/**
	 * @brief Removes asset event bindings and the pending scan-completion binding when the registry module is available.
	 * @note Does not load the module during shutdown. Callback storage is released after bindings are removed.
	 */
	~FDataAssetRegistrySubscription();
	/** @brief Prevents copying ownership of registry bindings that capture this subscription's address. */
	FDataAssetRegistrySubscription(const FDataAssetRegistrySubscription&) = delete;
	/** @brief Prevents replacing a subscription by copying another object's callback bindings. */
	FDataAssetRegistrySubscription& operator=(const FDataAssetRegistrySubscription&) = delete;
private:
	/**
	 * @brief Installs add, remove and rename bindings once the registry is ready.
	 * @note Repeated calls leave existing bindings unchanged to avoid duplicate notifications.
	 */
	void Attach();
	/** @brief Callback invoked by the asset-added binding. */
	TFunction<void(const FAssetData&)> Added;
	/** @brief Callback invoked by the asset-removed binding. */
	TFunction<void(const FAssetData&)> Removed;
	/** @brief Callback invoked with renamed asset metadata and its previous object path. */
	TFunction<void(const FAssetData&, const FString&)> Renamed;
	/** @brief Handles used to detach asset notifications and the initial scan-completion callback. */
	FDelegateHandle AddedHandle, RemovedHandle, RenamedHandle, LoadedHandle;
};
