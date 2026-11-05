// Copyright (c) 2026 DimAlek. All Rights Reserved.
#include "Tests/DataAssetManagerTestTypes.h"
#include "Services/DataAssetRandomValueService.h"
#include "UObject/UnrealType.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDataAssetRandomValueTest, "DataAssetManager.Unit.RandomValues.IntegerBounds", DataAssetManagerFlags::Flags)

/** @brief Verifies numeric type limits, fractional bounds and rejected non-finite input. */
bool FDataAssetRandomValueTest::RunTest(const FString& Parameters)
{
	const FNumericProperty* Byte = FindFProperty<FNumericProperty>(UTestDataAsset::StaticClass(), TEXT("ByteValue"));
	const FNumericProperty* Wide = FindFProperty<FNumericProperty>(UTestDataAsset::StaticClass(), TEXT("WideValue"));
	if (!Byte || !Wide) { AddError(TEXT("Numeric fixtures unavailable")); return false; }
	TestEqual(TEXT("Byte clamps high bound"), FDataAssetRandomValueService::GenerateInteger(*Byte, 255, 1000), FString(TEXT("255")));
	TestTrue(TEXT("Disjoint byte interval rejected"), FDataAssetRandomValueService::GenerateInteger(*Byte, 256, 1000).IsEmpty());
	TestEqual(TEXT("Wide integer is not restricted to int32"), FDataAssetRandomValueService::GenerateInteger(*Wide, 5000000000.0, 5000000000.0), FString(TEXT("5000000000")));
	TestEqual(TEXT("Fractional bounds retain the only integer"), FDataAssetRandomValueService::GenerateInteger(*Wide, 1.2, 2.8), FString(TEXT("2")));
	TestTrue(TEXT("Empty fractional interval rejected"), FDataAssetRandomValueService::GenerateInteger(*Wide, 1.2, 1.8).IsEmpty());
	TestTrue(TEXT("Reversed interval rejected"), FDataAssetRandomValueService::GenerateInteger(*Wide, 2, 1).IsEmpty());
	TestTrue(TEXT("NaN rejected before casting"), FDataAssetRandomValueService::GenerateInteger(*Wide, std::numeric_limits<double>::quiet_NaN(), 1).IsEmpty());
	TestTrue(TEXT("Infinity rejected before casting"), FDataAssetRandomValueService::GenerateInteger(*Wide, 0, std::numeric_limits<double>::infinity()).IsEmpty());
	for (int32 Index = 0; Index < 128; ++Index) {
		const int64 Value = FCString::Atoi64(*FDataAssetRandomValueService::GenerateInteger(*Byte, -1000, 1000));
		TestTrue(TEXT("Samples remain representable as a byte"), Value >= 0 && Value <= 255);
	}
	return true;
}
