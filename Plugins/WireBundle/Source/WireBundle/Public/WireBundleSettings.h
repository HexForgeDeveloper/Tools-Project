#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"
#include "WireBundleSettings.generated.h"

UENUM(BlueprintType)
enum class EWireBundleMovementAxis : uint8
{
	Free,
	Horizontal,
	Vertical
};

USTRUCT(BlueprintType)
struct WIREBUNDLE_API FWireBundleMovementSettings
{
	GENERATED_BODY()
    /** Search for a clear bundle corridor before creating reroutes. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Routing")
    bool bAvoidNodes = false;
    /** Add native reroutes when a single parallel corridor is not enough. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Routing", meta = (EditCondition = "bAvoidNodes"))
    bool bAllowExtraBends = true;
    /** Total reroutes allowed on each wire, including the wheel-selected columns. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Routing", meta = (ClampMin = "2", ClampMax = "64", EditCondition = "bAvoidNodes && bAllowExtraBends"))
    int32 MaxRoutingReroutes = 16;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Routing", meta = (ClampMin = "8", ClampMax = "256", EditCondition = "bAvoidNodes"))
    float ObstacleClearance = 24.0f;
    /** Maximum vertical displacement from the requested bundle center. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Routing", meta = (ClampMin = "32", ClampMax = "4096", EditCondition = "bAvoidNodes"))
    float MaxRouteDetour = 1024.0f;
	/** Initial mode for each gesture; the wheel can change it before release. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "1", ClampMax = "32"))
	int32 RerouteColumns = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "2", ClampMax = "32"))
	int32 MaxRerouteColumns = 8;
	/** Distance between the outer reroute columns, in graph units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "16", ClampMax = "4096"))
	float BundleWidth = 160.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "1", ClampMax = "256"))
	float WireSpacing = 12.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	FVector2D CenterOffset = FVector2D::ZeroVector;
	/** Snap bundle center and plugin mouse/keyboard movement to a grid in graph units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	bool bSnapToGrid = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "1", ClampMax = "512", EditCondition = "bSnapToGrid"))
	float GridSize = 16.0f;
	/** Ctrl+Alt+Arrow moves selected native reroutes by this many graph units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "1", ClampMax = "1024"))
	float NudgeStep = 16.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "1", ClampMax = "32"))
	float FastNudgeMultiplier = 4.0f;
	/** Applies to plugin keyboard movement; native mouse dragging keeps Unreal's controls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	EWireBundleMovementAxis KeyboardAxis = EWireBundleMovementAxis::Free;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouse Editing")
	bool bEnableMouseEditing = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouse Editing")
	EWireBundleMovementAxis MouseAxis = EWireBundleMovementAxis::Free;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection")
	bool bSelectCreatedReroutes = true;
};

USTRUCT(BlueprintType)
struct WIREBUNDLE_API FWireBundleGestureSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	FKey GestureButton = EKeys::LeftMouseButton;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	FKey BundleKey = EKeys::B;
	/** Explicitly register selected legacy reroute chains without changing their layout. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	FKey AdoptKey = EKeys::A;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	FKey CancelKey = EKeys::Escape;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bRequireControl = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bRequireAlt = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bRequireShift = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bWheelChangesColumns = true;
	/** Slate desktop coordinate units (before the overlay's local transform). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection", meta = (ClampMin = "1", ClampMax = "128"))
	float MinimumDragDistance = 8.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection", meta = (ClampMin = "0", ClampMax = "32"))
	float WireHitTolerance = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection", meta = (ClampMin = "2", ClampMax = "256"))
	int32 MinimumWireCount = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection", meta = (ClampMin = "16", ClampMax = "256"))
	int32 CurveSamples = 96;
};

USTRUCT(BlueprintType)
struct WIREBUNDLE_API FWireBundleAppearanceSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor ReadyColor = FLinearColor(0.15f, 1.0f, 0.55f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor PendingColor = FLinearColor(1.0f, 0.65f, 0.15f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor WireHighlightColor = FLinearColor(0.1f, 0.8f, 1.0f, 0.8f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lines", meta = (ClampMin = "0.5", ClampMax = "16"))
	float GuideThickness = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lines", meta = (ClampMin = "0.5", ClampMax = "16"))
	float HighlightThickness = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Route Preview")
	bool bShowRoutePreview = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Route Preview")
	FLinearColor RoutePreviewColor = FLinearColor(0.4f, 1.0f, 0.65f, 0.75f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Route Preview", meta = (ClampMin = "0.5", ClampMax = "16"))
	float RoutePreviewThickness = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape Handles")
	FLinearColor ShapeHandleColor = FLinearColor(1.0f, 0.8f, 0.15f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape Handles", meta = (ClampMin = "4", ClampMax = "24"))
	float ShapeHandleRadius = 8.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markers")
	bool bShowCursorMarkers = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markers", meta = (ClampMin = "1", ClampMax = "64"))
	float MarkerRadius = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lines")
	bool bHighlightCrossedWires = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hints")
	bool bShowGestureHint = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hints", meta = (ClampMin = "8", ClampMax = "32"))
	int32 HintFontSize = 11;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hints")
	FVector2D HintOffset = FVector2D(16, 12);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hints")
	bool bShowNotifications = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hints", meta = (ClampMin = "0.5", ClampMax = "30"))
	float NotificationDuration = 4.0f;
};

/** Editor Preferences -> Plugins -> Wire Bundle. Settings are stored in editor config. */
UCLASS(Config = EditorPerProjectUserSettings, meta = (DisplayName = "Wire Bundle"))
class WIREBUNDLE_API UWireBundleEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Config, Category = "Movement", meta = (ShowOnlyInnerProperties))
	FWireBundleMovementSettings Movement;
	UPROPERTY(EditAnywhere, Config, Category = "Gesture", meta = (ShowOnlyInnerProperties))
	FWireBundleGestureSettings Gesture;
	UPROPERTY(EditAnywhere, Config, Category = "Appearance", meta = (ShowOnlyInnerProperties))
	FWireBundleAppearanceSettings Appearance;
	UPROPERTY(EditAnywhere, Config, Category = "Editors")
	bool bEnableBlueprint = true;
	UPROPERTY(EditAnywhere, Config, Category = "Editors")
	bool		  bEnableMaterial = true;
	virtual FName GetContainerName() const override { return TEXT("Editor"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	virtual FName GetSectionName() const override { return TEXT("WireBundle"); }
};

/** A value snapshot; changes to preferences apply on the next command or gesture. */
struct WIREBUNDLE_API FWireBundleResolvedSettings
{
	FWireBundleMovementSettings	  Movement;
	FWireBundleGestureSettings	  Gesture;
	FWireBundleAppearanceSettings Appearance;
	bool						  bEnableBlueprint = true;
	bool						  bEnableMaterial = true;
	void						  Sanitize();
};

WIREBUNDLE_API FWireBundleResolvedSettings ResolveWireBundleSettings();
