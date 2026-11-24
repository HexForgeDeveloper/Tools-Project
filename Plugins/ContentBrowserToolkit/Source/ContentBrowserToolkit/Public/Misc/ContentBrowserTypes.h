// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"

/**
 *	@brief Conservative evidence categories; no category authorizes deletion.
 */
enum class ECBTUsageStatus : uint8
{
	MapDependency,
	Referenced,
	Protected,
	Review
};

/**
 *	@brief One asset and the evidence found by a read-only scan.
 */
struct FCBTUsageRow
{
	FAssetData Asset;
	FString Evidence;

	int64 Bytes = 0;

	ECBTUsageStatus Status = ECBTUsageStatus::Review;
};

/**
 *	@brief Scan results with explicit coverage limitations and cancellation state.
 */
struct FCBTUsageReport
{
	FString Error;
	FString Folder;
	FString MapPackage;

	TArray<TSharedPtr<FCBTUsageRow>> Rows;
	TArray<FString> Warnings;

	bool bCancelled = false;
};
