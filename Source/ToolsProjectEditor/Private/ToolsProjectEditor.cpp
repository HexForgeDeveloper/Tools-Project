#include "ToolsProjectEditor.h"

#include "Commandlets/Commandlet.h"
#include "DataAssetManager.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "TextureChannelPacker.h"
#include "ToolMenus.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY(ToolsProjectEditor);

#define LOCTEXT_NAMESPACE "FToolsProjectEditor"

namespace ToolsProjectEditorModule
{
	const FName ToolbarSectionName(TEXT("ToolsProjectEditor"));
	const FName DataAssetManagerModuleName(TEXT("DataAssetManager"));
	const FName OutlinerToolkitAuditTabId(TEXT("OutlinerToolkitAudit"));
	const FName OutlinerToolkitModuleName(TEXT("OutlinerToolkit"));
	const FName TextureChannelPackerModuleName(TEXT("TextureChannelPacker"));

	/**
	 * @brief Runs a project commandlet in a separate Unreal process with its default arguments.
	 * @param ClassPath Fully qualified commandlet class path.
	 */
	void LaunchCommandlet(const FString& ClassPath)
	{
		const FString ProjectPath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
		const FString Arguments = FString::Printf(TEXT("\"%s\" -run=\"%s\" -unattended"), *ProjectPath, *ClassPath);
		FProcHandle Process = FPlatformProcess::CreateProc(FPlatformProcess::ExecutablePath(), *Arguments,
			false, true, true, nullptr, 0, nullptr, nullptr);
		if (Process.IsValid())
		{
			FPlatformProcess::CloseProc(Process);
			return;
		}

		UE_LOG(ToolsProjectEditor, Error, TEXT("Could not start commandlet %s."), *ClassPath);
	}

	/**
	 * @brief Populates Cmd with concrete commandlets from the project editor module.
	 * @param Menu Submenu to populate when it opens.
	 */
	void PopulateCommandletMenu(UToolMenu* Menu)
	{
		FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("ProjectCommandlets"));
		const FName EditorPackageName(TEXT("/Script/ToolsProjectEditor"));
		TArray<UClass*> Commandlets;
		for (TObjectIterator<UClass> Class; Class; ++Class)
		{
			if (Class->GetOutermost()->GetFName() == EditorPackageName
				&& Class->IsChildOf(UCommandlet::StaticClass())
				&& !Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
			{
				Commandlets.Add(*Class);
			}
		}

		Commandlets.Sort([](const UClass& Left, const UClass& Right) {
			return Left.GetFName().LexicalLess(Right.GetFName());
		});
		for (const UClass* Commandlet : Commandlets)
		{
			const FString ClassPath = Commandlet->GetPathName();
			const UCommandlet* Defaults = Commandlet->GetDefaultObject<UCommandlet>();
			const FText Tooltip = Defaults->HelpDescription.IsEmpty()
				? LOCTEXT("RunCommandletTooltip", "Run this commandlet in a separate Unreal process without arguments.")
				: FText::FromString(Defaults->HelpDescription);
			Section.AddMenuEntry(Commandlet->GetFName(), Commandlet->GetDisplayNameText(), Tooltip, FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([ClassPath] { LaunchCommandlet(ClassPath); })));
		}

		if (Commandlets.IsEmpty())
		{
			Section.AddMenuEntry(TEXT("NoProjectCommandlets"), LOCTEXT("NoProjectCommandlets", "No project commandlets"),
				LOCTEXT("NoProjectCommandletsTooltip", "Add a concrete UCommandlet class to the ToolsProjectEditor module."),
				FSlateIcon(), FUIAction(FExecuteAction(), FCanExecuteAction::CreateLambda([] { return false; })));
		}
	}
} // namespace ToolsProjectEditorModule

static FAutoConsoleCommand ImportTexturesCmd(
	TEXT("Import.Textures"),
	TEXT("Import textures from folder"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) {

	}));

void FToolsProjectEditor::StartupModule()
{
	UE_LOG(ToolsProjectEditor, Warning, TEXT("ToolsProjectEditor module has been loaded"));

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FToolsProjectEditor::RegisterMenus));
}

void FToolsProjectEditor::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	if (UToolMenus::IsToolMenuUIEnabled())
	{
		UToolMenus::Get()->UnregisterOwner(this);
	}

	UE_LOG(ToolsProjectEditor, Warning, TEXT("ToolsProjectEditor module has been unloaded"));
}

void FToolsProjectEditor::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* MainMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu"));
	FToolMenuEntry& CommandletMenu = MainMenu->FindOrAddSection(NAME_None).AddSubMenu(
		TEXT("ToolsProjectCommandlets"),
		LOCTEXT("CommandletsMenu", "Cmd"),
		LOCTEXT("CommandletsMenuTooltip", "Run commandlets from the ToolsProjectEditor module."),
		FNewToolMenuDelegate::CreateStatic(&ToolsProjectEditorModule::PopulateCommandletMenu));
	CommandletMenu.InsertPosition = FToolMenuInsert(TEXT("Help"), EToolMenuInsertType::After);

	UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.User");
	FToolMenuSection& ToolbarSection = Toolbar->FindOrAddSection(ToolsProjectEditorModule::ToolbarSectionName);

	ToolbarSection.AddEntry(FToolMenuEntry::InitToolBarButton(
		"OpenDataAssetManagerToolbar",
		FUIAction(FExecuteAction::CreateRaw(this, &FToolsProjectEditor::OpenDataAssetManager)),
		LOCTEXT("OpenDataAssetManagerToolbar", "Data Assets"),
		LOCTEXT("OpenDataAssetManagerToolbarTooltip", "Open Data Asset Manager"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.DataAsset")));

	ToolbarSection.AddEntry(FToolMenuEntry::InitToolBarButton(
		"OpenTextureChannelPackerToolbar",
		FUIAction(FExecuteAction::CreateRaw(this, &FToolsProjectEditor::OpenTextureChannelPacker)),
		LOCTEXT("OpenTextureChannelPackerToolbar", "Texture Packer"),
		LOCTEXT("OpenTextureChannelPackerToolbarTooltip", "Open Texture Channel Packer"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Layout")));

	ToolbarSection.AddEntry(FToolMenuEntry::InitToolBarButton(
		"OpenOutlinerToolkitAuditToolbar",
		FUIAction(FExecuteAction::CreateRaw(this, &FToolsProjectEditor::OpenOutlinerToolkitAudit)),
		LOCTEXT("OpenOutlinerToolkitAuditToolbar", "Outliner Audit"),
		LOCTEXT("OpenOutlinerToolkitAuditToolbarTooltip", "Open Outliner Toolkit Audit"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Warning")));
}

void FToolsProjectEditor::OpenDataAssetManager()
{
	IDataAssetManagerModule& DataAssetManagerModule =
		FModuleManager::LoadModuleChecked<IDataAssetManagerModule>(ToolsProjectEditorModule::DataAssetManagerModuleName);
	DataAssetManagerModule.OpenManagerTab();
}

void FToolsProjectEditor::OpenOutlinerToolkitAudit()
{
	FModuleManager::LoadModuleChecked<IModuleInterface>(ToolsProjectEditorModule::OutlinerToolkitModuleName);
	FGlobalTabmanager::Get()->TryInvokeTab(ToolsProjectEditorModule::OutlinerToolkitAuditTabId);
}

void FToolsProjectEditor::OpenTextureChannelPacker()
{
	ITextureChannelPackerModule& TextureChannelPackerModule =
		FModuleManager::LoadModuleChecked<ITextureChannelPackerModule>(ToolsProjectEditorModule::TextureChannelPackerModuleName);
	TextureChannelPackerModule.OpenPluginWindow();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FToolsProjectEditor, ToolsProjectEditor)
