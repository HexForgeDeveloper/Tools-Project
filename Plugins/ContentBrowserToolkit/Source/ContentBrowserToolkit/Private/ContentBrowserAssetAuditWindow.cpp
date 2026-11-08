#include "ContentBrowserAssetAuditWindow.h"

#include "Animation/AnimationAsset.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/Blueprint.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInterface.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

namespace
{
	/** @brief Captures display values without keeping loaded assets alive. */
	struct FAssetAuditRow
	{
		FString Name;
		FString Path;
		FString Summary;
		FString Details;
	};

	/** @brief Records the selected material and each parent, stopping at missing parents or cycles. */
	void InspectMaterial(const UMaterialInterface& Material, FAssetAuditRow& Row)
	{
		TSet<const UMaterialInterface*> Visited;
		TArray<FString> Chain;
		const UMaterialInterface* Current = &Material;
		while (Current)
		{
			if (Visited.Contains(Current))
			{
				Row.Summary = TEXT("Parent cycle");
				Chain.Add(TEXT("Cycle at ") + Current->GetPathName());
				break;
			}
			Visited.Add(Current);
			Chain.Add(Current->GetPathName());
			const auto* Instance = Cast<UMaterialInstance>(Current);
			if (!Instance)
			{
				Row.Summary = Current == &Material ? TEXT("Base material") : TEXT("Base: ") + Current->GetName();
				break;
			}
			Current = Instance->Parent.Get();
			if (!Current)
			{
				Row.Summary = TEXT("Parent missing");
				Chain.Add(TEXT("Missing material parent"));
			}
		}
		Row.Details = FString::Join(Chain, TEXT("\n  -> "));
	}

	/** @brief Reports the resolved Blueprint parent, or flags the absence of a loaded parent class. */
	void InspectBlueprint(const UBlueprint& Blueprint, FAssetAuditRow& Row)
	{
		if (const UClass* Parent = Blueprint.ParentClass.Get())
		{
			Row.Summary = Parent->GetName();
			Row.Details = TEXT("Parent class: ") + Parent->GetPathName();
		}
		else
		{
			Row.Summary = TEXT("Parent unavailable");
			Row.Details = TEXT("This Blueprint has no resolved parent class.");
		}
	}

	/** @brief Records the skeleton path and the playback length when available. */
	void InspectAnimation(const UAnimationAsset& Animation, FAssetAuditRow& Row)
	{
		if (const USkeleton* Skeleton = Animation.GetSkeleton())
		{
			Row.Summary = Skeleton->GetName();
			Row.Details = TEXT("Skeleton: ") + Skeleton->GetPathName();
		}
		else
		{
			Row.Summary = TEXT("Skeleton missing");
			Row.Details = TEXT("This animation has no resolved skeleton.");
		}
		if (const auto* Sequence = Cast<UAnimSequenceBase>(&Animation))
		{
			Row.Details += FString::Printf(TEXT("\nPlayback length: %.3f s"), Sequence->GetPlayLength());
		}
	}

	/** @brief Presents one row per selected asset and the full evidence for the selected row. */
	class SAssetRelationshipAudit final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SAssetRelationshipAudit) {}
		SLATE_END_ARGS()

		/** @brief Captures the current editor values when the command is invoked. */
		void Construct(const FArguments&, ECBTAssetAudit Audit, const TArray<FAssetData>& Assets)
		{
			for (const FAssetData& Asset : Assets)
			{
				auto Row = MakeShared<FAssetAuditRow>();
				Row->Name = Asset.AssetName.ToString();
				Row->Path = Asset.GetObjectPathString();
				Row->Summary = TEXT("Asset unavailable");
				Row->Details = TEXT("Could not load this asset as the expected type.");
				UObject* LoadedAsset = Asset.GetAsset();
				switch (Audit)
				{
					case ECBTAssetAudit::MaterialParents:
						if (const auto* Material = Cast<UMaterialInterface>(LoadedAsset))
						{
							InspectMaterial(*Material, *Row);
						}
						break;
					case ECBTAssetAudit::BlueprintParent:
						if (const auto* Blueprint = Cast<UBlueprint>(LoadedAsset))
						{
							InspectBlueprint(*Blueprint, *Row);
						}
						break;
					case ECBTAssetAudit::AnimationSkeleton:
						if (const auto* Animation = Cast<UAnimationAsset>(LoadedAsset))
						{
							InspectAnimation(*Animation, *Row);
						}
						break;
				}
				Rows.Add(MoveTemp(Row));
			}
			Rows.Sort([](const TSharedPtr<FAssetAuditRow>& A, const TSharedPtr<FAssetAuditRow>& B) {
				return A->Summary == B->Summary ? A->Path < B->Path : A->Summary < B->Summary;
			});

			ChildSlot
				[SNew(SVerticalBox)
					+ SVerticalBox::Slot().FillHeight(0.65f).Padding(8.f)
						[SNew(SListView<TSharedPtr<FAssetAuditRow>>)
								.ListItemsSource(&Rows)
								.SelectionMode(ESelectionMode::Single)
								.OnGenerateRow_Lambda([](TSharedPtr<FAssetAuditRow> Row, const TSharedRef<STableViewBase>& Owner) {
									return SNew(STableRow<TSharedPtr<FAssetAuditRow>>, Owner)
										[SNew(STextBlock).Text(FText::FromString(Row->Name + TEXT("   |   ") + Row->Summary)).ToolTipText(FText::FromString(Row->Path))];
								})
								.OnSelectionChanged(this, &SAssetRelationshipAudit::SelectRow)]
					+ SVerticalBox::Slot().FillHeight(0.35f).Padding(8.f)
						[SNew(SScrollBox)
							+ SScrollBox::Slot()
								[SAssignNew(Details, STextBlock)
										.Text(NSLOCTEXT("ContentBrowserToolkit", "RelationshipAuditSelect", "Select an asset to view its details."))
										.AutoWrapText(true)]]];
		}

	private:
		/** @brief Displays the full asset path and relationship evidence. */
		void SelectRow(TSharedPtr<FAssetAuditRow> Row, ESelectInfo::Type)
		{
			Details->SetText(Row ? FText::FromString(Row->Path + TEXT("\n\n") + Row->Details)
								 : NSLOCTEXT("ContentBrowserToolkit", "RelationshipAuditSelect", "Select an asset to view its details."));
		}

		TArray<TSharedPtr<FAssetAuditRow>> Rows;
		TSharedPtr<STextBlock> Details;
	};
} // namespace

TSharedRef<SWindow> CreateContentBrowserAssetAuditWindow(const FText& Title, ECBTAssetAudit Audit, const TArray<FAssetData>& Assets)
{
	return SNew(SWindow)
		.Title(Title)
		.ClientSize(FVector2D(900.f, 500.f))
			[SNew(SAssetRelationshipAudit, Audit, Assets)];
}
