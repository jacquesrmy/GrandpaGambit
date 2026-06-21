#include "Data/Validation/GambitGameplayDataAuditCommandlet.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "Data/Assets/GambitItemCatalogDataAsset.h"
#include "Data/Assets/GambitItemEffectDefinition.h"
#include "Data/Assets/GambitSharedPoolDefinition.h"
#include "Data/Validation/GambitDataValidation.h"
#include "Dice/Data/GambitDiceDefinition.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Items/Consumables/GambitConsumableDefinition.h"
#include "Items/Data/GambitItemDefinition.h"
#include "Items/DiceItems/GambitDiceItemDefinition.h"
#include "Items/Effects/GambitEffectExecutionTypes.h"
#include "Items/Effects/GambitItemEffect.h"
#include "Items/Modules/GambitModuleDefinition.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Shop/Data/GambitShopLootTable.h"

namespace
{
	const TCHAR* DefaultContentPath()
	{
		return TEXT("/Game/GrandpaGambit/Data");
	}

	const TCHAR* DefaultJsonPath()
	{
		return TEXT("Saved/Automation/GambitDataAssetsAudit.json");
	}

	const TCHAR* DefaultCsvPath()
	{
		return TEXT("Saved/Automation/GambitDataAssetsAudit.csv");
	}

	const TCHAR* DefaultMatrixPath()
	{
		return TEXT("Docs/ObjectCreation/ObjectMatrix.csv");
	}

	struct FGameplayDataAuditRecord
	{
		FString AssetType;
		FString AssetPath;
		FString ObjectName;
		FString DisplayName;
		FString StableId;
		FString Type;
		FString Rarity;
		FString Cost;
		FString EffectHook;
		FString EffectType;
		FString TargetRuleId;
		FString NegativeCategories;
		FString ValidationSeverity = TEXT("Valid");
		TArray<FString> ValidationMessages;
		TArray<FString> References;
		FString MatrixProgress;
		FString MatrixType;
		FString MatrixStatus = TEXT("NotCompared");
		TArray<FString> MatrixMessages;
		TSharedPtr<FJsonObject> Details = MakeShared<FJsonObject>();
	};

	struct FObjectMatrixRecord
	{
		int32 LineNumber = 0;
		FString Progress;
		FString CreationClass;
		FString Id;
		FString DisplayName;
		FString Type;
		FString Rarity;
		FString Cost;
	};

	struct FMatrixComparisonIssue
	{
		FString Severity;
		FString IssueType;
		FString StableId;
		FString MatrixType;
		FString AssetType;
		FString MatrixProgress;
		FString Message;
		int32 LineNumber = 0;
	};

	struct FMatrixComparisonResult
	{
		FString Status = TEXT("Skipped");
		FString MatrixPath;
		FString Message;
		int32 RowsRead = 0;
		int32 RowsCompared = 0;
		int32 MatchedAssets = 0;
		int32 PlannedMissing = 0;
		int32 ActualMissingFromMatrix = 0;
		int32 TypeMismatches = 0;
		int32 StatusMismatches = 0;
		int32 DuplicateActualIds = 0;
		TArray<FMatrixComparisonIssue> Issues;
	};

	FString ParsePathParam(const FString& Params, const TCHAR* ParamName, const TCHAR* DefaultRelativePath)
	{
		FString Path;
		FParse::Value(*Params, ParamName, Path);
		if (Path.IsEmpty())
		{
			Path = FPaths::Combine(FPaths::ProjectDir(), DefaultRelativePath);
		}
		else if (FPaths::IsRelative(Path))
		{
			Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), Path);
		}
		return Path;
	}

	FString NormalizeContentPath(FString ContentPath)
	{
		if (ContentPath.IsEmpty())
		{
			ContentPath = DefaultContentPath();
		}

		if (!ContentPath.StartsWith(TEXT("/")))
		{
			ContentPath = FString::Printf(TEXT("/%s"), *ContentPath);
		}

		return ContentPath;
	}

	template <typename EnumType>
	FString EnumToString(const EnumType Value)
	{
		const UEnum* Enum = StaticEnum<EnumType>();
		if (!Enum)
		{
			return TEXT("Unknown");
		}

		const FString Name = Enum->GetNameStringByValue(static_cast<int64>(Value));
		return Name.IsEmpty() ? TEXT("Unknown") : Name;
	}

	FString NameToString(const FName Name)
	{
		return Name.IsNone() ? FString() : Name.ToString();
	}

	FString TextToString(const FText& Text)
	{
		return Text.IsEmpty() ? FString() : Text.ToString();
	}

	FString ObjectPath(const UObject* Object)
	{
		return Object ? Object->GetPathName() : FString();
	}

	FString ObjectPathFromAssetData(const FAssetData& AssetData)
	{
		return AssetData.GetSoftObjectPath().ToString();
	}

	void AddUniqueString(TArray<FString>& Values, const FString& Value)
	{
		if (!Value.IsEmpty())
		{
			Values.AddUnique(Value);
		}
	}

	FString JoinStrings(TArray<FString> Values, const TCHAR* Separator = TEXT("; "))
	{
		Values.RemoveAll([](const FString& Value)
		{
			return Value.IsEmpty();
		});
		Values.Sort();
		return FString::Join(Values, Separator);
	}

	TArray<TSharedPtr<FJsonValue>> StringArrayToJsonValues(TArray<FString> Values)
	{
		Values.RemoveAll([](const FString& Value)
		{
			return Value.IsEmpty();
		});
		Values.Sort();

		TArray<TSharedPtr<FJsonValue>> JsonValues;
		for (const FString& Value : Values)
		{
			JsonValues.Add(MakeShared<FJsonValueString>(Value));
		}
		return JsonValues;
	}

	TArray<TSharedPtr<FJsonValue>> IntArrayToJsonValues(const TArray<int32>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> JsonValues;
		for (const int32 Value : Values)
		{
			JsonValues.Add(MakeShared<FJsonValueNumber>(Value));
		}
		return JsonValues;
	}

	TArray<TSharedPtr<FJsonValue>> NameArrayToJsonValues(const TArray<FName>& Values)
	{
		TArray<FString> Strings;
		for (const FName Value : Values)
		{
			AddUniqueString(Strings, NameToString(Value));
		}
		return StringArrayToJsonValues(Strings);
	}

	FString CategoriesToString(const TArray<EGambitNegativeEffectCategory>& Categories)
	{
		TArray<FString> Values;
		for (const EGambitNegativeEffectCategory Category : Categories)
		{
			AddUniqueString(Values, EnumToString(Category));
		}
		return JoinStrings(Values);
	}

	TArray<FString> CategoriesToStrings(const TArray<EGambitNegativeEffectCategory>& Categories)
	{
		TArray<FString> Values;
		for (const EGambitNegativeEffectCategory Category : Categories)
		{
			AddUniqueString(Values, EnumToString(Category));
		}
		return Values;
	}

	FString IssuesToSeverity(const TArray<FGambitDataValidationIssue>& Issues)
	{
		bool bHasError = false;
		bool bHasWarning = false;
		for (const FGambitDataValidationIssue& Issue : Issues)
		{
			bHasError |= Issue.Severity == EGambitDataValidationSeverity::Error;
			bHasWarning |= Issue.Severity == EGambitDataValidationSeverity::Warning;
		}

		if (bHasError)
		{
			return TEXT("Error");
		}

		return bHasWarning ? TEXT("Warning") : TEXT("Valid");
	}

	FString SeverityToJsonStatus(const FString& Severity)
	{
		if (Severity == TEXT("Error"))
		{
			return TEXT("error");
		}
		if (Severity == TEXT("Warning"))
		{
			return TEXT("warning");
		}
		return TEXT("valid");
	}

	TArray<FString> FormatIssues(const TArray<FGambitDataValidationIssue>& Issues)
	{
		TArray<FString> Messages;
		for (const FGambitDataValidationIssue& Issue : Issues)
		{
			Messages.Add(FString::Printf(
				TEXT("%s: %s"),
				*GambitDataValidation::SeverityToString(Issue.Severity),
				*Issue.Message));
		}
		return Messages;
	}

	void ApplyValidation(FGameplayDataAuditRecord& Record, const TArray<FGambitDataValidationIssue>& Issues)
	{
		Record.ValidationSeverity = IssuesToSeverity(Issues);
		Record.ValidationMessages = FormatIssues(Issues);
	}

	void AppendAssetsByClass(
		IAssetRegistry& AssetRegistry,
		const UClass* AssetClass,
		const FString& ContentPath,
		TArray<FAssetData>& OutAssets)
	{
		if (!AssetClass)
		{
			return;
		}

		FARFilter Filter;
		Filter.PackagePaths.Add(FName(*ContentPath));
		Filter.ClassPaths.Add(AssetClass->GetClassPathName());
		Filter.bRecursivePaths = true;
		Filter.bRecursiveClasses = true;

		TArray<FAssetData> ClassAssets;
		AssetRegistry.GetAssets(Filter, ClassAssets);
		OutAssets.Append(ClassAssets);
	}

	void SortAssets(TArray<FAssetData>& Assets)
	{
		Assets.Sort([](const FAssetData& Left, const FAssetData& Right)
		{
			return Left.GetSoftObjectPath().ToString() < Right.GetSoftObjectPath().ToString();
		});
	}

	void AppendEffectSummary(
		const TArray<TObjectPtr<UGambitItemEffectDefinition>>& EffectDefinitions,
		const TArray<TSubclassOf<UGambitItemEffect>>& EffectClasses,
		FGameplayDataAuditRecord& Record)
	{
		TArray<FString> Hooks;
		TArray<FString> EffectTypes;
		TArray<FString> TargetRuleIds;
		TArray<FString> NegativeCategories;

		for (const TObjectPtr<UGambitItemEffectDefinition>& EffectDefinitionPtr : EffectDefinitions)
		{
			const UGambitItemEffectDefinition* EffectDefinition = EffectDefinitionPtr.Get();
			if (!EffectDefinition)
			{
				AddUniqueString(Record.References, TEXT("MissingEffectDefinition"));
				continue;
			}

			AddUniqueString(Hooks, EnumToString(EffectDefinition->Hook));
			AddUniqueString(EffectTypes, EnumToString(EffectDefinition->EffectType));
			AddUniqueString(TargetRuleIds, NameToString(EffectDefinition->TargetRuleId));
			for (const FString& Category : CategoriesToStrings(EffectDefinition->NegativeEffectCategories))
			{
				AddUniqueString(NegativeCategories, Category);
			}
			AddUniqueString(Record.References, FString::Printf(TEXT("EffectDefinition=%s"), *ObjectPath(EffectDefinition)));
		}

		for (const TSubclassOf<UGambitItemEffect>& EffectClass : EffectClasses)
		{
			if (const UClass* Class = EffectClass.Get())
			{
				AddUniqueString(EffectTypes, TEXT("EffectClass"));
				AddUniqueString(Record.References, FString::Printf(TEXT("EffectClass=%s"), *Class->GetPathName()));
			}
		}

		Record.EffectHook = JoinStrings(Hooks);
		Record.EffectType = JoinStrings(EffectTypes);
		Record.TargetRuleId = JoinStrings(TargetRuleIds);
		Record.NegativeCategories = JoinStrings(NegativeCategories);
	}

	TSharedPtr<FJsonObject> MakeValidationJson(const FGameplayDataAuditRecord& Record)
	{
		TSharedPtr<FJsonObject> ValidationJson = MakeShared<FJsonObject>();
		ValidationJson->SetStringField(TEXT("status"), SeverityToJsonStatus(Record.ValidationSeverity));
		ValidationJson->SetStringField(TEXT("severity"), Record.ValidationSeverity);
		ValidationJson->SetArrayField(TEXT("messages"), StringArrayToJsonValues(Record.ValidationMessages));
		return ValidationJson;
	}

	TSharedPtr<FJsonObject> MakeBaseAssetJson(const FGameplayDataAuditRecord& Record)
	{
		TSharedPtr<FJsonObject> AssetJson = MakeShared<FJsonObject>();
		AssetJson->SetStringField(TEXT("assetType"), Record.AssetType);
		AssetJson->SetStringField(TEXT("assetPath"), Record.AssetPath);
		AssetJson->SetStringField(TEXT("objectName"), Record.ObjectName);
		AssetJson->SetStringField(TEXT("displayName"), Record.DisplayName);
		AssetJson->SetStringField(TEXT("stableId"), Record.StableId);
		AssetJson->SetStringField(TEXT("type"), Record.Type);
		AssetJson->SetStringField(TEXT("rarity"), Record.Rarity);
		if (!Record.Cost.IsEmpty())
		{
			AssetJson->SetNumberField(TEXT("cost"), FCString::Atoi(*Record.Cost));
		}
		AssetJson->SetStringField(TEXT("effectHook"), Record.EffectHook);
		AssetJson->SetStringField(TEXT("effectType"), Record.EffectType);
		AssetJson->SetStringField(TEXT("targetRuleId"), Record.TargetRuleId);
		AssetJson->SetStringField(TEXT("negativeCategories"), Record.NegativeCategories);
		AssetJson->SetObjectField(TEXT("validation"), MakeValidationJson(Record));
		AssetJson->SetArrayField(TEXT("references"), StringArrayToJsonValues(Record.References));
		if (Record.Details.IsValid())
		{
			AssetJson->SetObjectField(TEXT("details"), Record.Details);
		}
		return AssetJson;
	}

	FGameplayDataAuditRecord MakeLoadFailureRecord(const FString& AssetType, const FAssetData& AssetData)
	{
		FGameplayDataAuditRecord Record;
		Record.AssetType = AssetType;
		Record.AssetPath = ObjectPathFromAssetData(AssetData);
		Record.ObjectName = AssetData.AssetName.ToString();
		Record.ValidationSeverity = TEXT("Error");
		Record.ValidationMessages.Add(TEXT("Error: Asset failed to load."));
		return Record;
	}

	FGameplayDataAuditRecord MakeDiceRecord(const UGambitDiceDefinition* DiceDefinition, const FAssetData& AssetData)
	{
		FGameplayDataAuditRecord Record;
		Record.AssetType = TEXT("DiceDefinition");
		Record.AssetPath = ObjectPath(DiceDefinition);
		Record.ObjectName = DiceDefinition ? DiceDefinition->GetName() : AssetData.AssetName.ToString();
		if (!DiceDefinition)
		{
			return MakeLoadFailureRecord(Record.AssetType, AssetData);
		}

		Record.DisplayName = TextToString(DiceDefinition->DisplayName);
		Record.StableId = NameToString(DiceDefinition->GetResolvedDiceId());
		Record.Type = EnumToString(DiceDefinition->DiceType);
		Record.Rarity = EnumToString(DiceDefinition->Rarity);
		AppendEffectSummary(DiceDefinition->EffectDefinitions, DiceDefinition->EffectClasses, Record);

		Record.Details->SetStringField(TEXT("diceId"), NameToString(DiceDefinition->DiceId));
		Record.Details->SetStringField(TEXT("diceTypeId"), NameToString(DiceDefinition->DiceTypeId));
		Record.Details->SetStringField(TEXT("rarityId"), NameToString(DiceDefinition->RarityId));
		Record.Details->SetArrayField(TEXT("tags"), NameArrayToJsonValues(DiceDefinition->Tags));
		Record.Details->SetArrayField(TEXT("faces"), IntArrayToJsonValues(DiceDefinition->Faces));
		Record.Details->SetBoolField(TEXT("allowDefaultFacesFallback"), DiceDefinition->bAllowDefaultFacesFallback);
		Record.Details->SetNumberField(TEXT("defaultComboContributionCount"), DiceDefinition->DefaultComboContributionCount);
		Record.Details->SetBoolField(TEXT("countsForScoreSum"), DiceDefinition->bCountsForScoreSum);
		Record.Details->SetBoolField(TEXT("countsForCombinations"), DiceDefinition->bCountsForCombinations);
		Record.Details->SetBoolField(TEXT("canBeRerolled"), DiceDefinition->bCanBeRerolled);
		Record.Details->SetBoolField(TEXT("canBeLocked"), DiceDefinition->bCanBeLocked);
		Record.Details->SetBoolField(TEXT("destroyedAfterRound"), DiceDefinition->bDestroyedAfterRound);
		Record.Details->SetBoolField(TEXT("overrideScoreContributionValue"), DiceDefinition->bOverrideScoreContributionValue);
		Record.Details->SetNumberField(TEXT("scoreContributionValueOverride"), DiceDefinition->ScoreContributionValueOverride);

		TArray<FGambitDataValidationIssue> Issues;
		GambitDataValidation::ValidateDiceDefinition(DiceDefinition, Issues);
		ApplyValidation(Record, Issues);
		return Record;
	}

	FGameplayDataAuditRecord MakeItemRecord(const UGambitItemDefinition* ItemDefinition, const FAssetData& AssetData, const FString& AssetType)
	{
		FGameplayDataAuditRecord Record;
		Record.AssetType = AssetType;
		Record.AssetPath = ObjectPath(ItemDefinition);
		Record.ObjectName = ItemDefinition ? ItemDefinition->GetName() : AssetData.AssetName.ToString();
		if (!ItemDefinition)
		{
			return MakeLoadFailureRecord(Record.AssetType, AssetData);
		}

		Record.DisplayName = TextToString(ItemDefinition->DisplayName);
		Record.StableId = NameToString(ItemDefinition->GetResolvedItemId());
		Record.Type = EnumToString(ItemDefinition->ItemType);
		Record.Rarity = EnumToString(ItemDefinition->Rarity);
		Record.Cost = FString::FromInt(ItemDefinition->Cost);
		AppendEffectSummary(ItemDefinition->EffectDefinitions, ItemDefinition->EffectClasses, Record);

		Record.Details->SetStringField(TEXT("itemId"), NameToString(ItemDefinition->ItemId));
		Record.Details->SetStringField(TEXT("itemTypeId"), NameToString(ItemDefinition->ItemTypeId));
		Record.Details->SetStringField(TEXT("rarityId"), NameToString(ItemDefinition->RarityId));
		Record.Details->SetArrayField(TEXT("tags"), NameArrayToJsonValues(ItemDefinition->Tags));
		Record.Details->SetBoolField(TEXT("usesSharedPool"), ItemDefinition->bUsesSharedPool);
		Record.Details->SetNumberField(TEXT("sharedPoolMaxStock"), ItemDefinition->SharedPoolMaxStock);
		Record.Details->SetNumberField(TEXT("sharedPoolPurchaseLimit"), ItemDefinition->SharedPoolPurchaseLimit);

		if (const UGambitDiceItemDefinition* DiceItemDefinition = Cast<UGambitDiceItemDefinition>(ItemDefinition))
		{
			const UGambitDiceDefinition* GrantedDiceDefinition = DiceItemDefinition->GrantedDiceDefinition.Get();
			Record.Details->SetStringField(TEXT("grantedDiceDefinition"), ObjectPath(GrantedDiceDefinition));
			Record.Details->SetStringField(TEXT("grantedDiceId"), GrantedDiceDefinition ? NameToString(GrantedDiceDefinition->GetResolvedDiceId()) : FString());
			if (GrantedDiceDefinition)
			{
				AddUniqueString(Record.References, FString::Printf(TEXT("GrantedDiceDefinition=%s"), *ObjectPath(GrantedDiceDefinition)));
			}
		}
		else if (const UGambitConsumableDefinition* ConsumableDefinition = Cast<UGambitConsumableDefinition>(ItemDefinition))
		{
			TArray<FString> UsablePhases;
			for (const EGambitRoundPhase Phase : ConsumableDefinition->UsablePhases)
			{
				AddUniqueString(UsablePhases, EnumToString(Phase));
			}
			Record.Details->SetArrayField(TEXT("usablePhases"), StringArrayToJsonValues(UsablePhases));
			Record.Details->SetBoolField(TEXT("canTargetOpponent"), ConsumableDefinition->bCanTargetOpponent);
		}

		TArray<FGambitDataValidationIssue> Issues;
		GambitDataValidation::ValidateItemDefinition(ItemDefinition, Issues);
		ApplyValidation(Record, Issues);
		return Record;
	}

	FGameplayDataAuditRecord MakeEffectRecord(const UGambitItemEffectDefinition* EffectDefinition, const FAssetData& AssetData)
	{
		FGameplayDataAuditRecord Record;
		Record.AssetType = TEXT("ItemEffectDefinition");
		Record.AssetPath = ObjectPath(EffectDefinition);
		Record.ObjectName = EffectDefinition ? EffectDefinition->GetName() : AssetData.AssetName.ToString();
		if (!EffectDefinition)
		{
			return MakeLoadFailureRecord(Record.AssetType, AssetData);
		}

		Record.StableId = NameToString(EffectDefinition->EffectId);
		Record.Type = EnumToString(EffectDefinition->Target);
		Record.EffectHook = EnumToString(EffectDefinition->Hook);
		Record.EffectType = EnumToString(EffectDefinition->EffectType);
		Record.TargetRuleId = NameToString(EffectDefinition->TargetRuleId);
		Record.NegativeCategories = CategoriesToString(EffectDefinition->NegativeEffectCategories);

		Record.Details->SetStringField(TEXT("effectId"), NameToString(EffectDefinition->EffectId));
		Record.Details->SetStringField(TEXT("effectTypeId"), NameToString(EffectDefinition->EffectTypeId));
		Record.Details->SetStringField(TEXT("hook"), Record.EffectHook);
		Record.Details->SetStringField(TEXT("effectType"), Record.EffectType);
		Record.Details->SetStringField(TEXT("target"), Record.Type);
		Record.Details->SetStringField(TEXT("targetRuleId"), Record.TargetRuleId);
		Record.Details->SetNumberField(TEXT("amount"), EffectDefinition->Amount);
		Record.Details->SetNumberField(TEXT("multiplier"), EffectDefinition->Multiplier);
		Record.Details->SetNumberField(TEXT("durationRounds"), EffectDefinition->DurationRounds);
		Record.Details->SetBoolField(TEXT("negativeEffect"), EffectDefinition->bNegativeEffect);
		Record.Details->SetArrayField(TEXT("negativeEffectCategories"), StringArrayToJsonValues(CategoriesToStrings(EffectDefinition->NegativeEffectCategories)));
		Record.Details->SetArrayField(TEXT("preventedNegativeEffectCategories"), StringArrayToJsonValues(CategoriesToStrings(EffectDefinition->PreventedNegativeEffectCategories)));
		Record.Details->SetNumberField(TEXT("preventNegativeEffectBlockCount"), EffectDefinition->PreventNegativeEffectBlockCount);
		Record.Details->SetStringField(TEXT("transformDiceDefinition"), ObjectPath(EffectDefinition->TransformDiceDefinition.Get()));
		Record.Details->SetStringField(TEXT("grantedConsumableDefinition"), ObjectPath(EffectDefinition->GrantedConsumableDefinition.Get()));
		Record.Details->SetStringField(TEXT("referencedItemDefinition"), ObjectPath(EffectDefinition->ReferencedItemDefinition.Get()));

		if (EffectDefinition->TransformDiceDefinition)
		{
			AddUniqueString(Record.References, FString::Printf(TEXT("TransformDiceDefinition=%s"), *ObjectPath(EffectDefinition->TransformDiceDefinition.Get())));
		}
		if (EffectDefinition->GrantedConsumableDefinition)
		{
			AddUniqueString(Record.References, FString::Printf(TEXT("GrantedConsumableDefinition=%s"), *ObjectPath(EffectDefinition->GrantedConsumableDefinition.Get())));
		}
		if (EffectDefinition->ReferencedItemDefinition)
		{
			AddUniqueString(Record.References, FString::Printf(TEXT("ReferencedItemDefinition=%s"), *ObjectPath(EffectDefinition->ReferencedItemDefinition.Get())));
		}

		TArray<FString> ScalarParameterStrings;
		for (const TPair<FName, float>& ScalarParameter : EffectDefinition->ScalarParameters)
		{
			ScalarParameterStrings.Add(FString::Printf(TEXT("%s=%f"), *NameToString(ScalarParameter.Key), ScalarParameter.Value));
		}
		Record.Details->SetArrayField(TEXT("scalarParameters"), StringArrayToJsonValues(ScalarParameterStrings));

		TArray<FGambitDataValidationIssue> Issues;
		GambitDataValidation::ValidateEffectDefinition(EffectDefinition, TEXT("EffectAsset"), 0, Issues);
		ApplyValidation(Record, Issues);
		return Record;
	}

	FGameplayDataAuditRecord MakeShopLootTableRecord(
		const UGambitShopLootTable* LootTable,
		const UGambitItemCatalogDataAsset* FallbackCatalog,
		const FAssetData& AssetData)
	{
		FGameplayDataAuditRecord Record;
		Record.AssetType = TEXT("ShopLootTable");
		Record.AssetPath = ObjectPath(LootTable);
		Record.ObjectName = LootTable ? LootTable->GetName() : AssetData.AssetName.ToString();
		if (!LootTable)
		{
			return MakeLoadFailureRecord(Record.AssetType, AssetData);
		}

		Record.StableId = NameToString(LootTable->LootTableId);
		Record.Type = TEXT("ShopLootTable");
		Record.Details->SetStringField(TEXT("lootTableId"), NameToString(LootTable->LootTableId));
		Record.Details->SetStringField(TEXT("itemCatalog"), ObjectPath(LootTable->ItemCatalog.Get()));
		Record.Details->SetNumberField(TEXT("offerCountOverride"), LootTable->OfferCountOverride);
		if (LootTable->ItemCatalog)
		{
			AddUniqueString(Record.References, FString::Printf(TEXT("ItemCatalog=%s"), *ObjectPath(LootTable->ItemCatalog.Get())));
		}

		TArray<TSharedPtr<FJsonValue>> EntriesJson;
		TArray<FString> EntryReferences;
		for (int32 EntryIndex = 0; EntryIndex < LootTable->WeightedItems.Num(); ++EntryIndex)
		{
			const FGambitShopWeightedEntry& Entry = LootTable->WeightedItems[EntryIndex];
			TSharedPtr<FJsonObject> EntryJson = MakeShared<FJsonObject>();
			EntryJson->SetNumberField(TEXT("index"), EntryIndex);
			EntryJson->SetStringField(TEXT("itemId"), NameToString(Entry.ItemId));
			EntryJson->SetStringField(TEXT("itemDefinition"), ObjectPath(Entry.ItemDefinition.Get()));
			EntryJson->SetNumberField(TEXT("weight"), Entry.Weight);
			EntryJson->SetNumberField(TEXT("priceOverride"), Entry.PriceOverride);
			EntriesJson.Add(MakeShared<FJsonValueObject>(EntryJson));

			if (!Entry.ItemId.IsNone())
			{
				AddUniqueString(EntryReferences, FString::Printf(TEXT("EntryItemId=%s"), *Entry.ItemId.ToString()));
			}
			if (Entry.ItemDefinition)
			{
				AddUniqueString(EntryReferences, FString::Printf(TEXT("EntryItemDefinition=%s"), *ObjectPath(Entry.ItemDefinition.Get())));
			}
		}
		Record.Details->SetArrayField(TEXT("lootTableEntries"), EntriesJson);
		for (const FString& EntryReference : EntryReferences)
		{
			AddUniqueString(Record.References, EntryReference);
		}

		TArray<FGambitDataValidationIssue> Issues;
		GambitDataValidation::ValidateShopLootTable(LootTable, FallbackCatalog, Issues);
		ApplyValidation(Record, Issues);
		return Record;
	}

	FGameplayDataAuditRecord MakeSharedPoolRecord(
		const UGambitSharedPoolDefinition* SharedPoolDefinition,
		const UGambitItemCatalogDataAsset* FallbackCatalog,
		const FAssetData& AssetData)
	{
		FGameplayDataAuditRecord Record;
		Record.AssetType = TEXT("SharedPoolDefinition");
		Record.AssetPath = ObjectPath(SharedPoolDefinition);
		Record.ObjectName = SharedPoolDefinition ? SharedPoolDefinition->GetName() : AssetData.AssetName.ToString();
		if (!SharedPoolDefinition)
		{
			return MakeLoadFailureRecord(Record.AssetType, AssetData);
		}

		Record.StableId = NameToString(SharedPoolDefinition->SharedPoolId);
		Record.Type = TEXT("SharedPoolDefinition");
		Record.Details->SetStringField(TEXT("sharedPoolId"), NameToString(SharedPoolDefinition->SharedPoolId));
		Record.Details->SetStringField(TEXT("itemCatalog"), ObjectPath(SharedPoolDefinition->ItemCatalog.Get()));
		if (SharedPoolDefinition->ItemCatalog)
		{
			AddUniqueString(Record.References, FString::Printf(TEXT("ItemCatalog=%s"), *ObjectPath(SharedPoolDefinition->ItemCatalog.Get())));
		}

		TArray<TSharedPtr<FJsonValue>> StockEntriesJson;
		TArray<FString> EntryReferences;
		for (int32 EntryIndex = 0; EntryIndex < SharedPoolDefinition->StockEntries.Num(); ++EntryIndex)
		{
			const FGambitSharedStockEntry& Entry = SharedPoolDefinition->StockEntries[EntryIndex];
			TSharedPtr<FJsonObject> EntryJson = MakeShared<FJsonObject>();
			EntryJson->SetNumberField(TEXT("index"), EntryIndex);
			EntryJson->SetStringField(TEXT("itemId"), NameToString(Entry.ItemId));
			EntryJson->SetStringField(TEXT("itemDefinition"), ObjectPath(Entry.ItemDefinition.Get()));
			EntryJson->SetNumberField(TEXT("maxStock"), Entry.MaxStock);
			EntryJson->SetNumberField(TEXT("globalPurchaseLimit"), Entry.GlobalPurchaseLimit);
			StockEntriesJson.Add(MakeShared<FJsonValueObject>(EntryJson));

			if (!Entry.ItemId.IsNone())
			{
				AddUniqueString(EntryReferences, FString::Printf(TEXT("StockItemId=%s"), *Entry.ItemId.ToString()));
			}
			if (Entry.ItemDefinition)
			{
				AddUniqueString(EntryReferences, FString::Printf(TEXT("StockItemDefinition=%s"), *ObjectPath(Entry.ItemDefinition.Get())));
			}
		}
		Record.Details->SetArrayField(TEXT("stockEntries"), StockEntriesJson);
		for (const FString& EntryReference : EntryReferences)
		{
			AddUniqueString(Record.References, EntryReference);
		}

		TArray<FGambitDataValidationIssue> Issues;
		GambitDataValidation::ValidateSharedPoolDefinition(SharedPoolDefinition, FallbackCatalog, Issues);
		ApplyValidation(Record, Issues);
		return Record;
	}

	UGambitItemCatalogDataAsset* FindFallbackItemCatalog(IAssetRegistry& AssetRegistry, const FString& ContentPath)
	{
		TArray<FAssetData> CatalogAssets;
		AppendAssetsByClass(AssetRegistry, UGambitItemCatalogDataAsset::StaticClass(), ContentPath, CatalogAssets);
		SortAssets(CatalogAssets);
		for (const FAssetData& CatalogAsset : CatalogAssets)
		{
			if (UGambitItemCatalogDataAsset* Catalog = Cast<UGambitItemCatalogDataAsset>(CatalogAsset.GetAsset()))
			{
				return Catalog;
			}
		}
		return nullptr;
	}

	void AppendRecordsForClass(
		IAssetRegistry& AssetRegistry,
		const UClass* AssetClass,
		const FString& ContentPath,
		TFunctionRef<FGameplayDataAuditRecord(const FAssetData&)> MakeRecord,
		TArray<FGameplayDataAuditRecord>& OutRecords)
	{
		TArray<FAssetData> Assets;
		AppendAssetsByClass(AssetRegistry, AssetClass, ContentPath, Assets);
		SortAssets(Assets);

		for (const FAssetData& Asset : Assets)
		{
			OutRecords.Add(MakeRecord(Asset));
		}
	}

	FString EscapeCsvCell(FString Value)
	{
		Value.ReplaceInline(TEXT("\""), TEXT("\"\""));
		const bool bNeedsQuotes = Value.Contains(TEXT(","))
			|| Value.Contains(TEXT("\""))
			|| Value.Contains(TEXT("\r"))
			|| Value.Contains(TEXT("\n"));
		if (bNeedsQuotes)
		{
			Value = FString::Printf(TEXT("\"%s\""), *Value);
		}
		return Value;
	}

	FString JoinCsvRow(const TArray<FString>& Cells)
	{
		TArray<FString> EscapedCells;
		for (const FString& Cell : Cells)
		{
			EscapedCells.Add(EscapeCsvCell(Cell));
		}
		return FString::Join(EscapedCells, TEXT(","));
	}

	bool WriteCsvReport(const FString& CsvPath, const TArray<FGameplayDataAuditRecord>& Records)
	{
		TArray<FString> Lines;
		Lines.Add(JoinCsvRow({
			TEXT("AssetType"),
			TEXT("AssetPath"),
			TEXT("ObjectName"),
			TEXT("DisplayName"),
			TEXT("StableId"),
			TEXT("Rarity"),
			TEXT("Cost"),
			TEXT("EffectHook"),
			TEXT("EffectType"),
			TEXT("TargetRuleId"),
			TEXT("NegativeCategories"),
			TEXT("ValidationSeverity"),
			TEXT("ValidationMessages"),
			TEXT("References"),
			TEXT("MatrixProgress"),
			TEXT("MatrixType"),
			TEXT("MatrixStatus"),
			TEXT("MatrixMessages")
		}));

		for (const FGameplayDataAuditRecord& Record : Records)
		{
			Lines.Add(JoinCsvRow({
				Record.AssetType,
				Record.AssetPath,
				Record.ObjectName,
				Record.DisplayName,
				Record.StableId,
				Record.Rarity,
				Record.Cost,
				Record.EffectHook,
				Record.EffectType,
				Record.TargetRuleId,
				Record.NegativeCategories,
				Record.ValidationSeverity,
				JoinStrings(Record.ValidationMessages, TEXT(" | ")),
				JoinStrings(Record.References, TEXT(" | ")),
				Record.MatrixProgress,
				Record.MatrixType,
				Record.MatrixStatus,
				JoinStrings(Record.MatrixMessages, TEXT(" | "))
			}));
		}

		IFileManager::Get().MakeDirectory(*FPaths::GetPath(CsvPath), true);
		return FFileHelper::SaveStringArrayToFile(Lines, *CsvPath);
	}

	void AddSummaryNumberFields(TSharedPtr<FJsonObject> JsonObject, const TMap<FString, int32>& Counts)
	{
		TArray<FString> Keys;
		Counts.GenerateKeyArray(Keys);
		Keys.Sort();
		for (const FString& Key : Keys)
		{
			JsonObject->SetNumberField(Key, Counts[Key]);
		}
	}

	bool WriteJsonReport(
		const FString& JsonPath,
		const FString& ContentPath,
		const FString& ProjectVersion,
		const FString& ProjectId,
		const TArray<FGameplayDataAuditRecord>& AssetRecords,
		const TMap<FString, int32>& CountsByType,
		const FMatrixComparisonResult& MatrixComparison)
	{
		TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();

		TSharedPtr<FJsonObject> Metadata = MakeShared<FJsonObject>();
		Metadata->SetStringField(TEXT("generatedAtUtc"), FDateTime::UtcNow().ToIso8601());
		Metadata->SetStringField(TEXT("projectName"), FApp::GetProjectName());
		Metadata->SetStringField(TEXT("projectVersion"), ProjectVersion);
		Metadata->SetStringField(TEXT("projectId"), ProjectId);
		Metadata->SetStringField(TEXT("engineVersion"), FEngineVersion::Current().ToString());
		Metadata->SetStringField(TEXT("contentPath"), ContentPath);
		Root->SetObjectField(TEXT("metadata"), Metadata);

		int32 ValidCount = 0;
		int32 WarningCount = 0;
		int32 ErrorCount = 0;
		int32 TotalRealAssetCount = 0;
		for (const FGameplayDataAuditRecord& Record : AssetRecords)
		{
			if (Record.AssetType.StartsWith(TEXT("Matrix")))
			{
				continue;
			}

			TotalRealAssetCount++;
			if (Record.ValidationSeverity == TEXT("Error"))
			{
				ErrorCount++;
			}
			else if (Record.ValidationSeverity == TEXT("Warning"))
			{
				WarningCount++;
			}
			else
			{
				ValidCount++;
			}
		}

		TSharedPtr<FJsonObject> Summary = MakeShared<FJsonObject>();
		Summary->SetNumberField(TEXT("totalAssets"), TotalRealAssetCount);
		TSharedPtr<FJsonObject> CountsByTypeJson = MakeShared<FJsonObject>();
		AddSummaryNumberFields(CountsByTypeJson, CountsByType);
		Summary->SetObjectField(TEXT("countsByType"), CountsByTypeJson);
		TSharedPtr<FJsonObject> ValidationSummary = MakeShared<FJsonObject>();
		ValidationSummary->SetNumberField(TEXT("valid"), ValidCount);
		ValidationSummary->SetNumberField(TEXT("warning"), WarningCount);
		ValidationSummary->SetNumberField(TEXT("error"), ErrorCount);
		Summary->SetObjectField(TEXT("validation"), ValidationSummary);
		Root->SetObjectField(TEXT("summary"), Summary);

		TMap<FString, TArray<TSharedPtr<FJsonValue>>> AssetsByType;
		for (const FGameplayDataAuditRecord& Record : AssetRecords)
		{
			if (Record.AssetType.StartsWith(TEXT("Matrix")))
			{
				continue;
			}
			AssetsByType.FindOrAdd(Record.AssetType).Add(MakeShared<FJsonValueObject>(MakeBaseAssetJson(Record)));
		}

		TSharedPtr<FJsonObject> AssetsByTypeJson = MakeShared<FJsonObject>();
		TArray<FString> AssetTypes;
		AssetsByType.GenerateKeyArray(AssetTypes);
		AssetTypes.Sort();
		for (const FString& AssetType : AssetTypes)
		{
			AssetsByTypeJson->SetArrayField(AssetType, AssetsByType[AssetType]);
		}
		Root->SetObjectField(TEXT("assetsByType"), AssetsByTypeJson);

		TSharedPtr<FJsonObject> MatrixJson = MakeShared<FJsonObject>();
		MatrixJson->SetStringField(TEXT("status"), MatrixComparison.Status);
		MatrixJson->SetStringField(TEXT("matrixPath"), MatrixComparison.MatrixPath);
		MatrixJson->SetStringField(TEXT("message"), MatrixComparison.Message);
		TSharedPtr<FJsonObject> MatrixSummaryJson = MakeShared<FJsonObject>();
		MatrixSummaryJson->SetNumberField(TEXT("rowsRead"), MatrixComparison.RowsRead);
		MatrixSummaryJson->SetNumberField(TEXT("rowsCompared"), MatrixComparison.RowsCompared);
		MatrixSummaryJson->SetNumberField(TEXT("matchedAssets"), MatrixComparison.MatchedAssets);
		MatrixSummaryJson->SetNumberField(TEXT("plannedMissing"), MatrixComparison.PlannedMissing);
		MatrixSummaryJson->SetNumberField(TEXT("actualMissingFromMatrix"), MatrixComparison.ActualMissingFromMatrix);
		MatrixSummaryJson->SetNumberField(TEXT("typeMismatches"), MatrixComparison.TypeMismatches);
		MatrixSummaryJson->SetNumberField(TEXT("statusMismatches"), MatrixComparison.StatusMismatches);
		MatrixSummaryJson->SetNumberField(TEXT("duplicateActualIds"), MatrixComparison.DuplicateActualIds);
		MatrixJson->SetObjectField(TEXT("summary"), MatrixSummaryJson);

		TArray<TSharedPtr<FJsonValue>> MatrixIssuesJson;
		for (const FMatrixComparisonIssue& Issue : MatrixComparison.Issues)
		{
			TSharedPtr<FJsonObject> IssueJson = MakeShared<FJsonObject>();
			IssueJson->SetStringField(TEXT("severity"), Issue.Severity);
			IssueJson->SetStringField(TEXT("issueType"), Issue.IssueType);
			IssueJson->SetStringField(TEXT("stableId"), Issue.StableId);
			IssueJson->SetStringField(TEXT("matrixType"), Issue.MatrixType);
			IssueJson->SetStringField(TEXT("assetType"), Issue.AssetType);
			IssueJson->SetStringField(TEXT("matrixProgress"), Issue.MatrixProgress);
			IssueJson->SetStringField(TEXT("message"), Issue.Message);
			IssueJson->SetNumberField(TEXT("lineNumber"), Issue.LineNumber);
			MatrixIssuesJson.Add(MakeShared<FJsonValueObject>(IssueJson));
		}
		MatrixJson->SetArrayField(TEXT("issues"), MatrixIssuesJson);
		Root->SetObjectField(TEXT("matrixComparison"), MatrixJson);

		FString Output;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
		{
			return false;
		}

		IFileManager::Get().MakeDirectory(*FPaths::GetPath(JsonPath), true);
		return FFileHelper::SaveStringToFile(Output, *JsonPath);
	}

	TArray<TArray<FString>> ParseCsvRows(const FString& CsvText)
	{
		TArray<TArray<FString>> Rows;
		TArray<FString> CurrentRow;
		FString CurrentCell;
		bool bInQuotes = false;

		for (int32 Index = 0; Index < CsvText.Len(); ++Index)
		{
			const TCHAR Character = CsvText[Index];
			if (Character == TCHAR('"'))
			{
				if (bInQuotes && Index + 1 < CsvText.Len() && CsvText[Index + 1] == TCHAR('"'))
				{
					CurrentCell.AppendChar(TCHAR('"'));
					Index++;
				}
				else
				{
					bInQuotes = !bInQuotes;
				}
				continue;
			}

			if (Character == TCHAR(',') && !bInQuotes)
			{
				CurrentRow.Add(CurrentCell);
				CurrentCell.Reset();
				continue;
			}

			if ((Character == TCHAR('\n') || Character == TCHAR('\r')) && !bInQuotes)
			{
				if (Character == TCHAR('\r') && Index + 1 < CsvText.Len() && CsvText[Index + 1] == TCHAR('\n'))
				{
					Index++;
				}

				CurrentRow.Add(CurrentCell);
				CurrentCell.Reset();
				if (CurrentRow.Num() > 1 || !CurrentRow[0].IsEmpty())
				{
					Rows.Add(CurrentRow);
				}
				CurrentRow.Reset();
				continue;
			}

			CurrentCell.AppendChar(Character);
		}

		if (!CurrentCell.IsEmpty() || CurrentRow.Num() > 0)
		{
			CurrentRow.Add(CurrentCell);
			Rows.Add(CurrentRow);
		}

		return Rows;
	}

	bool ReadObjectMatrix(const FString& MatrixPath, TArray<FObjectMatrixRecord>& OutRecords, FString& OutError)
	{
		FString CsvText;
		if (!FFileHelper::LoadFileToString(CsvText, *MatrixPath))
		{
			OutError = FString::Printf(TEXT("Object matrix file was not found or could not be read: %s"), *MatrixPath);
			return false;
		}

		const TArray<TArray<FString>> Rows = ParseCsvRows(CsvText);
		if (Rows.Num() == 0)
		{
			OutError = FString::Printf(TEXT("Object matrix file is empty: %s"), *MatrixPath);
			return false;
		}

		TMap<FString, int32> HeaderIndexByName;
		for (int32 HeaderIndex = 0; HeaderIndex < Rows[0].Num(); ++HeaderIndex)
		{
			HeaderIndexByName.Add(Rows[0][HeaderIndex], HeaderIndex);
		}

		const auto GetCell = [&HeaderIndexByName](const TArray<FString>& Row, const TCHAR* ColumnName) -> FString
		{
			if (const int32* ColumnIndex = HeaderIndexByName.Find(ColumnName))
			{
				return Row.IsValidIndex(*ColumnIndex) ? Row[*ColumnIndex] : FString();
			}
			return FString();
		};

		for (int32 RowIndex = 1; RowIndex < Rows.Num(); ++RowIndex)
		{
			const TArray<FString>& Row = Rows[RowIndex];
			FObjectMatrixRecord Record;
			Record.LineNumber = RowIndex + 1;
			Record.Progress = GetCell(Row, TEXT("Progress"));
			Record.CreationClass = GetCell(Row, TEXT("CreationClass"));
			Record.Id = GetCell(Row, TEXT("Id"));
			Record.DisplayName = GetCell(Row, TEXT("DisplayName"));
			Record.Type = GetCell(Row, TEXT("Type"));
			Record.Rarity = GetCell(Row, TEXT("Rarity"));
			Record.Cost = GetCell(Row, TEXT("Cost"));
			if (!Record.Id.IsEmpty())
			{
				OutRecords.Add(MoveTemp(Record));
			}
		}

		return true;
	}

	FString MatrixTypeToAssetType(const FString& MatrixType)
	{
		if (MatrixType == TEXT("Die"))
		{
			return TEXT("DiceDefinition");
		}
		if (MatrixType == TEXT("DiceItem"))
		{
			return TEXT("DiceItemDefinition");
		}
		if (MatrixType == TEXT("Module"))
		{
			return TEXT("ModuleDefinition");
		}
		if (MatrixType == TEXT("Consumable"))
		{
			return TEXT("ConsumableDefinition");
		}
		if (MatrixType == TEXT("Effect"))
		{
			return TEXT("ItemEffectDefinition");
		}
		return FString();
	}

	bool IsMatrixComparableAssetType(const FString& AssetType)
	{
		return AssetType == TEXT("DiceDefinition")
			|| AssetType == TEXT("DiceItemDefinition")
			|| AssetType == TEXT("ModuleDefinition")
			|| AssetType == TEXT("ConsumableDefinition");
	}

	bool IsMatrixProgressExpectedToExist(const FString& Progress)
	{
		const FString NormalizedProgress = Progress.ToLower();
		return NormalizedProgress == TEXT("validated")
			|| NormalizedProgress == TEXT("asset_created")
			|| NormalizedProgress == TEXT("effect_created");
	}

	FMatrixComparisonIssue MakeMatrixIssue(
		const FString& Severity,
		const FString& IssueType,
		const FString& StableId,
		const FString& MatrixType,
		const FString& AssetType,
		const FString& MatrixProgress,
		const FString& Message,
		const int32 LineNumber)
	{
		FMatrixComparisonIssue Issue;
		Issue.Severity = Severity;
		Issue.IssueType = IssueType;
		Issue.StableId = StableId;
		Issue.MatrixType = MatrixType;
		Issue.AssetType = AssetType;
		Issue.MatrixProgress = MatrixProgress;
		Issue.Message = Message;
		Issue.LineNumber = LineNumber;
		return Issue;
	}

	void ApplyMatrixComparison(TArray<FGameplayDataAuditRecord>& Records, const FString& MatrixPath, FMatrixComparisonResult& OutResult)
	{
		OutResult.Status = TEXT("Compared");
		OutResult.MatrixPath = MatrixPath;

		TArray<FObjectMatrixRecord> MatrixRecords;
		FString MatrixError;
		if (!ReadObjectMatrix(MatrixPath, MatrixRecords, MatrixError))
		{
			OutResult.Status = TEXT("Skipped");
			OutResult.Message = MatrixError;
			return;
		}

		OutResult.RowsRead = MatrixRecords.Num();

		TMap<FString, FObjectMatrixRecord> MatrixById;
		for (const FObjectMatrixRecord& MatrixRecord : MatrixRecords)
		{
			if (MatrixTypeToAssetType(MatrixRecord.Type).IsEmpty())
			{
				continue;
			}
			MatrixById.Add(MatrixRecord.Id, MatrixRecord);
			OutResult.RowsCompared++;
		}

		TMap<FString, int32> ActualRecordIndexById;
		for (int32 RecordIndex = 0; RecordIndex < Records.Num(); ++RecordIndex)
		{
			FGameplayDataAuditRecord& Record = Records[RecordIndex];
			if (!IsMatrixComparableAssetType(Record.AssetType) || Record.StableId.IsEmpty())
			{
				continue;
			}

			if (const int32* ExistingIndex = ActualRecordIndexById.Find(Record.StableId))
			{
				OutResult.DuplicateActualIds++;
				Record.MatrixStatus = TEXT("DuplicateActualId");
				Record.MatrixMessages.Add(FString::Printf(TEXT("Duplicate StableId also used by %s."), *Records[*ExistingIndex].AssetPath));
				OutResult.Issues.Add(MakeMatrixIssue(
					TEXT("Error"),
					TEXT("DuplicateActualId"),
					Record.StableId,
					FString(),
					Record.AssetType,
					FString(),
					Record.MatrixMessages.Last(),
					0));
				continue;
			}

			ActualRecordIndexById.Add(Record.StableId, RecordIndex);
		}

		for (const TPair<FString, FObjectMatrixRecord>& MatrixPair : MatrixById)
		{
			const FObjectMatrixRecord& MatrixRecord = MatrixPair.Value;
			const FString ExpectedAssetType = MatrixTypeToAssetType(MatrixRecord.Type);
			const int32* ActualRecordIndex = ActualRecordIndexById.Find(MatrixRecord.Id);
			if (!ActualRecordIndex)
			{
				OutResult.PlannedMissing++;
				const FString Severity = IsMatrixProgressExpectedToExist(MatrixRecord.Progress) ? TEXT("Warning") : TEXT("Info");
				const FString Message = FString::Printf(
					TEXT("Matrix row %d plans %s as %s but no matching asset StableId was found."),
					MatrixRecord.LineNumber,
					*MatrixRecord.Id,
					*MatrixRecord.Type);
				OutResult.Issues.Add(MakeMatrixIssue(
					Severity,
					TEXT("MatrixAssetMissing"),
					MatrixRecord.Id,
					MatrixRecord.Type,
					ExpectedAssetType,
					MatrixRecord.Progress,
					Message,
					MatrixRecord.LineNumber));
				FGameplayDataAuditRecord MatrixOnlyRecord;
				MatrixOnlyRecord.AssetType = TEXT("MatrixExpectedMissing");
				MatrixOnlyRecord.StableId = MatrixRecord.Id;
				MatrixOnlyRecord.DisplayName = MatrixRecord.DisplayName;
				MatrixOnlyRecord.Rarity = MatrixRecord.Rarity;
				MatrixOnlyRecord.Cost = MatrixRecord.Cost == TEXT("n/a") ? FString() : MatrixRecord.Cost;
				MatrixOnlyRecord.MatrixProgress = MatrixRecord.Progress;
				MatrixOnlyRecord.MatrixType = MatrixRecord.Type;
				MatrixOnlyRecord.MatrixStatus = TEXT("MatrixAssetMissing");
				MatrixOnlyRecord.MatrixMessages.Add(Message);
				Records.Add(MoveTemp(MatrixOnlyRecord));
				continue;
			}

			FGameplayDataAuditRecord& Record = Records[*ActualRecordIndex];
			Record.MatrixProgress = MatrixRecord.Progress;
			Record.MatrixType = MatrixRecord.Type;
			Record.MatrixStatus = TEXT("Matched");
			OutResult.MatchedAssets++;

			if (Record.AssetType != ExpectedAssetType)
			{
				OutResult.TypeMismatches++;
				Record.MatrixStatus = TEXT("TypeMismatch");
				const FString Message = FString::Printf(
					TEXT("Matrix type %s expects %s, but asset was exported as %s."),
					*MatrixRecord.Type,
					*ExpectedAssetType,
					*Record.AssetType);
				Record.MatrixMessages.Add(Message);
				OutResult.Issues.Add(MakeMatrixIssue(
					TEXT("Error"),
					TEXT("TypeMismatch"),
					Record.StableId,
					MatrixRecord.Type,
					Record.AssetType,
					MatrixRecord.Progress,
					Message,
					MatrixRecord.LineNumber));
			}

			if (MatrixRecord.Progress.ToLower() == TEXT("validated") && Record.ValidationSeverity != TEXT("Valid"))
			{
				OutResult.StatusMismatches++;
				Record.MatrixStatus = Record.MatrixStatus == TEXT("Matched") ? TEXT("MatrixStatusMismatch") : Record.MatrixStatus;
				const FString Message = FString::Printf(
					TEXT("Matrix marks asset as validated, but real DataAsset validation is %s."),
					*Record.ValidationSeverity);
				Record.MatrixMessages.Add(Message);
				OutResult.Issues.Add(MakeMatrixIssue(
					Record.ValidationSeverity == TEXT("Error") ? TEXT("Error") : TEXT("Warning"),
					TEXT("MatrixValidatedButValidationIssues"),
					Record.StableId,
					MatrixRecord.Type,
					Record.AssetType,
					MatrixRecord.Progress,
					Message,
					MatrixRecord.LineNumber));
			}

			if (!IsMatrixProgressExpectedToExist(MatrixRecord.Progress))
			{
				Record.MatrixStatus = Record.MatrixStatus == TEXT("Matched") ? TEXT("AssetExistsBeforeMatrixProgress") : Record.MatrixStatus;
				const FString Message = FString::Printf(
					TEXT("Asset exists while matrix Progress is %s."),
					*MatrixRecord.Progress);
				Record.MatrixMessages.Add(Message);
				OutResult.Issues.Add(MakeMatrixIssue(
					TEXT("Info"),
					TEXT("AssetExistsBeforeMatrixProgress"),
					Record.StableId,
					MatrixRecord.Type,
					Record.AssetType,
					MatrixRecord.Progress,
					Message,
					MatrixRecord.LineNumber));
			}
		}

		for (FGameplayDataAuditRecord& Record : Records)
		{
			if (!IsMatrixComparableAssetType(Record.AssetType) || Record.StableId.IsEmpty())
			{
				continue;
			}

			if (!MatrixById.Contains(Record.StableId))
			{
				OutResult.ActualMissingFromMatrix++;
				Record.MatrixStatus = TEXT("MissingFromMatrix");
				const FString Message = TEXT("Asset StableId is not present in ObjectMatrix.csv.");
				Record.MatrixMessages.Add(Message);
				OutResult.Issues.Add(MakeMatrixIssue(
					TEXT("Warning"),
					TEXT("ActualAssetMissingFromMatrix"),
					Record.StableId,
					FString(),
					Record.AssetType,
					FString(),
					Message,
					0));
			}
		}

		OutResult.Message = TEXT("Compared ObjectMatrix.csv IDs against exported DiceId/ItemId values for dice, dice items, modules and consumables.");
	}

	void ReadProjectSettings(FString& OutProjectVersion, FString& OutProjectId)
	{
		GConfig->GetString(
			TEXT("/Script/EngineSettings.GeneralProjectSettings"),
			TEXT("ProjectVersion"),
			OutProjectVersion,
			GGameIni);
		GConfig->GetString(
			TEXT("/Script/EngineSettings.GeneralProjectSettings"),
			TEXT("ProjectID"),
			OutProjectId,
			GGameIni);

		if (OutProjectVersion.IsEmpty())
		{
			OutProjectVersion = TEXT("Unknown");
		}
		if (OutProjectId.IsEmpty())
		{
			OutProjectId = TEXT("Unknown");
		}
	}
}

UGambitGameplayDataAuditCommandlet::UGambitGameplayDataAuditCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UGambitGameplayDataAuditCommandlet::Main(const FString& Params)
{
	FString ContentPath = DefaultContentPath();
	FParse::Value(*Params, TEXT("ContentPath="), ContentPath);
	ContentPath = NormalizeContentPath(ContentPath);

	const FString JsonPath = ParsePathParam(Params, TEXT("JsonPath="), DefaultJsonPath());
	const FString CsvPath = ParsePathParam(Params, TEXT("CsvPath="), DefaultCsvPath());
	const FString MatrixPath = ParsePathParam(Params, TEXT("MatrixPath="), DefaultMatrixPath());
	const bool bSkipMatrix = FParse::Param(*Params, TEXT("SkipMatrix"));
	const bool bFailOnValidationErrors = FParse::Param(*Params, TEXT("FailOnValidationErrors"));

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	AssetRegistry.SearchAllAssets(true);

	UGambitItemCatalogDataAsset* FallbackItemCatalog = FindFallbackItemCatalog(AssetRegistry, ContentPath);

	TArray<FGameplayDataAuditRecord> Records;
	AppendRecordsForClass(
		AssetRegistry,
		UGambitDiceDefinition::StaticClass(),
		ContentPath,
		[](const FAssetData& AssetData)
		{
			return MakeDiceRecord(Cast<UGambitDiceDefinition>(AssetData.GetAsset()), AssetData);
		},
		Records);

	AppendRecordsForClass(
		AssetRegistry,
		UGambitDiceItemDefinition::StaticClass(),
		ContentPath,
		[](const FAssetData& AssetData)
		{
			return MakeItemRecord(Cast<UGambitDiceItemDefinition>(AssetData.GetAsset()), AssetData, TEXT("DiceItemDefinition"));
		},
		Records);

	AppendRecordsForClass(
		AssetRegistry,
		UGambitModuleDefinition::StaticClass(),
		ContentPath,
		[](const FAssetData& AssetData)
		{
			return MakeItemRecord(Cast<UGambitModuleDefinition>(AssetData.GetAsset()), AssetData, TEXT("ModuleDefinition"));
		},
		Records);

	AppendRecordsForClass(
		AssetRegistry,
		UGambitConsumableDefinition::StaticClass(),
		ContentPath,
		[](const FAssetData& AssetData)
		{
			return MakeItemRecord(Cast<UGambitConsumableDefinition>(AssetData.GetAsset()), AssetData, TEXT("ConsumableDefinition"));
		},
		Records);

	AppendRecordsForClass(
		AssetRegistry,
		UGambitItemEffectDefinition::StaticClass(),
		ContentPath,
		[](const FAssetData& AssetData)
		{
			return MakeEffectRecord(Cast<UGambitItemEffectDefinition>(AssetData.GetAsset()), AssetData);
		},
		Records);

	AppendRecordsForClass(
		AssetRegistry,
		UGambitShopLootTable::StaticClass(),
		ContentPath,
		[FallbackItemCatalog](const FAssetData& AssetData)
		{
			return MakeShopLootTableRecord(Cast<UGambitShopLootTable>(AssetData.GetAsset()), FallbackItemCatalog, AssetData);
		},
		Records);

	AppendRecordsForClass(
		AssetRegistry,
		UGambitSharedPoolDefinition::StaticClass(),
		ContentPath,
		[FallbackItemCatalog](const FAssetData& AssetData)
		{
			return MakeSharedPoolRecord(Cast<UGambitSharedPoolDefinition>(AssetData.GetAsset()), FallbackItemCatalog, AssetData);
		},
		Records);

	Records.Sort([](const FGameplayDataAuditRecord& Left, const FGameplayDataAuditRecord& Right)
	{
		if (Left.AssetType != Right.AssetType)
		{
			return Left.AssetType < Right.AssetType;
		}
		return Left.AssetPath < Right.AssetPath;
	});

	FMatrixComparisonResult MatrixComparison;
	if (bSkipMatrix)
	{
		MatrixComparison.Status = TEXT("Skipped");
		MatrixComparison.MatrixPath = MatrixPath;
		MatrixComparison.Message = TEXT("Skipped by -SkipMatrix.");
	}
	else
	{
		ApplyMatrixComparison(Records, MatrixPath, MatrixComparison);
	}

	TMap<FString, int32> CountsByType;
	int32 ValidationErrorCount = 0;
	int32 ValidationWarningCount = 0;
	int32 RealAssetCount = 0;
	for (const FGameplayDataAuditRecord& Record : Records)
	{
		if (Record.AssetType.StartsWith(TEXT("Matrix")))
		{
			continue;
		}

		RealAssetCount++;
		CountsByType.FindOrAdd(Record.AssetType)++;
		if (Record.ValidationSeverity == TEXT("Error"))
		{
			ValidationErrorCount++;
		}
		else if (Record.ValidationSeverity == TEXT("Warning"))
		{
			ValidationWarningCount++;
		}
	}

	FString ProjectVersion;
	FString ProjectId;
	ReadProjectSettings(ProjectVersion, ProjectId);

	bool bWriteFailed = false;
	if (!WriteJsonReport(JsonPath, ContentPath, ProjectVersion, ProjectId, Records, CountsByType, MatrixComparison))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to write gameplay data audit JSON to %s"), *JsonPath);
		bWriteFailed = true;
	}

	if (!WriteCsvReport(CsvPath, Records))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to write gameplay data audit CSV to %s"), *CsvPath);
		bWriteFailed = true;
	}

	UE_LOG(LogTemp, Display, TEXT("Wrote gameplay data audit JSON to %s"), *JsonPath);
	UE_LOG(LogTemp, Display, TEXT("Wrote gameplay data audit CSV to %s"), *CsvPath);
	UE_LOG(
		LogTemp,
		Display,
		TEXT("GameplayDataAudit summary: Assets=%d Dice=%d DiceItems=%d Modules=%d Consumables=%d Effects=%d ShopLootTables=%d SharedPools=%d ValidationWarnings=%d ValidationErrors=%d MatrixStatus=%s MatrixIssues=%d"),
		RealAssetCount,
		CountsByType.FindRef(TEXT("DiceDefinition")),
		CountsByType.FindRef(TEXT("DiceItemDefinition")),
		CountsByType.FindRef(TEXT("ModuleDefinition")),
		CountsByType.FindRef(TEXT("ConsumableDefinition")),
		CountsByType.FindRef(TEXT("ItemEffectDefinition")),
		CountsByType.FindRef(TEXT("ShopLootTable")),
		CountsByType.FindRef(TEXT("SharedPoolDefinition")),
		ValidationWarningCount,
		ValidationErrorCount,
		*MatrixComparison.Status,
		MatrixComparison.Issues.Num());

	if (bWriteFailed)
	{
		return 1;
	}

	if (bFailOnValidationErrors && ValidationErrorCount > 0)
	{
		return 1;
	}

	return 0;
}
