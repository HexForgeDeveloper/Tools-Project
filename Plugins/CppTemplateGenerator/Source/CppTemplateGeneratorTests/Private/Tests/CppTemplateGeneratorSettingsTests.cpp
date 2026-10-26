// Copyright 2025 DimAlek. All Rights Reserved.

#include "CppTemplateGeneratorSettings.h"
#include "CppTemplateGenerator.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "ToolMenus.h"
#include "ToolMenu.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCppTemplateGeneratorSanitizeTemplateClassesRemovesInvalidAndDuplicateEntriesTest,
	"CppTemplateGenerator.Settings.SanitizeTemplateClasses.RemovesInvalidAndDuplicateEntries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCppTemplateGeneratorSanitizeTemplateClassesRemovesInvalidAndDuplicateEntriesTest::RunTest(const FString& Parameters)
{
	UCppTemplateGeneratorSettings* const Settings = NewObject<UCppTemplateGeneratorSettings>(GetTransientPackage());
	TestNotNull(TEXT("Transient settings object should be created"), Settings);
	if (!Settings)
	{
		return false;
	}

	Settings->TemplateClasses = 
	{
		AActor::StaticClass(),
		nullptr,
		APawn::StaticClass(),
		AActor::StaticClass(),
		UObject::StaticClass()
	};

	const bool bWasModified = Settings->SanitizeTemplateClasses();

	TestTrue(TEXT("SanitizeTemplateClasses should report modifications when invalid or duplicate classes exist"), bWasModified);
	TestEqual(TEXT("Only supported unique classes should remain"), Settings->TemplateClasses.Num(), 2);
	TestEqual(TEXT("First valid class should be preserved"), Settings->TemplateClasses[0].Get(), AActor::StaticClass());
	TestEqual(TEXT("Second valid class should be preserved"), Settings->TemplateClasses[1].Get(), APawn::StaticClass());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCppTemplateGeneratorSanitizeTemplateClassesKeepsValidUniqueEntriesTest,
	"CppTemplateGenerator.Settings.SanitizeTemplateClasses.KeepsValidUniqueEntries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCppTemplateGeneratorSanitizeTemplateClassesKeepsValidUniqueEntriesTest::RunTest(const FString& Parameters)
{
	UCppTemplateGeneratorSettings* const Settings = NewObject<UCppTemplateGeneratorSettings>(GetTransientPackage());
	TestNotNull(TEXT("Transient settings object should be created"), Settings);
	if (!Settings)
	{
		return false;
	}

	Settings->TemplateClasses = 
	{
		AActor::StaticClass(),
		UActorComponent::StaticClass(),
		ACharacter::StaticClass()
	};

	const bool bWasModified = Settings->SanitizeTemplateClasses();

	TestFalse(TEXT("SanitizeTemplateClasses should not report modifications for already valid unique classes"), bWasModified);
	TestEqual(TEXT("Valid unique classes should be preserved"), Settings->TemplateClasses.Num(), 3);
	TestEqual(TEXT("Actor entry should remain unchanged"), Settings->TemplateClasses[0].Get(), AActor::StaticClass());
	TestEqual(TEXT("Component entry should remain unchanged"), Settings->TemplateClasses[1].Get(), UActorComponent::StaticClass());
	TestEqual(TEXT("Character entry should remain unchanged"), Settings->TemplateClasses[2].Get(), ACharacter::StaticClass());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCppTemplateGeneratorRejectsUnsupportedClassesTest,
	"CppTemplateGenerator.Settings.RejectsUnsupportedClasses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCppTemplateGeneratorRejectsUnsupportedClassesTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Null is rejected"), UCppTemplateGeneratorSettings::IsSupportedTemplateClass(nullptr));
	TestFalse(TEXT("Unrelated native class is rejected"), UCppTemplateGeneratorSettings::IsSupportedTemplateClass(UObject::StaticClass()));
	UClass* Candidate = NewObject<UClass>(GetTransientPackage());
	Candidate->SetSuperStruct(AActor::StaticClass());
	TestFalse(TEXT("Non-native Actor is rejected"), UCppTemplateGeneratorSettings::IsSupportedTemplateClass(Candidate));
	Candidate->ClassFlags = CLASS_Native | CLASS_Deprecated;
	TestFalse(TEXT("Deprecated native Actor is rejected"), UCppTemplateGeneratorSettings::IsSupportedTemplateClass(Candidate));
	Candidate->ClassFlags = CLASS_Native | CLASS_NewerVersionExists;
	TestFalse(TEXT("Replaced native Actor is rejected"), UCppTemplateGeneratorSettings::IsSupportedTemplateClass(Candidate));
	Candidate->ClassFlags = CLASS_None;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCppTemplateGeneratorMenuLifecycleTest,
	"CppTemplateGenerator.Menu.RegistrationAndCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCppTemplateGeneratorMenuLifecycleTest::RunTest(const FString& Parameters)
{
	FCppTemplateGeneratorModule& LiveModule = FModuleManager::LoadModuleChecked<FCppTemplateGeneratorModule>(TEXT("CppTemplateGenerator"));
	FCppTemplateGeneratorModule TestModule;
	ON_SCOPE_EXIT
	{
		TestModule.ShutdownModule();
		LiveModule.RegisterMenus();
	};

	// Remove the live extension so it cannot hide a failed test registration.
	UToolMenus::Get()->UnregisterOwner(&LiveModule);
	TestModule.RegisterMenus();
	UToolMenu* Menu = UToolMenus::Get()->FindMenu(TEXT("MainFrame.MainMenu.Tools"));
	if (!TestNotNull(TEXT("Tools menu exists"), Menu))
	{
		return false;
	}
	const auto FindTemplateEntry = [Menu]() -> FToolMenuEntry*
	{
		FToolMenuSection* Section = Menu->FindSection(TEXT("Programming"));
		return Section ? Section->FindEntry(TEXT("CreateCppTemplate")) : nullptr;
	};
	TestNotNull(TEXT("Template submenu registered"), FindTemplateEntry());
	TestModule.RegisterMenus();
	int32 EntryCount = 0;
	for (const FToolMenuSection& Section : Menu->Sections)
	{
		for (const FToolMenuEntry& Entry : Section.Blocks)
		{
			EntryCount += Entry.Name == TEXT("CreateCppTemplate") ? 1 : 0;
		}
	}
	TestEqual(TEXT("Repeated registration does not duplicate entries"), EntryCount, 1);
	TestModule.ShutdownModule();
	TestNull(TEXT("Shutdown removes the owned submenu"), FindTemplateEntry());
	TestModule.ShutdownModule();
	TestModule.RegisterMenus();
	TestNotNull(TEXT("Menu can be registered again after cleanup"), FindTemplateEntry());
	return true;
}

#endif
