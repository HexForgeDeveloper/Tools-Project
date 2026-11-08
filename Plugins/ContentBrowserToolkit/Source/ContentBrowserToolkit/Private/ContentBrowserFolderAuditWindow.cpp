#include "ContentBrowserFolderAuditWindow.h"

#include "ContentBrowserFolderAudit.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

namespace
{
	/** @brief Formats on-disk sizes without treating an unavailable package as zero bytes. */
	FString FormatPackageSize(int64 Bytes)
	{
		return Bytes < 0 ? FString(TEXT("Unavailable")) : FText::AsMemory(Bytes).ToString();
	}

	/** @brief Presents folder totals, a virtualized asset list and selected reference evidence. */
	class SContentBrowserFolderAudit final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SContentBrowserFolderAudit) {}
		SLATE_END_ARGS()

		/** @brief Takes ownership of the snapshot used by the list and CSV export. */
		void Construct(const FArguments&, FCBTFolderAuditReport InReport)
		{
			Report = MoveTemp(InReport);
			const FString Summary = FString::Printf(TEXT("%s (recursive)   |   %d assets   |   %s on disk   |   %d unavailable packages"),
				*Report.Folder, Report.Rows.Num(), *FormatPackageSize(Report.TotalBytes), Report.UnavailablePackages);
			ChildSlot
				[SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(8.f)
						[SNew(STextBlock).Text(FText::FromString(Report.Error.IsEmpty() ? Summary : Report.Error)).AutoWrapText(true)]
					+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f, 8.f, 8.f)
						[SNew(STextBlock)
								.Text(FText::FromString(Report.bReferenceAudit
										? TEXT("Direct saved package references from outside this folder. Empty results do not establish that an asset is unused.")
										: TEXT("Sizes include package sidecars and are deduplicated in each total. Assets are listed largest first.")))
								.AutoWrapText(true)]
					+ SVerticalBox::Slot().AutoHeight().Padding(8.f)
						[SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth()
								[SNew(SButton).Text(FText::FromString(TEXT("Export CSV"))).IsEnabled(Report.Error.IsEmpty()).OnClicked(this, &SContentBrowserFolderAudit::Export)]
							+ SHorizontalBox::Slot().Padding(8.f, 0.f)
								[SAssignNew(ExportStatus, STextBlock).AutoWrapText(true)]]
					+ SVerticalBox::Slot().AutoHeight().Padding(8.f)
						[SNew(SExpandableArea).InitiallyCollapsed(true).HeaderContent()[SNew(STextBlock).Text(FText::FromString(TEXT("Largest classes and subfolders")))].BodyContent()[SNew(STextBlock).Text(FText::FromString(GroupSummary())).AutoWrapText(true)]]
					+ SVerticalBox::Slot().FillHeight(0.7f).Padding(8.f)
						[SNew(SListView<TSharedPtr<FCBTFolderAuditRow>>)
								.ListItemsSource(&Report.Rows)
								.SelectionMode(ESelectionMode::Single)
								.OnGenerateRow(this, &SContentBrowserFolderAudit::GenerateRow)
								.OnSelectionChanged(this, &SContentBrowserFolderAudit::SelectRow)]
					+ SVerticalBox::Slot().FillHeight(0.3f).Padding(8.f)
						[SNew(SScrollBox)
							+ SScrollBox::Slot()
								[SAssignNew(Details, STextBlock).Text(FText::FromString(TEXT("Select an asset to view its details."))).AutoWrapText(true)]]];
		}

	private:
		/** @brief Lists the ten largest groups of each kind; the CSV contains every group. */
		FString GroupSummary() const
		{
			TArray<FString> Lines;
			const auto Append = [&Lines](const TCHAR* Label, const TArray<FCBTFolderAuditGroup>& Groups) {
				Lines.Add(Label);
				for (int32 Index = 0; Index < FMath::Min(10, Groups.Num()); ++Index)
				{
					const auto& Group = Groups[Index];
					Lines.Add(FString::Printf(TEXT("%s   |   %d assets   |   %s"), *Group.Name, Group.AssetCount, *FormatPackageSize(Group.Bytes)));
				}
			};
			Append(TEXT("Classes"), Report.Classes);
			Append(TEXT("Immediate subfolders (recursive totals)"), Report.Subfolders);
			return FString::Join(Lines, TEXT("\n"));
		}

		/** @brief Shows the asset, package size and optional incoming reference count. */
		TSharedRef<ITableRow> GenerateRow(TSharedPtr<FCBTFolderAuditRow> Row, const TSharedRef<STableViewBase>& Owner) const
		{
			FString Text = Row->Asset.GetObjectPathString() + TEXT("   |   ") + FormatPackageSize(Row->Bytes);
			if (Report.bReferenceAudit)
			{
				Text += FString::Printf(TEXT("   |   %d external referencers"), Row->ExternalReferencers.Num());
			}
			return SNew(STableRow<TSharedPtr<FCBTFolderAuditRow>>, Owner)
				[SNew(STextBlock).Text(FText::FromString(Text)).ToolTipText(FText::FromString(Text))];
		}

		/** @brief Lists external packages for the selected asset. */
		void SelectRow(TSharedPtr<FCBTFolderAuditRow> Row, ESelectInfo::Type)
		{
			if (!Row)
			{
				Details->SetText(FText::FromString(TEXT("Select an asset to view its details.")));
				return;
			}
			FString Text = Row->Asset.GetObjectPathString() + TEXT("\n") + Row->Asset.AssetClassPath.ToString();
			if (Report.bReferenceAudit)
			{
				Text += Row->ExternalReferencers.IsEmpty() ? TEXT("\nNo external package references found.") : TEXT("\nExternal referencers:\n");
				Text += FString::JoinBy(Row->ExternalReferencers, TEXT("\n"), [](FName Name) { return Name.ToString(); });
			}
			Details->SetText(FText::FromString(Text));
		}

		/** @brief Exports the complete snapshot and reports the resulting path. */
		FReply Export()
		{
			const FString Path = FContentBrowserFolderAudit::Export(Report);
			ExportStatus->SetText(FText::FromString(Path.IsEmpty() ? TEXT("CSV export failed.") : TEXT("CSV saved: ") + Path));
			return FReply::Handled();
		}

		FCBTFolderAuditReport Report;
		TSharedPtr<STextBlock> Details;
		TSharedPtr<STextBlock> ExportStatus;
	};
} // namespace

TSharedRef<SWindow> CreateContentBrowserFolderAuditWindow(const FString& Folder, bool bFindReferencers)
{
	return SNew(SWindow)
		.Title(FText::FromString(bFindReferencers ? TEXT("Folder Reference Audit") : TEXT("Folder Inventory")))
		.ClientSize(FVector2D(1080, 700))
			[SNew(SContentBrowserFolderAudit, FContentBrowserFolderAudit::Scan(Folder, bFindReferencers))];
}
