#include "Data/Validation/GambitEffectDefinitionsAuditCommandlet.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Data/Assets/GambitItemEffectDefinition.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "HAL/FileManager.h"
#include "Items/Consumables/GambitConsumableDefinition.h"
#include "Items/Data/GambitItemDefinition.h"
#include "Items/Modules/GambitModuleDefinition.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
	const TCHAR* DefaultReportPath()
	{
		return TEXT("Docs/Audits/EffectDefinitionsMigrationAudit.md");
	}

	bool IsNeutralScoreModifier(const FGambitScoreModifierContext& Modifier)
	{
		return FMath::IsNearlyZero(Modifier.AdditiveBonus)
			&& FMath::IsNearlyZero(Modifier.DiceContributionMultiplierBonus)
			&& FMath::IsNearlyEqual(Modifier.Multiplier, 1.0f)
			&& Modifier.ScoreCap <= 0.0f
			&& Modifier.DiminishingThreshold <= 0.0f
			&& FMath::IsNearlyEqual(Modifier.DiminishingFactor, 1.0f);
	}

	FString EscapeMarkdownCell(FString Value)
	{
		Value.ReplaceInline(TEXT("|"), TEXT("\\|"));
		Value.ReplaceInline(TEXT("\r"), TEXT(" "));
		Value.ReplaceInline(TEXT("\n"), TEXT(" "));
		return Value;
	}

	FString FormatFloat(const float Value)
	{
		return FString::Printf(TEXT("%.3f"), Value);
	}

	void AppendField(
		TArray<FString>& OutFields,
		TArray<FString>& OutMissingMappings,
		const FString& FieldName,
		const float Value,
		const FString& Mapping,
		const bool bHasExactMapping)
	{
		OutFields.Add(FString::Printf(TEXT("%s=%s -> %s"), *FieldName, *FormatFloat(Value), *Mapping));
		if (!bHasExactMapping)
		{
			OutMissingMappings.Add(FieldName);
		}
	}

	void BuildLegacyFieldMapping(
		const FGambitScoreModifierContext& Modifier,
		TArray<FString>& OutFields,
		TArray<FString>& OutMissingMappings)
	{
		if (!FMath::IsNearlyZero(Modifier.AdditiveBonus))
		{
			AppendField(OutFields, OutMissingMappings, TEXT("AdditiveBonus"), Modifier.AdditiveBonus, TEXT("AddScoreFlat.Amount"), true);
		}

		if (!FMath::IsNearlyZero(Modifier.DiceContributionMultiplierBonus))
		{
			AppendField(
				OutFields,
				OutMissingMappings,
				TEXT("DiceContributionMultiplierBonus"),
				Modifier.DiceContributionMultiplierBonus,
				TEXT("ScoreModifier.ScoreModifier.DiceContributionMultiplierBonus"),
				true);
		}

		if (!FMath::IsNearlyEqual(Modifier.Multiplier, 1.0f))
		{
			AppendField(OutFields, OutMissingMappings, TEXT("Multiplier"), Modifier.Multiplier, TEXT("MultiplyScore.Multiplier"), true);
		}

		if (Modifier.ScoreCap > 0.0f)
		{
			AppendField(
				OutFields,
				OutMissingMappings,
				TEXT("ScoreCap"),
				Modifier.ScoreCap,
				TEXT("ScoreModifier.ScoreModifier.ScoreCap"),
				true);
		}

		if (Modifier.DiminishingThreshold > 0.0f)
		{
			AppendField(
				OutFields,
				OutMissingMappings,
				TEXT("DiminishingThreshold"),
				Modifier.DiminishingThreshold,
				TEXT("ScoreModifier.ScoreModifier.DiminishingThreshold"),
				true);
		}

		if (!FMath::IsNearlyEqual(Modifier.DiminishingFactor, 1.0f))
		{
			AppendField(
				OutFields,
				OutMissingMappings,
				TEXT("DiminishingFactor"),
				Modifier.DiminishingFactor,
				TEXT("ScoreModifier.ScoreModifier.DiminishingFactor"),
				true);
		}
	}

	FString JoinOrNone(const TArray<FString>& Values)
	{
		return Values.Num() > 0 ? FString::Join(Values, TEXT("; ")) : FString(TEXT("None"));
	}

	FString GetItemIdString(const UGambitItemDefinition* ItemDefinition)
	{
		if (!ItemDefinition)
		{
			return TEXT("None");
		}

		const FName ItemId = ItemDefinition->GetResolvedItemId();
		return ItemId.IsNone() ? ItemDefinition->GetName() : ItemId.ToString();
	}

	FString GetAssetPath(const UObject* Asset, const FAssetData& AssetData)
	{
		if (Asset)
		{
			return Asset->GetPathName();
		}

		return AssetData.GetSoftObjectPath().ToString();
	}

	FString SanitizeAssetName(const FString& Source)
	{
		FString Result;
		Result.Reserve(Source.Len());
		for (const TCHAR Character : Source)
		{
			Result.AppendChar(FChar::IsAlnum(Character) ? Character : TEXT('_'));
		}

		return Result;
	}

	FString BuildEffectFolderPath(const UGambitItemDefinition* ItemDefinition)
	{
		if (ItemDefinition && ItemDefinition->IsA<UGambitConsumableDefinition>())
		{
			return TEXT("/Game/GrandpaGambit/Data/Items/Effects/Consumables");
		}

		return TEXT("/Game/GrandpaGambit/Data/Items/Effects/Modules");
	}

	bool SaveAssetPackage(UObject* Asset)
	{
		if (!Asset)
		{
			return false;
		}

		UPackage* Package = Asset->GetPackage();
		if (!Package)
		{
			return false;
		}

		const FString PackageFilename = FPackageName::LongPackageNameToFilename(
			Package->GetName(),
			FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, Asset, *PackageFilename, SaveArgs);
	}

	UGambitItemEffectDefinition* CreateOrUpdateEffectAsset(
		UGambitItemDefinition* ItemDefinition,
		const FString& Suffix,
		const EGambitEffectHook Hook,
		const EGambitItemEffectType EffectType,
		const FName EffectTypeId,
		const float Amount,
		const float Multiplier,
		const FGambitScoreModifierContext* ScoreModifierOverride = nullptr)
	{
		if (!ItemDefinition)
		{
			return nullptr;
		}

		const FString ItemAssetName = SanitizeAssetName(ItemDefinition->GetName());
		const FString EffectAssetName = FString::Printf(TEXT("DA_Effect_%s_Legacy_%s"), *ItemAssetName, *Suffix);
		const FString PackageName = FString::Printf(TEXT("%s/%s"), *BuildEffectFolderPath(ItemDefinition), *EffectAssetName);
		const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackageName, *EffectAssetName);

		UGambitItemEffectDefinition* EffectDefinition = FPackageName::DoesPackageExist(PackageName)
			? LoadObject<UGambitItemEffectDefinition>(nullptr, *ObjectPath)
			: nullptr;
		UPackage* Package = EffectDefinition ? EffectDefinition->GetPackage() : CreatePackage(*PackageName);
		if (!Package)
		{
			return nullptr;
		}

		if (!EffectDefinition)
		{
			EffectDefinition = NewObject<UGambitItemEffectDefinition>(
				Package,
				UGambitItemEffectDefinition::StaticClass(),
				*EffectAssetName,
				RF_Public | RF_Standalone);
			FAssetRegistryModule::AssetCreated(EffectDefinition);
		}

		EffectDefinition->Hook = Hook;
		EffectDefinition->EffectType = EffectType;
		EffectDefinition->Target = EGambitEffectTarget::Source;
		EffectDefinition->EffectId = FName(*FString::Printf(
			TEXT("%s.legacy.%s"),
			*ItemDefinition->GetResolvedItemId().ToString(),
			*Suffix.ToLower()));
		EffectDefinition->EffectTypeId = EffectTypeId;
		EffectDefinition->TargetRuleId = TEXT("target.source");
		EffectDefinition->ScoreModifier = ScoreModifierOverride ? *ScoreModifierOverride : FGambitScoreModifierContext();
		EffectDefinition->Conditions.Reset();
		EffectDefinition->Amount = Amount;
		EffectDefinition->Multiplier = Multiplier;
		EffectDefinition->ScalarParameters.Reset();
		EffectDefinition->Modify();
		EffectDefinition->MarkPackageDirty();

		return SaveAssetPackage(EffectDefinition) ? EffectDefinition : nullptr;
	}

	bool CreateEffectDefinitionsFromLegacyModifier(
		UGambitItemDefinition* ItemDefinition,
		const FGambitScoreModifierContext& Modifier,
		TArray<TObjectPtr<UGambitItemEffectDefinition>>& OutEffectDefinitions,
		TArray<FString>& OutActions)
	{
		if (!ItemDefinition)
		{
			return false;
		}

		const EGambitEffectHook Hook = ItemDefinition->IsA<UGambitConsumableDefinition>()
			? EGambitEffectHook::ConsumableUse
			: EGambitEffectHook::ScoreModifier;
		const bool bRequiresGenericScoreModifier = !FMath::IsNearlyZero(Modifier.DiceContributionMultiplierBonus)
			|| Modifier.ScoreCap > 0.0f
			|| Modifier.DiminishingThreshold > 0.0f
			|| !FMath::IsNearlyEqual(Modifier.DiminishingFactor, 1.0f);

		if (bRequiresGenericScoreModifier)
		{
			UGambitItemEffectDefinition* EffectDefinition = CreateOrUpdateEffectAsset(
				ItemDefinition,
				TEXT("ScoreModifier"),
				Hook,
				EGambitItemEffectType::ScoreModifier,
				TEXT("score.modifier"),
				0.0f,
				1.0f,
				&Modifier);
			if (!EffectDefinition)
			{
				return false;
			}

			OutEffectDefinitions.Add(EffectDefinition);
			OutActions.Add(FString::Printf(
				TEXT("Created `%s` for `%s` generic ScoreModifier legacy migration."),
				*EffectDefinition->GetPathName(),
				*ItemDefinition->GetPathName()));
			return true;
		}

		if (!FMath::IsNearlyZero(Modifier.AdditiveBonus))
		{
			UGambitItemEffectDefinition* EffectDefinition = CreateOrUpdateEffectAsset(
				ItemDefinition,
				TEXT("AddScore"),
				Hook,
				EGambitItemEffectType::AddScoreFlat,
				TEXT("score.flat"),
				Modifier.AdditiveBonus,
				1.0f);
			if (!EffectDefinition)
			{
				return false;
			}

			OutEffectDefinitions.Add(EffectDefinition);
			OutActions.Add(FString::Printf(
				TEXT("Created `%s` for `%s` AdditiveBonus=%s."),
				*EffectDefinition->GetPathName(),
				*ItemDefinition->GetPathName(),
				*FormatFloat(Modifier.AdditiveBonus)));
		}

		if (!FMath::IsNearlyEqual(Modifier.Multiplier, 1.0f))
		{
			UGambitItemEffectDefinition* EffectDefinition = CreateOrUpdateEffectAsset(
				ItemDefinition,
				TEXT("MultiplyScore"),
				Hook,
				EGambitItemEffectType::MultiplyScore,
				TEXT("score.multiplier"),
				0.0f,
				Modifier.Multiplier);
			if (!EffectDefinition)
			{
				return false;
			}

			OutEffectDefinitions.Add(EffectDefinition);
			OutActions.Add(FString::Printf(
				TEXT("Created `%s` for `%s` Multiplier=%s."),
				*EffectDefinition->GetPathName(),
				*ItemDefinition->GetPathName(),
				*FormatFloat(Modifier.Multiplier)));
		}

		return OutEffectDefinitions.Num() > 0;
	}

	struct FLegacyItemAuditRecord
	{
		FString AssetPath;
		FString ItemType;
		FString ItemId;
		int32 EffectDefinitionCount = 0;
		int32 EffectClassCount = 0;
		bool bLegacyNonNeutral = false;
		bool bLegacyOnly = false;
		bool bMixed = false;
		TArray<FString> LegacyFields;
		TArray<FString> MissingMappings;
		FString Recommendation;
	};

	void AppendRecordLines(const TCHAR* Heading, const TArray<FLegacyItemAuditRecord>& Records, TArray<FString>& Lines)
	{
		Lines.Add(FString::Printf(TEXT("## %s"), Heading));
		Lines.Add(TEXT(""));
		if (Records.Num() == 0)
		{
			Lines.Add(TEXT("None found."));
			Lines.Add(TEXT(""));
			return;
		}

		Lines.Add(TEXT("| Asset | Type | ItemId | EffectDefinitions | EffectClasses | Legacy fields | Missing mappings | Recommendation |"));
		Lines.Add(TEXT("| --- | --- | --- | ---: | ---: | --- | --- | --- |"));
		for (const FLegacyItemAuditRecord& Record : Records)
		{
			Lines.Add(FString::Printf(
				TEXT("| `%s` | %s | `%s` | %d | %d | %s | %s | %s |"),
				*EscapeMarkdownCell(Record.AssetPath),
				*EscapeMarkdownCell(Record.ItemType),
				*EscapeMarkdownCell(Record.ItemId),
				Record.EffectDefinitionCount,
				Record.EffectClassCount,
				*EscapeMarkdownCell(JoinOrNone(Record.LegacyFields)),
				*EscapeMarkdownCell(JoinOrNone(Record.MissingMappings)),
				*EscapeMarkdownCell(Record.Recommendation)));
		}
		Lines.Add(TEXT(""));
	}
}

UGambitEffectDefinitionsAuditCommandlet::UGambitEffectDefinitionsAuditCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UGambitEffectDefinitionsAuditCommandlet::Main(const FString& Params)
{
	const bool bMigrate = FParse::Param(*Params, TEXT("Migrate"));
	FString ReportPath;
	FParse::Value(*Params, TEXT("ReportPath="), ReportPath);
	if (ReportPath.IsEmpty())
	{
		ReportPath = FPaths::Combine(FPaths::ProjectDir(), DefaultReportPath());
	}
	else if (FPaths::IsRelative(ReportPath))
	{
		ReportPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), ReportPath);
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	AssetRegistry.SearchAllAssets(true);

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByPath(FName(TEXT("/Game/GrandpaGambit/Data/Items")), AssetDataList, true);

	TArray<FLegacyItemAuditRecord> LegacyOnlyRecords;
	TArray<FLegacyItemAuditRecord> MixedRecords;
	TArray<FLegacyItemAuditRecord> AllLegacyRecords;
	TArray<FLegacyItemAuditRecord> BlockedRecords;
	TArray<FString> MigrationActions;
	int32 ModuleCount = 0;
	int32 ConsumableCount = 0;
	int32 LoadFailureCount = 0;

	for (const FAssetData& AssetData : AssetDataList)
	{
		UObject* Asset = AssetData.GetAsset();
		if (!Asset)
		{
			LoadFailureCount++;
			continue;
		}

		UGambitModuleDefinition* ModuleDefinition = Cast<UGambitModuleDefinition>(Asset);
		UGambitConsumableDefinition* ConsumableDefinition = Cast<UGambitConsumableDefinition>(Asset);
		UGambitItemDefinition* ItemDefinition = Cast<UGambitItemDefinition>(Asset);
		if (!ItemDefinition || (!ModuleDefinition && !ConsumableDefinition))
		{
			continue;
		}

		const FGambitScoreModifierContext& LegacyModifier = ModuleDefinition
			? ModuleDefinition->PersistentScoreModifier
			: ConsumableDefinition->ActionScoreModifier;
		const bool bLegacyNonNeutral = !IsNeutralScoreModifier(LegacyModifier);
		const int32 EffectDefinitionCount = ItemDefinition->EffectDefinitions.Num();
		const bool bHasEffectDefinitions = EffectDefinitionCount > 0;

		if (ModuleDefinition)
		{
			ModuleCount++;
		}
		else
		{
			ConsumableCount++;
		}

		if (!bLegacyNonNeutral)
		{
			continue;
		}

		FLegacyItemAuditRecord Record;
		Record.AssetPath = GetAssetPath(Asset, AssetData);
		Record.ItemType = ModuleDefinition ? TEXT("Module") : TEXT("Consumable");
		Record.ItemId = GetItemIdString(ItemDefinition);
		Record.EffectDefinitionCount = EffectDefinitionCount;
		Record.EffectClassCount = ItemDefinition->EffectClasses.Num();
		Record.bLegacyNonNeutral = bLegacyNonNeutral;
		Record.bLegacyOnly = !bHasEffectDefinitions;
		Record.bMixed = bHasEffectDefinitions;
		BuildLegacyFieldMapping(LegacyModifier, Record.LegacyFields, Record.MissingMappings);
		if (Record.MissingMappings.Num() > 0)
		{
			Record.Recommendation = TEXT("Block migration until missing EffectDefinition types exist; keep legacy fallback temporarily.");
			BlockedRecords.Add(Record);
		}
		else if (Record.bMixed)
		{
			Record.Recommendation = TEXT("Clear legacy field after verifying EffectDefinitions are authoritative.");
		}
		else
		{
			Record.Recommendation = TEXT("Migratable now with exact EffectDefinitions mapping, then reset legacy field to neutral.");
		}

		AllLegacyRecords.Add(Record);
		if (Record.bLegacyOnly)
		{
			LegacyOnlyRecords.Add(Record);
		}
		if (Record.bMixed)
		{
			MixedRecords.Add(Record);
		}

		if (bMigrate && Record.bLegacyOnly && Record.MissingMappings.Num() == 0)
		{
			TArray<TObjectPtr<UGambitItemEffectDefinition>> CreatedEffectDefinitions;
			TArray<FString> CurrentActions;
			if (CreateEffectDefinitionsFromLegacyModifier(ItemDefinition, LegacyModifier, CreatedEffectDefinitions, CurrentActions))
			{
				ItemDefinition->Modify();
				for (UGambitItemEffectDefinition* EffectDefinition : CreatedEffectDefinitions)
				{
					ItemDefinition->EffectDefinitions.AddUnique(EffectDefinition);
				}

				if (ModuleDefinition)
				{
					ModuleDefinition->PersistentScoreModifier = FGambitScoreModifierContext();
				}
				else
				{
					ConsumableDefinition->ActionScoreModifier = FGambitScoreModifierContext();
				}

				ItemDefinition->MarkPackageDirty();
				if (SaveAssetPackage(ItemDefinition))
				{
					MigrationActions.Append(CurrentActions);
					MigrationActions.Add(FString::Printf(
						TEXT("Migrated `%s` and reset legacy modifier to neutral."),
						*ItemDefinition->GetPathName()));
				}
				else
				{
					MigrationActions.Add(FString::Printf(TEXT("Failed to save migrated item `%s`."), *ItemDefinition->GetPathName()));
				}
			}
			else
			{
				MigrationActions.Add(FString::Printf(TEXT("Skipped `%s`: unable to create exact EffectDefinitions."), *Record.AssetPath));
			}
		}
	}

	TArray<FString> Lines;
	Lines.Add(TEXT("# EffectDefinitions Migration Audit"));
	Lines.Add(TEXT(""));
	Lines.Add(bMigrate
		? TEXT("Generated by `GambitEffectDefinitionsAuditCommandlet -Migrate`. This run created/updated EffectDefinition assets and saved migrated item DataAssets through Unreal package saving.")
		: TEXT("Generated by `GambitEffectDefinitionsAuditCommandlet`. The commandlet loads gameplay item DataAssets and does not modify `.uasset` files."));
	Lines.Add(TEXT(""));
	Lines.Add(TEXT("## Summary"));
	Lines.Add(TEXT(""));
	Lines.Add(FString::Printf(TEXT("- Audited modules: %d"), ModuleCount));
	Lines.Add(FString::Printf(TEXT("- Audited consumables: %d"), ConsumableCount));
	Lines.Add(FString::Printf(TEXT("- Legacy non-neutral assets: %d"), AllLegacyRecords.Num()));
	Lines.Add(FString::Printf(TEXT("- Legacy-only assets: %d"), LegacyOnlyRecords.Num()));
	Lines.Add(FString::Printf(TEXT("- Mixed legacy + EffectDefinitions assets: %d"), MixedRecords.Num()));
	Lines.Add(FString::Printf(TEXT("- Assets blocked by missing dedicated mappings: %d"), BlockedRecords.Num()));
	Lines.Add(FString::Printf(TEXT("- Asset load failures: %d"), LoadFailureCount));
	Lines.Add(TEXT(""));
	if (bMigrate)
	{
		Lines.Add(TEXT("## Migration Actions"));
		Lines.Add(TEXT(""));
		if (MigrationActions.Num() == 0)
		{
			Lines.Add(TEXT("No assets were migrated."));
		}
		else
		{
			for (const FString& Action : MigrationActions)
			{
				Lines.Add(FString::Printf(TEXT("- %s"), *Action));
			}
		}
		Lines.Add(TEXT(""));
	}
	Lines.Add(TEXT("## Mapping Coverage"));
	Lines.Add(TEXT(""));
	Lines.Add(TEXT("| Legacy field | EffectDefinitions mapping | Status |"));
	Lines.Add(TEXT("| --- | --- | --- |"));
	Lines.Add(TEXT("| `AdditiveBonus` | `AddScoreFlat.Amount` | Available |"));
	Lines.Add(TEXT("| `Multiplier` | `MultiplyScore.Multiplier` | Available |"));
	Lines.Add(TEXT("| `DiceContributionMultiplierBonus` | Generic `ScoreModifier.ScoreModifier.DiceContributionMultiplierBonus` | Available |"));
	Lines.Add(TEXT("| `ScoreCap` | Generic `ScoreModifier.ScoreModifier.ScoreCap` | Available |"));
	Lines.Add(TEXT("| `DiminishingThreshold` / `DiminishingFactor` | Generic `ScoreModifier.ScoreModifier` fields | Available |"));
	Lines.Add(TEXT(""));
	AppendRecordLines(TEXT("Legacy-Only Assets"), LegacyOnlyRecords, Lines);
	AppendRecordLines(TEXT("Mixed Assets"), MixedRecords, Lines);
	AppendRecordLines(TEXT("Legacy Effects Blocked By Missing Mappings"), BlockedRecords, Lines);
	AppendRecordLines(TEXT("All Non-Neutral Legacy Assets"), AllLegacyRecords, Lines);
	Lines.Add(TEXT("## C++ Files Still Concerned"));
	Lines.Add(TEXT(""));
	Lines.Add(TEXT("This commandlet audits assets only. The static C++ audit must be kept in sync manually after code review."));
	Lines.Add(TEXT(""));
	Lines.Add(TEXT("## Risks"));
	Lines.Add(TEXT(""));
	Lines.Add(TEXT("- Binary asset values are read through Unreal asset loading; direct text scans of `.uasset` files are insufficient for value-level migration decisions."));
	Lines.Add(TEXT("- Do not remove legacy fields until this report shows no legacy-only assets, no mixed assets, and no missing mappings."));
	Lines.Add(TEXT(""));

	const FString Report = FString::Join(Lines, LINE_TERMINATOR);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
	if (!FFileHelper::SaveStringToFile(Report, *ReportPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to write EffectDefinitions migration audit report to %s"), *ReportPath);
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("Wrote EffectDefinitions migration audit report to %s"), *ReportPath);
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Audit summary: Modules=%d Consumables=%d Legacy=%d LegacyOnly=%d Mixed=%d Blocked=%d MigratedActions=%d LoadFailures=%d"),
		ModuleCount,
		ConsumableCount,
		AllLegacyRecords.Num(),
		LegacyOnlyRecords.Num(),
		MixedRecords.Num(),
		BlockedRecords.Num(),
		MigrationActions.Num(),
		LoadFailureCount);
	return 0;
}
