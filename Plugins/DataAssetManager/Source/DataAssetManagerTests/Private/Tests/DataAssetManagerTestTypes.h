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
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::CriticalPriority;
} // namespace DataAssetManagerFlags

/**
 * @brief Instanced subobject used to verify reset ownership semantics.
 */
UCLASS(BlueprintType)

class UTestDataAssetSubobject : public UObject
{
	GENERATED_BODY()

public:
	/** @brief Editable value restored from the class default subobject. */
	UPROPERTY(EditDefaultsOnly, Category = "Automation")
	int32 TestProperty = 7;
};

/** @brief Derived subobject whose class identity and extra value must survive JSON import. */
UCLASS()

class UTestDerivedDataAssetSubobject : public UTestDataAssetSubobject
{
	GENERATED_BODY()

public:
	/** @brief Property that would be lost if import fell back to the declared base class. */
	UPROPERTY(EditDefaultsOnly, Category = "Automation")
	int32 DerivedValue = 0;
};

/**
 * @brief Data asset with an instanced subobject created by its constructor.
 */
UCLASS(BlueprintType)

class UTestDataAssetWithInstancedObject : public UDataAsset
{
	GENERATED_BODY()

public:
	UTestDataAssetWithInstancedObject()
	{
		InstancedObject = CreateDefaultSubobject<UTestDataAssetSubobject>(TEXT("InstancedObject"));
	}

	/** @brief Instanced object that must remain independent from the CDO after reset. */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Automation")
	TObjectPtr<UTestDataAssetSubobject> InstancedObject;
};

/** @brief Fixture for rejecting reset of containers with instanced references. */
UCLASS()

class UTestDataAssetWithInstancedArray : public UDataAsset
{
	GENERATED_BODY()
public:
	/** @brief Creates the default array element under the owning asset. */
	UTestDataAssetWithInstancedArray()
	{
		Items.Add(CreateDefaultSubobject<UTestDataAssetSubobject>(TEXT("ArrayItem")));
	}
	/** @brief Ordinary field used to verify that reset rejection is atomic per object. */
	UPROPERTY(EditDefaultsOnly, Category = "Automation")
	int32 Value = 7;
	/** @brief Nested instanced references must never be shallow-copied from the CDO. */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Automation")
	TArray<TObjectPtr<UTestDataAssetSubobject>> Items;
};

/**
 * @brief Minimal data asset type used by Data Asset Manager automation tests.
 */
UCLASS(BlueprintType)

class UTestDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** @brief Integer property used by tests that need editable asset data. */
	UPROPERTY(EditDefaultsOnly, Category = "Automation")
	int32 TestProperty = 0;

	/** @brief Narrow unsigned value used to test random range clamping. */
	UPROPERTY(EditDefaultsOnly, Category = "Automation")
	uint8 ByteValue = 0;

	/** @brief Wide integer used to test values outside the int32 range. */
	UPROPERTY(EditDefaultsOnly, Category = "Automation")
	int64 WideValue = 0;

	/** @brief Editable color used by copy, reset and randomization tests. */
	UPROPERTY(EditDefaultsOnly, Category = "Automation")
	FLinearColor Color = FLinearColor::Red;

	/** @brief Editable color array used to verify ordering and transaction behavior. */
	UPROPERTY(EditDefaultsOnly, Category = "Automation")
	TArray<FLinearColor> Colors;

	/** @brief Hard asset reference used to verify reflected copy transactions. */
	UPROPERTY(EditDefaultsOnly, Category = "Automation")
	TObjectPtr<UDataAsset> AssetReference;

	/** @brief Soft asset reference used to verify path preservation across undo and redo. */
	UPROPERTY(EditDefaultsOnly, Category = "Automation")
	TSoftObjectPtr<UDataAsset> SoftAssetReference;
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
	UPROPERTY(EditDefaultsOnly, Category = "Automation")
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
