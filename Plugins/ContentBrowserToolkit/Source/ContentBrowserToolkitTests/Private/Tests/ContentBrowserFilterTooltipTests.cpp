// Copyright (c) 2026 DimAlek. All Rights Reserved.

#include "ContentBrowserToolkitFilterExtension.h"
#include "Misc/AutomationTest.h"
#include "Settings/ContentBrowserToolkitSettings.h"
#include "Templates/UnrealTemplate.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** @brief Builds the filter list through the editor extension with optional audit filters enabled. */
	TArray<TSharedRef<FFrontendFilter>> CollectFiltersWithAuditEnabled()
	{
		const TGuardValue<bool> EnableAuditFilters(UContentBrowserToolkitSettings::GetMutable()->bEnableCachedAuditFilters, true);
		TArray<TSharedRef<FFrontendFilter>> Filters;
		GetDefault<UContentBrowserToolkitFilterExtension>()->AddFrontEndFilterExtensions(nullptr, Filters);
		return Filters;
	}
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FContentBrowserFilterTooltipTest, "ContentBrowserToolkit.Filters.Tooltips",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** @brief Registers one case per filter ID and reports empty discovery as a failing case. */
void FContentBrowserFilterTooltipTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	const TArray<TSharedRef<FFrontendFilter>> Filters = CollectFiltersWithAuditEnabled();
	for (const TSharedRef<FFrontendFilter>& Filter : Filters)
	{
		const FString FilterId = Filter->GetName();
		OutBeautifiedNames.Add(FilterId);
		OutTestCommands.Add(FilterId);
	}

	if (Filters.IsEmpty())
	{
		OutBeautifiedNames.Add(TEXT("NoFiltersRegistered"));
		OutTestCommands.Add(TEXT("NoFiltersRegistered"));
	}
}

/** @brief Checks that the registered filter exposes tooltip text beyond whitespace. */
bool FContentBrowserFilterTooltipTest::RunTest(const FString& Parameters)
{
	const TArray<TSharedRef<FFrontendFilter>> Filters = CollectFiltersWithAuditEnabled();
	const TSharedRef<FFrontendFilter>* Filter = Filters.FindByPredicate([&Parameters](const TSharedRef<FFrontendFilter>& Candidate) {
		return Candidate->GetName() == Parameters;
	});
	if (Filter == nullptr)
	{
		AddError(FString::Printf(TEXT("No registered Content Browser Toolkit filter for case '%s'."), *Parameters));
		return false;
	}

	const FString Tooltip = (*Filter)->GetToolTipText().ToString().TrimStartAndEnd();
	return TestFalse(FString::Printf(TEXT("Filter '%s' has an empty tooltip"), *Parameters), Tooltip.IsEmpty());
}

#endif // WITH_DEV_AUTOMATION_TESTS
