#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "ContentBrowserToolkitSettings.generated.h"

/** @brief Optional audio import expectations for a project content folder. */
USTRUCT()

struct FCBTAudioImportProfile
{
	GENERATED_BODY()

	/** @brief Expected source channels. Zero leaves the channel count unrestricted. */
	UPROPERTY(EditAnywhere, Category = "Audio")
	int32 Channels = 0;

	/** @brief Expected imported sample rate in Hz. Zero leaves the rate unrestricted. */
	UPROPERTY(EditAnywhere, Category = "Audio")
	int32 SampleRate = 0;
};

UCLASS(config = Editor, defaultconfig, meta = (DisplayName = "Content Browser Toolkit"))

class CONTENTBROWSERTOOLKIT_API UContentBrowserToolkitSettings final : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UContentBrowserToolkitSettings* Get();
	static UContentBrowserToolkitSettings* GetMutable();

	UPROPERTY(EditAnywhere, config, Category = "Thresholds", meta = (ClampMin = "1", UIMin = "1", DisplayName = "Large Asset Threshold MB"))
	int32 LargeAssetThresholdMB = 50;

	UPROPERTY(EditAnywhere, config, Category = "Thresholds", meta = (ClampMin = "1", UIMin = "1", DisplayName = "Tiny Asset Threshold KB"))
	int32 TinyAssetThresholdKB = 4;

	UPROPERTY(EditAnywhere, config, Category = "Thresholds", meta = (ClampMin = "256", UIMin = "256", DisplayName = "Large Texture Max Size"))
	int32 LargeTextureMaxSize = 4096;

	UPROPERTY(EditAnywhere, config, Category = "Thresholds", meta = (ClampMin = "1", UIMin = "1", DisplayName = "High Triangle Threshold"))
	int32 HighTriangleThreshold = 100000;

	UPROPERTY(EditAnywhere, config, Category = "Thresholds", meta = (ClampMin = "1", UIMin = "1", DisplayName = "Too Many Material Slots Threshold"))
	int32 TooManyMaterialSlotsThreshold = 8;

	UPROPERTY(EditAnywhere, config, Category = "Thresholds", meta = (ClampMin = "1", UIMin = "1", DisplayName = "Too Many UV Channels Threshold"))
	int32 TooManyUVChannelsThreshold = 4;

	UPROPERTY(EditAnywhere, config, Category = "Thresholds", meta = (ClampMin = "1", ClampMax = "100", UIMin = "1", UIMax = "100", DisplayName = "Low Audio Quality Threshold"))
	int32 LowAudioQualityThreshold = 35;

	UPROPERTY(EditAnywhere, config, Category = "Audit", meta = (DisplayName = "Enable Cached Audit Filters"))
	bool bEnableCachedAuditFilters = true;

	/** @brief Case-sensitive prefixes by full asset class path; an empty value disables a rule. */
	UPROPERTY(EditAnywhere, config, Category = "Naming Audit")
	TMap<FString, FString> NamingPrefixes = {
		{ TEXT("/Script/Engine.Texture"), TEXT("T_") },
		{ TEXT("/Script/Engine.MaterialInterface"), TEXT("M_") },
		{ TEXT("/Script/Engine.MaterialInstance"), TEXT("MI_") },
		{ TEXT("/Script/Engine.Blueprint"), TEXT("BP_") },
		{ TEXT("/Script/Engine.AnimSequence"), TEXT("AS_") },
		{ TEXT("/Script/Engine.Skeleton"), TEXT("SKEL_") },
		{ TEXT("/Script/Engine.StaticMesh"), TEXT("SM_") },
		{ TEXT("/Script/Engine.SkeletalMesh"), TEXT("SK_") }
	};

	/** @brief Notify names required by the Animation Set Audit for each selected sequence. */
	UPROPERTY(EditAnywhere, Config, Category = "Animation Audit")
	TArray<FName> RequiredAnimationNotifies;

	/** @brief Float curve names required by the Animation Set Audit. */
	UPROPERTY(EditAnywhere, Config, Category = "Animation Audit")
	TArray<FName> RequiredAnimationCurves;

	/** @brief Audio profiles keyed by package folder. The most specific matching folder wins. */
	UPROPERTY(EditAnywhere, Config, Category = "Audio Audit")
	TMap<FString, FCBTAudioImportProfile> AudioImportProfiles;
};
