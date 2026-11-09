#include "ContentBrowserInspection.h"

#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/MorphTarget.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Hash/Blake3.h"
#include "Materials/MaterialInstance.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Settings/ContentBrowserToolkitSettings.h"
#include "Sound/SoundWave.h"

namespace
{
	/** @brief Gets saved sibling assets of the exact selected registry class, without loading them. */
	TArray<FAssetData> FindSiblings(const FAssetData& Selected)
	{
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		TArray<FAssetData> Assets;
		Registry.GetAssetsByPath(Selected.PackagePath, Assets, false, true);
		Assets.RemoveAll([&Selected](const FAssetData& Candidate) {
			return Candidate.AssetClassPath != Selected.AssetClassPath || Candidate.IsRedirector();
		});
		return Assets;
	}

	/** @brief Describes stable properties useful for evaluating a replacement. */
	FString DescribeReplacement(const UObject* Asset)
	{
		if (const auto* Texture = Cast<UTexture2D>(Asset))
		{
			return FString::Printf(TEXT("%d x %d, sRGB %s, compression %d"), Texture->Source.GetSizeX(), Texture->Source.GetSizeY(),
				Texture->SRGB ? TEXT("on") : TEXT("off"), static_cast<int32>(Texture->CompressionSettings));
		}
		if (const auto* Animation = Cast<UAnimationAsset>(Asset))
		{
			return TEXT("Skeleton: ") + GetPathNameSafe(Animation->GetSkeleton());
		}
		if (const auto* Mesh = Cast<UStaticMesh>(Asset))
		{
			return FString::Printf(TEXT("%d material slots, %d LODs"), Mesh->GetStaticMaterials().Num(), Mesh->GetNumLODs());
		}
		if (const auto* Mesh = Cast<USkeletalMesh>(Asset))
		{
			return FString::Printf(TEXT("%d material slots, skeleton: %s"), Mesh->GetMaterials().Num(), *GetPathNameSafe(Mesh->GetSkeleton()));
		}
		return TEXT("Same asset class. Compare asset-specific settings before replacing.");
	}

	/** @brief Lists same-class assets in the selected folder and compares known compatibility properties. */
	void FindReplacementCandidates(const FAssetData& Selected, TArray<FAssetAuditRow>& Rows)
	{
		const FString Source = DescribeReplacement(Selected.GetAsset());
		const int32 FirstRow = Rows.Num();
		for (const FAssetData& Candidate : FindSiblings(Selected))
		{
			if (Candidate.GetSoftObjectPath() == Selected.GetSoftObjectPath())
			{
				continue;
			}
			const FString Target = DescribeReplacement(Candidate.GetAsset());
			Rows.Add({ Candidate.AssetName.ToString(), Candidate.GetObjectPathString(), TEXT("Same class in folder"),
				FString::Printf(TEXT("Selected: %s\n%s\n\nCandidate:\n%s"), *Selected.GetObjectPathString(), *Source, *Target) });
		}
		if (Rows.Num() == FirstRow)
		{
			Rows.Add({ Selected.AssetName.ToString(), Selected.GetObjectPathString(), TEXT("No same-class siblings"), Source });
		}
	}

	/** @brief Hashes top source mip pixels and records the layout needed for a valid pixel comparison. */
	struct FTextureFingerprint
	{
		FIntPoint Size;
		ETextureSourceFormat Format;
		FBlake3Hash Pixels;

		bool operator==(const FTextureFingerprint& Other) const = default;
	};

	/** @brief Fingerprints single-layer, single-block 2D sources with one slice. */
	TOptional<FTextureFingerprint> ReadFingerprint(UTexture2D* Texture)
	{
		if (!Texture || !Texture->Source.IsValid() || Texture->Source.GetNumBlocks() != 1
			|| Texture->Source.GetNumLayers() != 1 || Texture->Source.GetNumSlices() != 1)
		{
			return {};
		}
		TArray64<uint8> Pixels;
		if (!Texture->Source.GetMipData(Pixels, 0))
		{
			return {};
		}
		return FTextureFingerprint{ FIntPoint(Texture->Source.GetSizeX(), Texture->Source.GetSizeY()), Texture->Source.GetFormat(),
			FBlake3::HashBuffer(Pixels.GetData(), Pixels.Num()) };
	}

	/** @brief Lists pixel matches and shows settings that may make the textures render differently. */
	void FindTextureDuplicates(const FAssetData& Selected, TMap<FSoftObjectPath, TOptional<FTextureFingerprint>>& Fingerprints,
		TArray<FAssetAuditRow>& Rows)
	{
		auto* Source = Cast<UTexture2D>(Selected.GetAsset());
		if (!Fingerprints.Contains(Selected.GetSoftObjectPath()))
		{
			Fingerprints.Add(Selected.GetSoftObjectPath(), ReadFingerprint(Source));
		}
		const auto SourceKey = Fingerprints.FindChecked(Selected.GetSoftObjectPath());
		if (!SourceKey.IsSet())
		{
			Rows.Add({ Selected.AssetName.ToString(), Selected.GetObjectPathString(), TEXT("Unsupported or unreadable source"),
				TEXT("Expected a single-block, single-layer 2D source with one slice.") });
			return;
		}
		const int32 FirstRow = Rows.Num();
		for (const FAssetData& Candidate : FindSiblings(Selected))
		{
			if (Candidate.GetSoftObjectPath() == Selected.GetSoftObjectPath())
			{
				continue;
			}
			auto* Texture = Cast<UTexture2D>(Candidate.GetAsset());
			if (!Texture || Texture->Source.GetSizeX() != SourceKey->Size.X || Texture->Source.GetSizeY() != SourceKey->Size.Y
				|| Texture->Source.GetFormat() != SourceKey->Format)
			{
				continue;
			}
			auto* CandidateKey = Fingerprints.Find(Candidate.GetSoftObjectPath());
			if (!CandidateKey)
			{
				CandidateKey = &Fingerprints.Add(Candidate.GetSoftObjectPath(), ReadFingerprint(Texture));
			}
			if (CandidateKey->IsSet() && CandidateKey->GetValue() == SourceKey.GetValue())
			{
				Rows.Add({ Candidate.AssetName.ToString(), Candidate.GetObjectPathString(), TEXT("Matching source pixels"),
					FString::Printf(TEXT("Selected: %s\nCandidate settings shown after selected settings.\n"
										 "sRGB: %d / %d\nCompression: %d / %d\nMip generation: %d / %d"),
						*Selected.GetObjectPathString(), Source->SRGB, Texture->SRGB,
						static_cast<int32>(Source->CompressionSettings), static_cast<int32>(Texture->CompressionSettings),
						static_cast<int32>(Source->MipGenSettings), static_cast<int32>(Texture->MipGenSettings)) });
			}
		}
		if (Rows.Num() == FirstRow)
		{
			Rows.Add({ Selected.AssetName.ToString(), Selected.GetObjectPathString(), TEXT("No source pixel matches in folder"), {} });
		}
	}

	/** @brief Displays a stable parameter identity, including the material layer association. */
	FString ParameterLabel(const FMaterialParameterInfo& Info)
	{
		return FString::Printf(TEXT("%s (association %d, index %d)"), *Info.Name.ToString(), static_cast<int32>(Info.Association), Info.Index);
	}

	/** @brief Compares effective scalar, vector, texture and switch values across the selection and against each parent. */
	TArray<FAssetAuditRow> CompareMaterialOverrides(const TArray<FAssetData>& Assets)
	{
		TArray<FAssetAuditRow> Rows;
		TArray<const UMaterialInstance*> Instances;

		for (const FAssetData& Asset : Assets)
		{
			const auto* Instance = Cast<UMaterialInstance>(Asset.GetAsset());
			if (Instance)
			{
				Instances.Add(Instance);
			}
		}
		for (const EMaterialParameterType Type : { EMaterialParameterType::Scalar, EMaterialParameterType::Vector,
				 EMaterialParameterType::Texture, EMaterialParameterType::StaticSwitch })
		{
			TSet<FMaterialParameterInfo> Parameters;
			for (const auto* Instance : Instances)
			{
				TArray<FMaterialParameterInfo> Infos;
				TArray<FGuid> Ids;
				Instance->GetAllParameterInfoOfType(Type, Infos, Ids);
				for (const auto& Info : Infos)
				{
					Parameters.Add(Info);
				}
			}
			for (const auto& Info : Parameters)
			{
				FAssetAuditRow Row{ ParameterLabel(Info), {}, {}, {} };
				TArray<FString> Values;
				TOptional<FMaterialParameterValue> FirstValue;
				bool bSameValue = true;
				for (const auto* Instance : Instances)
				{
					FMaterialParameterMetadata Effective;

					FMaterialParameterMetadata Parent;
					const FMemoryImageMaterialParameterInfo Key(Info);
					if (!Instance->GetParameterValue(Type, Key, Effective))
					{
						continue;
					}
					const bool bOverride = Effective.bOverride;
					const bool bParent = Instance->Parent && Instance->GetParameterValue(Type, Key, Parent, EMaterialGetParameterValueFlags::CheckNonOverrides);
					if (FirstValue.IsSet())
					{
						bSameValue &= Effective.Value == FirstValue.GetValue();
					}
					else
					{
						FirstValue = Effective.Value;
					}
					FString Value;
					switch (Type)
					{
						case EMaterialParameterType::Scalar:
							Value = FString::SanitizeFloat(Effective.Value.AsScalar());
							break;
						case EMaterialParameterType::Vector:
							Value = Effective.Value.AsLinearColor().ToString();
							break;
						case EMaterialParameterType::Texture:
							Value = GetPathNameSafe(Effective.Value.Texture);
							break;
						case EMaterialParameterType::StaticSwitch:
							Value = Effective.Value.AsStaticSwitch() ? TEXT("true") : TEXT("false");
							break;
						default:
							break;
					}
					Values.Add(FString::Printf(TEXT("%s: %s [%s%s]"), *Instance->GetPathName(), *Value,
						bOverride ? TEXT("local override") : TEXT("inherited"),
						bParent && Effective.Value == Parent.Value ? TEXT(", matches parent") : TEXT("")));
				}
				if (!Values.IsEmpty())
				{
					Row.Summary = FString::Printf(TEXT("%s: %s (%d/%d instances)"), Type == EMaterialParameterType::Scalar ? TEXT("Scalar") : Type == EMaterialParameterType::Vector ? TEXT("Vector")
							: Type == EMaterialParameterType::Texture																												 ? TEXT("Texture")
																																													 : TEXT("Static switch"),
						bSameValue ? TEXT("same value") : TEXT("different values"), Values.Num(), Instances.Num());
					Row.Details = FString::Join(Values, TEXT("\n"));
					Rows.Add(MoveTemp(Row));
				}
			}
		}
		return Rows;
	}

	/** @brief Reads sequence settings and compares notify and float-curve names with project rules. */
	void InspectAnimation(const UAnimSequence& Sequence, FAssetAuditRow& Row)
	{
		const auto* Settings = UContentBrowserToolkitSettings::Get();
		TSet<FName> Notifies;
		for (const auto& Notify : Sequence.Notifies)
		{
			if (!Notify.NotifyName.IsNone())
			{
				Notifies.Add(Notify.NotifyName);
			}
		}
		TSet<FName> Curves;
		if (const auto* Model = Sequence.GetDataModel())
		{
			for (const FFloatCurve& Curve : Model->GetFloatCurves())
			{
				Curves.Add(Curve.GetName());
			}
		}
		TArray<FString> Missing;
		for (FName Name : Settings->RequiredAnimationNotifies)
		{
			if (!Name.IsNone() && !Notifies.Contains(Name))
			{
				Missing.Add(TEXT("Missing notify: ") + Name.ToString());
			}
		}
		for (FName Name : Settings->RequiredAnimationCurves)
		{
			if (!Name.IsNone() && !Curves.Contains(Name))
			{
				Missing.Add(TEXT("Missing float curve: ") + Name.ToString());
			}
		}
		Row.Summary = FString::Printf(TEXT("%.2fs / %.2f fps / root motion %s"), Sequence.GetPlayLength(),
			Sequence.GetSamplingFrameRate().AsDecimal(), Sequence.bEnableRootMotion ? TEXT("on") : TEXT("off"));
		Row.Details = FString::Printf(TEXT("Skeleton: %s\nNotify names: %d\nFloat curves: %d\n%s"), *GetPathNameSafe(Sequence.GetSkeleton()),
			Notifies.Num(), Curves.Num(), *FString::Join(Missing, TEXT("\n")));
	}

	/** @brief Reports mesh setup while keeping source and runtime asset data unchanged. */
	void InspectStaticMesh(const UStaticMesh& Mesh, FAssetAuditRow& Row)
	{
		const UBodySetup* Body = Mesh.GetBodySetup();
		Row.Summary = FString::Printf(TEXT("%d LODs / %d material slots / Nanite %s"),
			Mesh.GetNumLODs(), Mesh.GetStaticMaterials().Num(), Mesh.IsNaniteEnabled() ? TEXT("on") : TEXT("off"));
		Row.Details = Body ? FString::Printf(TEXT("Simple collision shapes: %d\nCollision complexity: %s"),
								 Body->AggGeom.GetElementCount(), *UEnum::GetValueAsString(Body->GetCollisionTraceFlag().GetValue()))
						   : TEXT("No body setup.");
		for (const FStaticMaterial& Slot : Mesh.GetStaticMaterials())
		{
			Row.Details += FString::Printf(TEXT("\n%s: %s"), *Slot.MaterialSlotName.ToString(), *GetPathNameSafe(Slot.MaterialInterface));
		}
	}

	/** @brief Lists the main setup assets, material slots and morph targets of a skeletal mesh. */
	void InspectSkeletalMesh(const USkeletalMesh& Mesh, FAssetAuditRow& Row)
	{
		Row.Summary = FString::Printf(TEXT("%d LODs / %d material slots / %d morph targets"),
			Mesh.GetLODNum(), Mesh.GetMaterials().Num(), Mesh.GetMorphTargets().Num());
		Row.Details = FString::Printf(TEXT("Skeleton: %s\nPhysics Asset: %s"), *GetPathNameSafe(Mesh.GetSkeleton()),
			*GetPathNameSafe(Mesh.GetPhysicsAsset()));
		for (const FSkeletalMaterial& Slot : Mesh.GetMaterials())
		{
			Row.Details += FString::Printf(TEXT("\n%s: %s"), *Slot.MaterialSlotName.ToString(), *GetPathNameSafe(Slot.MaterialInterface));
		}
		for (const UMorphTarget* Target : Mesh.GetMorphTargets())
		{
			Row.Details += TEXT("\nMorph target: ") + GetNameSafe(Target);
		}
	}

	/** @brief Uses the closest configured folder profile to check imported audio properties. */
	void InspectSound(const USoundWave& Sound, const FAssetData& Asset, FAssetAuditRow& Row)
	{
		const auto& Profiles = UContentBrowserToolkitSettings::Get()->AudioImportProfiles;
		const FString Folder = Asset.PackagePath.ToString();
		const FCBTAudioImportProfile* Profile = nullptr;
		FString ProfilePath;
		for (const auto& Entry : Profiles)
		{
			if (Entry.Key.Len() > ProfilePath.Len() && (Folder.Equals(Entry.Key, ESearchCase::IgnoreCase) || Folder.StartsWith(Entry.Key + TEXT("/"), ESearchCase::IgnoreCase)))
			{
				ProfilePath = Entry.Key;
				Profile = &Entry.Value;
			}
		}
		const uint32 Rate = Sound.GetImportedSampleRate();
		Row.Summary = FString::Printf(TEXT("%.2fs / %d channels / %u Hz"), Sound.Duration, Sound.NumChannels, Rate);
		Row.Details = Profile ? TEXT("Profile: ") + ProfilePath : TEXT("No matching folder profile.");
		if (Profile && Profile->Channels > 0 && Profile->Channels != Sound.NumChannels)
		{
			Row.Details += FString::Printf(TEXT("\nExpected %d channels"), Profile->Channels);
		}
		if (Profile && Profile->SampleRate > 0 && static_cast<uint32>(Profile->SampleRate) != Rate)
		{
			Row.Details += FString::Printf(TEXT("\nExpected %d Hz"), Profile->SampleRate);
		}
	}
} // namespace

TArray<FAssetAuditRow> BuildCBTTypeInspection(ECBTInspection Inspection, const TArray<FAssetData>& Assets)
{
	if (Inspection == ECBTInspection::MaterialOverrides)
	{
		return CompareMaterialOverrides(Assets);
	}

	TArray<FAssetAuditRow> Rows;
	TMap<FSoftObjectPath, TOptional<FTextureFingerprint>> Fingerprints;
	for (const FAssetData& Asset : Assets)
	{
		if (Inspection == ECBTInspection::ReplacementCandidates)
		{
			FindReplacementCandidates(Asset, Rows);
			continue;
		}
		if (Inspection == ECBTInspection::TextureDuplicates)
		{
			FindTextureDuplicates(Asset, Fingerprints, Rows);
			continue;
		}
		const UObject* Loaded = Asset.GetAsset();
		FAssetAuditRow Row{ Asset.AssetName.ToString(), Asset.GetObjectPathString(), TEXT("Asset unavailable"), {} };
		if (const auto* Sequence = Cast<UAnimSequence>(Loaded))
		{
			InspectAnimation(*Sequence, Row);
		}
		else if (const auto* StaticMesh = Cast<UStaticMesh>(Loaded))
		{
			InspectStaticMesh(*StaticMesh, Row);
		}
		else if (const auto* SkeletalMesh = Cast<USkeletalMesh>(Loaded))
		{
			InspectSkeletalMesh(*SkeletalMesh, Row);
		}
		else if (const auto* Sound = Cast<USoundWave>(Loaded))
		{
			InspectSound(*Sound, Asset, Row);
		}
		Rows.Add(MoveTemp(Row));
	}
	return Rows;
}
