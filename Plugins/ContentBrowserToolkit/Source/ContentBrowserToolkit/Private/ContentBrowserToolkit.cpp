// Copyright 2025 DimAlek. All Rights Reserved.
#include "ContentBrowserToolkit.h"
#include "ContentBrowserMenuContexts.h"
#include "ContentBrowserModule.h"
#include "UI/ContentBrowserFolderAuditWindow.h"
#include "UI/ContentBrowserAssetAuditWindow.h"
#include "Services/ContentBrowserAssetSearch.h"
#include "Services/ContentBrowserInspection.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstance.h"
#include "Sound/SoundWave.h"
#include "Animation/Skeleton.h"
#include "Engine/Texture2D.h"
#include "UI/ContentBrowserTextureAuditWindow.h"
#include "Animation/AnimationAsset.h"
#include "Engine/Blueprint.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"
#include "Services/ContentBrowserUsageAudit.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "IContentBrowserSingleton.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "UI/SUsageRowWidget.h"
#include "Misc/ContentBrowserUtils.h"
#include "UI/SContentUsageAuditWidget.h"




void FContentBrowserToolkitModule::StartupModule()
{
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FContentBrowserToolkitModule::RegisterMenus));
}

void FContentBrowserToolkitModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	for (const auto& Weak : Windows)
	{
		if (const auto Window = Weak.Pin())
		{
			Window->SetContent(SNullWidget::NullWidget);
			if (FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());
			}
		}
	}
}

void FContentBrowserToolkitModule::Open(FString Folder, TArray<FAssetData> Assets)
{
	const auto Window = SNew(SWindow)
						.Title(FText::FromString(TEXT("Asset Audit to Map")))
						.ClientSize(FVector2D(1050, 680))
						[
							SNew(SContentUsageAuditWidget)
							.Folder(Folder)
							.Assets(Assets)
						];

	Windows.Add(Window);
	FSlateApplication::Get().AddWindow(Window);
}

void FContentBrowserToolkitModule::OpenFolderReport(const FString& Folder, bool bFindReferencers)
{
	const auto Window = CreateContentBrowserFolderAuditWindow(Folder, bFindReferencers);
	Windows.Add(Window);
	FSlateApplication::Get().AddWindow(Window);
}

void FContentBrowserToolkitModule::OpenInspection(const FText& Title, TArray<FAssetAuditRow> Rows)
{
	const auto Window = CreateContentBrowserAssetReportWindow(Title, MoveTemp(Rows), NSLOCTEXT("ContentBrowserToolkit", "InspectionEmpty", "No findings for this selection."));
	Windows.Add(Window);
	FSlateApplication::Get().AddWindow(Window);
}

void FContentBrowserToolkitModule::OpenMigrationPreview(const FString& Folder)
{
	const FText Title = NSLOCTEXT("ContentBrowserToolkit", "MigrationPreview", "Folder Migration Preview...");
	const auto Destination = MakeShared<FString>();
	const auto bAccepted = MakeShared<bool>(false);
	FPathPickerConfig Config;
	Config.DefaultPath = TEXT("/Game");
	Config.bNotifyDefaultPathSelected = true;
	Config.OnPathSelected = FOnPathSelected::CreateLambda([Destination](const FString& Path) { *Destination = Path; });
	/* clang-format off */
	const auto Dialog = SNew(SWindow)
						.Title(Title)
						.ClientSize(FVector2D(480, 450))
						.SupportsMaximize(false)
						.SupportsMinimize(false);

	const TWeakPtr<SWindow> WeakDialog = Dialog;
	Dialog->SetContent(
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8)
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("ContentBrowserToolkit", "MigrationPickDestination", "Choose the destination folder for the preview."))
		]

		+ SVerticalBox::Slot()
		.FillHeight(1)
		.Padding(8)
		[
			FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().CreatePathPicker(Config)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.Padding(8)
		[
			SNew(SButton)
			.Text(NSLOCTEXT("ContentBrowserToolkit", "MigrationRunPreview", "Preview"))
			.IsEnabled_Lambda([Destination] { return !Destination->IsEmpty(); })
			.OnClicked_Lambda([WeakDialog, bAccepted]
				{
					if (const auto Window = WeakDialog.Pin())
					{
						  *bAccepted = true;
						  Window->RequestDestroyWindow();
					}

					return FReply::Handled();
				})
		]);

	Windows.Add(Dialog);
	FSlateApplication::Get().AddModalWindow(Dialog, FSlateApplication::Get().GetActiveTopLevelWindow());
	if (*bAccepted)
	{
		OpenInspection(Title, BuildCBTMigrationPreview(Folder, *Destination));
	}
}

void FContentBrowserToolkitModule::RegisterMenus()
{
	FToolMenuOwnerScoped Owner(this);
	UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Window"))->FindOrAddSection(TEXT("ContentBrowserToolkit"))
		.AddMenuEntry(TEXT("CBTUsageAudit"), FText::FromString(TEXT("Asset Audit to Map")),
			FText::FromString(TEXT("Check project content against a map. For a specific selection, use its right-click menu.")),
			ContentBrowserToolkit::MenuIcon(TEXT("ClassIcon.World")), FUIAction(FExecuteAction::CreateLambda([this] { Open(TEXT("/Game")); })));

	UToolMenus::Get()->ExtendMenu(TEXT("ContentBrowser.AssetContextMenu"))->AddDynamicSection(
			TEXT("CBTUsageAudit"), FNewToolMenuDelegate::CreateLambda([this](UToolMenu* Menu) {
				const auto* Context = Menu->FindContext<UContentBrowserAssetContextMenuContext>();
				if (!Context || Context->SelectedAssets.IsEmpty())
				{
					return;
				}
				const TArray<FAssetData> Selected = Context->SelectedAssets;
				Menu->FindOrAddSection(TEXT("ContentBrowserToolkit")).AddSubMenu(TEXT("CBTCommands"), NSLOCTEXT("ContentBrowserToolkit", "CommandsMenu", "Content Browser Toolkit"), NSLOCTEXT("ContentBrowserToolkit", "CommandsMenuTip", "Open reports for the current Content Browser selection."), FNewToolMenuDelegate::CreateLambda([this, Selected](UToolMenu* SubMenu) {
					FToolMenuSection& Section = SubMenu->FindOrAddSection(TEXT("CBTCommands"));
					Section.AddMenuEntry(TEXT("CBTAssetAuditToMap"), FText::FromString(TEXT("Asset Audit to Map...")),
						FText::FromString(TEXT("Choose a map and check the selected assets and their references.")), ContentBrowserToolkit::MenuIcon(TEXT("ClassIcon.World")),
						FUIAction(FExecuteAction::CreateLambda([this, Selected] { Open(FString(), Selected); })));
					const auto AddRelationshipAudit = [this, &Section, &Selected](const UClass* AssetClass, FName Entry, const FText& Label,
														  const FText& Tooltip, FName Icon, ECBTAssetAudit Audit) {
						TArray<FAssetData> Matches = Selected.FilterByPredicate([AssetClass](const FAssetData& Asset) {
							return Asset.IsInstanceOf(AssetClass);
						});
						if (Matches.IsEmpty())
						{
							return;
						}
						Section.AddMenuEntry(Entry, Label, Tooltip, ContentBrowserToolkit::MenuIcon(Icon),
							FUIAction(FExecuteAction::CreateLambda([this, Audit, Label, Matches = MoveTemp(Matches)] {
								const auto Window = CreateContentBrowserAssetAuditWindow(Label, Audit, Matches);
								Windows.Add(Window);
								FSlateApplication::Get().AddWindow(Window);
							})));
					};
					AddRelationshipAudit(UMaterialInterface::StaticClass(), TEXT("CBTMaterialParentChain"),
						NSLOCTEXT("ContentBrowserToolkit", "MaterialParentMenu", "Material Parent Chain..."),
						NSLOCTEXT("ContentBrowserToolkit", "MaterialParentTooltip", "Trace selected material instances to their base materials."),
						TEXT("ClassIcon.Material"), ECBTAssetAudit::MaterialParents);
					AddRelationshipAudit(UBlueprint::StaticClass(), TEXT("CBTBlueprintParentAudit"),
						NSLOCTEXT("ContentBrowserToolkit", "BlueprintParentMenu", "Blueprint Parent Audit..."),
						NSLOCTEXT("ContentBrowserToolkit", "BlueprintParentTooltip", "Inspect the resolved parent class of each selected Blueprint."),
						TEXT("ClassIcon.Blueprint"), ECBTAssetAudit::BlueprintParent);
					AddRelationshipAudit(UAnimationAsset::StaticClass(), TEXT("CBTAnimationSkeletonAudit"),
						NSLOCTEXT("ContentBrowserToolkit", "AnimationSkeletonMenu", "Animation Skeleton Audit..."),
						NSLOCTEXT("ContentBrowserToolkit", "AnimationSkeletonTooltip", "Inspect skeletons and playback lengths for the selected animation assets."),
						TEXT("ClassIcon.Skeleton"), ECBTAssetAudit::AnimationSkeleton);
					/** @brief Adds a report command only when at least one selected asset supports it. */
					const auto AddSearch = [this, &Section, &Selected](FName Entry, const FText& Label, const FText& Tooltip, FName Icon, ECBTAssetSearch Search, auto IsSupported) {
						TArray<FAssetData> Matches = Selected.FilterByPredicate(IsSupported);
						if (Matches.IsEmpty())
						{
							return;
						}
						Section.AddMenuEntry(Entry, Label, Tooltip,ContentBrowserToolkit::MenuIcon(Icon),
							FUIAction(FExecuteAction::CreateLambda([this, Search, Label, Matches = MoveTemp(Matches)] {
								const auto Window = CreateContentBrowserAssetReportWindow(Label, BuildContentBrowserAssetSearch(Search, Matches),
									NSLOCTEXT("ContentBrowserToolkit", "AssetSearchEmpty", "No matching assets found."));
								Windows.Add(Window);
								FSlateApplication::Get().AddWindow(Window);
							})));
					};
					AddSearch(TEXT("CBTFindMaterialInstances"),
						NSLOCTEXT("ContentBrowserToolkit", "FindInstances", "Find Material Instances..."),
						NSLOCTEXT("ContentBrowserToolkit", "FindInstancesTip", "Find saved instances that directly or indirectly inherit from the selected materials."),
						TEXT("ClassIcon.MaterialInstanceConstant"), ECBTAssetSearch::MaterialInstances, [](const FAssetData& Asset) { return Asset.IsInstanceOf<UMaterialInterface>(); });
					AddSearch(TEXT("CBTFindChildBlueprints"),
						NSLOCTEXT("ContentBrowserToolkit", "FindChildBlueprints", "Find Child Blueprints..."),
						NSLOCTEXT("ContentBrowserToolkit", "FindChildBlueprintsTip", "Find saved Blueprints that directly or indirectly inherit from the selection."),
						TEXT("Icons.Search"), ECBTAssetSearch::ChildBlueprints, [](const FAssetData& Asset) { return Asset.IsInstanceOf<UBlueprint>(); });
					AddSearch(TEXT("CBTFindSkeletonAnimations"),
						NSLOCTEXT("ContentBrowserToolkit", "FindSkeletonAnimations", "Find Animations for Skeleton..."),
						NSLOCTEXT("ContentBrowserToolkit", "FindSkeletonAnimationsTip", "Find saved animations that reference the selected skeletons or use the same skeleton as the selected animations."),
						TEXT("ClassIcon.AnimSequence"), ECBTAssetSearch::SkeletonAnimations,
						[](const FAssetData& Asset) { return Asset.IsInstanceOf<USkeleton>() || Asset.IsInstanceOf<UAnimationAsset>(); });
					AddSearch(TEXT("CBTCheckTextureSets"),
						NSLOCTEXT("ContentBrowserToolkit", "CheckTextureSets", "Check Texture Sets..."),
						NSLOCTEXT("ContentBrowserToolkit", "CheckTextureSetsTip", "Check matching Base Color, Normal and ORM textures in the same folder."),
						TEXT("ClassIcon.Texture2D"), ECBTAssetSearch::TextureSets, [](const FAssetData& Asset) { return Asset.IsInstanceOf<UTexture2D>(); });
					/** @brief Adds an inspection with the relevant subset of the captured selection. */
					const auto AddInspection = [this, &Section, &Selected](FName Entry, const FText& Label, const FText& Tip, FName Icon, ECBTInspection Inspection, const UClass* Class) {
						TArray<FAssetData> Matches = Selected.FilterByPredicate([Class](const FAssetData& Asset) {
							return !Asset.IsRedirector() && (!Class || Asset.IsInstanceOf(Class));
						});
						if (!Matches.IsEmpty())
						{
							Section.AddMenuEntry(Entry, Label, Tip, ContentBrowserToolkit::MenuIcon(Icon), FUIAction(FExecuteAction::CreateLambda([this, Label, Inspection, Matches = MoveTemp(Matches)] { OpenInspection(Label, BuildCBTInspection(Inspection, Matches)); })));
						}
					};
					AddInspection(TEXT("CBTUsageChains"), NSLOCTEXT("ContentBrowserToolkit", "UsageChains", "Show Usage Chains..."),
						NSLOCTEXT("ContentBrowserToolkit", "UsageChainsTip", "Show one shortest saved reference chain from each map to the selected assets."), TEXT("Icons.Link"), ECBTInspection::UsageChains, nullptr);
					AddInspection(TEXT("CBTReplacementCandidates"), NSLOCTEXT("ContentBrowserToolkit", "ReplacementCandidates", "Find Replacement Candidates..."),
						NSLOCTEXT("ContentBrowserToolkit", "ReplacementCandidatesTip", "Compare assets of the same class in the selected asset's folder."), TEXT("Icons.Refresh"), ECBTInspection::ReplacementCandidates, nullptr);
					AddInspection(TEXT("CBTTextureDuplicates"), NSLOCTEXT("ContentBrowserToolkit", "TextureDuplicates", "Find Exact Texture Duplicates..."),
						NSLOCTEXT("ContentBrowserToolkit", "TextureDuplicatesTip", "Compare top source mip pixels and texture settings within the same folder."), TEXT("Icons.Duplicate"), ECBTInspection::TextureDuplicates, UTexture2D::StaticClass());
					AddInspection(TEXT("CBTBlueprintDependencies"), NSLOCTEXT("ContentBrowserToolkit", "BlueprintDependencies", "Blueprint Dependency Audit..."),
						NSLOCTEXT("ContentBrowserToolkit", "BlueprintDependenciesTip", "Inspect direct hard and soft references and the largest reachable packages."), TEXT("ContentBrowser.ReferenceViewer"), ECBTInspection::BlueprintDependencies, UBlueprint::StaticClass());
					AddInspection(TEXT("CBTMaterialOverrides"), NSLOCTEXT("ContentBrowserToolkit", "MaterialOverrides", "Compare Instance Overrides..."),
						NSLOCTEXT("ContentBrowserToolkit", "MaterialOverridesTip", "Compare scalar, vector, texture and switch values and highlight local overrides."), TEXT("ClassIcon.MaterialInstanceConstant"), ECBTInspection::MaterialOverrides, UMaterialInstance::StaticClass());
					AddInspection(TEXT("CBTAnimationSet"), NSLOCTEXT("ContentBrowserToolkit", "AnimationSet", "Animation Set Audit..."),
						NSLOCTEXT("ContentBrowserToolkit", "AnimationSetTip", "Compare root motion and timing, and check required notify and float curve names."), TEXT("ClassIcon.AnimSequence"), ECBTInspection::AnimationSet, UAnimSequence::StaticClass());
					AddInspection(TEXT("CBTStaticMeshSetup"), NSLOCTEXT("ContentBrowserToolkit", "StaticMeshSetup", "Mesh Setup Audit..."),
						NSLOCTEXT("ContentBrowserToolkit", "StaticMeshSetupTip", "Review static mesh collision, LODs, Nanite and material slots."), TEXT("ClassIcon.StaticMesh"), ECBTInspection::StaticMeshes, UStaticMesh::StaticClass());
					AddInspection(TEXT("CBTSkeletalMeshSetup"), NSLOCTEXT("ContentBrowserToolkit", "SkeletalMeshSetup", "Character Asset Audit..."),
						NSLOCTEXT("ContentBrowserToolkit", "SkeletalMeshSetupTip", "Review the skeleton, Physics Asset, LODs, material slots and morph targets."), TEXT("ClassIcon.SkeletalMesh"), ECBTInspection::SkeletalMeshes, USkeletalMesh::StaticClass());
					AddInspection(TEXT("CBTAudioImport"), NSLOCTEXT("ContentBrowserToolkit", "AudioImport", "Audio Import Audit..."),
						NSLOCTEXT("ContentBrowserToolkit", "AudioImportTip", "Compare imported sound wave channels and sample rates with folder profiles."), TEXT("ClassIcon.SoundCue"), ECBTInspection::AudioImport, USoundWave::StaticClass());
					TArray<FAssetData> Textures = Selected.FilterByPredicate([](const FAssetData& Asset) { return Asset.IsInstanceOf<UTexture>(); });
					if (!Textures.IsEmpty())
					{
						Section.AddMenuEntry(TEXT("CBTTextureSettingsAudit"),
							NSLOCTEXT("ContentBrowserToolkit", "TextureAuditMenu", "Texture Settings Audit..."),
							NSLOCTEXT("ContentBrowserToolkit", "TextureAuditMenuTooltip", "Compare source size, format, compression, sRGB and mip generation for the selected textures."),
							ContentBrowserToolkit::MenuIcon(TEXT("ClassIcon.Texture2D")), FUIAction(FExecuteAction::CreateLambda([this, Textures = MoveTemp(Textures)] {
								const auto Window = CreateContentBrowserTextureAuditWindow(Textures);
								Windows.Add(Window);
								FSlateApplication::Get().AddWindow(Window);
							})));
					}
				}), false, ContentBrowserToolkit::MenuIcon(TEXT("Icons.Audit")));
			}));
	UToolMenus::Get()
		->ExtendMenu(TEXT("ContentBrowser.FolderContextMenu"))
		->AddDynamicSection(TEXT("CBTUsageAudit"), FNewToolMenuDelegate::CreateLambda([this](UToolMenu* Menu) {
			const auto* Context = Menu->FindContext<UContentBrowserFolderContext>();
			if (!Context || Context->SelectedPackagePaths.IsEmpty())
				return;
			const FString Folder = Context->SelectedPackagePaths[0];
			const bool bSingleFolder = Context->SelectedPackagePaths.Num() == 1;
			Menu->FindOrAddSection(TEXT("ContentBrowserToolkit")).AddSubMenu(TEXT("CBTCommands"), NSLOCTEXT("ContentBrowserToolkit", "CommandsMenu", "Content Browser Toolkit"), NSLOCTEXT("ContentBrowserToolkit", "CommandsMenuTip", "Open reports for the current Content Browser selection."), FNewToolMenuDelegate::CreateLambda([this, Folder, bSingleFolder](UToolMenu* SubMenu) {
				FToolMenuSection& Section = SubMenu->FindOrAddSection(TEXT("CBTCommands"));
				Section.AddMenuEntry(TEXT("CBTMapUsage"), FText::FromString(TEXT("Asset Audit to Map...")),
					FText::FromString(TEXT("Choose a map and check this folder, including subfolders.")),
					ContentBrowserToolkit::MenuIcon(TEXT("ClassIcon.World")), FUIAction(FExecuteAction::CreateLambda([this, Folder] { Open(Folder); })));
				if (bSingleFolder)
				{
					Section.AddMenuEntry(TEXT("CBTFolderInventory"), FText::FromString(TEXT("Folder Inventory...")),
						FText::FromString(TEXT("List assets, package sizes and subfolder totals.")), ContentBrowserToolkit::MenuIcon(TEXT("ContentBrowser.SizeMap")),
						FUIAction(FExecuteAction::CreateLambda([this, Folder] { OpenFolderReport(Folder, false); })));
					const FText NamingLabel = NSLOCTEXT("ContentBrowserToolkit", "FolderNamingAudit", "Folder Naming Audit...");
					Section.AddMenuEntry(TEXT("CBTFolderNamingAudit"), NamingLabel,
						NSLOCTEXT("ContentBrowserToolkit", "FolderNamingTip", "Check asset name prefixes in this folder and its subfolders."), ContentBrowserToolkit::MenuIcon(TEXT("Icons.Edit")),
						FUIAction(FExecuteAction::CreateLambda([this, Folder, NamingLabel] {
							const auto Window = CreateContentBrowserAssetReportWindow(NamingLabel, BuildContentBrowserNamingAudit(Folder),
								NSLOCTEXT("ContentBrowserToolkit", "FolderNamingEmpty", "No naming violations found for the configured rules."));
							Windows.Add(Window);
							FSlateApplication::Get().AddWindow(Window);
						})));
					Section.AddMenuEntry(TEXT("CBTMigrationPreview"),
						NSLOCTEXT("ContentBrowserToolkit", "MigrationPreview", "Folder Migration Preview..."),
						NSLOCTEXT("ContentBrowserToolkit", "MigrationPreviewTip", "Preview external dependencies, referencers and destination conflicts."), ContentBrowserToolkit::MenuIcon(TEXT("Icons.FolderOpen")),
						FUIAction(FExecuteAction::CreateLambda([this, Folder] { OpenMigrationPreview(Folder); })));
					const FText Cleanup = NSLOCTEXT("ContentBrowserToolkit", "CleanupCandidates", "Find Cleanup Candidates...");
					Section.AddMenuEntry(TEXT("CBTCleanupCandidates"), Cleanup,
						NSLOCTEXT("ContentBrowserToolkit", "CleanupCandidatesTip", "Review redirectors, empty folders and assets with no known saved referencers."), ContentBrowserToolkit::MenuIcon(TEXT("Icons.Warning")),
						FUIAction(FExecuteAction::CreateLambda([this, Folder, Cleanup] { OpenInspection(Cleanup, BuildCBTCleanupCandidates(Folder)); })));
					Section.AddMenuEntry(TEXT("CBTFolderReferences"), FText::FromString(TEXT("Folder Reference Audit...")),
						FText::FromString(TEXT("Find saved packages outside this folder that reference its assets.")), ContentBrowserToolkit::MenuIcon(TEXT("ContentBrowser.ReferenceViewer")),
						FUIAction(FExecuteAction::CreateLambda([this, Folder] { OpenFolderReport(Folder, true); })));
				}
			}),
				false, ContentBrowserToolkit::MenuIcon(TEXT("Icons.Audit")));
		}));
}

IMPLEMENT_MODULE(FContentBrowserToolkitModule, ContentBrowserToolkit)
