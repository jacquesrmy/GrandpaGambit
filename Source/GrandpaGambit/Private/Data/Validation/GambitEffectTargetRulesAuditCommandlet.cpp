#include "Data/Validation/GambitEffectTargetRulesAuditCommandlet.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Data/Assets/GambitItemEffectDefinition.h"
#include "Data/Validation/GambitDataValidation.h"
#include "Dice/Data/GambitDiceDefinition.h"
#include "Items/Data/GambitItemDefinition.h"
#include "Items/Effects/GambitEffectTargetRules.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

namespace
{
	const TCHAR* GetEffectTargetRulesAuditDefaultReportPath()
	{
		return TEXT("Docs/Audits/EffectTargetRulesAudit.md");
	}

	const TCHAR* GetEffectTargetRulesAuditDefaultContentPath()
	{
		return TEXT("/Game");
	}

	FString EscapeEffectTargetRulesAuditMarkdownCell(FString Value)
	{
		Value.ReplaceInline(TEXT("|"), TEXT("\\|"));
		Value.ReplaceInline(TEXT("\r"), TEXT(" "));
		Value.ReplaceInline(TEXT("\n"), TEXT(" "));
		return Value;
	}

	FString FormatEffectTargetRulesAuditBool(const bool bValue)
	{
		return bValue ? TEXT("Yes") : TEXT("No");
	}

	FString FormatEffectTargetRulesAuditName(const FName Name)
	{
		return Name.IsNone() ? TEXT("(empty)") : Name.ToString();
	}

	FString GetEffectTargetRulesAuditAssetPath(const UObject* Asset, const FAssetData& AssetData)
	{
		if (Asset)
		{
			return Asset->GetPathName();
		}

		return AssetData.GetSoftObjectPath().ToString();
	}

	FString GetEffectTargetRulesAuditObjectPath(const UObject* Object)
	{
		return Object ? Object->GetPathName() : FString(TEXT("None"));
	}

	FString JoinEffectTargetRulesAuditSortedValues(const TSet<FString>& Values)
	{
		TArray<FString> SortedValues = Values.Array();
		SortedValues.Sort();
		return FString::Join(SortedValues, TEXT(", "));
	}

	FString GetTargetRuleStatus(const FName TargetRuleId, const bool bKnownRule)
	{
		if (TargetRuleId.IsNone())
		{
			return TEXT("Empty");
		}

		return bKnownRule ? TEXT("Known") : TEXT("Unknown");
	}

	struct FEffectTargetRuleAuditRecord
	{
		FString AssetPath;
		FString EffectId;
		FString EffectType;
		FString Hook;
		FString Target;
		FString TargetRuleId;
		FString Status;
		bool bKnownRule = false;
		bool bSelectedDieRule = false;
		bool bFirstRerolledDieRule = false;
		bool bOpponentRule = false;
		bool bRequiresSelectedDie = false;
		FString Description;
		TSet<FString> UsedBy;
	};

	void AddTargetRuleCount(TMap<FName, int32>& CountsByTargetRuleId, const FName TargetRuleId)
	{
		int32& Count = CountsByTargetRuleId.FindOrAdd(TargetRuleId);
		Count++;
	}

	void AppendAssetsByClass(
		IAssetRegistry& AssetRegistry,
		const FTopLevelAssetPath& ClassPath,
		const FString& ContentPath,
		TMap<FString, FAssetData>& OutAssetDataByPath)
	{
		FARFilter Filter;
		Filter.PackagePaths.Add(FName(*ContentPath));
		Filter.ClassPaths.Add(ClassPath);
		Filter.bRecursivePaths = true;
		Filter.bRecursiveClasses = true;

		TArray<FAssetData> ClassAssets;
		AssetRegistry.GetAssets(Filter, ClassAssets);
		for (const FAssetData& AssetData : ClassAssets)
		{
			OutAssetDataByPath.Add(AssetData.GetSoftObjectPath().ToString(), AssetData);
		}
	}

	FEffectTargetRuleAuditRecord MakeAuditRecord(const UGambitItemEffectDefinition* EffectDefinition)
	{
		FEffectTargetRuleAuditRecord Record;
		Record.AssetPath = GetEffectTargetRulesAuditObjectPath(EffectDefinition);
		Record.EffectId = FormatEffectTargetRulesAuditName(EffectDefinition ? EffectDefinition->EffectId : NAME_None);
		Record.EffectType = EffectDefinition
			? GambitDataValidation::EffectTypeToString(EffectDefinition->EffectType)
			: TEXT("None");
		Record.Hook = EffectDefinition
			? GambitDataValidation::EffectHookToString(EffectDefinition->Hook)
			: TEXT("None");
		Record.Target = EffectDefinition
			? GambitDataValidation::EffectTargetToString(EffectDefinition->Target)
			: TEXT("None");

		const FName TargetRuleId = EffectDefinition ? EffectDefinition->TargetRuleId : NAME_None;
		Record.bKnownRule = GambitEffectTargetRules::IsKnownRule(TargetRuleId);
		Record.bSelectedDieRule = GambitEffectTargetRules::IsSelectedDieRule(TargetRuleId);
		Record.bFirstRerolledDieRule = GambitEffectTargetRules::IsFirstRerolledDieRule(TargetRuleId);
		Record.bOpponentRule = GambitEffectTargetRules::IsOpponentRule(TargetRuleId);
		Record.bRequiresSelectedDie = GambitEffectTargetRules::RequiresSelectedDie(TargetRuleId);
		Record.TargetRuleId = FormatEffectTargetRulesAuditName(TargetRuleId);
		Record.Status = GetTargetRuleStatus(TargetRuleId, Record.bKnownRule);
		Record.Description = GambitEffectTargetRules::DescribeRule(TargetRuleId);
		return Record;
	}

	void AppendEffectRecord(
		const UGambitItemEffectDefinition* EffectDefinition,
		const FString& UsedBy,
		TArray<FEffectTargetRuleAuditRecord>& Records,
		TMap<FString, int32>& RecordIndexByEffectPath,
		TMap<FName, int32>& CountsByTargetRuleId,
		int32& MissingEffectReferenceCount)
	{
		if (!EffectDefinition)
		{
			MissingEffectReferenceCount++;
			return;
		}

		const FString EffectPath = GetEffectTargetRulesAuditObjectPath(EffectDefinition);
		if (int32* ExistingIndex = RecordIndexByEffectPath.Find(EffectPath))
		{
			Records[*ExistingIndex].UsedBy.Add(UsedBy);
			return;
		}

		FEffectTargetRuleAuditRecord Record = MakeAuditRecord(EffectDefinition);
		Record.UsedBy.Add(UsedBy);
		const int32 NewIndex = Records.Add(MoveTemp(Record));
		RecordIndexByEffectPath.Add(EffectPath, NewIndex);
		AddTargetRuleCount(CountsByTargetRuleId, EffectDefinition->TargetRuleId);
	}

	void AppendEffectDefinitionsFromOwner(
		const TArray<TObjectPtr<UGambitItemEffectDefinition>>& EffectDefinitions,
		const FString& OwnerPath,
		TArray<FEffectTargetRuleAuditRecord>& Records,
		TMap<FString, int32>& RecordIndexByEffectPath,
		TMap<FName, int32>& CountsByTargetRuleId,
		int32& MissingEffectReferenceCount)
	{
		for (const TObjectPtr<UGambitItemEffectDefinition>& EffectDefinition : EffectDefinitions)
		{
			AppendEffectRecord(
				EffectDefinition.Get(),
				OwnerPath,
				Records,
				RecordIndexByEffectPath,
				CountsByTargetRuleId,
				MissingEffectReferenceCount);
		}
	}

	void AppendTargetRuleUsageLines(
		const TMap<FName, int32>& CountsByTargetRuleId,
		TArray<FString>& Lines)
	{
		TArray<FName> TargetRuleIds;
		CountsByTargetRuleId.GenerateKeyArray(TargetRuleIds);
		TargetRuleIds.Sort([](const FName Left, const FName Right)
		{
			return FormatEffectTargetRulesAuditName(Left) < FormatEffectTargetRulesAuditName(Right);
		});

		Lines.Add(TEXT("| TargetRuleId | Count | Status | Description | SelectedDie | FirstRerolledDie | Opponent | RequiresSelectedDie |"));
		Lines.Add(TEXT("| --- | ---: | --- | --- | --- | --- | --- | --- |"));
		for (const FName TargetRuleId : TargetRuleIds)
		{
			const bool bKnownRule = GambitEffectTargetRules::IsKnownRule(TargetRuleId);
			Lines.Add(FString::Printf(
				TEXT("| `%s` | %d | %s | %s | %s | %s | %s | %s |"),
				*EscapeEffectTargetRulesAuditMarkdownCell(FormatEffectTargetRulesAuditName(TargetRuleId)),
				CountsByTargetRuleId[TargetRuleId],
				*GetTargetRuleStatus(TargetRuleId, bKnownRule),
				*EscapeEffectTargetRulesAuditMarkdownCell(GambitEffectTargetRules::DescribeRule(TargetRuleId)),
				*FormatEffectTargetRulesAuditBool(GambitEffectTargetRules::IsSelectedDieRule(TargetRuleId)),
				*FormatEffectTargetRulesAuditBool(GambitEffectTargetRules::IsFirstRerolledDieRule(TargetRuleId)),
				*FormatEffectTargetRulesAuditBool(GambitEffectTargetRules::IsOpponentRule(TargetRuleId)),
				*FormatEffectTargetRulesAuditBool(GambitEffectTargetRules::RequiresSelectedDie(TargetRuleId))));
		}
		Lines.Add(TEXT(""));
	}

	void AppendRecordTable(
		const TArray<FEffectTargetRuleAuditRecord>& Records,
		TArray<FString>& Lines)
	{
		if (Records.Num() == 0)
		{
			Lines.Add(TEXT("No EffectDefinitions found."));
			Lines.Add(TEXT(""));
			return;
		}

		Lines.Add(TEXT("| Effect Asset | EffectId | EffectType | Hook | Target | TargetRuleId | Status | RequiresSelectedDie | Opponent | SelectedDie | FirstRerolledDie | Description | Used By |"));
		Lines.Add(TEXT("| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |"));
		for (const FEffectTargetRuleAuditRecord& Record : Records)
		{
			Lines.Add(FString::Printf(
				TEXT("| `%s` | `%s` | %s | %s | %s | `%s` | %s | %s | %s | %s | %s | %s | `%s` |"),
				*EscapeEffectTargetRulesAuditMarkdownCell(Record.AssetPath),
				*EscapeEffectTargetRulesAuditMarkdownCell(Record.EffectId),
				*EscapeEffectTargetRulesAuditMarkdownCell(Record.EffectType),
				*EscapeEffectTargetRulesAuditMarkdownCell(Record.Hook),
				*EscapeEffectTargetRulesAuditMarkdownCell(Record.Target),
				*EscapeEffectTargetRulesAuditMarkdownCell(Record.TargetRuleId),
				*Record.Status,
				*FormatEffectTargetRulesAuditBool(Record.bRequiresSelectedDie),
				*FormatEffectTargetRulesAuditBool(Record.bOpponentRule),
				*FormatEffectTargetRulesAuditBool(Record.bSelectedDieRule),
				*FormatEffectTargetRulesAuditBool(Record.bFirstRerolledDieRule),
				*EscapeEffectTargetRulesAuditMarkdownCell(Record.Description),
				*EscapeEffectTargetRulesAuditMarkdownCell(JoinEffectTargetRulesAuditSortedValues(Record.UsedBy))));
		}
		Lines.Add(TEXT(""));
	}
}

UGambitEffectTargetRulesAuditCommandlet::UGambitEffectTargetRulesAuditCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UGambitEffectTargetRulesAuditCommandlet::Main(const FString& Params)
{
	FString ReportPath;
	FParse::Value(*Params, TEXT("ReportPath="), ReportPath);
	if (ReportPath.IsEmpty())
	{
		ReportPath = FPaths::Combine(FPaths::ProjectDir(), GetEffectTargetRulesAuditDefaultReportPath());
	}
	else if (FPaths::IsRelative(ReportPath))
	{
		ReportPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), ReportPath);
	}

	FString ContentPath = GetEffectTargetRulesAuditDefaultContentPath();
	FParse::Value(*Params, TEXT("ContentPath="), ContentPath);
	if (!ContentPath.StartsWith(TEXT("/")))
	{
		ContentPath = FString::Printf(TEXT("/%s"), *ContentPath);
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	AssetRegistry.SearchAllAssets(true);

	TMap<FString, FAssetData> AssetDataByPath;
	AppendAssetsByClass(
		AssetRegistry,
		UGambitItemEffectDefinition::StaticClass()->GetClassPathName(),
		ContentPath,
		AssetDataByPath);
	AppendAssetsByClass(
		AssetRegistry,
		UGambitItemDefinition::StaticClass()->GetClassPathName(),
		ContentPath,
		AssetDataByPath);
	AppendAssetsByClass(
		AssetRegistry,
		UGambitDiceDefinition::StaticClass()->GetClassPathName(),
		ContentPath,
		AssetDataByPath);

	TArray<FAssetData> AssetDataList;
	AssetDataByPath.GenerateValueArray(AssetDataList);
	AssetDataList.Sort([](const FAssetData& Left, const FAssetData& Right)
	{
		return Left.GetSoftObjectPath().ToString() < Right.GetSoftObjectPath().ToString();
	});

	TArray<FEffectTargetRuleAuditRecord> Records;
	TMap<FString, int32> RecordIndexByEffectPath;
	TMap<FName, int32> CountsByTargetRuleId;
	int32 StandaloneEffectAssetCount = 0;
	int32 OwnerAssetCount = 0;
	int32 MissingEffectReferenceCount = 0;
	int32 LoadFailureCount = 0;

	for (const FAssetData& AssetData : AssetDataList)
	{
		UObject* Asset = AssetData.GetAsset();
		if (!Asset)
		{
			LoadFailureCount++;
			continue;
		}

		const FString AssetPath = GetEffectTargetRulesAuditAssetPath(Asset, AssetData);
		if (const UGambitItemEffectDefinition* EffectDefinition = Cast<UGambitItemEffectDefinition>(Asset))
		{
			StandaloneEffectAssetCount++;
			AppendEffectRecord(
				EffectDefinition,
				TEXT("Standalone asset"),
				Records,
				RecordIndexByEffectPath,
				CountsByTargetRuleId,
				MissingEffectReferenceCount);
		}

		if (const UGambitItemDefinition* ItemDefinition = Cast<UGambitItemDefinition>(Asset))
		{
			OwnerAssetCount++;
			AppendEffectDefinitionsFromOwner(
				ItemDefinition->EffectDefinitions,
				AssetPath,
				Records,
				RecordIndexByEffectPath,
				CountsByTargetRuleId,
				MissingEffectReferenceCount);
		}

		if (const UGambitDiceDefinition* DiceDefinition = Cast<UGambitDiceDefinition>(Asset))
		{
			OwnerAssetCount++;
			AppendEffectDefinitionsFromOwner(
				DiceDefinition->EffectDefinitions,
				AssetPath,
				Records,
				RecordIndexByEffectPath,
				CountsByTargetRuleId,
				MissingEffectReferenceCount);
		}
	}

	Records.Sort([](const FEffectTargetRuleAuditRecord& Left, const FEffectTargetRuleAuditRecord& Right)
	{
		return Left.AssetPath < Right.AssetPath;
	});

	int32 EmptyRuleCount = 0;
	int32 KnownRuleCount = 0;
	int32 UnknownRuleCount = 0;
	TArray<FEffectTargetRuleAuditRecord> UnknownRecords;
	for (const FEffectTargetRuleAuditRecord& Record : Records)
	{
		if (Record.TargetRuleId == TEXT("(empty)"))
		{
			EmptyRuleCount++;
		}
		else if (Record.bKnownRule)
		{
			KnownRuleCount++;
		}
		else
		{
			UnknownRuleCount++;
			UnknownRecords.Add(Record);
		}
	}

	const bool bBlocked = UnknownRuleCount > 0 || MissingEffectReferenceCount > 0 || LoadFailureCount > 0;

	TArray<FString> Lines;
	Lines.Add(TEXT("# Effect Target Rules Audit"));
	Lines.Add(TEXT(""));
	Lines.Add(TEXT("Generated by `GambitEffectTargetRulesAuditCommandlet`. This report is read-only and does not modify assets."));
	Lines.Add(TEXT(""));
	Lines.Add(TEXT("## Summary"));
	Lines.Add(TEXT(""));
	Lines.Add(FString::Printf(TEXT("- Content path: `%s`"), *ContentPath));
	Lines.Add(FString::Printf(TEXT("- Total EffectDefinitions audited: %d"), Records.Num()));
	Lines.Add(FString::Printf(TEXT("- Standalone effect assets found: %d"), StandaloneEffectAssetCount));
	Lines.Add(FString::Printf(TEXT("- Owner assets inspected: %d"), OwnerAssetCount));
	Lines.Add(FString::Printf(TEXT("- Empty TargetRuleId count: %d"), EmptyRuleCount));
	Lines.Add(FString::Printf(TEXT("- Known non-empty TargetRuleId count: %d"), KnownRuleCount));
	Lines.Add(FString::Printf(TEXT("- Unknown TargetRuleId count: %d"), UnknownRuleCount));
	Lines.Add(FString::Printf(TEXT("- Missing effect references: %d"), MissingEffectReferenceCount));
	Lines.Add(FString::Printf(TEXT("- Asset load failures: %d"), LoadFailureCount));
	Lines.Add(FString::Printf(
		TEXT("- Recommendation: %s"),
		bBlocked
			? TEXT("Blocked by assets to correct before implementing the target resolver.")
			: TEXT("Ready for the target resolver pass; all audited TargetRuleId values are empty or known.")));
	Lines.Add(TEXT(""));

	Lines.Add(TEXT("## TargetRuleId Usage"));
	Lines.Add(TEXT(""));
	AppendTargetRuleUsageLines(CountsByTargetRuleId, Lines);

	Lines.Add(TEXT("## Unknown TargetRuleIds"));
	Lines.Add(TEXT(""));
	if (UnknownRecords.Num() == 0)
	{
		Lines.Add(TEXT("None found."));
		Lines.Add(TEXT(""));
	}
	else
	{
		AppendRecordTable(UnknownRecords, Lines);
	}

	Lines.Add(TEXT("## Audited EffectDefinitions"));
	Lines.Add(TEXT(""));
	AppendRecordTable(Records, Lines);

	const FString Report = FString::Join(Lines, TEXT("\n"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
	if (!FFileHelper::SaveStringToFile(Report, *ReportPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to write effect target rules audit report to %s"), *ReportPath);
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("Wrote effect target rules audit report to %s"), *ReportPath);
	UE_LOG(
		LogTemp,
		Display,
		TEXT("EffectTargetRules audit summary: Effects=%d Empty=%d Known=%d Unknown=%d MissingRefs=%d LoadFailures=%d"),
		Records.Num(),
		EmptyRuleCount,
		KnownRuleCount,
		UnknownRuleCount,
		MissingEffectReferenceCount,
		LoadFailureCount);

	return bBlocked ? 1 : 0;
}
