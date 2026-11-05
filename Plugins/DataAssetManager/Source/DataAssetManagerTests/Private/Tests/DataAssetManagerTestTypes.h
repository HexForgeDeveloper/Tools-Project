// Copyright (c) 2026 DimAlek. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Misc/DataValidation.h"
#include "Misc/AutomationTest.h"

#include "DataAssetManagerTestTypes.generated.h"

/**
 * @brief Shared automation-test flags used by Data Asset Manager test cases.
 */
namespace DataAssetManagerFlags
{
	/** @brief Default flag set for editor automation tests in this module. */
	constexpr EAutomationTestFlags Flags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::CriticalPriority;
} // namespace DataAssetManagerFlags

/**
 * @brief Minimal data asset type used by Data Asset Manager automation tests.
 */
UCLASS(BlueprintType)
class UTestDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** @brief Integer property used by tests that need editable asset data. */
	UPROPERTY(EditDefaultsOnly)
	int32 TestProperty = 0;

	UPROPERTY(EditDefaultsOnly)
	FLinearColor Color = FLinearColor::Red;

	UPROPERTY(EditDefaultsOnly)
	TArray<FLinearColor> Colors;
};

/**
 * @brief Alternate data asset class used by tests that need class mismatches.
 */
UCLASS(BlueprintType)
class UAlternateTestDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** @brief Distinct editable property used by class-filter and diff tests. */
	UPROPERTY(EditDefaultsOnly)
	FString AlternateProperty;
};

/**
 * @brief Data asset type that always fails validation for service tests.
 */
UCLASS(BlueprintType)
class UInvalidTestDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override
	{
		Context.AddError(NSLOCTEXT("DataAssetManagerTests", "InvalidTestDataAssetError", "Invalid test data asset."));
		return EDataValidationResult::Invalid;
	}
#endif
};
