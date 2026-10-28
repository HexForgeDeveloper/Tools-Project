#pragma once

#include "CoreMinimal.h"
#include "Blueprint/BlueprintExtension.h"
#include "Engine/AssetUserData.h"
#include "EdGraph/EdGraphPin.h"
#include "WireBundleAssetData.generated.h"

// Internal serialized editor records, not user-created Data Assets.
USTRUCT()
struct WIREBUNDLE_API FWireBundlePinIdentity
{
	GENERATED_BODY()
	UPROPERTY()
	FGuid Node;
	UPROPERTY()
	FGuid Pin;
	UPROPERTY()
	FName Name;
	UPROPERTY()
	int32 Slot = INDEX_NONE;
	UPROPERTY()
	bool bOutput = false;
	UPROPERTY()
	bool bMaterialRoot = false;
	UPROPERTY()
	FString Type;
	// Blueprint duplication remaps these references even when node/pin GUIDs change.
	// Materials must not serialize references to their transient graph nodes.
	UPROPERTY()
	TObjectPtr<UEdGraphNode> BlueprintNode;
	UPROPERTY()
	FEdGraphPinType BlueprintType;
};

USTRUCT()
struct WIREBUNDLE_API FWireBundleStoredWire
{
	GENERATED_BODY()
	UPROPERTY()
	FWireBundlePinIdentity Source;
	UPROPERTY()
	FWireBundlePinIdentity Target;
	UPROPERTY()
	TArray<FGuid> Reroutes;
	UPROPERTY()
	TArray<TObjectPtr<UEdGraphNode>> BlueprintReroutes;
};

USTRUCT()
struct WIREBUNDLE_API FWireBundleRecord
{
	GENERATED_BODY()
	UPROPERTY()
	int32 Version = 1;
	UPROPERTY()
	FGuid Id;
	UPROPERTY()
	FGuid Graph;
	UPROPERTY()
	TArray<FWireBundleStoredWire> Wires;
};

UCLASS()
class WIREBUNDLE_API UWireBundleBlueprintData final : public UBlueprintExtension
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<FWireBundleRecord> Bundles;
	virtual bool			  IsEditorOnly() const override { return true; }
};

UCLASS()
class WIREBUNDLE_API UWireBundleMaterialData final : public UAssetUserData
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<FWireBundleRecord> Bundles;
	virtual bool			  IsEditorOnly() const override { return true; }
};
