#include "Data/Validation/GambitDataValidation.h"

#include "Data/Assets/GambitItemCatalogDataAsset.h"
#include "Data/Assets/GambitItemEffectDefinition.h"
#include "Data/Assets/GambitSharedPoolDefinition.h"
#include "Dice/Data/GambitDiceDefinition.h"
#include "Items/Consumables/GambitConsumableDefinition.h"
#include "Items/Data/GambitItemDefinition.h"
#include "Items/DiceItems/GambitDiceItemDefinition.h"
#include "Items/Effects/GambitEffectTargetRules.h"
#include "Items/Effects/GambitItemEffect.h"
#include "Items/Modules/GambitModuleDefinition.h"
#include "Shop/Data/GambitShopLootTable.h"

namespace
{
	constexpr int32 DefaultFallbackFaceCount = 6;

	template <typename EnumType>
	bool IsValidEnumValue(const EnumType Value)
	{
		const UEnum* Enum = StaticEnum<EnumType>();
		return Enum && !Enum->GetNameStringByValue(static_cast<int64>(Value)).IsEmpty();
	}

	FString EnumValueToString(const UEnum* Enum, const int64 Value)
	{
		if (!Enum)
		{
			return TEXT("Unknown");
		}

		const FString Name = Enum->GetNameStringByValue(Value);
		return Name.IsEmpty() ? TEXT("Unknown") : Name;
	}

	FString ObjectLabel(const UObject* Object)
	{
		return Object ? Object->GetPathName() : FString(TEXT("None"));
	}

	bool IsDataValidationNeutralScoreModifier(const FGambitScoreModifierContext& Modifier)
	{
		return FMath::IsNearlyZero(Modifier.AdditiveBonus)
			&& FMath::IsNearlyZero(Modifier.DiceContributionMultiplierBonus)
			&& FMath::IsNearlyEqual(Modifier.Multiplier, 1.0f)
			&& Modifier.ScoreCap <= 0.0f
			&& Modifier.DiminishingThreshold <= 0.0f
			&& FMath::IsNearlyEqual(Modifier.DiminishingFactor, 1.0f);
	}

	void AddError(TArray<FGambitDataValidationIssue>& OutIssues, const FString& Message)
	{
		GambitDataValidation::AddIssue(OutIssues, EGambitDataValidationSeverity::Error, Message);
	}

	void AddWarning(TArray<FGambitDataValidationIssue>& OutIssues, const FString& Message)
	{
		GambitDataValidation::AddIssue(OutIssues, EGambitDataValidationSeverity::Warning, Message);
	}

	bool HasScalarParameter(const UGambitItemEffectDefinition* EffectDefinition, const FName ParameterName)
	{
		return EffectDefinition && EffectDefinition->ScalarParameters.Contains(ParameterName);
	}

	float ResolveScalarParameter(
		const UGambitItemEffectDefinition* EffectDefinition,
		const FName ParameterName,
		const float FallbackValue)
	{
		if (!EffectDefinition)
		{
			return FallbackValue;
		}

		if (const float* Value = EffectDefinition->ScalarParameters.Find(ParameterName))
		{
			return *Value;
		}

		return FallbackValue;
	}

	bool HasConfiguredAmount(const UGambitItemEffectDefinition* EffectDefinition)
	{
		return EffectDefinition
			&& (!FMath::IsNearlyZero(EffectDefinition->Amount)
				|| HasScalarParameter(EffectDefinition, TEXT("Amount"))
				|| HasScalarParameter(EffectDefinition, TEXT("AmountPerMatchingDie"))
				|| HasScalarParameter(EffectDefinition, TEXT("AmountPerOtherMatchingDie"))
				|| HasScalarParameter(EffectDefinition, TEXT("AmountPerOwnedDie"))
				|| HasScalarParameter(EffectDefinition, TEXT("AmountFromSourceDieValue"))
				|| HasScalarParameter(EffectDefinition, TEXT("AmountPerGold"))
				|| HasScalarParameter(EffectDefinition, TEXT("AmountPerGoldTier"))
				|| HasScalarParameter(EffectDefinition, TEXT("AmountPercentOfRoundGoldReward"))
				|| HasScalarParameter(EffectDefinition, TEXT("AmountPerGlobalPurchaseForItemType"))
				|| HasScalarParameter(EffectDefinition, TEXT("AmountPerGlobalPurchaseForItemRarity"))
				|| HasScalarParameter(EffectDefinition, TEXT("AmountPerGlobalPurchaseForItemTypeAndRarity"))
				|| HasScalarParameter(EffectDefinition, TEXT("AmountPerRerollUsed"))
				|| HasScalarParameter(EffectDefinition, TEXT("AmountPerUnusedReroll"))
				|| HasScalarParameter(EffectDefinition, TEXT("AmountPerDistinctOwnedDiceType"))
				|| HasScalarParameter(EffectDefinition, TEXT("AmountPerMostRepeatedOwnedDiceType"))
				|| HasScalarParameter(EffectDefinition, TEXT("AmountPerOwnedDiceRarityCount"))
				|| HasScalarParameter(EffectDefinition, TEXT("AmountPerOwnedItemRarityCount"))
				|| HasScalarParameter(EffectDefinition, TEXT("AmountPerOwnedModuleRarityCount"))
				|| HasScalarParameter(EffectDefinition, TEXT("AmountPerPairCount"))
				|| HasScalarParameter(EffectDefinition, TEXT("AmountPerMatchingPairCount"))
				|| HasScalarParameter(EffectDefinition, TEXT("AmountPerMatchingDieChance"))
				|| HasScalarParameter(EffectDefinition, TEXT("AmountPerRerolledDieValueIncreased"))
				|| HasScalarParameter(EffectDefinition, TEXT("AmountPerRerolledDieValueDecreased"))
				|| HasScalarParameter(EffectDefinition, TEXT("AmountPerRerolledDieValueUnchanged")));
	}

	bool HasConfiguredMultiplier(const UGambitItemEffectDefinition* EffectDefinition)
	{
		return EffectDefinition
			&& (!FMath::IsNearlyEqual(EffectDefinition->Multiplier, 1.0f)
				|| HasScalarParameter(EffectDefinition, TEXT("Multiplier"))
				|| HasScalarParameter(EffectDefinition, TEXT("MultiplierDeltaPerMatchingDie"))
				|| HasScalarParameter(EffectDefinition, TEXT("MultiplierPerMatchingDie"))
				|| HasScalarParameter(EffectDefinition, TEXT("MultiplierPerOwnedDiceRarityCount"))
				|| HasScalarParameter(EffectDefinition, TEXT("MultiplierDeltaPerOwnedDiceRarityCount"))
				|| HasScalarParameter(EffectDefinition, TEXT("MultiplierPerGoldTier"))
				|| HasScalarParameter(EffectDefinition, TEXT("MultiplierDeltaPerGoldTier")));
	}

	bool EffectRequestsOpponentTarget(const UGambitItemEffectDefinition* EffectDefinition)
	{
		return EffectDefinition
			&& (EffectDefinition->Target == EGambitEffectTarget::Target
				|| EffectDefinition->Target == EGambitEffectTarget::SourceAndTarget);
	}

	bool EffectHasOpponentTargetRule(const UGambitItemEffectDefinition* EffectDefinition)
	{
		return EffectDefinition && GambitEffectTargetRules::IsOpponentRule(EffectDefinition->TargetRuleId);
	}

	bool EffectTypeIsClearlySingleTarget(const EGambitItemEffectType EffectType)
	{
		switch (EffectType)
		{
		case EGambitItemEffectType::GrantConsumable:
		case EGambitItemEffectType::PreventNegativeEffect:
		case EGambitItemEffectType::SellItem:
		case EGambitItemEffectType::SellDie:
		case EGambitItemEffectType::CopyLastTriggeredEffect:
			return true;
		default:
			return false;
		}
	}

	bool EffectTypeCanUseDieTargetRule(const EGambitItemEffectType EffectType)
	{
		switch (EffectType)
		{
		case EGambitItemEffectType::ModifyDieValue:
		case EGambitItemEffectType::SetDieValue:
		case EGambitItemEffectType::LockDie:
		case EGambitItemEffectType::RerollDie:
		case EGambitItemEffectType::MultiplyDiceContribution:
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
		case EGambitItemEffectType::SellDie:
			return true;
		default:
			return false;
		}
	}

	void ValidateTargetRuleCompatibility(
		const UGambitItemEffectDefinition* EffectDefinition,
		const FString& EffectLabel,
		TArray<FGambitDataValidationIssue>& OutIssues)
	{
		if (!EffectDefinition || EffectDefinition->TargetRuleId.IsNone() || !GambitEffectTargetRules::IsKnownRule(EffectDefinition->TargetRuleId))
		{
			return;
		}

		if (GambitEffectTargetRules::IsMultiTargetRule(EffectDefinition->TargetRuleId)
			&& EffectTypeIsClearlySingleTarget(EffectDefinition->EffectType))
		{
			AddWarning(
				OutIssues,
				FString::Printf(
					TEXT("%s uses multi-target TargetRuleId %s with a clearly single-target effect type."),
					*EffectLabel,
					*EffectDefinition->TargetRuleId.ToString()));
		}

		if (GambitEffectTargetRules::IsDieRule(EffectDefinition->TargetRuleId)
			&& !EffectTypeCanUseDieTargetRule(EffectDefinition->EffectType))
		{
			AddWarning(
				OutIssues,
				FString::Printf(
					TEXT("%s uses die TargetRuleId %s but effect type %s does not directly mutate dice."),
					*EffectLabel,
					*EffectDefinition->TargetRuleId.ToString(),
					*GambitDataValidation::EffectTypeToString(EffectDefinition->EffectType)));
		}
	}

	bool HasConcreteNegativeEffectCategory(const TArray<EGambitNegativeEffectCategory>& Categories)
	{
		return Categories.ContainsByPredicate([](const EGambitNegativeEffectCategory Category)
		{
			return IsValidEnumValue(Category) && Category != EGambitNegativeEffectCategory::None;
		});
	}

	void ValidateNegativeEffectCategoryList(
		const TArray<EGambitNegativeEffectCategory>& Categories,
		const FString& CategoryLabel,
		TArray<FGambitDataValidationIssue>& OutIssues)
	{
		for (int32 CategoryIndex = 0; CategoryIndex < Categories.Num(); ++CategoryIndex)
		{
			const EGambitNegativeEffectCategory Category = Categories[CategoryIndex];
			const FString EntryLabel = FString::Printf(TEXT("%s[%d]"), *CategoryLabel, CategoryIndex);
			if (!IsValidEnumValue(Category))
			{
				AddError(OutIssues, FString::Printf(TEXT("%s has an invalid negative effect category."), *EntryLabel));
				continue;
			}

			if (Category == EGambitNegativeEffectCategory::None)
			{
				AddWarning(
					OutIssues,
					FString::Printf(
						TEXT("%s uses category None; leave the category list empty for fallback/global behavior instead."),
						*EntryLabel));
			}
		}
	}

	void ValidateNegativeEffectDefenseConfiguration(
		const UGambitItemEffectDefinition* EffectDefinition,
		const FString& EffectLabel,
		TArray<FGambitDataValidationIssue>& OutIssues)
	{
		if (!EffectDefinition)
		{
			return;
		}

		ValidateNegativeEffectCategoryList(EffectDefinition->NegativeEffectCategories, FString::Printf(TEXT("%s NegativeEffectCategories"), *EffectLabel), OutIssues);
		ValidateNegativeEffectCategoryList(EffectDefinition->PreventedNegativeEffectCategories, FString::Printf(TEXT("%s PreventedNegativeEffectCategories"), *EffectLabel), OutIssues);

		const bool bIsDefense = EffectDefinition->EffectType == EGambitItemEffectType::PreventNegativeEffect;
		if (EffectDefinition->bNegativeEffect && !HasConcreteNegativeEffectCategory(EffectDefinition->NegativeEffectCategories))
		{
			AddWarning(
				OutIssues,
				FString::Printf(
					TEXT("%s is marked bNegativeEffect but has no explicit negative category; resolver will use Generic fallback."),
					*EffectLabel));
		}

		if (!EffectDefinition->bNegativeEffect && EffectDefinition->NegativeEffectCategories.Num() > 0)
		{
			AddWarning(
				OutIssues,
				FString::Printf(
					TEXT("%s declares NegativeEffectCategories but is not marked bNegativeEffect; the categories will be ignored."),
					*EffectLabel));
		}

		if (bIsDefense)
		{
			if (EffectDefinition->bNegativeEffect)
			{
				AddWarning(
					OutIssues,
					FString::Printf(TEXT("%s is a PreventNegativeEffect defense but is also marked bNegativeEffect."), *EffectLabel));
			}

			if (EffectDefinition->NegativeEffectCategories.Num() > 0)
			{
				AddWarning(
					OutIssues,
					FString::Printf(
						TEXT("%s is a defense; use PreventedNegativeEffectCategories instead of NegativeEffectCategories."),
						*EffectLabel));
			}

			if (EffectDefinition->PreventedNegativeEffectCategories.Num() > 0
				&& !HasConcreteNegativeEffectCategory(EffectDefinition->PreventedNegativeEffectCategories))
			{
				AddWarning(
					OutIssues,
					FString::Printf(
						TEXT("%s has a typed defense filter with no concrete category; it will not add protection."),
						*EffectLabel));
			}

			if (EffectDefinition->PreventNegativeEffectBlockCount < 0)
			{
				AddError(OutIssues, FString::Printf(TEXT("%s has a negative PreventNegativeEffectBlockCount."), *EffectLabel));
			}
		}
		else
		{
			if (EffectDefinition->PreventedNegativeEffectCategories.Num() > 0)
			{
				AddWarning(
					OutIssues,
					FString::Printf(
						TEXT("%s declares PreventedNegativeEffectCategories but is not a PreventNegativeEffect defense."),
						*EffectLabel));
			}

			if (EffectDefinition->PreventNegativeEffectBlockCount > 0)
			{
				AddWarning(
					OutIssues,
					FString::Printf(
						TEXT("%s declares PreventNegativeEffectBlockCount but is not a PreventNegativeEffect defense."),
						*EffectLabel));
			}
		}
	}

	bool IsShopContextHook(const EGambitEffectHook Hook)
	{
		switch (Hook)
		{
		case EGambitEffectHook::PreShopGenerate:
		case EGambitEffectHook::PostShopGenerate:
		case EGambitEffectHook::PrePriceResolve:
		case EGambitEffectHook::PostPriceResolve:
		case EGambitEffectHook::PrePurchase:
		case EGambitEffectHook::PostPurchase:
		case EGambitEffectHook::ShopSkipped:
			return true;
		default:
			return false;
		}
	}

	UGambitItemDefinition* ResolveDataValidationItemFromCatalog(const FName ItemId, const UGambitItemCatalogDataAsset* Catalog)
	{
		if (ItemId.IsNone() || !Catalog)
		{
			return nullptr;
		}

		const auto FindInEntries = [ItemId](const TArray<FGambitItemCatalogEntry>& Entries) -> UGambitItemDefinition*
		{
			for (const FGambitItemCatalogEntry& Entry : Entries)
			{
				if (!Entry.ItemDefinition)
				{
					continue;
				}

				const FName EntryId = !Entry.ItemId.IsNone() ? Entry.ItemId : Entry.ItemDefinition->GetStableItemId();
				if (EntryId == ItemId)
				{
					return Entry.ItemDefinition;
				}
			}

			return nullptr;
		};

		if (UGambitItemDefinition* Module = FindInEntries(Catalog->ModuleEntries))
		{
			return Module;
		}

		if (UGambitItemDefinition* Consumable = FindInEntries(Catalog->ConsumableEntries))
		{
			return Consumable;
		}

		return FindInEntries(Catalog->DiceItemEntries);
	}

	UGambitItemDefinition* ResolveDataValidationCatalogEntryDefinition(
		const FName ItemId,
		UGambitItemDefinition* ItemDefinition,
		const UGambitItemCatalogDataAsset* Catalog)
	{
		return ItemDefinition ? ItemDefinition : ResolveDataValidationItemFromCatalog(ItemId, Catalog);
	}

	FName ResolveDataValidationCatalogEntryId(const FName EntryId, const UGambitItemDefinition* ItemDefinition)
	{
		if (!EntryId.IsNone())
		{
			return EntryId;
		}

		return ItemDefinition ? ItemDefinition->GetResolvedItemId() : NAME_None;
	}

	void ValidateScoreModifier(
		const FGambitScoreModifierContext& Modifier,
		const FString& OwnerLabel,
		TArray<FGambitDataValidationIssue>& OutIssues)
	{
		if (Modifier.Multiplier <= 0.0f)
		{
			AddError(OutIssues, FString::Printf(TEXT("%s has a non-positive score multiplier."), *OwnerLabel));
		}

		if (Modifier.DiminishingFactor <= 0.0f)
		{
			AddError(OutIssues, FString::Printf(TEXT("%s has a non-positive diminishing factor."), *OwnerLabel));
		}
	}

	void ValidateEffectConditions(
		const UGambitItemEffectDefinition* EffectDefinition,
		const FString& EffectLabel,
		TArray<FGambitDataValidationIssue>& OutIssues)
	{
		if (!EffectDefinition)
		{
			return;
		}

		for (int32 ConditionIndex = 0; ConditionIndex < EffectDefinition->Conditions.Num(); ++ConditionIndex)
		{
			const FGambitEffectConditionDefinition& Condition = EffectDefinition->Conditions[ConditionIndex];
			const FString ConditionLabel = FString::Printf(TEXT("%s Condition[%d]"), *EffectLabel, ConditionIndex);

			if (!IsValidEnumValue(Condition.ConditionType))
			{
				AddError(OutIssues, FString::Printf(TEXT("%s has an invalid condition type."), *ConditionLabel));
				continue;
			}

			if (!IsValidEnumValue(Condition.ConditionTarget))
			{
				AddError(OutIssues, FString::Printf(TEXT("%s has an invalid target."), *ConditionLabel));
			}

			if (!IsValidEnumValue(Condition.Comparison))
			{
				AddError(OutIssues, FString::Printf(TEXT("%s has an invalid comparison."), *ConditionLabel));
			}

			switch (Condition.ConditionType)
			{
			case EGambitEffectConditionType::None:
				AddWarning(OutIssues, FString::Printf(TEXT("%s is set to None and will always pass."), *ConditionLabel));
				break;
			case EGambitEffectConditionType::HasCombinationType:
				if (!IsValidEnumValue(Condition.CombinationType) || Condition.CombinationType == EGambitCombinationType::None)
				{
					AddError(OutIssues, FString::Printf(TEXT("%s requires a concrete combination type."), *ConditionLabel));
				}
				break;
			case EGambitEffectConditionType::AllDiceValueBetween:
				if (Condition.Count < Condition.Value)
				{
					AddError(OutIssues, FString::Printf(TEXT("%s has max value below min value."), *ConditionLabel));
				}
				break;
			case EGambitEffectConditionType::AllDiceValueInSet:
				if (Condition.Value == Condition.Count)
				{
					AddWarning(OutIssues, FString::Printf(TEXT("%s uses the same value twice; use DieValueEquals or AllDiceValueBetween for a single allowed value."), *ConditionLabel));
				}
				break;
			case EGambitEffectConditionType::ItemRarity:
			case EGambitEffectConditionType::ShopItemRarity:
			case EGambitEffectConditionType::OwnedDiceRarityCount:
			case EGambitEffectConditionType::OwnedDiceRarityAtLeastCount:
			case EGambitEffectConditionType::OwnedItemRarityCount:
			case EGambitEffectConditionType::OwnedItemRarityAtLeastCount:
			case EGambitEffectConditionType::OwnedModuleRarityCount:
			case EGambitEffectConditionType::OwnedModuleRarityAtLeastCount:
				if (!IsValidEnumValue(Condition.Rarity))
				{
					AddError(OutIssues, FString::Printf(TEXT("%s uses an invalid rarity."), *ConditionLabel));
				}
				break;
			case EGambitEffectConditionType::ItemTag:
			case EGambitEffectConditionType::ShopItemTag:
				if (Condition.Tag.IsNone())
				{
					AddError(OutIssues, FString::Printf(TEXT("%s requires a tag."), *ConditionLabel));
				}
				break;
			case EGambitEffectConditionType::ChancePercentage:
				if (Condition.ChancePercent <= 0.0f)
				{
					AddError(OutIssues, FString::Printf(TEXT("%s has a non-positive chance and can never pass."), *ConditionLabel));
				}
				break;
			default:
				break;
			}
		}
	}

	void ValidateEffectParameters(
		const UGambitItemEffectDefinition* EffectDefinition,
		const FString& EffectLabel,
		TArray<FGambitDataValidationIssue>& OutIssues)
	{
		if (!EffectDefinition)
		{
			return;
		}

		switch (EffectDefinition->EffectType)
		{
		case EGambitItemEffectType::ScoreModifier:
			if (IsDataValidationNeutralScoreModifier(EffectDefinition->ScoreModifier))
			{
				AddError(OutIssues, FString::Printf(TEXT("%s is ScoreModifier but has no score modifier values."), *EffectLabel));
			}
			ValidateScoreModifier(EffectDefinition->ScoreModifier, EffectLabel, OutIssues);
			break;
		case EGambitItemEffectType::AddScoreFlat:
		case EGambitItemEffectType::AddGold:
		case EGambitItemEffectType::SpendGold:
		case EGambitItemEffectType::ModifyDieValue:
		case EGambitItemEffectType::AddReroll:
		case EGambitItemEffectType::ModifyRerollLimit:
		case EGambitItemEffectType::StealScore:
		case EGambitItemEffectType::StealGold:
		case EGambitItemEffectType::ModifyShopOfferCount:
		case EGambitItemEffectType::AddShopFlatPriceDelta:
		case EGambitItemEffectType::AddPurchaseGoldDelta:
		case EGambitItemEffectType::ModifyDebtLimit:
		case EGambitItemEffectType::ModifyMaxGold:
		case EGambitItemEffectType::ModifyInterestInterval:
		case EGambitItemEffectType::ModifyMaxInterest:
		case EGambitItemEffectType::ModifyInterestBonus:
		case EGambitItemEffectType::AddRecurringGoldIncome:
		case EGambitItemEffectType::SellItem:
		case EGambitItemEffectType::SellDie:
			if (!HasConfiguredAmount(EffectDefinition))
			{
				AddError(OutIssues, FString::Printf(TEXT("%s requires Amount or ScalarParameters[Amount]."), *EffectLabel));
			}
			break;
		case EGambitItemEffectType::MultiplyScore:
		case EGambitItemEffectType::MultiplyDiceContribution:
			if (!HasConfiguredMultiplier(EffectDefinition))
			{
				AddError(OutIssues, FString::Printf(TEXT("%s requires Multiplier or ScalarParameters[Multiplier]."), *EffectLabel));
			}
			if (ResolveScalarParameter(EffectDefinition, TEXT("Multiplier"), EffectDefinition->Multiplier) <= 0.0f)
			{
				AddError(OutIssues, FString::Printf(TEXT("%s has a non-positive multiplier."), *EffectLabel));
			}
			break;
		case EGambitItemEffectType::GrantConsumable:
			if (!EffectDefinition->GrantedConsumableDefinition)
			{
				AddError(OutIssues, FString::Printf(TEXT("%s requires GrantedConsumableDefinition."), *EffectLabel));
			}
			break;
		case EGambitItemEffectType::AddTemporaryScoreModifier:
			if (IsDataValidationNeutralScoreModifier(EffectDefinition->ScoreModifier)
				&& !HasConfiguredAmount(EffectDefinition)
				&& !HasConfiguredMultiplier(EffectDefinition))
			{
				AddError(OutIssues, FString::Printf(TEXT("%s has no temporary score modifier values."), *EffectLabel));
			}
			ValidateScoreModifier(EffectDefinition->ScoreModifier, EffectLabel, OutIssues);
			if (ResolveScalarParameter(EffectDefinition, TEXT("Multiplier"), EffectDefinition->Multiplier) <= 0.0f)
			{
				AddError(OutIssues, FString::Printf(TEXT("%s has a non-positive temporary multiplier."), *EffectLabel));
			}
			break;
		case EGambitItemEffectType::TransformDiceForRound:
			if (!EffectDefinition->TransformDiceDefinition
				&& EffectDefinition->TransformMinValue > EffectDefinition->TransformMaxValue)
			{
				AddError(OutIssues, FString::Printf(TEXT("%s has invalid transform dice min/max values."), *EffectLabel));
			}
			if (!EffectDefinition->TransformDiceDefinition)
			{
				AddWarning(OutIssues, FString::Printf(TEXT("%s transforms to a runtime fallback die instead of a concrete dice definition."), *EffectLabel));
			}
			break;
		case EGambitItemEffectType::AddTemporaryDie:
			if (!EffectDefinition->TransformDiceDefinition)
			{
				AddWarning(OutIssues, FString::Printf(TEXT("%s will add a fallback temporary die because TransformDiceDefinition is missing."), *EffectLabel));
			}
			break;
		case EGambitItemEffectType::SetDieComboContributionCount:
			if (EffectDefinition->ComboContributionCount < 0)
			{
				AddError(OutIssues, FString::Printf(TEXT("%s has a negative ComboContributionCount."), *EffectLabel));
			}
			break;
		case EGambitItemEffectType::AddDieRuntimeTags:
			if (EffectDefinition->RuntimeTagsToAdd.Num() == 0)
			{
				AddError(OutIssues, FString::Printf(TEXT("%s requires at least one RuntimeTagsToAdd entry."), *EffectLabel));
			}
			break;
		case EGambitItemEffectType::RemoveDieRuntimeTags:
			if (EffectDefinition->RuntimeTagsToRemove.Num() == 0)
			{
				AddError(OutIssues, FString::Printf(TEXT("%s requires at least one RuntimeTagsToRemove entry."), *EffectLabel));
			}
			break;
		case EGambitItemEffectType::AddShopDiscountPercent:
		case EGambitItemEffectType::AddShopSurchargePercent:
		case EGambitItemEffectType::AddShopCashbackPercent:
			if (FMath::IsNearlyZero(ResolveScalarParameter(EffectDefinition, TEXT("Percent"), EffectDefinition->Amount)))
			{
				AddError(OutIssues, FString::Printf(TEXT("%s requires Amount or ScalarParameters[Percent]."), *EffectLabel));
			}
			break;
		case EGambitItemEffectType::AddSharedPoolStock:
		case EGambitItemEffectType::SetSharedPoolPurchaseLimit:
		case EGambitItemEffectType::SetSharedPoolItemUnavailable:
			if (EffectDefinition->EffectType != EGambitItemEffectType::SetSharedPoolItemUnavailable
				&& !HasConfiguredAmount(EffectDefinition))
			{
				AddError(OutIssues, FString::Printf(TEXT("%s requires Amount or ScalarParameters[Amount]."), *EffectLabel));
			}
			if (!IsShopContextHook(EffectDefinition->Hook) && !EffectDefinition->ReferencedItemDefinition)
			{
				AddWarning(
					OutIssues,
					FString::Printf(
						TEXT("%s relies on shop purchase context; set ReferencedItemDefinition when using this outside shop hooks."),
						*EffectLabel));
			}
			break;
		default:
			break;
		}
	}

	void ValidateEffectDefinitions(
		const TArray<TObjectPtr<UGambitItemEffectDefinition>>& EffectDefinitions,
		const FString& OwnerLabel,
		TArray<FGambitDataValidationIssue>& OutIssues)
	{
		for (int32 EffectIndex = 0; EffectIndex < EffectDefinitions.Num(); ++EffectIndex)
		{
			const UGambitItemEffectDefinition* EffectDefinition = EffectDefinitions[EffectIndex];
			GambitDataValidation::ValidateEffectDefinition(EffectDefinition, OwnerLabel, EffectIndex, OutIssues);
			if (EffectDefinition
				&& EffectDefinition->EffectType == EGambitItemEffectType::CopyLastTriggeredEffect
				&& EffectIndex == 0)
			{
				AddError(
					OutIssues,
					FString::Printf(
						TEXT("%s EffectDefinitions[0] copies the previous effect but no previous effect exists."),
						*OwnerLabel));
			}
		}
	}

	void ValidateEffectClasses(
		const TArray<TSubclassOf<UGambitItemEffect>>& EffectClasses,
		const FString& OwnerLabel,
		TArray<FGambitDataValidationIssue>& OutIssues)
	{
		for (int32 EffectIndex = 0; EffectIndex < EffectClasses.Num(); ++EffectIndex)
		{
			if (!EffectClasses[EffectIndex])
			{
				AddError(
					OutIssues,
					FString::Printf(TEXT("%s EffectClasses[%d] is empty."), *OwnerLabel, EffectIndex));
			}
		}
	}

	bool HasUsableEffectPayload(const UGambitItemDefinition* ItemDefinition)
	{
		return ItemDefinition
			&& (ItemDefinition->EffectDefinitions.Num() > 0 || ItemDefinition->EffectClasses.Num() > 0);
	}
}

void GambitDataValidation::AddIssue(
	TArray<FGambitDataValidationIssue>& OutIssues,
	const EGambitDataValidationSeverity Severity,
	const FString& Message)
{
	OutIssues.Emplace(Severity, Message);
}

bool GambitDataValidation::HasBlockingIssues(const TArray<FGambitDataValidationIssue>& Issues)
{
	return Issues.ContainsByPredicate([](const FGambitDataValidationIssue& Issue)
	{
		return Issue.IsBlocking();
	});
}

FString GambitDataValidation::SeverityToString(const EGambitDataValidationSeverity Severity)
{
	switch (Severity)
	{
	case EGambitDataValidationSeverity::Info: return TEXT("Info");
	case EGambitDataValidationSeverity::Warning: return TEXT("Warning");
	case EGambitDataValidationSeverity::Error: return TEXT("Error");
	default: return TEXT("Unknown");
	}
}

FString GambitDataValidation::EffectTypeToString(const EGambitItemEffectType EffectType)
{
	return EnumValueToString(StaticEnum<EGambitItemEffectType>(), static_cast<int64>(EffectType));
}

FString GambitDataValidation::EffectHookToString(const EGambitEffectHook Hook)
{
	return EnumValueToString(StaticEnum<EGambitEffectHook>(), static_cast<int64>(Hook));
}

FString GambitDataValidation::EffectTargetToString(const EGambitEffectTarget Target)
{
	return EnumValueToString(StaticEnum<EGambitEffectTarget>(), static_cast<int64>(Target));
}

bool GambitDataValidation::IsSupportedEffectType(const EGambitItemEffectType EffectType)
{
	if (!IsValidEnumValue(EffectType))
	{
		return false;
	}

	switch (EffectType)
	{
	case EGambitItemEffectType::None:
		return false;
	case EGambitItemEffectType::ScoreModifier:
	case EGambitItemEffectType::AddScoreFlat:
	case EGambitItemEffectType::MultiplyScore:
	case EGambitItemEffectType::MultiplyDiceContribution:
	case EGambitItemEffectType::AddGold:
	case EGambitItemEffectType::SpendGold:
	case EGambitItemEffectType::ModifyDieValue:
	case EGambitItemEffectType::SetDieValue:
	case EGambitItemEffectType::LockDie:
	case EGambitItemEffectType::RerollDie:
	case EGambitItemEffectType::AddReroll:
	case EGambitItemEffectType::ModifyRerollLimit:
	case EGambitItemEffectType::GrantConsumable:
	case EGambitItemEffectType::AddTemporaryScoreModifier:
	case EGambitItemEffectType::StealScore:
	case EGambitItemEffectType::StealGold:
	case EGambitItemEffectType::PreventNegativeEffect:
	case EGambitItemEffectType::DestroyOrRemoveDiceChance:
	case EGambitItemEffectType::TransformDiceForRound:
	case EGambitItemEffectType::AddTemporaryDie:
	case EGambitItemEffectType::SetDieScoreContributionValue:
	case EGambitItemEffectType::SetDieComboContributionCount:
	case EGambitItemEffectType::SetDieCountsForScoreSum:
	case EGambitItemEffectType::SetDieCountsForCombinations:
	case EGambitItemEffectType::SetDieCanBeRerolled:
	case EGambitItemEffectType::SetDieCanBeLocked:
	case EGambitItemEffectType::MarkDieDestroyedAfterRound:
	case EGambitItemEffectType::AddDieRuntimeTags:
	case EGambitItemEffectType::RemoveDieRuntimeTags:
	case EGambitItemEffectType::ModifyShopOfferCount:
	case EGambitItemEffectType::AddShopDiscountPercent:
	case EGambitItemEffectType::AddShopSurchargePercent:
	case EGambitItemEffectType::AddShopFlatPriceDelta:
	case EGambitItemEffectType::MakeShopOfferFree:
	case EGambitItemEffectType::AddShopCashbackPercent:
	case EGambitItemEffectType::AddPurchaseGoldDelta:
	case EGambitItemEffectType::BlockShopPurchase:
	case EGambitItemEffectType::ModifyDebtLimit:
	case EGambitItemEffectType::ModifyMaxGold:
	case EGambitItemEffectType::ModifyInterestInterval:
	case EGambitItemEffectType::ModifyMaxInterest:
	case EGambitItemEffectType::ModifyInterestBonus:
	case EGambitItemEffectType::AddRecurringGoldIncome:
	case EGambitItemEffectType::SellItem:
	case EGambitItemEffectType::SellDie:
	case EGambitItemEffectType::AddSharedPoolStock:
	case EGambitItemEffectType::SetSharedPoolPurchaseLimit:
	case EGambitItemEffectType::SetSharedPoolItemUnavailable:
	case EGambitItemEffectType::CopyLastTriggeredEffect:
		return true;
	default:
		return false;
	}
}

void GambitDataValidation::ValidateDiceDefinition(
	const UGambitDiceDefinition* DiceDefinition,
	TArray<FGambitDataValidationIssue>& OutIssues)
{
	if (!DiceDefinition)
	{
		AddError(OutIssues, TEXT("Dice definition is missing."));
		return;
	}

	const FString Label = FString::Printf(TEXT("Dice %s"), *ObjectLabel(DiceDefinition));
	if (DiceDefinition->DiceId.IsNone())
	{
		AddError(OutIssues, FString::Printf(TEXT("%s is missing DiceId."), *Label));
	}

	if (!IsValidEnumValue(DiceDefinition->DiceType))
	{
		AddError(OutIssues, FString::Printf(TEXT("%s has an invalid dice type."), *Label));
	}

	if (!IsValidEnumValue(DiceDefinition->Rarity))
	{
		AddError(OutIssues, FString::Printf(TEXT("%s has an invalid rarity."), *Label));
	}

	if (DiceDefinition->Faces.Num() == 0)
	{
		if (!DiceDefinition->bAllowDefaultFacesFallback)
		{
			AddError(OutIssues, FString::Printf(TEXT("%s has no faces and has not explicitly allowed default D6 fallback faces."), *Label));
		}
		else
		{
			AddWarning(OutIssues, FString::Printf(TEXT("%s uses explicit default D6 fallback faces."), *Label));
		}
	}

	if (DiceDefinition->DefaultComboContributionCount < 0)
	{
		AddError(OutIssues, FString::Printf(TEXT("%s has a negative DefaultComboContributionCount."), *Label));
	}

	const int32 FaceCount = DiceDefinition->Faces.Num() > 0
		? DiceDefinition->Faces.Num()
		: (DiceDefinition->bAllowDefaultFacesFallback ? DefaultFallbackFaceCount : 0);
	if (DiceDefinition->FaceWeights.Num() > 0)
	{
		if (FaceCount > 0 && DiceDefinition->FaceWeights.Num() != FaceCount)
		{
			AddError(
				OutIssues,
				FString::Printf(
					TEXT("%s has %d face weights for %d resolved faces."),
					*Label,
					DiceDefinition->FaceWeights.Num(),
					FaceCount));
		}

		float TotalWeight = 0.0f;
		for (int32 WeightIndex = 0; WeightIndex < DiceDefinition->FaceWeights.Num(); ++WeightIndex)
		{
			const float Weight = DiceDefinition->FaceWeights[WeightIndex];
			if (Weight < 0.0f)
			{
				AddError(OutIssues, FString::Printf(TEXT("%s has a negative face weight at index %d."), *Label, WeightIndex));
			}
			TotalWeight += FMath::Max(0.0f, Weight);
		}

		if (TotalWeight <= 0.0f)
		{
			AddError(OutIssues, FString::Printf(TEXT("%s has face weights but their total weight is not positive."), *Label));
		}
	}

	ValidateEffectDefinitions(DiceDefinition->EffectDefinitions, Label, OutIssues);
	ValidateEffectClasses(DiceDefinition->EffectClasses, Label, OutIssues);
}

void GambitDataValidation::ValidateItemDefinition(
	const UGambitItemDefinition* ItemDefinition,
	TArray<FGambitDataValidationIssue>& OutIssues)
{
	if (!ItemDefinition)
	{
		AddError(OutIssues, TEXT("Item definition is missing."));
		return;
	}

	const FString Label = FString::Printf(TEXT("Item %s"), *ObjectLabel(ItemDefinition));
	if (ItemDefinition->ItemId.IsNone())
	{
		AddError(OutIssues, FString::Printf(TEXT("%s is missing ItemId."), *Label));
	}

	if (!IsValidEnumValue(ItemDefinition->ItemType) || ItemDefinition->ItemType == EGambitItemType::None)
	{
		AddError(OutIssues, FString::Printf(TEXT("%s has an invalid item type."), *Label));
	}

	if (!IsValidEnumValue(ItemDefinition->Rarity))
	{
		AddError(OutIssues, FString::Printf(TEXT("%s has an invalid rarity."), *Label));
	}

	if (ItemDefinition->Cost < 0)
	{
		AddError(OutIssues, FString::Printf(TEXT("%s has a negative cost."), *Label));
	}

	if (ItemDefinition->bUsesSharedPool && ItemDefinition->SharedPoolMaxStock <= 0)
	{
		AddError(OutIssues, FString::Printf(TEXT("%s uses the shared pool but has no positive SharedPoolMaxStock."), *Label));
	}

	if (ItemDefinition->bUsesSharedPool
		&& ItemDefinition->SharedPoolPurchaseLimit > 0
		&& ItemDefinition->SharedPoolMaxStock > 0
		&& ItemDefinition->SharedPoolPurchaseLimit > ItemDefinition->SharedPoolMaxStock)
	{
		AddWarning(OutIssues, FString::Printf(TEXT("%s has a purchase limit above its max shared stock."), *Label));
	}

	ValidateEffectDefinitions(ItemDefinition->EffectDefinitions, Label, OutIssues);
	ValidateEffectClasses(ItemDefinition->EffectClasses, Label, OutIssues);

	if (Cast<UGambitModuleDefinition>(ItemDefinition))
	{
		if (ItemDefinition->ItemType != EGambitItemType::Module)
		{
			AddError(OutIssues, FString::Printf(TEXT("%s is a module class but ItemType is not Module."), *Label));
		}

		if (!HasUsableEffectPayload(ItemDefinition))
		{
			AddError(OutIssues, FString::Printf(TEXT("%s is a module with no effect payload."), *Label));
		}
	}

	if (const UGambitConsumableDefinition* ConsumableDefinition = Cast<UGambitConsumableDefinition>(ItemDefinition))
	{
		if (ItemDefinition->ItemType != EGambitItemType::Consumable)
		{
			AddError(OutIssues, FString::Printf(TEXT("%s is a consumable class but ItemType is not Consumable."), *Label));
		}

		bool bHasUsablePhase = false;
		for (const EGambitRoundPhase UsablePhase : ConsumableDefinition->UsablePhases)
		{
			if (!IsValidEnumValue(UsablePhase))
			{
				AddError(OutIssues, FString::Printf(TEXT("%s has an invalid usable phase."), *Label));
				continue;
			}

			if (UsablePhase != EGambitRoundPhase::None)
			{
				bHasUsablePhase = true;
			}
		}

		if (!bHasUsablePhase)
		{
			AddError(OutIssues, FString::Printf(TEXT("%s is a consumable with no usable phases."), *Label));
		}

		for (int32 EffectIndex = 0; EffectIndex < ItemDefinition->EffectDefinitions.Num(); ++EffectIndex)
		{
			const UGambitItemEffectDefinition* EffectDefinition = ItemDefinition->EffectDefinitions[EffectIndex].Get();
			if (!EffectDefinition || EffectDefinition->Hook == EGambitEffectHook::ConsumableUse)
			{
				continue;
			}

			AddError(
				OutIssues,
				FString::Printf(
					TEXT("%s effect[%d] uses hook %s; player-activated consumables must execute with ConsumableUse. UsablePhases only controls activation windows."),
					*Label,
					EffectIndex,
					*GambitDataValidation::EffectHookToString(EffectDefinition->Hook)));
		}

		if (!HasUsableEffectPayload(ItemDefinition))
		{
			AddError(OutIssues, FString::Printf(TEXT("%s is a consumable with no effect payload."), *Label));
		}

		if (ConsumableDefinition->bCanTargetOpponent)
		{
			const bool bHasOpponentTargetEffect = ItemDefinition->EffectDefinitions.ContainsByPredicate(
				[](const TObjectPtr<UGambitItemEffectDefinition>& EffectDefinition)
				{
					return EffectRequestsOpponentTarget(EffectDefinition.Get())
						|| EffectHasOpponentTargetRule(EffectDefinition.Get());
				});
			if (!bHasOpponentTargetEffect)
			{
				AddError(
					OutIssues,
					FString::Printf(
						TEXT("%s can target opponents but has no effect definition with an opponent target rule."),
						*Label));
			}
		}
	}

	if (const UGambitDiceItemDefinition* DiceItemDefinition = Cast<UGambitDiceItemDefinition>(ItemDefinition))
	{
		if (ItemDefinition->ItemType != EGambitItemType::Dice)
		{
			AddError(OutIssues, FString::Printf(TEXT("%s is a dice item class but ItemType is not Dice."), *Label));
		}

		if (!DiceItemDefinition->GrantedDiceDefinition)
		{
			AddError(OutIssues, FString::Printf(TEXT("%s is missing GrantedDiceDefinition."), *Label));
		}
	}
}

void GambitDataValidation::ValidateEffectDefinition(
	const UGambitItemEffectDefinition* EffectDefinition,
	const FString& OwnerLabel,
	const int32 EffectIndex,
	TArray<FGambitDataValidationIssue>& OutIssues)
{
	const FString EffectLabel = FString::Printf(TEXT("%s EffectDefinitions[%d]"), *OwnerLabel, EffectIndex);
	if (!EffectDefinition)
	{
		AddError(OutIssues, FString::Printf(TEXT("%s is empty."), *EffectLabel));
		return;
	}

	const FString FullEffectLabel = FString::Printf(TEXT("%s (%s)"), *EffectLabel, *ObjectLabel(EffectDefinition));
	if (EffectDefinition->EffectId.IsNone())
	{
		AddWarning(OutIssues, FString::Printf(TEXT("%s is missing EffectId."), *FullEffectLabel));
	}

	if (!IsValidEnumValue(EffectDefinition->Hook))
	{
		AddError(OutIssues, FString::Printf(TEXT("%s has an invalid hook."), *FullEffectLabel));
	}

	if (!IsValidEnumValue(EffectDefinition->Target))
	{
		AddError(OutIssues, FString::Printf(TEXT("%s has an invalid target."), *FullEffectLabel));
	}

	if (EffectDefinition->EffectType == EGambitItemEffectType::None)
	{
		AddError(OutIssues, FString::Printf(TEXT("%s has no effect type."), *FullEffectLabel));
	}
	else if (!IsSupportedEffectType(EffectDefinition->EffectType))
	{
		AddError(
			OutIssues,
			FString::Printf(
				TEXT("%s uses unsupported effect type %s."),
				*FullEffectLabel,
				*EffectTypeToString(EffectDefinition->EffectType)));
	}

	if (!EffectDefinition->TargetRuleId.IsNone()
		&& !GambitEffectTargetRules::IsKnownRule(EffectDefinition->TargetRuleId))
	{
		AddError(
			OutIssues,
			FString::Printf(
				TEXT("%s uses unknown TargetRuleId %s."),
				*FullEffectLabel,
				*GambitEffectTargetRules::DescribeRule(EffectDefinition->TargetRuleId)));
	}

	if (EffectRequestsOpponentTarget(EffectDefinition) && EffectDefinition->TargetRuleId.IsNone())
	{
		AddError(
			OutIssues,
			FString::Printf(
				TEXT("%s targets an opponent but TargetRuleId is missing."),
				*FullEffectLabel));
	}

	ValidateTargetRuleCompatibility(EffectDefinition, FullEffectLabel, OutIssues);
	ValidateNegativeEffectDefenseConfiguration(EffectDefinition, FullEffectLabel, OutIssues);
	ValidateEffectParameters(EffectDefinition, FullEffectLabel, OutIssues);
	ValidateEffectConditions(EffectDefinition, FullEffectLabel, OutIssues);
}

void GambitDataValidation::ValidateShopLootTable(
	const UGambitShopLootTable* LootTable,
	const UGambitItemCatalogDataAsset* FallbackCatalog,
	TArray<FGambitDataValidationIssue>& OutIssues)
{
	if (!LootTable)
	{
		AddError(OutIssues, TEXT("Shop loot table is missing."));
		return;
	}

	const FString Label = FString::Printf(TEXT("ShopLootTable %s"), *ObjectLabel(LootTable));
	if (LootTable->LootTableId.IsNone())
	{
		AddError(OutIssues, FString::Printf(TEXT("%s is missing LootTableId."), *Label));
	}

	const UGambitItemCatalogDataAsset* Catalog = LootTable->ItemCatalog ? LootTable->ItemCatalog.Get() : FallbackCatalog;
	if (!Catalog)
	{
		AddWarning(OutIssues, FString::Printf(TEXT("%s has no item catalog; entries with only ItemId cannot be resolved."), *Label));
	}

	if (LootTable->OfferCountOverride < 0)
	{
		AddError(OutIssues, FString::Printf(TEXT("%s has a negative OfferCountOverride."), *Label));
	}

	if (LootTable->WeightedItems.Num() == 0)
	{
		AddError(OutIssues, FString::Printf(TEXT("%s has no weighted item entries."), *Label));
	}

	TSet<FName> SeenItemIds;
	for (int32 EntryIndex = 0; EntryIndex < LootTable->WeightedItems.Num(); ++EntryIndex)
	{
		const FGambitShopWeightedEntry& Entry = LootTable->WeightedItems[EntryIndex];
		const UGambitItemDefinition* ItemDefinition = ResolveDataValidationCatalogEntryDefinition(Entry.ItemId, Entry.ItemDefinition, Catalog);
		const FName ItemId = ResolveDataValidationCatalogEntryId(Entry.ItemId, ItemDefinition);
		const FString EntryLabel = FString::Printf(TEXT("%s WeightedItems[%d]"), *Label, EntryIndex);

		if (!ItemDefinition && Entry.ItemId.IsNone())
		{
			AddError(OutIssues, FString::Printf(TEXT("%s has neither ItemDefinition nor ItemId."), *EntryLabel));
		}

		if (!Entry.ItemId.IsNone() && Catalog && !ItemDefinition)
		{
			AddError(OutIssues, FString::Printf(TEXT("%s cannot resolve ItemId %s from its catalog."), *EntryLabel, *Entry.ItemId.ToString()));
		}

		if (ItemDefinition)
		{
			ValidateItemDefinition(ItemDefinition, OutIssues);
		}

		if (!ItemId.IsNone())
		{
			if (SeenItemIds.Contains(ItemId))
			{
				AddWarning(OutIssues, FString::Printf(TEXT("%s duplicates item id %s."), *EntryLabel, *ItemId.ToString()));
			}
			SeenItemIds.Add(ItemId);
		}

		if (Entry.Weight <= 0.0f)
		{
			AddError(OutIssues, FString::Printf(TEXT("%s has a non-positive weight."), *EntryLabel));
		}

		if (Entry.PriceOverride < -1)
		{
			AddError(OutIssues, FString::Printf(TEXT("%s has an invalid negative PriceOverride."), *EntryLabel));
		}

		const int32 ResolvedPrice = Entry.PriceOverride >= 0 ? Entry.PriceOverride : (ItemDefinition ? ItemDefinition->Cost : 0);
		if (ResolvedPrice < 0)
		{
			AddError(OutIssues, FString::Printf(TEXT("%s resolves to a negative price."), *EntryLabel));
		}
	}
}

void GambitDataValidation::ValidateSharedPoolDefinition(
	const UGambitSharedPoolDefinition* SharedPoolDefinition,
	const UGambitItemCatalogDataAsset* FallbackCatalog,
	TArray<FGambitDataValidationIssue>& OutIssues)
{
	if (!SharedPoolDefinition)
	{
		AddError(OutIssues, TEXT("Shared pool definition is missing."));
		return;
	}

	const FString Label = FString::Printf(TEXT("SharedPool %s"), *ObjectLabel(SharedPoolDefinition));
	if (SharedPoolDefinition->SharedPoolId.IsNone())
	{
		AddError(OutIssues, FString::Printf(TEXT("%s is missing SharedPoolId."), *Label));
	}

	const UGambitItemCatalogDataAsset* Catalog = SharedPoolDefinition->ItemCatalog
		? SharedPoolDefinition->ItemCatalog.Get()
		: FallbackCatalog;
	if (!Catalog)
	{
		AddWarning(OutIssues, FString::Printf(TEXT("%s has no item catalog; entries with only ItemId cannot be resolved."), *Label));
	}

	if (SharedPoolDefinition->StockEntries.Num() == 0)
	{
		AddWarning(OutIssues, FString::Printf(TEXT("%s has no stock entries."), *Label));
	}

	TSet<FName> SeenItemIds;
	for (int32 EntryIndex = 0; EntryIndex < SharedPoolDefinition->StockEntries.Num(); ++EntryIndex)
	{
		const FGambitSharedStockEntry& Entry = SharedPoolDefinition->StockEntries[EntryIndex];
		const UGambitItemDefinition* ItemDefinition = ResolveDataValidationCatalogEntryDefinition(Entry.ItemId, Entry.ItemDefinition, Catalog);
		const FName ItemId = ResolveDataValidationCatalogEntryId(Entry.ItemId, ItemDefinition);
		const FString EntryLabel = FString::Printf(TEXT("%s StockEntries[%d]"), *Label, EntryIndex);

		if (!ItemDefinition && Entry.ItemId.IsNone())
		{
			AddError(OutIssues, FString::Printf(TEXT("%s has neither ItemDefinition nor ItemId."), *EntryLabel));
		}

		if (!Entry.ItemId.IsNone() && Catalog && !ItemDefinition)
		{
			AddError(OutIssues, FString::Printf(TEXT("%s cannot resolve ItemId %s from its catalog."), *EntryLabel, *Entry.ItemId.ToString()));
		}

		if (ItemDefinition)
		{
			ValidateItemDefinition(ItemDefinition, OutIssues);
			if (!ItemDefinition->bUsesSharedPool)
			{
				AddWarning(OutIssues, FString::Printf(TEXT("%s references an item that is not marked bUsesSharedPool."), *EntryLabel));
			}
		}

		if (!ItemId.IsNone())
		{
			if (SeenItemIds.Contains(ItemId))
			{
				AddWarning(OutIssues, FString::Printf(TEXT("%s duplicates item id %s."), *EntryLabel, *ItemId.ToString()));
			}
			SeenItemIds.Add(ItemId);
		}

		const int32 ResolvedMaxStock = Entry.MaxStock > 0 ? Entry.MaxStock : (ItemDefinition ? ItemDefinition->SharedPoolMaxStock : 0);
		if (ResolvedMaxStock <= 0)
		{
			AddError(OutIssues, FString::Printf(TEXT("%s has no positive stock."), *EntryLabel));
		}

		if (Entry.GlobalPurchaseLimit < 0)
		{
			AddError(OutIssues, FString::Printf(TEXT("%s has a negative GlobalPurchaseLimit."), *EntryLabel));
		}
	}
}

#if WITH_EDITOR
EDataValidationResult GambitDataValidation::ApplyIssuesToContext(
	const TArray<FGambitDataValidationIssue>& Issues,
	FDataValidationContext& Context,
	const EDataValidationResult DefaultResult)
{
	bool bHasError = DefaultResult == EDataValidationResult::Invalid;
	for (const FGambitDataValidationIssue& Issue : Issues)
	{
		switch (Issue.Severity)
		{
		case EGambitDataValidationSeverity::Error:
			Context.AddError(FText::FromString(Issue.Message));
			bHasError = true;
			break;
		case EGambitDataValidationSeverity::Warning:
			Context.AddWarning(FText::FromString(Issue.Message));
			break;
		case EGambitDataValidationSeverity::Info:
			break;
		default:
			break;
		}
	}

	if (bHasError)
	{
		return EDataValidationResult::Invalid;
	}

	return DefaultResult == EDataValidationResult::NotValidated
		? EDataValidationResult::Valid
		: DefaultResult;
}
#endif
