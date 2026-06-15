#include "Data/Validation/GambitEffectTargetRulesFixupCommandlet.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Data/Assets/GambitItemEffectDefinition.h"
#include "Items/Effects/GambitEffectTargetRules.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
	const TCHAR* GetEffectTargetRulesFixupDefaultContentPath()
	{
		return TEXT("/Game");
	}

	const FName LegacyTargetSelf(TEXT("target.self"));
	const FName LegacyTargetSource(TEXT("target.source"));
	const FName LegacyTargetOpponentSelected(TEXT("target.opponent.selected"));

	bool IsDieSelectionEffectType(const EGambitItemEffectType EffectType)
	{
		switch (EffectType)
		{
		case EGambitItemEffectType::ModifyDieValue:
		case EGambitItemEffectType::SetDieValue:
		case EGambitItemEffectType::LockDie:
		case EGambitItemEffectType::RerollDie:
		case EGambitItemEffectType::DestroyOrRemoveDiceChance:
		case EGambitItemEffectType::TransformDiceForRound:
		case EGambitItemEffectType::SetDieScoreContributionValue:
		case EGambitItemEffectType::SetDieComboContributionCount:
		case EGambitItemEffectType::SetDieCountsForScoreSum:
		case EGambitItemEffectType::SetDieCountsForCombinations:
		case EGambitItemEffectType::SetDieCanBeRerolled:
		case EGambitItemEffectType::SetDieCanBeLocked:
		case EGambitItemEffectType::MarkDieDestroyedAfterRound:
		case EGambitItemEffectType::AddDieRuntimeTags:
		case EGambitItemEffectType::RemoveDieRuntimeTags:
			return true;
		default:
			return false;
		}
	}

	bool TryResolveReplacementRule(const UGambitItemEffectDefinition* EffectDefinition, FName& OutReplacementRule)
	{
		if (!EffectDefinition)
		{
			return false;
		}

		if (EffectDefinition->TargetRuleId == LegacyTargetSelf
			|| EffectDefinition->TargetRuleId == LegacyTargetSource)
		{
			OutReplacementRule = NAME_None;
			return true;
		}

		if (EffectDefinition->TargetRuleId != LegacyTargetOpponentSelected)
		{
			return false;
		}

		if (IsDieSelectionEffectType(EffectDefinition->EffectType))
		{
			OutReplacementRule = GambitEffectTargetRules::TargetSelectedDie;
			return true;
		}

		if (EffectDefinition->Target == EGambitEffectTarget::Target
			|| EffectDefinition->Target == EGambitEffectTarget::SourceAndTarget)
		{
			OutReplacementRule = GambitEffectTargetRules::TargetOpponent;
			return true;
		}

		OutReplacementRule = NAME_None;
		return true;
	}

	bool SaveEffectPackage(const UGambitItemEffectDefinition* EffectDefinition)
	{
		if (!EffectDefinition)
		{
			return false;
		}

		UPackage* Package = EffectDefinition->GetOutermost();
		if (!Package)
		{
			return false;
		}

		const FString PackageName = Package->GetName();
		const FString PackageFilename = FPackageName::LongPackageNameToFilename(
			PackageName,
			FPackageName::GetAssetPackageExtension());

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_None;
		return UPackage::SavePackage(Package, nullptr, *PackageFilename, SaveArgs);
	}
}

UGambitEffectTargetRulesFixupCommandlet::UGambitEffectTargetRulesFixupCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UGambitEffectTargetRulesFixupCommandlet::Main(const FString& Params)
{
	FString ContentPath = GetEffectTargetRulesFixupDefaultContentPath();
	FParse::Value(*Params, TEXT("ContentPath="), ContentPath);
	if (!ContentPath.StartsWith(TEXT("/")))
	{
		ContentPath = FString::Printf(TEXT("/%s"), *ContentPath);
	}

	const bool bDryRun = FParse::Param(*Params, TEXT("DryRun"));

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	AssetRegistry.SearchAllAssets(true);

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*ContentPath));
	Filter.ClassPaths.Add(UGambitItemEffectDefinition::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> EffectAssetData;
	AssetRegistry.GetAssets(Filter, EffectAssetData);
	EffectAssetData.Sort([](const FAssetData& Left, const FAssetData& Right)
	{
		return Left.GetSoftObjectPath().ToString() < Right.GetSoftObjectPath().ToString();
	});

	int32 LoadFailureCount = 0;
	int32 ChangedCount = 0;
	int32 SaveFailureCount = 0;
	for (const FAssetData& AssetData : EffectAssetData)
	{
		UGambitItemEffectDefinition* EffectDefinition = Cast<UGambitItemEffectDefinition>(AssetData.GetAsset());
		if (!EffectDefinition)
		{
			LoadFailureCount++;
			continue;
		}

		FName ReplacementRule = NAME_None;
		if (!TryResolveReplacementRule(EffectDefinition, ReplacementRule))
		{
			continue;
		}

		const FName PreviousRule = EffectDefinition->TargetRuleId;
		UE_LOG(
			LogTemp,
			Display,
			TEXT("EffectTargetRulesFixup: %s TargetRuleId %s -> %s"),
			*EffectDefinition->GetPathName(),
			*PreviousRule.ToString(),
			ReplacementRule.IsNone() ? TEXT("(empty)") : *ReplacementRule.ToString());

		if (!bDryRun)
		{
			EffectDefinition->Modify();
			EffectDefinition->TargetRuleId = ReplacementRule;
			if (UPackage* Package = EffectDefinition->GetOutermost())
			{
				Package->MarkPackageDirty();
			}

			if (!SaveEffectPackage(EffectDefinition))
			{
				SaveFailureCount++;
				UE_LOG(LogTemp, Error, TEXT("EffectTargetRulesFixup: failed to save %s"), *EffectDefinition->GetPathName());
				continue;
			}
		}

		ChangedCount++;
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("EffectTargetRulesFixup summary: ContentPath=%s DryRun=%s Changed=%d LoadFailures=%d SaveFailures=%d"),
		*ContentPath,
		bDryRun ? TEXT("Yes") : TEXT("No"),
		ChangedCount,
		LoadFailureCount,
		SaveFailureCount);

	return (LoadFailureCount == 0 && SaveFailureCount == 0) ? 0 : 1;
}
