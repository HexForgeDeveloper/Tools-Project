#include "ContentBrowserTextureAuditWindow.h"

#include "AssetRegistry/AssetData.h"
#include "Engine/Texture.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

namespace
{
	/** @brief Holds copied display values so the report does not retain loaded textures. */
	struct FTextureSettingsRow
	{
		FString Path;
		TMap<FName, FString> Values;
	};

	/** @brief Displays one selected texture and its settings in aligned columns. */
	class STextureSettingsRow final : public SMultiColumnTableRow<TSharedPtr<FTextureSettingsRow>>
	{
	public:
		SLATE_BEGIN_ARGS(STextureSettingsRow) {}
		SLATE_ARGUMENT(TSharedPtr<FTextureSettingsRow>, Item)
		SLATE_END_ARGS()

		/** @brief Binds a captured snapshot to a virtualized row. */
		void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& Owner)
		{
			Item = Args._Item;
			SMultiColumnTableRow::Construct(FSuperRowType::FArguments().Padding(4.f), Owner);
		}

		/** @brief Displays a setting with a tooltip for values wider than their column. */
		TSharedRef<SWidget> GenerateWidgetForColumn(const FName& Column) override
		{
			const FText Value = FText::FromString(Item->Values.FindRef(Column));
			return SNew(STextBlock)
				.Text(Value)
				.ToolTipText(Column == TEXT("Asset") ? FText::FromString(Item->Path) : Value);
		}

	private:
		TSharedPtr<FTextureSettingsRow> Item;
	};

	/** @brief Loads only the selected textures when the user opens the report. */
	class STextureSettingsAudit final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(STextureSettingsAudit) {}
		SLATE_END_ARGS()

		/** @brief Takes a read-only snapshot and displays failures alongside successful loads. */
		void Construct(const FArguments&, const TArray<FAssetData>& Assets)
		{
			for (const FAssetData& Asset : Assets)
			{
				auto Row = MakeShared<FTextureSettingsRow>();
				Row->Path = Asset.GetObjectPathString();
				Row->Values.Add(TEXT("Asset"), Asset.AssetName.ToString());
				if (const UTexture* Texture = Cast<UTexture>(Asset.GetAsset()))
				{
					if (Texture->Source.IsValid())
					{
						Row->Values.Add(TEXT("Size"), FString::Printf(TEXT("%lld x %lld"), Texture->Source.GetSizeX(), Texture->Source.GetSizeY()));
						Row->Values.Add(TEXT("Format"), UEnum::GetValueAsString(Texture->Source.GetFormat()));
					}
					Row->Values.Add(TEXT("Compression"), UEnum::GetValueAsString(Texture->CompressionSettings.GetValue()));
					Row->Values.Add(TEXT("SRGB"), Texture->SRGB ? TEXT("Yes") : TEXT("No"));
					Row->Values.Add(TEXT("Mips"), UTexture::GetMipGenSettingsString(Texture->MipGenSettings.GetValue()));
					Row->Values.Add(TEXT("Status"), Texture->Source.IsValid() ? TEXT("OK") : TEXT("Source unavailable"));
				}
				else
				{
					Row->Values.Add(TEXT("Status"), TEXT("Could not load texture"));
				}
				Rows.Add(MoveTemp(Row));
			}

			ChildSlot
				[SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(8.f)
						[SNew(STextBlock).Text(FText::Format(NSLOCTEXT("ContentBrowserToolkit", "TextureAuditSummary", "{0} selected textures. Source dimensions and format refer to the base block and first layer."), FText::AsNumber(Rows.Num()))).AutoWrapText(true)]
					+ SVerticalBox::Slot().FillHeight(1.f).Padding(8.f)
						[SNew(SListView<TSharedPtr<FTextureSettingsRow>>)
								.ListItemsSource(&Rows)
								.OnGenerateRow_Lambda([](TSharedPtr<FTextureSettingsRow> Item, const TSharedRef<STableViewBase>& Owner) {
									return SNew(STextureSettingsRow, Owner).Item(MoveTemp(Item));
								})
								.HeaderRow(
									SNew(SHeaderRow)
									+ SHeaderRow::Column(TEXT("Asset")).DefaultLabel(NSLOCTEXT("ContentBrowserToolkit", "TextureAuditAsset", "Texture")).FillWidth(2.f)
									+ SHeaderRow::Column(TEXT("Size")).DefaultLabel(NSLOCTEXT("ContentBrowserToolkit", "TextureAuditSize", "Source size")).FillWidth(1.f)
									+ SHeaderRow::Column(TEXT("Format")).DefaultLabel(NSLOCTEXT("ContentBrowserToolkit", "TextureAuditFormat", "Source format")).FillWidth(1.f)
									+ SHeaderRow::Column(TEXT("Compression")).DefaultLabel(NSLOCTEXT("ContentBrowserToolkit", "TextureAuditCompression", "Compression")).FillWidth(1.4f)
									+ SHeaderRow::Column(TEXT("SRGB")).DefaultLabel(NSLOCTEXT("ContentBrowserToolkit", "TextureAuditSRGB", "sRGB")).FillWidth(0.5f)
									+ SHeaderRow::Column(TEXT("Mips")).DefaultLabel(NSLOCTEXT("ContentBrowserToolkit", "TextureAuditMips", "Mip generation")).FillWidth(1.3f)
									+ SHeaderRow::Column(TEXT("Status")).DefaultLabel(NSLOCTEXT("ContentBrowserToolkit", "TextureAuditStatus", "Status")).FillWidth(1.f))]];
		}

	private:
		TArray<TSharedPtr<FTextureSettingsRow>> Rows;
	};
} // namespace

TSharedRef<SWindow> CreateContentBrowserTextureAuditWindow(const TArray<FAssetData>& Assets)
{
	return SNew(SWindow)
		.Title(NSLOCTEXT("ContentBrowserToolkit", "TextureAuditTitle", "Texture Settings Audit"))
		.ClientSize(FVector2D(1150.f, 480.f))
			[SNew(STextureSettingsAudit, Assets)];
}
