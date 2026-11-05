// Copyright (c) 2026 DimAlek. All Rights Reserved.
#include "Services/DataAssetRandomValueService.h"
#include "UObject/UnrealType.h"
#include <cmath>

FString FDataAssetRandomValueService::GenerateInteger(const FNumericProperty& Property, double Min, double Max)
{
	if (!Property.IsInteger() || Property.IsEnum() || !FMath::IsFinite(Min) || !FMath::IsFinite(Max) || Min > Max)
	{
		return FString();
	}
	constexpr double ExactLimit = 9007199254740991.0;
	const bool bUnsigned = !Property.CanHoldValue(static_cast<int64>(-1));
	const int32 Bits = Property.ElementSize * 8;
	const double TypeMin = bUnsigned ? 0.0 : (Bits < 64 ? -std::ldexp(1.0, Bits - 1) : -ExactLimit);
	const double TypeMax = Bits < 64 ? std::ldexp(1.0, bUnsigned ? Bits : Bits - 1) - 1.0 : ExactLimit;
	Min = std::ceil(FMath::Max(Min, FMath::Max(TypeMin, -ExactLimit)));
	Max = std::floor(FMath::Min(Max, FMath::Min(TypeMax, ExactLimit)));
	if (Min > Max) { return FString(); }
	const int64 Low = static_cast<int64>(Min);
	const uint64 Count = static_cast<uint64>(static_cast<int64>(Max) - Low) + 1;
	// Rejection sampling avoids modulo bias, including intervals wider than int32.
	FRandomStream Random(FMath::Rand());
	const uint64 Threshold = (uint64(0) - Count) % Count;
	uint64 Sample;
	do { Sample = (uint64(Random.GetUnsignedInt()) << 32) | Random.GetUnsignedInt(); } while (Sample < Threshold);
	return LexToString(Low + static_cast<int64>(Sample % Count));
}
