// Copyright (c) 2026 DimAlek. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
class FNumericProperty;

/** @brief Generates numeric property text without depending on Slate or property handles. */
class DATAASSETMANAGER_API FDataAssetRandomValueService final
{
public:
	/** @brief Samples an integer inside both the requested interval and the property type.
	 * @param Property Integer property, excluding enums.
	 * @param Min Inclusive lower bound; fractional values are rounded upward.
	 * @param Max Inclusive upper bound; fractional values are rounded downward.
	 * @return Formatted integer, or empty text for invalid/empty intervals.
	 * @note Double-based UI bounds are limited to exactly representable integers (+/- 2^53 - 1).
	 */
	static FString GenerateInteger(const FNumericProperty& Property, double Min, double Max);
};
