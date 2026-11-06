// Copyright (c) 2026 DimAlek. All Rights Reserved.
#include "Tests/DataAssetManagerTestHelpers.h"
#include "Services/DataAssetRegistrySubscription.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRegistrySubscriptionLifetimeTest, "DataAssetManager.Integration.RegistrySubscription.ReleaseStopsCallbacks", DataAssetManagerFlags::Flags)

/** @brief Uses real registry events to verify callbacks stop when the subscription owner is released. */
bool FRegistrySubscriptionLifetimeTest::RunTest(const FString& Parameters)
{
	using namespace DataAssetManagerTests;
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().WaitForCompletion();
	int32 Added = 0;
	TUniquePtr<FDataAssetRegistrySubscription> Subscription = MakeUnique<FDataAssetRegistrySubscription>(
		[&Added](const FAssetData&) { ++Added; },
		[](const FAssetData&) {},
		[](const FAssetData&, const FString&) {});
	{
		FScopedTestDataAsset First(MakeUniqueAssetName(TEXT("Subscribed")));
		TestNotNull(TEXT("First fixture created"), First.GetAsset());
		TestTrue(TEXT("Live subscription receives registry events"), Added > 0);
	}
	Subscription.Reset();
	const int32 Before = Added;
	{
		FScopedTestDataAsset Second(MakeUniqueAssetName(TEXT("Unsubscribed")));
		TestNotNull(TEXT("Second fixture created"), Second.GetAsset());
	}
	TestEqual(TEXT("Released subscription receives no further events"), Added, Before);
	return true;
}
