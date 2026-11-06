// Copyright 2025 DimAlek. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture.h"
#include "TextureChannelPackerTypes.generated.h"

class UTexture;
class UTexture2D;

/**
 * @brief Supported texture channel packing operations.
 */
UENUM()
enum class ETextureChannelPackerOperation : uint8
{
	/** Pack multiple source textures into a single texture. */
	Pack UMETA(DisplayName = "Pack"),

	/** Repack channels from an existing packed texture. */
	Repack UMETA(DisplayName = "Repack"),

	/** Extract individual channels into separate textures. */
	Unpack UMETA(DisplayName = "Unpack"),

	/** Copy mapped channels into a newly named asset; existing assets are not overwritten. */
	Copy UMETA(DisplayName = "Copy Channels")
};

/**
 * @brief Texture channel identifiers.
 */
UENUM()
enum class ETextureChannelPackerChannel : uint8
{
	Red UMETA(DisplayName = "R"),
	Green UMETA(DisplayName = "G"),
	Blue UMETA(DisplayName = "B"),
	Alpha UMETA(DisplayName = "A")
};

/**
 * @brief Preview display modes.
 */
enum class ETextureChannelPackerPreviewMode : uint8
{
	Composite,
	Red,
	Green,
	Blue,
	Alpha
};

/**
 * @brief Output pixel formats supported by the packer.
 */
UENUM()
enum class ETextureChannelPackerOutputFormat : uint8
{
	Auto UMETA(DisplayName = "Auto"),
	G8 UMETA(DisplayName = "G8"),
	G16 UMETA(DisplayName = "G16"),
	R16F UMETA(DisplayName = "R16F"),
	R32F UMETA(DisplayName = "R32F"),
	BGRA8 UMETA(DisplayName = "BGRA8"),
	RGBA16 UMETA(DisplayName = "RGBA16"),
	RGBA16F UMETA(DisplayName = "RGBA16F"),
	RGBA32F UMETA(DisplayName = "RGBA32F")
};

/**
 * @brief Output texture creation settings.
 */
USTRUCT()

struct TEXTURECHANNELPACKER_API FTextureChannelPackerOutputSettings
{
	GENERATED_BODY()

	/** Use the first readable input dimensions; otherwise use the manual square resolution. */
	UPROPERTY(EditAnywhere, Category = "Texture")
	bool bUseFirstInputResolution = true;

	/** Manual output width and height, clamped to [1, 16384]. */
	UPROPERTY(EditAnywhere, Category = "Texture", meta = (ClampMin = "1", ClampMax = "16384", EditCondition = "!bUseFirstInputResolution"))
	int32 Resolution = 2048;

	/** Editor source precision and channel layout; Auto derives precision from the inputs. */
	UPROPERTY(EditAnywhere, Category = "Texture")
	ETextureChannelPackerOutputFormat SourceFormat = ETextureChannelPackerOutputFormat::Auto;

	/** Compression used when Unreal builds the output platform texture. */
	UPROPERTY(EditAnywhere, Category = "Texture")
	TEnumAsByte<TextureCompressionSettings> CompressionSettings = TC_Masks;

	/** Mip generation policy applied to the output asset. */
	UPROPERTY(EditAnywhere, Category = "Texture")
	TEnumAsByte<TextureMipGenSettings> MipGenSettings = TMGS_FromTextureGroup;

	/** Runtime texture sampling filter; CPU resizing always uses nearest-neighbor sampling. */
	UPROPERTY(EditAnywhere, Category = "Texture")
	TEnumAsByte<TextureFilter> Filter = TF_Default;

	/** Texture group governing platform LOD and streaming defaults. */
	UPROPERTY(EditAnywhere, Category = "Texture")
	TEnumAsByte<TextureGroup> LODGroup = TEXTUREGROUP_World;

	/** Output sampling color-space flag; source channel arithmetic is performed without gamma conversion. */
	UPROPERTY(EditAnywhere, Category = "Texture")
	bool bSRGB = false;

	/** Enable virtual texture streaming on the generated asset. */
	UPROPERTY(EditAnywhere, Category = "Texture")
	bool bVirtualTextureStreaming = false;
};

/**
 * @brief Describes a single channel mapping inside a preset.
 *
 * Specifies which source channel should be copied,
 * how it is identified, and where it should be written.
 */
USTRUCT()

struct TEXTURECHANNELPACKER_API FTextureChannelPackerPresetChannel
{
	GENERATED_BODY()

	/** Whether this output is enabled for Unpack. Defaults to true for older presets. */
	UPROPERTY(EditAnywhere, Category = "Channel")
	bool bEnabled = true;

	/** Component sampled from the matching input texture. */
	UPROPERTY(EditAnywhere, Category = "Channel")
	ETextureChannelPackerChannel SourceChannel = ETextureChannelPackerChannel::Red;

	/** Destination component for Pack, Repack and Copy. */
	UPROPERTY(EditAnywhere, Category = "Channel")
	ETextureChannelPackerChannel OutputChannel = ETextureChannelPackerChannel::Red;

	/** Case-insensitive wildcard or substring patterns separated by commas or semicolons. */
	UPROPERTY(EditAnywhere, Category = "Channel")
	FString SourcePattern;

	/** Per-channel suffix used to name an Unpack output. */
	UPROPERTY(EditAnywhere, Category = "Channel")
	FString OutputSuffix;

	/** Use ConstantValue instead of sampling a source texture. */
	UPROPERTY(EditAnywhere, Category = "Channel")
	bool bUseConstant = false;

	/** Unsigned constant normalized by 255 before output conversion. */
	UPROPERTY(EditAnywhere, Category = "Channel", meta = (ClampMin = "0", ClampMax = "255"))
	uint8 ConstantValue = 0;
};

/**
 * @brief Preset describing a complete packing or unpacking operation.
 *
 * Combines channel mappings, output naming rules and
 * texture creation settings into a reusable configuration.
 */
USTRUCT()

struct TEXTURECHANNELPACKER_API FTextureChannelPackerPreset
{
	GENERATED_BODY()

	/** Preset identifier; settings lookups and replacement are case-insensitive. */
	UPROPERTY(EditAnywhere, Category = "Preset")
	FString Name;

	/** Operation restored when this preset is applied. */
	UPROPERTY(EditAnywhere, Category = "Preset")
	ETextureChannelPackerOperation Operation = ETextureChannelPackerOperation::Pack;

	/** Suffix appended to automatically generated base names; empty is a valid naming rule. */
	UPROPERTY(EditAnywhere, Category = "Preset")
	FString OutputSuffix = TEXT("_Packed");

	/** Reusable channel mappings or Unpack outputs, including disabled outputs. */
	UPROPERTY(EditAnywhere, Category = "Preset")
	TArray<FTextureChannelPackerPresetChannel> Channels;

	/** Texture creation settings restored with the preset. */
	UPROPERTY(EditAnywhere, Category = "Preset")
	FTextureChannelPackerOutputSettings OutputSettings;
};

/**
 * @brief Source channel description used during processing.
 *
 * References a source texture together with the channel
 * that should be sampled, or a constant value when enabled.
 */
struct TEXTURECHANNELPACKER_API FTextureChannelPackerSource
{
	/** Weak input reference; processing callers must keep source assets alive. */
	TWeakObjectPtr<UTexture> Texture;
	/** Component to sample when a texture is provided. */
	ETextureChannelPackerChannel Channel = ETextureChannelPackerChannel::Red;
	/** When true, ignore Texture and use ConstantValue. */
	bool bUseConstant = false;
	/** Unsigned constant converted to normalized units by dividing by 255. */
	uint8 ConstantValue = 0;
};

/**
 * @brief Maps a source channel to an output channel.
 */
struct TEXTURECHANNELPACKER_API FTextureChannelPackerMapping
{
	/** Source texture component or explicit constant. */
	FTextureChannelPackerSource Source;
	
	/** Destination component; missing RGB components default to zero and alpha to one. */
	ETextureChannelPackerChannel OutputChannel = ETextureChannelPackerChannel::Red;
	
	/** Naming rule retained when this request is captured as a preset. */
	FString SourcePattern;
};

/**
 * @brief Describes an output texture generated during an unpack operation.
 */
struct TEXTURECHANNELPACKER_API FTextureChannelPackerUnpackOutput
{
	/** Packed input component copied into the red component of a separate output. */
	ETextureChannelPackerChannel SourceChannel = ETextureChannelPackerChannel::Red;
	
	/** Suffix appended to the request base name for this output. */
	FString OutputSuffix;
	
	/** Disabled outputs are retained in presets but skipped during generation. */
	bool bEnabled = true;
};

/** @brief Mutable state of one Slate channel row; converted to a request before processing. */
struct FChannelRowState
{
	/** Weak texture reference selected in the row picker. */
	TWeakObjectPtr<UTexture> Texture;
	
	/** Source component selected by the row combo box. */
	ETextureChannelPackerChannel SourceChannel = ETextureChannelPackerChannel::Red;
	
	/** Fixed output component represented by this row. */
	ETextureChannelPackerChannel OutputChannel = ETextureChannelPackerChannel::Red;
	
	/** Whether the row uses a constant instead of its texture. */
	bool bUseConstant = false;
	
	/** UI constant, clamped to [0, 255] when building a processing request. */
	int32 ConstantValue = 0;
	
	/** Pattern used when applying Content Browser selection. */
	FString SourcePattern;
};

/**
 * @brief Request describing a texture channel packing operation.
 *
 * Contains all information required to execute a pack,
 * repack, unpack or copy operation.
 */
struct TEXTURECHANNELPACKER_API FTextureChannelPackerRequest
{
	/** Persistent operation or preview mapping mode. */
	ETextureChannelPackerOperation Operation = ETextureChannelPackerOperation::Pack;
	
	/** Destination /Game folder or absolute directory inside project Content. */
	FString OutputPackagePath = TEXT("/Game");
	
	/** Base asset name sanitized before unique package naming. */
	FString OutputBaseName = TEXT("T_Packed_Texture");
	
	/** Requested source format, dimensions and asset settings. */
	FTextureChannelPackerOutputSettings OutputSettings;
	
	/** Channel mappings for Pack, Repack and Copy; ignored by Unpack. */
	TArray<FTextureChannelPackerMapping> Mappings;
	
	/** Weak packed input reference used only by Unpack. */
	TWeakObjectPtr<UTexture> UnpackSourceTexture;
	
	/** Ordered Unpack output definitions; generation skips disabled entries. */
	TArray<FTextureChannelPackerUnpackOutput> UnpackOutputs;
};

/**
 * @brief Result returned after executing a texture packing operation.
 *
 * Contains execution status, a descriptive message and
 * references to any textures created during the operation.
 */
struct TEXTURECHANNELPACKER_API FTextureChannelPackerResult
{
	/** True only when the requested operation completed successfully. */
	bool bSuccess = false;
	
	/** User-facing completion or error description. */
	FText Message;
	
	/** Saved outputs; on Unpack failure, contains only outputs that could not be removed during rollback. */
	TArray<TWeakObjectPtr<UTexture2D>> CreatedTextures;
};

/**
 * @brief Result returned after generating a preview image.
 *
 * Contains the generated preview texture together with
 * its dimensions and execution status.
 */
struct TEXTURECHANNELPACKER_API FTextureChannelPackerPreviewResult
{
	/** True when PreviewTexture was generated successfully. */
	bool bSuccess = false;
	
	/** User-facing preview status or failure description. */
	FText Message;
	
	/** Transient output owned by the caller after return; retain a strong reference while displaying it. */
	UTexture2D* PreviewTexture = nullptr;
	
	/** Dimensions of the display texture, bounded to 512 pixels on its longest side. */
	FIntPoint PreviewSize = FIntPoint::ZeroValue;
};
