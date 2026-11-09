// Copyright 2025 DimAlek. All Rights Reserved.
#include "ContentBrowserMenuContexts.h"
#include "ContentBrowserModule.h"
#include "ContentBrowserFolderAuditWindow.h"
#include "ContentBrowserAssetAuditWindow.h"
#include "ContentBrowserAssetSearch.h"
#include "ContentBrowserInspection.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstance.h"
#include "Sound/SoundWave.h"
#include "Animation/Skeleton.h"
#include "Engine/Texture2D.h"
#include "ContentBrowserTextureAuditWindow.h"
#include "Animation/AnimationAsset.h"
#include "Engine/Blueprint.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"
#include "ContentBrowserUsageAudit.h"
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

namespace
{
/** @brief Creates a menu icon from a built-in editor style brush. */
FSlateIcon MenuIcon(FName BrushName)
{
    return FSlateIcon(FAppStyle::GetAppStyleSetName(), BrushName);
}

/** @brief Short labels for the result table; detailed evidence stays in the selection panel. */
FString UsageLabel(ECBTUsageStatus Status)
{
    switch (Status)
    {
    case ECBTUsageStatus::MapDependency:
        return TEXT("Used in map");
    case ECBTUsageStatus::Referenced:
        return TEXT("Used elsewhere");
    case ECBTUsageStatus::Protected:
        return TEXT("Needs checking");
    default:
        return TEXT("No references found");
    }
}

/** @brief Compact, fixed-height result with independent columns and a full-path tooltip. */
class SUsageRow : public SMultiColumnTableRow<TSharedPtr<FCBTUsageRow>>
{
  public:
    SLATE_BEGIN_ARGS(SUsageRow)
    {
    }
    SLATE_ARGUMENT(TSharedPtr<FCBTUsageRow>, Item)
    SLATE_END_ARGS()
        /** @brief Binds one result to its virtualized row. */
        void Construct(const FArguments &Args, const TSharedRef<STableViewBase> &Owner)
    {
        Item = Args._Item;
        SMultiColumnTableRow::Construct(FSuperRowType::FArguments().Padding(5), Owner);
    }
    /** @brief Generates one compact result cell. */
    virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName &Column) override
    {
        FString Value;
        FLinearColor Color = FLinearColor::White;
        if (Column == TEXT("Asset"))
            Value = Item->Asset.AssetName.ToString();
        else if (Column == TEXT("Type"))
            Value = Item->Asset.AssetClassPath.GetAssetName().ToString();
        else if (Column == TEXT("Size"))
            Value = FString::Printf(TEXT("%.2f MiB"), Item->Bytes / (1024.0 * 1024.0));
        else
        {
            Value = UsageLabel(Item->Status);
            Color = Item->Status == ECBTUsageStatus::MapDependency ? FLinearColor(0.35f, 0.85f, 0.45f)
                    : Item->Status == ECBTUsageStatus::Referenced  ? FLinearColor(0.45f, 0.7f, 1.f)
                                                                   : FLinearColor(1.f, 0.7f, 0.3f);
        }
        return SNew(STextBlock)
            .Text(FText::FromString(Value))
            .ColorAndOpacity(Color)
            .ToolTipText(FText::FromString(Item->Asset.GetObjectPathString()));
    }

  private:
    TSharedPtr<FCBTUsageRow> Item; ///< @brief Immutable report row.
};

/** @brief Context-driven audit: selected assets/folder, map picker, compact table and evidence panel. */
class SContentUsageAudit : public SCompoundWidget
{
  public:
    SLATE_BEGIN_ARGS(SContentUsageAudit)
    {
    }
    SLATE_ARGUMENT(FString, Folder)
    SLATE_ARGUMENT(TArray<FAssetData>, Assets) SLATE_END_ARGS()
        /** @brief Builds a selection-specific audit without requiring manual path entry. */
        void Construct(const FArguments &Args)
    {
        Folder = Args._Folder;
        Assets = Args._Assets;
        for (const TCHAR *Label :
             {TEXT("All results"), TEXT("Used in map"), TEXT("Used elsewhere"), TEXT("No references found"), TEXT("Needs checking")})
            Filters.Add(MakeShared<FString>(Label));
        ActiveFilter = Filters[0];
        const FString Scope = Assets.IsEmpty()    ? TEXT("Folder: ") + Folder + TEXT(" (including subfolders)")
                              : Assets.Num() == 1 ? TEXT("Asset: ") + Assets[0].AssetName.ToString()
                                                  : FString::Printf(TEXT("Selected assets: %d"), Assets.Num());
        ChildSlot
            [SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight().Padding(12)[SNew(STextBlock).Text(FText::FromString(Scope))] +
             SVerticalBox::Slot().AutoHeight().Padding(12, 0, 12, 12)
                 [SNew(SHorizontalBox) +
                  SHorizontalBox::Slot()
                      .AutoWidth()
                      .VAlign(VAlign_Center)
                      .Padding(0, 0, 8, 0)[SNew(STextBlock).Text(FText::FromString(TEXT("Check against map:")))] +
                  SHorizontalBox::Slot().FillWidth(1)[SNew(SObjectPropertyEntryBox)
                                                          .AllowedClass(UWorld::StaticClass())
                                                          .ObjectPath(this, &SContentUsageAudit::MapPath)
                                                          .OnObjectChanged(this, &SContentUsageAudit::SelectMap)
                                                          .AllowClear(true)] +
                  SHorizontalBox::Slot().AutoWidth().Padding(12, 0, 0, 0)[SNew(SButton)
                                                                              .Text(FText::FromString(TEXT("Scan")))
                                                                              .OnClicked(this, &SContentUsageAudit::Scan)
                                                                              .IsEnabled_Lambda([this] { return SelectedMap.IsValid(); })] +
                  SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0,
                                                             0)[SNew(SButton)
                                                                    .Text(FText::FromString(TEXT("Export CSV")))
                                                                    .OnClicked(this, &SContentUsageAudit::Export)
                                                                    .IsEnabled_Lambda([this] { return !Report.Rows.IsEmpty(); })]] +
             SVerticalBox::Slot().AutoHeight().Padding(
                 12, 0, 12,
                 10)[SAssignNew(Summary, STextBlock)
                         .Text(FText::FromString(TEXT("1. Choose a map.   2. Click Scan.   3. Select a result to see its references.")))
                         .AutoWrapText(true)] +
             SVerticalBox::Slot().AutoHeight().Padding(12, 0, 12, 10)
                 [SNew(SHorizontalBox) +
                  SHorizontalBox::Slot().FillWidth(1)
                      [SNew(SSearchBox).HintText(FText::FromString(TEXT("Find asset..."))).OnTextChanged_Lambda([this](const FText &Text) {
                          Search = Text.ToString();
                          Refresh();
                      })] +
                  SHorizontalBox::Slot().AutoWidth().Padding(
                      12, 0, 0, 0)[SAssignNew(FilterBox, SComboBox<TSharedPtr<FString>>)
                                       .OptionsSource(&Filters)
                                       .InitiallySelectedItem(ActiveFilter)
                                       .OnGenerateWidget_Lambda(
                                           [](TSharedPtr<FString> Label) { return SNew(STextBlock).Text(FText::FromString(*Label)); })
                                       .OnSelectionChanged_Lambda([this](TSharedPtr<FString> Label, ESelectInfo::Type) {
                                           ActiveFilter = Label;
                                           Refresh();
                                       })[SNew(STextBlock).Text_Lambda([this] { return FText::FromString(*ActiveFilter); })]]] +
             SVerticalBox::Slot().FillHeight(1).Padding(
                 12, 0, 12,
                 10)[SAssignNew(List, SListView<TSharedPtr<FCBTUsageRow>>)
                         .ListItemsSource(&VisibleRows)
                         .SelectionMode(ESelectionMode::Single)
                         .OnGenerateRow_Lambda([](TSharedPtr<FCBTUsageRow> Item, const TSharedRef<STableViewBase> &Owner) {
                             return SNew(SUsageRow, Owner).Item(Item);
                         })
                         .OnSelectionChanged(this, &SContentUsageAudit::SelectRow)
                         .OnMouseButtonDoubleClick(this, &SContentUsageAudit::Locate)
                         .HeaderRow(SNew(SHeaderRow) +
                                    SHeaderRow::Column(TEXT("Asset")).DefaultLabel(FText::FromString(TEXT("Asset"))).FillWidth(0.4f) +
                                    SHeaderRow::Column(TEXT("Type")).DefaultLabel(FText::FromString(TEXT("Type"))).FillWidth(0.2f) +
                                    SHeaderRow::Column(TEXT("Status")).DefaultLabel(FText::FromString(TEXT("Usage"))).FillWidth(0.28f) +
                                    SHeaderRow::Column(TEXT("Size")).DefaultLabel(FText::FromString(TEXT("Size"))).FillWidth(0.12f))] +
             SVerticalBox::Slot().AutoHeight().Padding(12, 0, 12,
                                                       4)[SNew(STextBlock).Text(FText::FromString(TEXT("References for selected asset")))] +
             SVerticalBox::Slot().AutoHeight().Padding(12, 0, 12, 10)[SNew(SBox).HeightOverride(
                 120)[SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(Details, STextBlock)
                                                                .Text(FText::FromString(TEXT("Select a result above. Double-click to "
                                                                                             "locate it in Content Browser.")))
                                                                .AutoWrapText(true)]]] +
             SVerticalBox::Slot().AutoHeight().Padding(
                 12, 0, 12,
                 12)[SNew(STextBlock)
                         .Text(FText::FromString(TEXT("Read-only. No references found = review candidate, not permission to delete.")))
                         .ToolTipText_Lambda([this] { return FText::FromString(FString::Join(Report.Warnings, TEXT("\n"))); })]];
    }

  private:
    /** @brief Returns the picker path without loading the selected map. */
    FString MapPath() const
    {
        return SelectedMap.IsValid() ? SelectedMap.GetObjectPathString() : FString();
    }
    /** @brief Invalidates the previous snapshot when the target map changes. */
    void SelectMap(const FAssetData &Asset)
    {
        SelectedMap = Asset;
        Report = FCBTUsageReport();
        Refresh();
        Summary->SetText(FText::FromString(TEXT("Click Scan to check the selection against this map.")));
    }
    /** @brief Filters the current snapshot without rescanning assets. */
    void Refresh()
    {
        VisibleRows.Reset();
        for (const auto &Item : Report.Rows)
        {
            if ((!ActiveFilter.IsValid() || ActiveFilter == Filters[0] || UsageLabel(Item->Status) == *ActiveFilter) &&
                (Search.IsEmpty() || Item->Asset.GetObjectPathString().Contains(Search)))
                VisibleRows.Add(Item);
        }
        if (List.IsValid())
        {
            List->ClearSelection();
            List->RequestListRefresh();
        }
        if (Details.IsValid())
            Details->SetText(FText::FromString(TEXT("Select a result to inspect references.")));
    }
    /** @brief Scans selected folders, retaining only explicitly selected assets in asset mode. */
    FReply Scan()
    {
        Report = FCBTUsageReport();
        TArray<FString> Folders;
        if (Assets.IsEmpty())
            Folders.Add(Folder);
        else
            for (const FAssetData &Asset : Assets)
                Folders.AddUnique(Asset.PackagePath.ToString());
        TSet<FSoftObjectPath> Requested;
        for (const FAssetData &Asset : Assets)
            Requested.Add(Asset.ToSoftObjectPath());
        TSet<FSoftObjectPath> Added;
        for (const FString &Path : Folders)
        {
            const FCBTUsageReport Partial = FContentBrowserUsageAudit::Scan(Path, SelectedMap);
            Report.MapPackage = Partial.MapPackage;
            Report.Folder = Assets.IsEmpty() ? Folder : TEXT("Selected assets");
            for (const FString &Warning : Partial.Warnings)
                Report.Warnings.AddUnique(Warning);
            for (const auto &Item : Partial.Rows)
            {
                const FSoftObjectPath Object = Item->Asset.ToSoftObjectPath();
                if ((Assets.IsEmpty() || Requested.Contains(Object)) && !Added.Contains(Object))
                {
                    Report.Rows.Add(Item);
                    Added.Add(Object);
                }
            }
            if (!Partial.Error.IsEmpty())
            {
                Report.Error = Partial.Error;
                break;
            }
            if (Partial.bCancelled)
            {
                Report.bCancelled = true;
                break;
            }
        }
        if (!Report.Error.IsEmpty() || Report.bCancelled)
        {
            for (auto &Item : Report.Rows)
                if (Item->Status == ECBTUsageStatus::Review)
                    Item->Status = ECBTUsageStatus::Protected;
        }
        int32 Used = 0, Elsewhere = 0, Review = 0, Protected = 0;
        for (const auto &Item : Report.Rows)
        {
            switch (Item->Status)
            {
            case ECBTUsageStatus::MapDependency:
                ++Used;
                break;
            case ECBTUsageStatus::Referenced:
                ++Elsewhere;
                break;
            case ECBTUsageStatus::Review:
                ++Review;
                break;
            default:
                ++Protected;
                break;
            }
        }
        Refresh();
        Summary->SetText(FText::FromString(
            !Report.Error.IsEmpty()
                ? Report.Error
                : FString::Printf(
                      TEXT("%s   %d checked  |  %d used in map  |  %d used elsewhere  |  %d no references  |  %d need checking"),
                      Report.bCancelled ? TEXT("Cancelled:") : TEXT("Done:"), Report.Rows.Num(), Used, Elsewhere, Review, Protected)));
        return FReply::Handled();
    }
    /** @brief Displays evidence only for the selected row, keeping the table compact. */
    void SelectRow(TSharedPtr<FCBTUsageRow> Item, ESelectInfo::Type)
    {
        if (Item.IsValid())
            Details->SetText(FText::FromString(Item->Asset.GetObjectPathString() + TEXT("\n\n") + Item->Evidence));
    }
    /** @brief Exports all results, including rows hidden by the current UI filter. */
    FReply Export()
    {
        const FString Filename = FContentBrowserUsageAudit::Export(Report);
        Details->SetText(FText::FromString(Filename.IsEmpty() ? TEXT("Export failed.") : TEXT("CSV saved: ") + Filename));
        return FReply::Handled();
    }
    /** @brief Selects an audited asset in Content Browser without editing it. */
    void Locate(TSharedPtr<FCBTUsageRow> Item)
    {
        if (Item.IsValid())
            FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().SyncBrowserToAssets({Item->Asset});
    }
    FString Folder;                                       ///< @brief Context folder; no manual path entry required.
    TArray<FAssetData> Assets;                            ///< @brief Exact asset selection captured when opening the context menu.
    FAssetData SelectedMap;                               ///< @brief Target map.
    FCBTUsageReport Report;                               ///< @brief Full results, independent of filters.
    TArray<TSharedPtr<FCBTUsageRow>> VisibleRows;         ///< @brief Filtered view.
    TArray<TSharedPtr<FString>> Filters;                  ///< @brief Stable status options.
    TSharedPtr<FString> ActiveFilter;                     ///< @brief Selected status filter.
    FString Search;                                       ///< @brief Asset name/path search.
    TSharedPtr<STextBlock> Summary, Details;              ///< @brief Counts and selected-row evidence.
    TSharedPtr<SComboBox<TSharedPtr<FString>>> FilterBox; ///< @brief Status selector.
    TSharedPtr<SListView<TSharedPtr<FCBTUsageRow>>> List; ///< @brief Compact results.
};
} // namespace

/** @brief Exposes Asset Audit to Map directly on Content Browser assets and folders. */
class FContentBrowserToolkitModule : public IModuleInterface
{
  public:
    /** @brief Registers menu extensions when ToolMenus is ready. */
    virtual void StartupModule() override;
    /** @brief Removes callbacks and module-owned windows before unloading. */
    virtual void ShutdownModule() override;

  private:
    /** @brief Adds matching asset, folder and Window menu actions. */
    void RegisterMenus();
    /** @brief Opens an audit scoped to a captured folder or exact asset selection. */
    void Open(FString Folder, TArray<FAssetData> Assets = {});
    /** @brief Opens a read-only folder report without requiring a map. */
    void OpenFolderReport(const FString& Folder, bool bFindReferencers);
    /** @brief Displays inspection results in the shared asset report window. */
    void OpenInspection(const FText& Title, TArray<FAssetAuditRow> Rows);
    /** @brief Prompts for a destination before generating a migration preview. */
    void OpenMigrationPreview(const FString& Folder);
    TArray<TWeakPtr<SWindow>> Windows; ///< @brief Windows holding module callback code.
};

void FContentBrowserToolkitModule::StartupModule()
{
    UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FContentBrowserToolkitModule::RegisterMenus));
}
void FContentBrowserToolkitModule::ShutdownModule()
{
    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);
    for (const auto &Weak : Windows)
        if (const auto Window = Weak.Pin())
        {
            Window->SetContent(SNullWidget::NullWidget);
            if (FSlateApplication::IsInitialized())
                FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());
        }
}
void FContentBrowserToolkitModule::Open(FString Folder, TArray<FAssetData> Assets)
{
    const auto Window = SNew(SWindow)
                            .Title(FText::FromString(TEXT("Asset Audit to Map")))
                            .ClientSize(FVector2D(1050, 680))[SNew(SContentUsageAudit).Folder(Folder).Assets(Assets)];
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
    const auto Window = CreateContentBrowserAssetReportWindow(Title, MoveTemp(Rows),
        NSLOCTEXT("ContentBrowserToolkit", "InspectionEmpty", "No findings for this selection."));
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

    const auto Dialog = SNew(SWindow).Title(Title).ClientSize(FVector2D(480, 450)).SupportsMaximize(false).SupportsMinimize(false);
    const TWeakPtr<SWindow> WeakDialog = Dialog;
    Dialog->SetContent(SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(8)
        [
            SNew(STextBlock).Text(NSLOCTEXT("ContentBrowserToolkit", "MigrationPickDestination", "Choose the destination folder for the preview."))
        ]
        + SVerticalBox::Slot().FillHeight(1).Padding(8)
        [
            FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().CreatePathPicker(Config)
        ]
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(8)
        [
            SNew(SButton).Text(NSLOCTEXT("ContentBrowserToolkit", "MigrationRunPreview", "Preview"))
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
    UToolMenus::Get()
        ->ExtendMenu(TEXT("LevelEditor.MainMenu.Window"))
        ->FindOrAddSection(TEXT("ContentBrowserToolkit"))
        .AddMenuEntry(TEXT("CBTUsageAudit"), FText::FromString(TEXT("Asset Audit to Map")),
                      FText::FromString(TEXT("Check project content against a map. For a specific selection, use its right-click menu.")),
                      MenuIcon(TEXT("ClassIcon.World")), FUIAction(FExecuteAction::CreateLambda([this] { Open(TEXT("/Game")); })));
    UToolMenus::Get()
        ->ExtendMenu(TEXT("ContentBrowser.AssetContextMenu"))
        ->AddDynamicSection(
            TEXT("CBTUsageAudit"), FNewToolMenuDelegate::CreateLambda([this](UToolMenu *Menu) {
                const auto *Context = Menu->FindContext<UContentBrowserAssetContextMenuContext>();
                if (!Context || Context->SelectedAssets.IsEmpty())
                    return;
                const TArray<FAssetData> Selected = Context->SelectedAssets;
                Menu->FindOrAddSection(TEXT("ContentBrowserToolkit")).AddSubMenu(
                    TEXT("CBTCommands"),
                    NSLOCTEXT("ContentBrowserToolkit", "CommandsMenu", "Content Browser Toolkit"),
                    NSLOCTEXT("ContentBrowserToolkit", "CommandsMenuTip", "Open reports for the current Content Browser selection."),
                    FNewToolMenuDelegate::CreateLambda([this, Selected](UToolMenu* SubMenu) {
                        FToolMenuSection& Section = SubMenu->FindOrAddSection(TEXT("CBTCommands"));
                        Section.AddMenuEntry(TEXT("CBTAssetAuditToMap"), FText::FromString(TEXT("Asset Audit to Map...")),
                                          FText::FromString(TEXT("Choose a map and check the selected assets and their references.")), MenuIcon(TEXT("ClassIcon.World")),
                                          FUIAction(FExecuteAction::CreateLambda([this, Selected] { Open(FString(), Selected); })));
                        const auto AddRelationshipAudit = [this, &Section, &Selected](const UClass* AssetClass, FName Entry, const FText& Label,
                            const FText& Tooltip, FName Icon, ECBTAssetAudit Audit)
                        {
                            TArray<FAssetData> Matches = Selected.FilterByPredicate([AssetClass](const FAssetData& Asset)
                            {
                                return Asset.IsInstanceOf(AssetClass);
                            });
                            if (Matches.IsEmpty())
                            {
                                return;
                            }
                            Section.AddMenuEntry(Entry, Label, Tooltip, MenuIcon(Icon),
                                FUIAction(FExecuteAction::CreateLambda([this, Audit, Label, Matches = MoveTemp(Matches)]
                                {
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
                        const auto AddSearch = [this, &Section, &Selected](FName Entry, const FText& Label, const FText& Tooltip, FName Icon, ECBTAssetSearch Search, auto IsSupported)
                        {
                            TArray<FAssetData> Matches = Selected.FilterByPredicate(IsSupported);
                            if (Matches.IsEmpty())
                            {
                                return;
                            }
                            Section.AddMenuEntry(Entry, Label, Tooltip, MenuIcon(Icon),
                                FUIAction(FExecuteAction::CreateLambda([this, Search, Label, Matches = MoveTemp(Matches)]
                                {
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
                        const auto AddInspection = [this, &Section, &Selected](FName Entry, const FText& Label, const FText& Tip, FName Icon, ECBTInspection Inspection, const UClass* Class)
                        {
                            TArray<FAssetData> Matches = Selected.FilterByPredicate([Class](const FAssetData& Asset) {
                                return !Asset.IsRedirector() && (!Class || Asset.IsInstanceOf(Class));
                            });
                            if (!Matches.IsEmpty())
                            {
                                Section.AddMenuEntry(Entry, Label, Tip, MenuIcon(Icon), FUIAction(FExecuteAction::CreateLambda(
                                    [this, Label, Inspection, Matches = MoveTemp(Matches)] { OpenInspection(Label, BuildCBTInspection(Inspection, Matches)); })));
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
                                MenuIcon(TEXT("ClassIcon.Texture2D")), FUIAction(FExecuteAction::CreateLambda([this, Textures = MoveTemp(Textures)]
                                {
                                    const auto Window = CreateContentBrowserTextureAuditWindow(Textures);
                                    Windows.Add(Window);
                                    FSlateApplication::Get().AddWindow(Window);
                                })));
                        }
                    }), false, MenuIcon(TEXT("Icons.Audit")));
            }));
    UToolMenus::Get()
        ->ExtendMenu(TEXT("ContentBrowser.FolderContextMenu"))
        ->AddDynamicSection(TEXT("CBTUsageAudit"), FNewToolMenuDelegate::CreateLambda([this](UToolMenu *Menu) {
                                const auto *Context = Menu->FindContext<UContentBrowserFolderContext>();
                                if (!Context || Context->SelectedPackagePaths.IsEmpty())
                                    return;
                                const FString Folder = Context->SelectedPackagePaths[0];
                                const bool bSingleFolder = Context->SelectedPackagePaths.Num() == 1;
                                Menu->FindOrAddSection(TEXT("ContentBrowserToolkit")).AddSubMenu(
                                    TEXT("CBTCommands"),
                                    NSLOCTEXT("ContentBrowserToolkit", "CommandsMenu", "Content Browser Toolkit"),
                                    NSLOCTEXT("ContentBrowserToolkit", "CommandsMenuTip", "Open reports for the current Content Browser selection."),
                                    FNewToolMenuDelegate::CreateLambda([this, Folder, bSingleFolder](UToolMenu* SubMenu) {
                                        FToolMenuSection& Section = SubMenu->FindOrAddSection(TEXT("CBTCommands"));
                                        Section.AddMenuEntry(TEXT("CBTMapUsage"), FText::FromString(TEXT("Asset Audit to Map...")),
                                                          FText::FromString(TEXT("Choose a map and check this folder, including subfolders.")),
                                                          MenuIcon(TEXT("ClassIcon.World")), FUIAction(FExecuteAction::CreateLambda([this, Folder] { Open(Folder); })));
                                        if (bSingleFolder)
                                        {
                                            Section.AddMenuEntry(TEXT("CBTFolderInventory"), FText::FromString(TEXT("Folder Inventory...")),
                                                FText::FromString(TEXT("List assets, package sizes and subfolder totals.")), MenuIcon(TEXT("ContentBrowser.SizeMap")),
                                                FUIAction(FExecuteAction::CreateLambda([this, Folder] { OpenFolderReport(Folder, false); })));
                                            const FText NamingLabel = NSLOCTEXT("ContentBrowserToolkit", "FolderNamingAudit", "Folder Naming Audit...");
                                            Section.AddMenuEntry(TEXT("CBTFolderNamingAudit"), NamingLabel,
                                                NSLOCTEXT("ContentBrowserToolkit", "FolderNamingTip", "Check asset name prefixes in this folder and its subfolders."), MenuIcon(TEXT("Icons.Edit")),
                                                FUIAction(FExecuteAction::CreateLambda([this, Folder, NamingLabel]
                                                {
                                                    const auto Window = CreateContentBrowserAssetReportWindow(NamingLabel, BuildContentBrowserNamingAudit(Folder),
                                                        NSLOCTEXT("ContentBrowserToolkit", "FolderNamingEmpty", "No naming violations found for the configured rules."));
                                                    Windows.Add(Window);
                                                    FSlateApplication::Get().AddWindow(Window);
                                                })));
                                            Section.AddMenuEntry(TEXT("CBTMigrationPreview"),
                                                NSLOCTEXT("ContentBrowserToolkit", "MigrationPreview", "Folder Migration Preview..."),
                                                NSLOCTEXT("ContentBrowserToolkit", "MigrationPreviewTip", "Preview external dependencies, referencers and destination conflicts."), MenuIcon(TEXT("Icons.FolderOpen")),
                                                FUIAction(FExecuteAction::CreateLambda([this, Folder] { OpenMigrationPreview(Folder); })));
                                            const FText Cleanup = NSLOCTEXT("ContentBrowserToolkit", "CleanupCandidates", "Find Cleanup Candidates...");
                                            Section.AddMenuEntry(TEXT("CBTCleanupCandidates"), Cleanup,
                                                NSLOCTEXT("ContentBrowserToolkit", "CleanupCandidatesTip", "Review redirectors, empty folders and assets with no known saved referencers."), MenuIcon(TEXT("Icons.Warning")),
                                                FUIAction(FExecuteAction::CreateLambda([this, Folder, Cleanup] { OpenInspection(Cleanup, BuildCBTCleanupCandidates(Folder)); })));
                                            Section.AddMenuEntry(TEXT("CBTFolderReferences"), FText::FromString(TEXT("Folder Reference Audit...")),
                                                FText::FromString(TEXT("Find saved packages outside this folder that reference its assets.")), MenuIcon(TEXT("ContentBrowser.ReferenceViewer")),
                                                FUIAction(FExecuteAction::CreateLambda([this, Folder] { OpenFolderReport(Folder, true); })));
                                        }
                                    }), false, MenuIcon(TEXT("Icons.Audit")));
                            }));
}

IMPLEMENT_MODULE(FContentBrowserToolkitModule, ContentBrowserToolkit)
