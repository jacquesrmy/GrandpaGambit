#include "Data/Validation/GambitEffectDefinitionsAuditCommandlet.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Items/Consumables/GambitConsumableDefinition.h"
#include "Items/Data/GambitItemDefinition.h"
#include "Items/Modules/GambitModuleDefinition.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

namespace
{
	const TCHAR* DefaultReportPath()
	{
		return TEXT("Docs/Audits/EffectDefinitionsMigrationAudit.md");
	}

	FString EscapeMarkdownCell(FString Value)
	{
		Value.ReplaceInline(TEXT("|"), TEXT("\\|"));
		Value.ReplaceInline(TEXT("\r"), TEXT(" "));
		Value.ReplaceInline(TEXT("\n"), TEXT(" "));
		return Value;
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

	bool HasAuditUsableEffectPayload(const UGambitItemDefinition* ItemDefinition)
	{
		return ItemDefinition
			&& (ItemDefinition->EffectDefinitions.Num() > 0 || ItemDefinition->EffectClasses.Num() > 0);
	}

	struct FEffectDefinitionsAuditRecord
	{
		FString AssetPath;
		FString ItemType;
		FString ItemId;
		int32 EffectDefinitionCount = 0;
		int32 EffectClassCount = 0;
		FString Recommendation;
	};

	void AppendRecordLines(const TCHAR* Heading, const TArray<FEffectDefinitionsAuditRecord>& Records, TArray<FString>& Lines)
	{
		Lines.Add(FString::Printf(TEXT("## %s"), Heading));
		Lines.Add(TEXT(""));
		if (Records.Num() == 0)
		{
			Lines.Add(TEXT("None found."));
			Lines.Add(TEXT(""));
			return;
		}

		Lines.Add(TEXT("| Asset | Type | ItemId | EffectDefinitions | EffectClasses | Recommendation |"));
		Lines.Add(TEXT("| --- | --- | --- | ---: | ---: | --- |"));
		for (const FEffectDefinitionsAuditRecord& Record : Records)
		{
			Lines.Add(FString::Printf(
				TEXT("| `%s` | %s | `%s` | %d | %d | %s |"),
				*EscapeMarkdownCell(Record.AssetPath),
				*EscapeMarkdownCell(Record.ItemType),
				*EscapeMarkdownCell(Record.ItemId),
				Record.EffectDefinitionCount,
				Record.EffectClassCount,
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

	TArray<FEffectDefinitionsAuditRecord> MissingPayloadRecords;
	int32 ModuleCount = 0;
	int32 ConsumableCount = 0;
	int32 EffectDefinitionBackedCount = 0;
	int32 EffectClassOnlyCount = 0;
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

		if (ModuleDefinition)
		{
			ModuleCount++;
		}
		else
		{
			ConsumableCount++;
		}

		if (ItemDefinition->EffectDefinitions.Num() > 0)
		{
			EffectDefinitionBackedCount++;
		}
		else if (ItemDefinition->EffectClasses.Num() > 0)
		{
			EffectClassOnlyCount++;
		}

		if (!HasAuditUsableEffectPayload(ItemDefinition))
		{
			FEffectDefinitionsAuditRecord Record;
			Record.AssetPath = GetAssetPath(Asset, AssetData);
			Record.ItemType = ModuleDefinition ? TEXT("Module") : TEXT("Consumable");
			Record.ItemId = GetItemIdString(ItemDefinition);
			Record.EffectDefinitionCount = ItemDefinition->EffectDefinitions.Num();
			Record.EffectClassCount = ItemDefinition->EffectClasses.Num();
			Record.Recommendation = TEXT("Add EffectDefinitions or an EffectClass; score shortcuts no longer exist.");
			MissingPayloadRecords.Add(Record);
		}
	}

	TArray<FString> Lines;
	Lines.Add(TEXT("# EffectDefinitions Migration Audit"));
	Lines.Add(TEXT(""));
	Lines.Add(TEXT("Generated by `GambitEffectDefinitionsAuditCommandlet`. Legacy score shortcut fields have been removed from C++; this report verifies item assets are backed by effect payloads."));
	Lines.Add(TEXT(""));
	Lines.Add(TEXT("## Summary"));
	Lines.Add(TEXT(""));
	Lines.Add(FString::Printf(TEXT("- Audited modules: %d"), ModuleCount));
	Lines.Add(FString::Printf(TEXT("- Audited consumables: %d"), ConsumableCount));
	Lines.Add(FString::Printf(TEXT("- Items with EffectDefinitions: %d"), EffectDefinitionBackedCount));
	Lines.Add(FString::Printf(TEXT("- Items with EffectClasses only: %d"), EffectClassOnlyCount));
	Lines.Add(TEXT("- Legacy-only items: 0"));
	Lines.Add(TEXT("- Mixed legacy + EffectDefinitions items: 0"));
	Lines.Add(FString::Printf(TEXT("- Items missing effect payloads: %d"), MissingPayloadRecords.Num()));
	Lines.Add(FString::Printf(TEXT("- Blocked items: %d"), MissingPayloadRecords.Num()));
	Lines.Add(FString::Printf(TEXT("- Asset load failures: %d"), LoadFailureCount));
	Lines.Add(TEXT(""));

	AppendRecordLines(TEXT("Items Missing Effect Payloads"), MissingPayloadRecords, Lines);

	Lines.Add(TEXT("## Notes"));
	Lines.Add(TEXT(""));
	Lines.Add(TEXT("- Legacy score shortcut fields are no longer part of module or consumable definitions."));
	Lines.Add(TEXT("- Runtime score changes must be authored through `EffectDefinitions` or explicit `EffectClasses`."));
	Lines.Add(TEXT("- This commandlet no longer migrates assets; it is a verification tool for post-migration content."));
	Lines.Add(TEXT(""));

	const FString Report = FString::Join(Lines, TEXT("\n"));
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
		TEXT("Audit summary: Modules=%d Consumables=%d LegacyOnly=0 Mixed=0 Blocked=%d EffectDefinitions=%d EffectClassesOnly=%d LoadFailures=%d"),
		ModuleCount,
		ConsumableCount,
		MissingPayloadRecords.Num(),
		EffectDefinitionBackedCount,
		EffectClassOnlyCount,
		LoadFailureCount);

	return (MissingPayloadRecords.Num() == 0 && LoadFailureCount == 0) ? 0 : 1;
}
