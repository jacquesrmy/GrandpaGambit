#include "Items/Effects/GambitEffectResolver.h"

#include "Core/Logging/GambitLog.h"
#include "Data/Assets/GambitItemEffectDefinition.h"
#include "Dice/Data/GambitDiceDefinition.h"
#include "Items/Consumables/GambitConsumableDefinition.h"
#include "Items/Data/GambitItemDefinition.h"
#include "Items/Effects/GambitEffectTargetRules.h"
#include "Items/Effects/GambitItemEffect.h"
#include "Players/Components/GambitDiceComponent.h"
#include "Players/Components/GambitEconomyComponent.h"
#include "Players/Components/GambitInventoryComponent.h"
#include "Players/States/GambitPlayerState.h"
#include "Managers/SharedPool/GambitSharedPoolComponent.h"

namespace
{
	FString GetItemName(const UGambitItemDefinition* ItemDefinition)
	{
		if (!ItemDefinition)
		{
			return TEXT("None");
		}

		if (!ItemDefinition->DisplayName.IsEmpty())
		{
			return ItemDefinition->DisplayName.ToString();
		}

		return ItemDefinition->GetResolvedItemId().ToString();
	}

	FString GetEffectName(const UGambitItemEffectDefinition* EffectDefinition)
	{
		if (!EffectDefinition)
		{
			return TEXT("None");
		}

		if (!EffectDefinition->EffectId.IsNone())
		{
			return EffectDefinition->EffectId.ToString();
		}

		return EffectDefinition->GetName();
	}

	FName GetEffectSourceId(const UGambitItemEffectDefinition* EffectDefinition)
	{
		if (!EffectDefinition)
		{
			return NAME_None;
		}

		if (!EffectDefinition->EffectId.IsNone())
		{
			return EffectDefinition->EffectId;
		}

		return FName(*EffectDefinition->GetPathName());
	}

	FString GetDiceName(const UGambitDiceDefinition* DiceDefinition)
	{
		if (!DiceDefinition)
		{
			return TEXT("None");
		}

		if (!DiceDefinition->DisplayName.IsEmpty())
		{
			return DiceDefinition->DisplayName.ToString();
		}

		return DiceDefinition->GetResolvedDiceId().ToString();
	}

	float ResolveScalar(const UGambitItemEffectDefinition* EffectDefinition, const FName ParameterName, const float Fallback)
	{
		if (!EffectDefinition)
		{
			return Fallback;
		}

		if (const float* Value = EffectDefinition->ScalarParameters.Find(ParameterName))
		{
			return *Value;
		}

		return Fallback;
	}

	int32 ResolveIntScalar(const UGambitItemEffectDefinition* EffectDefinition, const FName ParameterName, const int32 Fallback)
	{
		return FMath::RoundToInt(ResolveScalar(EffectDefinition, ParameterName, static_cast<float>(Fallback)));
	}

	int32 ResolveDurationRounds(const UGambitItemEffectDefinition* EffectDefinition)
	{
		return FMath::Max(0, ResolveIntScalar(EffectDefinition, TEXT("Rounds"), EffectDefinition ? EffectDefinition->DurationRounds : 0));
	}

	float ResolveMultiplier(const UGambitItemEffectDefinition* EffectDefinition)
	{
		const float Fallback = EffectDefinition ? EffectDefinition->Multiplier : 1.0f;
		return ResolveScalar(EffectDefinition, TEXT("Multiplier"), Fallback);
	}

	bool CompareFloat(const float Left, const EGambitEffectComparison Comparison, const float Right)
	{
		switch (Comparison)
		{
		case EGambitEffectComparison::Equal: return FMath::IsNearlyEqual(Left, Right);
		case EGambitEffectComparison::NotEqual: return !FMath::IsNearlyEqual(Left, Right);
		case EGambitEffectComparison::Greater: return Left > Right;
		case EGambitEffectComparison::GreaterOrEqual: return Left >= Right || FMath::IsNearlyEqual(Left, Right);
		case EGambitEffectComparison::Lower: return Left < Right;
		case EGambitEffectComparison::LowerOrEqual: return Left <= Right || FMath::IsNearlyEqual(Left, Right);
		default: return false;
		}
	}

	AGambitPlayerState* ResolvePlayer(const FGambitEffectExecutionContext& Context, const EGambitEffectTarget Target)
	{
		return Target == EGambitEffectTarget::Target ? Context.TargetPlayer.Get() : Context.SourcePlayer.Get();
	}

	UGambitDiceComponent* ResolveDiceComponent(const FGambitEffectExecutionContext& Context, const EGambitEffectTarget Target)
	{
		return Target == EGambitEffectTarget::Target ? Context.TargetDiceComponent.Get() : Context.SourceDiceComponent.Get();
	}

	UGambitEconomyComponent* ResolveEconomyComponent(const FGambitEffectExecutionContext& Context, const EGambitEffectTarget Target)
	{
		return Target == EGambitEffectTarget::Target ? Context.TargetEconomyComponent.Get() : Context.SourceEconomyComponent.Get();
	}

	UGambitInventoryComponent* ResolveInventoryComponent(const FGambitEffectExecutionContext& Context, const EGambitEffectTarget Target)
	{
		return Target == EGambitEffectTarget::Target ? Context.TargetInventoryComponent.Get() : Context.SourceInventoryComponent.Get();
	}

	UGambitItemDefinition* ResolveReferencedShopItem(const UGambitItemEffectDefinition* EffectDefinition, const FGambitEffectExecutionContext& Context)
	{
		if (EffectDefinition && EffectDefinition->ReferencedItemDefinition)
		{
			return EffectDefinition->ReferencedItemDefinition;
		}

		return Context.ShopPurchase.ItemDefinition.Get();
	}

	TArray<FGambitDieRuntimeState>& ResolveDiceSnapshot(FGambitEffectExecutionContext& Context, const EGambitEffectTarget Target)
	{
		return Target == EGambitEffectTarget::Target ? Context.TargetDice : Context.SourceDice;
	}

	const TArray<FGambitDieRuntimeState>& ResolveDiceSnapshotConst(const FGambitEffectExecutionContext& Context, const EGambitEffectTarget Target)
	{
		return Target == EGambitEffectTarget::Target ? Context.TargetDice : Context.SourceDice;
	}

	void RefreshDiceSnapshot(FGambitEffectExecutionContext& Context, const EGambitEffectTarget Target)
	{
		if (const UGambitDiceComponent* DiceComponent = ResolveDiceComponent(Context, Target))
		{
			ResolveDiceSnapshot(Context, Target) = DiceComponent->GetDiceStates();
		}
	}

	void RefreshRuntimeScoreContribution(FGambitDieRuntimeState& DieState)
	{
		if (const UGambitDiceDefinition* DiceDefinition = DieState.DiceDefinition.Get())
		{
			DieState.ScoreContributionValue = DiceDefinition->bOverrideScoreContributionValue
				? DiceDefinition->ScoreContributionValueOverride
				: DieState.EffectiveValue;
			return;
		}

		DieState.ScoreContributionValue = DieState.EffectiveValue;
	}

	int32 ResolveSelectedDieHandIndex(const FGambitEffectExecutionContext& Context, const EGambitEffectTarget Target)
	{
		return Target == EGambitEffectTarget::Target ? Context.TargetDieHandIndex : Context.SourceDieHandIndex;
	}

	TArray<int32> BuildAffectedDieIndexes(
		const UGambitItemEffectDefinition* EffectDefinition,
		const FGambitEffectExecutionContext& Context,
		const EGambitEffectTarget Target,
		const TArray<FGambitDieRuntimeState>& DiceStates)
	{
		TArray<int32> Indexes;
		if (DiceStates.Num() == 0)
		{
			return Indexes;
		}

		if (EffectDefinition && EffectDefinition->bAffectAllDice)
		{
			Indexes.Reserve(DiceStates.Num());
			for (int32 Index = 0; Index < DiceStates.Num(); ++Index)
			{
				Indexes.Add(Index);
			}
			return Indexes;
		}

		if (EffectDefinition && GambitEffectTargetRules::IsSelectedDieRule(EffectDefinition->TargetRuleId))
		{
			const int32 SelectedDieIndex = ResolveSelectedDieHandIndex(Context, Target);
			if (DiceStates.IsValidIndex(SelectedDieIndex))
			{
				Indexes.Add(SelectedDieIndex);
			}
			return Indexes;
		}

		if (EffectDefinition && GambitEffectTargetRules::IsFirstRerolledDieRule(EffectDefinition->TargetRuleId))
		{
			const int32 FirstRerolledDieIndex = Context.FirstRerolledDieHandIndexThisRound;
			if (DiceStates.IsValidIndex(FirstRerolledDieIndex))
			{
				Indexes.Add(FirstRerolledDieIndex);
			}
			return Indexes;
		}

		const int32 RequestedIndex = ResolveIntScalar(EffectDefinition, TEXT("DieIndex"), EffectDefinition ? EffectDefinition->DieIndex : INDEX_NONE);
		if (DiceStates.IsValidIndex(RequestedIndex))
		{
			Indexes.Add(RequestedIndex);
			return Indexes;
		}

		if (Target == EGambitEffectTarget::Source && DiceStates.IsValidIndex(Context.SourceDieHandIndex))
		{
			Indexes.Add(Context.SourceDieHandIndex);
			return Indexes;
		}

		Indexes.Add(0);
		return Indexes;
	}

	float CalculateDiceAverage(const TArray<FGambitDieRuntimeState>& DiceStates)
	{
		if (DiceStates.Num() == 0)
		{
			return 0.0f;
		}

		float DiceValueSum = 0.0f;
		for (const FGambitDieRuntimeState& DieState : DiceStates)
		{
			DiceValueSum += static_cast<float>(DieState.EffectiveValue);
		}

		return DiceValueSum / static_cast<float>(DiceStates.Num());
	}

	bool IsEvenDiceValue(const int32 Value)
	{
		return Value % 2 == 0;
	}

	bool IsOddDiceValue(const int32 Value)
	{
		return FMath::Abs(Value) % 2 == 1;
	}

	bool DoesDieHaveTag(const FGambitDieRuntimeState& DieState, const FName Tag)
	{
		if (Tag.IsNone())
		{
			return false;
		}

		if (DieState.RuntimeTags.Contains(Tag))
		{
			return true;
		}

		const UGambitDiceDefinition* DiceDefinition = DieState.DiceDefinition.Get();
		return DiceDefinition && DiceDefinition->Tags.Contains(Tag);
	}

	bool EffectResolverRarityMatches(const EGambitItemRarity ActualRarity, const EGambitItemRarity ExpectedRarity, const bool bAtLeastRarity)
	{
		if (bAtLeastRarity)
		{
			return static_cast<int32>(ActualRarity) >= static_cast<int32>(ExpectedRarity);
		}

		return ActualRarity == ExpectedRarity;
	}

	int32 CountMatchingDiceForCondition(
		const TArray<FGambitDieRuntimeState>& DiceStates,
		const FGambitEffectConditionDefinition& Condition)
	{
		int32 MatchCount = 0;
		for (const FGambitDieRuntimeState& DieState : DiceStates)
		{
			switch (Condition.ConditionType)
			{
			case EGambitEffectConditionType::DieValueEquals:
			case EGambitEffectConditionType::HasAtLeastMatchingDice:
				if (DieState.EffectiveValue == Condition.Value)
				{
					MatchCount++;
				}
				break;
			case EGambitEffectConditionType::DieValueGreater:
				if (DieState.EffectiveValue > Condition.Value)
				{
					MatchCount++;
				}
				break;
			case EGambitEffectConditionType::DieValueLower:
				if (DieState.EffectiveValue < Condition.Value)
				{
					MatchCount++;
				}
				break;
			case EGambitEffectConditionType::EvenDiceCount:
			case EGambitEffectConditionType::AllDiceEven:
				if (IsEvenDiceValue(DieState.EffectiveValue))
				{
					MatchCount++;
				}
				break;
			case EGambitEffectConditionType::OddDiceCount:
			case EGambitEffectConditionType::AllDiceOdd:
				if (IsOddDiceValue(DieState.EffectiveValue))
				{
					MatchCount++;
				}
				break;
			case EGambitEffectConditionType::AllDiceValueGreaterOrEqual:
				if (DieState.EffectiveValue >= Condition.Value)
				{
					MatchCount++;
				}
				break;
			case EGambitEffectConditionType::AllDiceValueLowerOrEqual:
				if (DieState.EffectiveValue <= Condition.Value)
				{
					MatchCount++;
				}
				break;
			case EGambitEffectConditionType::AllDiceValueBetween:
				if (DieState.EffectiveValue >= Condition.Value && DieState.EffectiveValue <= Condition.Count)
				{
					MatchCount++;
				}
				break;
			case EGambitEffectConditionType::AllDiceValueInSet:
				if (DieState.EffectiveValue == Condition.Value || DieState.EffectiveValue == Condition.Count)
				{
					MatchCount++;
				}
				break;
			case EGambitEffectConditionType::ItemTag:
				if (DoesDieHaveTag(DieState, Condition.Tag))
				{
					MatchCount++;
				}
				break;
			default:
				break;
			}
		}

		return MatchCount;
	}

	bool IsDieSelectorCondition(const EGambitEffectConditionType ConditionType)
	{
		switch (ConditionType)
		{
		case EGambitEffectConditionType::DieValueEquals:
		case EGambitEffectConditionType::DieValueGreater:
		case EGambitEffectConditionType::DieValueLower:
		case EGambitEffectConditionType::HasAtLeastMatchingDice:
		case EGambitEffectConditionType::EvenDiceCount:
		case EGambitEffectConditionType::OddDiceCount:
		case EGambitEffectConditionType::AllDiceEven:
		case EGambitEffectConditionType::AllDiceOdd:
		case EGambitEffectConditionType::AllDiceValueGreaterOrEqual:
		case EGambitEffectConditionType::AllDiceValueLowerOrEqual:
		case EGambitEffectConditionType::AllDiceValueBetween:
		case EGambitEffectConditionType::AllDiceValueInSet:
		case EGambitEffectConditionType::ItemTag:
			return true;
		default:
			return false;
		}
	}

	bool DoesDieMatchSelectorCondition(
		const FGambitDieRuntimeState& DieState,
		const FGambitEffectConditionDefinition& Condition)
	{
		switch (Condition.ConditionType)
		{
		case EGambitEffectConditionType::DieValueEquals:
		case EGambitEffectConditionType::HasAtLeastMatchingDice:
			return DieState.EffectiveValue == Condition.Value;
		case EGambitEffectConditionType::DieValueGreater:
			return DieState.EffectiveValue > Condition.Value;
		case EGambitEffectConditionType::DieValueLower:
			return DieState.EffectiveValue < Condition.Value;
		case EGambitEffectConditionType::EvenDiceCount:
		case EGambitEffectConditionType::AllDiceEven:
			return IsEvenDiceValue(DieState.EffectiveValue);
		case EGambitEffectConditionType::OddDiceCount:
		case EGambitEffectConditionType::AllDiceOdd:
			return IsOddDiceValue(DieState.EffectiveValue);
		case EGambitEffectConditionType::AllDiceValueGreaterOrEqual:
			return DieState.EffectiveValue >= Condition.Value;
		case EGambitEffectConditionType::AllDiceValueLowerOrEqual:
			return DieState.EffectiveValue <= Condition.Value;
	case EGambitEffectConditionType::AllDiceValueBetween:
		return DieState.EffectiveValue >= Condition.Value && DieState.EffectiveValue <= Condition.Count;
	case EGambitEffectConditionType::AllDiceValueInSet:
		return DieState.EffectiveValue == Condition.Value || DieState.EffectiveValue == Condition.Count;
	case EGambitEffectConditionType::ItemTag:
		return DoesDieHaveTag(DieState, Condition.Tag);
	default:
		return true;
		}
	}

	bool DoesDieMatchAllSelectorConditions(
		const FGambitDieRuntimeState& DieState,
		const UGambitItemEffectDefinition* EffectDefinition)
	{
		if (!EffectDefinition)
		{
			return true;
		}

		for (const FGambitEffectConditionDefinition& Condition : EffectDefinition->Conditions)
		{
			if (!IsDieSelectorCondition(Condition.ConditionType))
			{
				continue;
			}

			const bool bMatchesCondition = DoesDieMatchSelectorCondition(DieState, Condition);
			if (Condition.bInvert ? bMatchesCondition : !bMatchesCondition)
			{
				return false;
			}
		}

		return true;
	}

	int32 CountMatchingDiceForEffect(
		const TArray<FGambitDieRuntimeState>& DiceStates,
		const UGambitItemEffectDefinition* EffectDefinition,
		const int32 ExcludedDieIndex = INDEX_NONE)
	{
		int32 MatchCount = 0;
		for (int32 DieIndex = 0; DieIndex < DiceStates.Num(); ++DieIndex)
		{
			if (DieIndex == ExcludedDieIndex)
			{
				continue;
			}

			if (DoesDieMatchAllSelectorConditions(DiceStates[DieIndex], EffectDefinition))
			{
				MatchCount++;
			}
		}

		return MatchCount;
	}

	bool DoesSourceDieHaveMatchingValue(const FGambitEffectExecutionContext& Context)
	{
		const int32 SourceDieIndex = Context.SourceDieHandIndex;
		if (!Context.SourceDice.IsValidIndex(SourceDieIndex))
		{
			return false;
		}

		const int32 SourceValue = Context.SourceDice[SourceDieIndex].EffectiveValue;
		for (int32 DieIndex = 0; DieIndex < Context.SourceDice.Num(); ++DieIndex)
		{
			if (DieIndex != SourceDieIndex && Context.SourceDice[DieIndex].EffectiveValue == SourceValue)
			{
				return true;
			}
		}

		return false;
	}

	bool IsGoldExtremum(
		const FGambitEffectExecutionContext& Context,
		const EGambitEffectTarget Target,
		const bool bLowest)
	{
		const UGambitEconomyComponent* CandidateEconomy = ResolveEconomyComponent(Context, Target);
		if (!CandidateEconomy)
		{
			return false;
		}

		TArray<const UGambitEconomyComponent*> Economies;
		Economies.Reserve(Context.MatchEconomyComponents.Num() + 1);
		Economies.Add(CandidateEconomy);
		for (const TObjectPtr<UGambitEconomyComponent>& EconomyComponent : Context.MatchEconomyComponents)
		{
			if (EconomyComponent)
			{
				Economies.AddUnique(EconomyComponent.Get());
			}
		}

		const int32 CandidateGold = CandidateEconomy->GetCurrentGold();
		for (const UGambitEconomyComponent* EconomyComponent : Economies)
		{
			if (!EconomyComponent || EconomyComponent == CandidateEconomy)
			{
				continue;
			}

			const int32 OtherGold = EconomyComponent->GetCurrentGold();
			if (bLowest && CandidateGold > OtherGold)
			{
				return false;
			}
			if (!bLowest && CandidateGold < OtherGold)
			{
				return false;
			}
		}

		return true;
	}

	int32 CountRerollDeltasByValueDirection(const FGambitEffectExecutionContext& Context, const int32 Direction)
	{
		int32 Count = 0;
		for (const FGambitRerollDieDelta& Delta : Context.RerollDeltas)
		{
			if (Delta.ValueBefore == INDEX_NONE || Delta.ValueAfter == INDEX_NONE)
			{
				continue;
			}

			if (Direction > 0 && Delta.ValueAfter > Delta.ValueBefore)
			{
				Count++;
			}
			else if (Direction < 0 && Delta.ValueAfter < Delta.ValueBefore)
			{
				Count++;
			}
			else if (Direction == 0 && Delta.ValueAfter == Delta.ValueBefore)
			{
				Count++;
			}
		}

		return Count;
	}

	bool DidRerollChangeNoDiceValues(const FGambitEffectExecutionContext& Context)
	{
		if (Context.RerollDeltas.Num() == 0)
		{
			return false;
		}

		for (const FGambitRerollDieDelta& Delta : Context.RerollDeltas)
		{
			if (Delta.DidValueChange())
			{
				return false;
			}
		}

		return true;
	}

	int32 ResolveOwnedDiceCount(const FGambitEffectExecutionContext& Context, const EGambitEffectTarget Target)
	{
		if (const UGambitInventoryComponent* InventoryComponent = ResolveInventoryComponent(Context, Target))
		{
			return InventoryComponent->GetOwnedDiceCount();
		}

		return ResolveDiceSnapshotConst(Context, Target).Num();
	}

	int32 ResolveOwnedDiceDistinctTypeCount(const FGambitEffectExecutionContext& Context, const EGambitEffectTarget Target)
	{
		if (const UGambitInventoryComponent* InventoryComponent = ResolveInventoryComponent(Context, Target))
		{
			return InventoryComponent->GetDistinctOwnedDiceTypeCount();
		}

		TSet<FName> DiceIds;
		for (const FGambitDieRuntimeState& DieState : ResolveDiceSnapshotConst(Context, Target))
		{
			if (const UGambitDiceDefinition* DiceDefinition = DieState.DiceDefinition.Get())
			{
				DiceIds.Add(DiceDefinition->GetResolvedDiceId());
			}
		}
		return DiceIds.Num();
	}

	int32 ResolveOwnedDiceMostRepeatedTypeCount(const FGambitEffectExecutionContext& Context, const EGambitEffectTarget Target)
	{
		if (const UGambitInventoryComponent* InventoryComponent = ResolveInventoryComponent(Context, Target))
		{
			return InventoryComponent->GetMostRepeatedOwnedDiceTypeCount();
		}

		TMap<FName, int32> CountsByDiceId;
		for (const FGambitDieRuntimeState& DieState : ResolveDiceSnapshotConst(Context, Target))
		{
			if (const UGambitDiceDefinition* DiceDefinition = DieState.DiceDefinition.Get())
			{
				int32& Count = CountsByDiceId.FindOrAdd(DiceDefinition->GetResolvedDiceId());
				Count++;
			}
		}

		int32 HighestCount = 0;
		for (const TPair<FName, int32>& Pair : CountsByDiceId)
		{
			HighestCount = FMath::Max(HighestCount, Pair.Value);
		}
		return HighestCount;
	}

	int32 ResolveOwnedDiceRarityCount(
		const FGambitEffectExecutionContext& Context,
		const EGambitEffectTarget Target,
		const EGambitItemRarity Rarity,
		const bool bAtLeastRarity)
	{
		if (const UGambitInventoryComponent* InventoryComponent = ResolveInventoryComponent(Context, Target))
		{
			return InventoryComponent->CountOwnedDiceByRarity(Rarity, bAtLeastRarity);
		}

		int32 Count = 0;
		for (const FGambitDieRuntimeState& DieState : ResolveDiceSnapshotConst(Context, Target))
		{
			if (const UGambitDiceDefinition* DiceDefinition = DieState.DiceDefinition.Get())
			{
				if (EffectResolverRarityMatches(DiceDefinition->Rarity, Rarity, bAtLeastRarity))
				{
					Count++;
				}
			}
		}
		return Count;
	}

	int32 ResolveOwnedItemRarityCount(
		const FGambitEffectExecutionContext& Context,
		const EGambitEffectTarget Target,
		const EGambitItemRarity Rarity,
		const bool bAtLeastRarity)
	{
		if (const UGambitInventoryComponent* InventoryComponent = ResolveInventoryComponent(Context, Target))
		{
			return InventoryComponent->CountOwnedItemsByRarity(Rarity, bAtLeastRarity);
		}

		return ResolveOwnedDiceRarityCount(Context, Target, Rarity, bAtLeastRarity);
	}

	int32 ResolveOwnedModuleRarityCount(
		const FGambitEffectExecutionContext& Context,
		const EGambitEffectTarget Target,
		const EGambitItemRarity Rarity,
		const bool bAtLeastRarity)
	{
		if (const UGambitInventoryComponent* InventoryComponent = ResolveInventoryComponent(Context, Target))
		{
			return InventoryComponent->CountActiveModulesByRarity(Rarity, bAtLeastRarity);
		}

		return 0;
	}

	int32 ResolveInventoryCountForCondition(
		const FGambitEffectExecutionContext& Context,
		const EGambitEffectTarget Target,
		const FGambitEffectConditionDefinition& Condition)
	{
		switch (Condition.ConditionType)
		{
		case EGambitEffectConditionType::OwnedDiceDistinctTypeCount:
			return ResolveOwnedDiceDistinctTypeCount(Context, Target);
		case EGambitEffectConditionType::OwnedDiceMostRepeatedTypeCount:
			return ResolveOwnedDiceMostRepeatedTypeCount(Context, Target);
		case EGambitEffectConditionType::OwnedDiceRarityCount:
			return ResolveOwnedDiceRarityCount(Context, Target, Condition.Rarity, false);
		case EGambitEffectConditionType::OwnedDiceRarityAtLeastCount:
			return ResolveOwnedDiceRarityCount(Context, Target, Condition.Rarity, true);
		case EGambitEffectConditionType::OwnedItemRarityCount:
			return ResolveOwnedItemRarityCount(Context, Target, Condition.Rarity, false);
		case EGambitEffectConditionType::OwnedItemRarityAtLeastCount:
			return ResolveOwnedItemRarityCount(Context, Target, Condition.Rarity, true);
		case EGambitEffectConditionType::OwnedModuleRarityCount:
			return ResolveOwnedModuleRarityCount(Context, Target, Condition.Rarity, false);
		case EGambitEffectConditionType::OwnedModuleRarityAtLeastCount:
			return ResolveOwnedModuleRarityCount(Context, Target, Condition.Rarity, true);
		default:
			return 0;
		}
	}

	bool AreDiceValuesPalindrome(const TArray<FGambitDieRuntimeState>& DiceStates)
	{
		if (DiceStates.Num() <= 1)
		{
			return DiceStates.Num() == 1;
		}

		TArray<FGambitDieRuntimeState> OrderedDiceStates = DiceStates;
		OrderedDiceStates.Sort([](const FGambitDieRuntimeState& Left, const FGambitDieRuntimeState& Right)
		{
			return Left.HandIndex < Right.HandIndex;
		});

		int32 LeftIndex = 0;
		int32 RightIndex = OrderedDiceStates.Num() - 1;
		while (LeftIndex < RightIndex)
		{
			if (OrderedDiceStates[LeftIndex].EffectiveValue != OrderedDiceStates[RightIndex].EffectiveValue)
			{
				return false;
			}

			LeftIndex++;
			RightIndex--;
		}

		return true;
	}

	int32 ResolveRawScore(const FGambitEffectExecutionContext& Context)
	{
		if (Context.CurrentCombinationResult.RawScore != 0)
		{
			return Context.CurrentCombinationResult.RawScore;
		}

		const int32 CombinationFallback = Context.CurrentCombinationResult.BaseCombinationScore + Context.CurrentCombinationResult.DiceSum;
		if (CombinationFallback != 0)
		{
			return CombinationFallback;
		}

		if (Context.CurrentScoreBreakdown.RawScore != 0)
		{
			return Context.CurrentScoreBreakdown.RawScore;
		}

		return Context.CurrentScoreBreakdown.BaseCombinationScore + Context.CurrentScoreBreakdown.DiceSum;
	}

	int32 ResolveContextualAmountAsInt(
		const UGambitItemEffectDefinition* EffectDefinition,
		FGambitEffectExecutionContext& Context,
		const EGambitEffectTarget Target)
	{
		float Amount = ResolveScalar(EffectDefinition, TEXT("Amount"), EffectDefinition ? EffectDefinition->Amount : 0.0f);
		if (!EffectDefinition)
		{
			return FMath::RoundToInt(Amount);
		}

		const TArray<FGambitDieRuntimeState>& DiceStates = ResolveDiceSnapshotConst(Context, Target);
		for (const TPair<FName, float>& ScalarParameter : EffectDefinition->ScalarParameters)
		{
			if (ScalarParameter.Key == TEXT("AmountPerMatchingDie"))
			{
				int32 MatchingDiceCount = 0;
				for (const FGambitEffectConditionDefinition& Condition : EffectDefinition->Conditions)
				{
					MatchingDiceCount = FMath::Max(MatchingDiceCount, CountMatchingDiceForCondition(DiceStates, Condition));
				}
				Amount += ScalarParameter.Value * static_cast<float>(MatchingDiceCount);
			}
			else if (ScalarParameter.Key == TEXT("AmountPerOtherMatchingDie"))
			{
				Amount += ScalarParameter.Value * static_cast<float>(
					CountMatchingDiceForEffect(DiceStates, EffectDefinition, ResolveSelectedDieHandIndex(Context, Target)));
			}
			else if (ScalarParameter.Key == TEXT("AmountPerOwnedDie"))
			{
				Amount += ScalarParameter.Value * static_cast<float>(ResolveOwnedDiceCount(Context, Target));
			}
			else if (ScalarParameter.Key == TEXT("AmountFromSourceDieValue"))
			{
				if (Context.SourceDice.IsValidIndex(Context.SourceDieHandIndex))
				{
					Amount += static_cast<float>(Context.SourceDice[Context.SourceDieHandIndex].EffectiveValue) * ScalarParameter.Value;
				}
			}
			else if (ScalarParameter.Key == TEXT("AmountPerGold"))
			{
				if (const UGambitEconomyComponent* EconomyComponent = ResolveEconomyComponent(Context, Target))
				{
					Amount += ScalarParameter.Value * static_cast<float>(EconomyComponent->GetCurrentGold());
				}
			}
			else if (ScalarParameter.Key == TEXT("AmountPerGoldTier"))
			{
				if (const UGambitEconomyComponent* EconomyComponent = ResolveEconomyComponent(Context, Target))
				{
					const int32 TierSize = FMath::Max(1, ResolveIntScalar(EffectDefinition, TEXT("GoldTierSize"), 10));
					Amount += ScalarParameter.Value * static_cast<float>(FMath::Max(0, EconomyComponent->GetCurrentGold()) / TierSize);
				}
			}
			else if (ScalarParameter.Key == TEXT("AmountPercentOfRoundGoldReward"))
			{
				const int32 RoundGoldReward = FMath::Max(0, Context.BaseGoldReward + Context.InterestGoldReward);
				Amount += static_cast<float>(RoundGoldReward) * ScalarParameter.Value / 100.0f;
			}
			else if (ScalarParameter.Key == TEXT("AmountPerGlobalPurchaseForItemType"))
			{
				Amount += ScalarParameter.Value * static_cast<float>(FMath::Max(0, Context.ShopPurchase.GlobalPurchasesForItemType));
			}
			else if (ScalarParameter.Key == TEXT("AmountPerGlobalPurchaseForItemRarity"))
			{
				Amount += ScalarParameter.Value * static_cast<float>(FMath::Max(0, Context.ShopPurchase.GlobalPurchasesForItemRarity));
			}
			else if (ScalarParameter.Key == TEXT("AmountPerGlobalPurchaseForItemTypeAndRarity"))
			{
				Amount += ScalarParameter.Value * static_cast<float>(FMath::Max(0, Context.ShopPurchase.GlobalPurchasesForItemTypeAndRarity));
			}
			else if (ScalarParameter.Key == TEXT("AmountPerRerollUsed"))
			{
				Amount += ScalarParameter.Value * static_cast<float>(FMath::Max(0, Context.RerollsUsed));
			}
			else if (ScalarParameter.Key == TEXT("AmountPerUnusedReroll"))
			{
				Amount += ScalarParameter.Value * static_cast<float>(FMath::Max(0, Context.RerollLimit - Context.RerollsUsed));
			}
			else if (ScalarParameter.Key == TEXT("AmountPerDistinctOwnedDiceType"))
			{
				Amount += ScalarParameter.Value * static_cast<float>(ResolveOwnedDiceDistinctTypeCount(Context, Target));
			}
			else if (ScalarParameter.Key == TEXT("AmountPerMostRepeatedOwnedDiceType"))
			{
				Amount += ScalarParameter.Value * static_cast<float>(ResolveOwnedDiceMostRepeatedTypeCount(Context, Target));
			}
			else if (ScalarParameter.Key == TEXT("AmountPerOwnedDiceRarityCount"))
			{
				int32 RarityCount = 0;
				for (const FGambitEffectConditionDefinition& Condition : EffectDefinition->Conditions)
				{
					if (Condition.ConditionType == EGambitEffectConditionType::OwnedDiceRarityCount
						|| Condition.ConditionType == EGambitEffectConditionType::OwnedDiceRarityAtLeastCount)
					{
						RarityCount = FMath::Max(RarityCount, ResolveInventoryCountForCondition(Context, Target, Condition));
					}
				}
				Amount += ScalarParameter.Value * static_cast<float>(RarityCount);
			}
			else if (ScalarParameter.Key == TEXT("AmountPerOwnedItemRarityCount"))
			{
				int32 RarityCount = 0;
				for (const FGambitEffectConditionDefinition& Condition : EffectDefinition->Conditions)
				{
					if (Condition.ConditionType == EGambitEffectConditionType::OwnedItemRarityCount
						|| Condition.ConditionType == EGambitEffectConditionType::OwnedItemRarityAtLeastCount)
					{
						RarityCount = FMath::Max(RarityCount, ResolveInventoryCountForCondition(Context, Target, Condition));
					}
				}
				Amount += ScalarParameter.Value * static_cast<float>(RarityCount);
			}
			else if (ScalarParameter.Key == TEXT("AmountPerOwnedModuleRarityCount"))
			{
				int32 RarityCount = 0;
				for (const FGambitEffectConditionDefinition& Condition : EffectDefinition->Conditions)
				{
					if (Condition.ConditionType == EGambitEffectConditionType::OwnedModuleRarityCount
						|| Condition.ConditionType == EGambitEffectConditionType::OwnedModuleRarityAtLeastCount)
					{
						RarityCount = FMath::Max(RarityCount, ResolveInventoryCountForCondition(Context, Target, Condition));
					}
				}
				Amount += ScalarParameter.Value * static_cast<float>(RarityCount);
			}
			else if (ScalarParameter.Key == TEXT("AmountPerPairCount"))
			{
				Amount += ScalarParameter.Value * static_cast<float>(Context.CurrentCombinationResult.PairCount);
			}
			else if (ScalarParameter.Key == TEXT("AmountPerMatchingPairCount"))
			{
				int32 MatchingDiceCount = 0;
				for (const FGambitEffectConditionDefinition& Condition : EffectDefinition->Conditions)
				{
					MatchingDiceCount = FMath::Max(MatchingDiceCount, CountMatchingDiceForCondition(DiceStates, Condition));
				}
				Amount += ScalarParameter.Value * static_cast<float>(MatchingDiceCount / 2);
			}
			else if (ScalarParameter.Key == TEXT("AmountPerMatchingDieChance"))
			{
				const float ChancePercent = FMath::Clamp(ResolveScalar(EffectDefinition, TEXT("PerDieChancePercent"), 100.0f), 0.0f, 100.0f);
				for (const FGambitDieRuntimeState& DieState : DiceStates)
				{
					if (!DoesDieMatchAllSelectorConditions(DieState, EffectDefinition))
					{
						continue;
					}

					const float Roll = Context.RandomStream.FRandRange(0.0f, 100.0f);
					if (Roll <= ChancePercent)
					{
						Amount += ScalarParameter.Value;
					}
					UE_LOG(
						LogGambit,
						Log,
						TEXT("EffectResolver: PerDieChance Amount=%.2f Roll=%.2f Chance=%.2f Matched=%s"),
						ScalarParameter.Value,
						Roll,
						ChancePercent,
						Roll <= ChancePercent ? TEXT("Yes") : TEXT("No"));
				}
			}
			else if (ScalarParameter.Key == TEXT("AmountPerRerolledDieValueIncreased"))
			{
				Amount += ScalarParameter.Value * static_cast<float>(CountRerollDeltasByValueDirection(Context, 1));
			}
			else if (ScalarParameter.Key == TEXT("AmountPerRerolledDieValueDecreased"))
			{
				Amount += ScalarParameter.Value * static_cast<float>(CountRerollDeltasByValueDirection(Context, -1));
			}
			else if (ScalarParameter.Key == TEXT("AmountPerRerolledDieValueUnchanged"))
			{
				Amount += ScalarParameter.Value * static_cast<float>(CountRerollDeltasByValueDirection(Context, 0));
			}
		}

		if (const float* MaxAmount = EffectDefinition->ScalarParameters.Find(TEXT("MaxAmount")))
		{
			Amount = FMath::Min(Amount, *MaxAmount);
		}

		return FMath::RoundToInt(Amount);
	}

	float ResolveContextualMultiplier(
		const UGambitItemEffectDefinition* EffectDefinition,
		const FGambitEffectExecutionContext& Context,
		const EGambitEffectTarget Target)
	{
		float Multiplier = ResolveMultiplier(EffectDefinition);
		if (!EffectDefinition)
		{
			return Multiplier;
		}

		const TArray<FGambitDieRuntimeState>& DiceStates = ResolveDiceSnapshotConst(Context, Target);
		for (const TPair<FName, float>& ScalarParameter : EffectDefinition->ScalarParameters)
		{
			if (ScalarParameter.Key == TEXT("MultiplierDeltaPerMatchingDie"))
			{
				int32 MatchingDiceCount = 0;
				for (const FGambitEffectConditionDefinition& Condition : EffectDefinition->Conditions)
				{
					MatchingDiceCount = FMath::Max(MatchingDiceCount, CountMatchingDiceForCondition(DiceStates, Condition));
				}
				Multiplier += ScalarParameter.Value * static_cast<float>(MatchingDiceCount);
			}
			else if (ScalarParameter.Key == TEXT("MultiplierPerMatchingDie"))
			{
				int32 MatchingDiceCount = 0;
				for (const FGambitEffectConditionDefinition& Condition : EffectDefinition->Conditions)
				{
					MatchingDiceCount = FMath::Max(MatchingDiceCount, CountMatchingDiceForCondition(DiceStates, Condition));
				}
				Multiplier *= FMath::Pow(FMath::Max(0.0f, ScalarParameter.Value), static_cast<float>(MatchingDiceCount));
			}
			else if (ScalarParameter.Key == TEXT("MultiplierPerOwnedDiceRarityCount"))
			{
				int32 RarityCount = 0;
				for (const FGambitEffectConditionDefinition& Condition : EffectDefinition->Conditions)
				{
					if (Condition.ConditionType == EGambitEffectConditionType::OwnedDiceRarityCount
						|| Condition.ConditionType == EGambitEffectConditionType::OwnedDiceRarityAtLeastCount)
					{
						RarityCount = FMath::Max(RarityCount, ResolveInventoryCountForCondition(Context, Target, Condition));
					}
				}
				Multiplier *= FMath::Pow(FMath::Max(0.0f, ScalarParameter.Value), static_cast<float>(RarityCount));
			}
			else if (ScalarParameter.Key == TEXT("MultiplierDeltaPerOwnedDiceRarityCount"))
			{
				int32 RarityCount = 0;
				for (const FGambitEffectConditionDefinition& Condition : EffectDefinition->Conditions)
				{
					if (Condition.ConditionType == EGambitEffectConditionType::OwnedDiceRarityCount
						|| Condition.ConditionType == EGambitEffectConditionType::OwnedDiceRarityAtLeastCount)
					{
						RarityCount = FMath::Max(RarityCount, ResolveInventoryCountForCondition(Context, Target, Condition));
					}
				}
				Multiplier += ScalarParameter.Value * static_cast<float>(RarityCount);
			}
			else if (ScalarParameter.Key == TEXT("MultiplierPerGoldTier"))
			{
				if (const UGambitEconomyComponent* EconomyComponent = ResolveEconomyComponent(Context, Target))
				{
					const int32 TierSize = FMath::Max(1, ResolveIntScalar(EffectDefinition, TEXT("GoldTierSize"), 10));
					const int32 TierCount = FMath::Max(0, EconomyComponent->GetCurrentGold()) / TierSize;
					Multiplier *= FMath::Pow(FMath::Max(0.0f, ScalarParameter.Value), static_cast<float>(TierCount));
				}
			}
			else if (ScalarParameter.Key == TEXT("MultiplierDeltaPerGoldTier"))
			{
				if (const UGambitEconomyComponent* EconomyComponent = ResolveEconomyComponent(Context, Target))
				{
					const int32 TierSize = FMath::Max(1, ResolveIntScalar(EffectDefinition, TEXT("GoldTierSize"), 10));
					const int32 TierCount = FMath::Max(0, EconomyComponent->GetCurrentGold()) / TierSize;
					Multiplier += ScalarParameter.Value * static_cast<float>(TierCount);
				}
			}
		}

		return Multiplier;
	}

	float ResolveDiceContributionMultiplierBonus(
		const UGambitItemEffectDefinition* EffectDefinition,
		const FGambitEffectExecutionContext& Context,
		const EGambitEffectTarget Target)
	{
		const float Multiplier = ResolveContextualMultiplier(EffectDefinition, Context, Target);
		if (FMath::IsNearlyEqual(Multiplier, 1.0f))
		{
			return 0.0f;
		}

		float Bonus = 0.0f;
		for (const FGambitDieRuntimeState& DieState : ResolveDiceSnapshotConst(Context, Target))
		{
			if (!DieState.bCountsForScoreSum || !DoesDieMatchAllSelectorConditions(DieState, EffectDefinition))
			{
				continue;
			}

			Bonus += static_cast<float>(DieState.ScoreContributionValue) * (Multiplier - 1.0f);
		}

		return Bonus;
	}

	void ApplyFlatScoreToBreakdown(FGambitScoreBreakdown& Breakdown, const float Amount)
	{
		Breakdown.AdditiveBonus += Amount;
		Breakdown.ScoreBeforeCap += Amount;
		Breakdown.ScoreAfterCap += Amount;
		Breakdown.FinalScore = FMath::Max(0, Breakdown.FinalScore + FMath::RoundToInt(Amount));
	}

	void ApplyScoreMultiplierToBreakdown(FGambitScoreBreakdown& Breakdown, const float Multiplier)
	{
		const float SafeMultiplier = FMath::Max(0.0f, Multiplier);
		Breakdown.Multiplier *= SafeMultiplier;
		Breakdown.ScoreBeforeCap *= SafeMultiplier;
		Breakdown.ScoreAfterCap *= SafeMultiplier;
		Breakdown.FinalScore = FMath::Max(0, FMath::RoundToInt(static_cast<float>(Breakdown.FinalScore) * SafeMultiplier));
	}

	bool IsPostScoreHook(const EGambitEffectHook Hook)
	{
		return Hook == EGambitEffectHook::PostScoreCalculation || Hook == EGambitEffectHook::Ranking;
	}

	void ApplyScoreModifier(FGambitScoreModifierContext& Target, const FGambitScoreModifierContext& Source)
	{
		Target = UGambitEffectResolver::MergeScoreModifiers(Target, Source);
	}

	FGambitScoreModifierContext& ResolveScoreModifierDelta(
		FGambitEffectExecutionContext& Context,
		const EGambitEffectTarget Target)
	{
		if (Context.Hook == EGambitEffectHook::ConsumableUse)
		{
			return Target == EGambitEffectTarget::Target
				? Context.TargetTemporaryScoreModifierDelta
				: Context.TemporaryScoreModifierDelta;
		}

		return Context.ScoreModifierDelta;
	}

	int32 RollFallbackD6Value(FRandomStream& RandomStream)
	{
		static const TArray<int32> FallbackFaces = { 1, 2, 3, 4, 5, 6 };
		return FallbackFaces[RandomStream.RandRange(0, FallbackFaces.Num() - 1)];
	}

	bool RerollRuntimeDieState(FGambitDieRuntimeState& DieState, FRandomStream& RandomStream)
	{
		if (!DieState.bCanBeRerolled)
		{
			return false;
		}

		if (const UGambitDiceDefinition* DiceDefinition = DieState.DiceDefinition.Get())
		{
			DieState.RolledFaceIndex = DiceDefinition->RollFaceIndex(RandomStream);
			DieState.RawValue = DiceDefinition->GetFaceValue(DieState.RolledFaceIndex);
		}
		else
		{
			DieState.RolledFaceIndex = INDEX_NONE;
			DieState.RawValue = RollFallbackD6Value(RandomStream);
		}

		DieState.EffectiveValue = DieState.RawValue;
		RefreshRuntimeScoreContribution(DieState);
		return true;
	}

	bool IsEffectResolverNeutralScoreModifier(const FGambitScoreModifierContext& Modifier)
	{
		return FMath::IsNearlyZero(Modifier.AdditiveBonus)
			&& FMath::IsNearlyZero(Modifier.DiceContributionMultiplierBonus)
			&& FMath::IsNearlyEqual(Modifier.Multiplier, 1.0f)
			&& Modifier.ScoreCap <= 0.0f
			&& Modifier.DiminishingThreshold <= 0.0f
			&& FMath::IsNearlyEqual(Modifier.DiminishingFactor, 1.0f);
	}

	template <typename TEnum>
	FString EffectResolverEnumToDebugString(const TEnum Value)
	{
		if (const UEnum* Enum = StaticEnum<TEnum>())
		{
			return Enum->GetNameStringByValue(static_cast<int64>(Value));
		}

		return TEXT("Unknown");
	}

	FName ResolveDebugSourceId(const FGambitEffectExecutionContext& Context)
	{
		if (const UGambitItemDefinition* SourceItem = Context.SourceItem.Get())
		{
			return SourceItem->GetResolvedItemId();
		}

		if (const UGambitDiceDefinition* SourceDice = Context.SourceDiceDefinition.Get())
		{
			return SourceDice->GetResolvedDiceId();
		}

		return NAME_None;
	}

	FString ResolveDebugSourceName(const FGambitEffectExecutionContext& Context)
	{
		if (const UGambitItemDefinition* SourceItem = Context.SourceItem.Get())
		{
			return GetItemName(SourceItem);
		}

		if (const UGambitDiceDefinition* SourceDice = Context.SourceDiceDefinition.Get())
		{
			return GetDiceName(SourceDice);
		}

		return TEXT("Unknown Source");
	}

	FString ResolveDebugTargetName(const FGambitEffectExecutionContext& Context, const EGambitEffectTarget Target)
	{
		const AGambitPlayerState* TargetPlayer = ResolvePlayer(Context, Target);
		if (!TargetPlayer)
		{
			return TEXT("None");
		}

		const FString PlayerName = TargetPlayer->GetPlayerName();
		return PlayerName.IsEmpty() ? TEXT("Unnamed Player") : PlayerName;
	}

	FGambitDebugEffectEvent MakeEffectDebugEvent(
		const FGambitEffectExecutionContext& Context,
		const UGambitItemEffectDefinition* EffectDefinition,
		const bool bTriggered,
		const bool bPrevented,
		const FString& Summary)
	{
		FGambitDebugEffectEvent Event;
		Event.Category = (bPrevented || (EffectDefinition && EffectDefinition->EffectType == EGambitItemEffectType::PreventNegativeEffect))
			? EGambitDebugEventCategory::Protection
			: EGambitDebugEventCategory::Effect;
		Event.Phase = Context.CurrentPhase;
		Event.HookId = FName(*EffectResolverEnumToDebugString(Context.Hook));
		Event.SourceId = ResolveDebugSourceId(Context);
		Event.SourceName = ResolveDebugSourceName(Context);
		Event.EffectId = GetEffectSourceId(EffectDefinition);
		Event.EffectTypeId = EffectDefinition ? EffectDefinition->EffectTypeId : NAME_None;
		Event.EffectTypeName = EffectDefinition ? EffectResolverEnumToDebugString(EffectDefinition->EffectType) : TEXT("RuntimeEffect");
		Event.TargetRuleId = EffectDefinition ? EffectDefinition->TargetRuleId : NAME_None;
		Event.TargetName = ResolveDebugTargetName(Context, EffectDefinition ? EffectDefinition->Target : EGambitEffectTarget::Source);
		Event.bTriggered = bTriggered;
		Event.bNegative = EffectDefinition ? EffectDefinition->bNegativeEffect : false;
		Event.bPrevented = bPrevented;
		Event.Summary = Summary;
		return Event;
	}

	FGambitDebugEffectEvent MakeRuntimeEffectDebugEvent(
		const FGambitEffectExecutionContext& Context,
		const FString& EffectName,
		const FString& Summary)
	{
		FGambitDebugEffectEvent Event;
		Event.Category = EGambitDebugEventCategory::Effect;
		Event.Phase = Context.CurrentPhase;
		Event.HookId = FName(*EffectResolverEnumToDebugString(Context.Hook));
		Event.SourceId = ResolveDebugSourceId(Context);
		Event.SourceName = ResolveDebugSourceName(Context);
		Event.EffectId = FName(*EffectName);
		Event.EffectTypeName = TEXT("RuntimeEffect");
		Event.TargetName = ResolveDebugTargetName(Context, EGambitEffectTarget::Source);
		Event.bTriggered = true;
		Event.Summary = Summary;
		return Event;
	}

	FGambitDebugScoreLine MakeScoreDebugLine(
		const FGambitEffectExecutionContext& Context,
		const EGambitDebugScoreLineType LineType,
		const FString& Label,
		const float AdditiveDelta,
		const float DiceContributionDelta,
		const float MultiplierValue,
		const float ScoreBefore,
		const float ScoreAfter,
		const FString& Summary)
	{
		FGambitDebugScoreLine Line;
		Line.LineType = LineType;
		Line.Phase = Context.CurrentPhase;
		Line.SourceId = ResolveDebugSourceId(Context);
		Line.SourceName = ResolveDebugSourceName(Context);
		Line.Label = Label;
		Line.AdditiveDelta = AdditiveDelta;
		Line.DiceContributionDelta = DiceContributionDelta;
		Line.MultiplierValue = MultiplierValue;
		Line.ScoreBefore = ScoreBefore;
		Line.ScoreAfter = ScoreAfter;
		Line.Summary = Summary;
		return Line;
	}

	void AddScoreModifierDebugLine(
		FGambitEffectExecutionContext& Context,
		const FGambitScoreModifierContext& Modifier,
		const FString& Label)
	{
		if (!FMath::IsNearlyZero(Modifier.AdditiveBonus))
		{
			Context.DebugScoreLines.Add(MakeScoreDebugLine(
				Context,
				EGambitDebugScoreLineType::Additive,
				Label,
				Modifier.AdditiveBonus,
				0.0f,
				1.0f,
				0.0f,
				0.0f,
				FString::Printf(TEXT("%s adds %+0.2f score"), *Label, Modifier.AdditiveBonus)));
		}

		if (!FMath::IsNearlyZero(Modifier.DiceContributionMultiplierBonus))
		{
			Context.DebugScoreLines.Add(MakeScoreDebugLine(
				Context,
				EGambitDebugScoreLineType::DiceContribution,
				Label,
				0.0f,
				Modifier.DiceContributionMultiplierBonus,
				1.0f,
				0.0f,
				0.0f,
				FString::Printf(TEXT("%s changes dice contribution by %+0.2f"), *Label, Modifier.DiceContributionMultiplierBonus)));
		}

		if (!FMath::IsNearlyEqual(Modifier.Multiplier, 1.0f))
		{
			Context.DebugScoreLines.Add(MakeScoreDebugLine(
				Context,
				EGambitDebugScoreLineType::Multiplier,
				Label,
				0.0f,
				0.0f,
				Modifier.Multiplier,
				0.0f,
				0.0f,
				FString::Printf(TEXT("%s multiplies score by x%0.2f"), *Label, Modifier.Multiplier)));
		}

		if (Modifier.ScoreCap > 0.0f)
		{
			Context.DebugScoreLines.Add(MakeScoreDebugLine(
				Context,
				EGambitDebugScoreLineType::Cap,
				Label,
				0.0f,
				0.0f,
				1.0f,
				0.0f,
				Modifier.ScoreCap,
				FString::Printf(TEXT("%s sets score cap to %0.2f"), *Label, Modifier.ScoreCap)));
		}
	}

	FGambitDebugGoldLine MakeGoldDebugLine(
		const FGambitEffectExecutionContext& Context,
		const EGambitDebugGoldLineType LineType,
		const int32 RequestedDelta,
		const int32 GoldBefore,
		const int32 GoldAfter,
		const FString& Summary)
	{
		FGambitDebugGoldLine Line;
		Line.LineType = LineType;
		Line.Phase = Context.CurrentPhase;
		Line.SourceId = ResolveDebugSourceId(Context);
		Line.SourceName = ResolveDebugSourceName(Context);
		Line.RequestedDelta = RequestedDelta;
		Line.ActualDelta = GoldAfter - GoldBefore;
		Line.GoldBefore = GoldBefore;
		Line.GoldAfter = GoldAfter;
		Line.bClamped = RequestedDelta != Line.ActualDelta;
		Line.Summary = Summary;
		return Line;
	}
}

int32 UGambitEffectResolver::ExecuteItemEffects(UGambitItemDefinition* ItemDefinition, FGambitEffectExecutionContext& Context) const
{
	if (!ItemDefinition)
	{
		return 0;
	}

	Context.SourceItem = ItemDefinition;
	int32 TriggeredCount = 0;

	for (UGambitItemEffectDefinition* EffectDefinition : ItemDefinition->EffectDefinitions)
	{
		if (ExecuteEffectDefinition(EffectDefinition, Context))
		{
			TriggeredCount++;
		}
	}

	for (const TSubclassOf<UGambitItemEffect>& EffectClass : ItemDefinition->EffectClasses)
	{
		if (!EffectClass)
		{
			continue;
		}

		UGambitItemEffect* RuntimeEffect = NewObject<UGambitItemEffect>(const_cast<UGambitEffectResolver*>(this), EffectClass);
		if (!RuntimeEffect || !RuntimeEffect->CanExecute(Context))
		{
			continue;
		}

		if (RuntimeEffect->ExecuteEffect(Context))
		{
			TriggeredCount++;
			Context.DebugEffectEvents.Add(MakeRuntimeEffectDebugEvent(
				Context,
				EffectClass->GetName(),
				FString::Printf(TEXT("Runtime effect class %s triggered"), *EffectClass->GetName())));
			UE_LOG(
				LogGambit,
				Log,
				TEXT("EffectResolver: Hook=%s Item=%s EffectClass=%s Triggered"),
				*EffectHookToString(Context.Hook),
				*GetItemName(ItemDefinition),
				*EffectClass->GetName());
		}
	}

	return TriggeredCount;
}

int32 UGambitEffectResolver::ExecuteDiceEffects(UGambitDiceDefinition* DiceDefinition, FGambitEffectExecutionContext& Context) const
{
	if (!DiceDefinition)
	{
		return 0;
	}

	Context.SourceItem = nullptr;
	Context.SourceDiceDefinition = DiceDefinition;
	int32 TriggeredCount = 0;

	for (UGambitItemEffectDefinition* EffectDefinition : DiceDefinition->EffectDefinitions)
	{
		if (ExecuteEffectDefinition(EffectDefinition, Context))
		{
			TriggeredCount++;
		}
	}

	for (const TSubclassOf<UGambitItemEffect>& EffectClass : DiceDefinition->EffectClasses)
	{
		if (!EffectClass)
		{
			continue;
		}

		UGambitItemEffect* RuntimeEffect = NewObject<UGambitItemEffect>(const_cast<UGambitEffectResolver*>(this), EffectClass);
		if (!RuntimeEffect || !RuntimeEffect->CanExecute(Context))
		{
			continue;
		}

		if (RuntimeEffect->ExecuteEffect(Context))
		{
			TriggeredCount++;
			Context.DebugEffectEvents.Add(MakeRuntimeEffectDebugEvent(
				Context,
				EffectClass->GetName(),
				FString::Printf(TEXT("Runtime dice effect class %s triggered"), *EffectClass->GetName())));
			UE_LOG(
				LogGambit,
				Log,
				TEXT("EffectResolver: Hook=%s Dice=%s EffectClass=%s Triggered"),
				*EffectHookToString(Context.Hook),
				*GetDiceName(DiceDefinition),
				*EffectClass->GetName());
		}
	}

	return TriggeredCount;
}

bool UGambitEffectResolver::ExecuteEffectDefinition(UGambitItemEffectDefinition* EffectDefinition, FGambitEffectExecutionContext& Context) const
{
	if (!EffectDefinition || EffectDefinition->Hook != Context.Hook)
	{
		return false;
	}

	if (EffectDefinition->bNegativeEffect && Context.bPreventNegativeEffects)
	{
		Context.DebugEffectEvents.Add(MakeEffectDebugEvent(
			Context,
			EffectDefinition,
			false,
			true,
			FString::Printf(TEXT("%s was prevented by active negative-effect protection"), *GetEffectName(EffectDefinition))));
		UE_LOG(
			LogGambit,
			Log,
			TEXT("EffectResolver: Hook=%s Effect=%s Type=%s prevented by PreventNegativeEffect"),
			*EffectHookToString(Context.Hook),
			*GetEffectName(EffectDefinition),
			*EffectTypeToString(EffectDefinition->EffectType));
		return false;
	}

	if (!AreConditionsMet(EffectDefinition, Context))
	{
		UE_LOG(
			LogGambit,
			Verbose,
			TEXT("EffectResolver: Hook=%s Effect=%s Type=%s skipped by conditions"),
			*EffectHookToString(Context.Hook),
			*GetEffectName(EffectDefinition),
			*EffectTypeToString(EffectDefinition->EffectType));
		return false;
	}

	const bool bTriggered = ApplyEffectDefinition(EffectDefinition, Context);
	if (bTriggered && EffectDefinition->EffectType != EGambitItemEffectType::CopyLastTriggeredEffect)
	{
		Context.LastTriggeredEffectDefinition = EffectDefinition;
	}

	if (bTriggered)
	{
		Context.DebugEffectEvents.Add(MakeEffectDebugEvent(
			Context,
			EffectDefinition,
			true,
			false,
			FString::Printf(
				TEXT("%s triggered %s"),
				*ResolveDebugSourceName(Context),
				*GetEffectName(EffectDefinition))));
		UE_LOG(
			LogGambit,
			Log,
			TEXT("EffectResolver: Hook=%s Item=%s Effect=%s Type=%s Triggered"),
			*EffectHookToString(Context.Hook),
			*GetItemName(Context.SourceItem),
			*GetEffectName(EffectDefinition),
			*EffectTypeToString(EffectDefinition->EffectType));
	}

	return bTriggered;
}

FGambitScoreModifierContext UGambitEffectResolver::MergeScoreModifiers(
	const FGambitScoreModifierContext& A,
	const FGambitScoreModifierContext& B)
{
	FGambitScoreModifierContext Result;
	Result.AdditiveBonus = A.AdditiveBonus + B.AdditiveBonus;
	Result.DiceContributionMultiplierBonus = A.DiceContributionMultiplierBonus + B.DiceContributionMultiplierBonus;
	Result.Multiplier = A.Multiplier * B.Multiplier;

	if (A.ScoreCap > 0.0f && B.ScoreCap > 0.0f)
	{
		Result.ScoreCap = FMath::Min(A.ScoreCap, B.ScoreCap);
	}
	else
	{
		Result.ScoreCap = FMath::Max(A.ScoreCap, B.ScoreCap);
	}

	if (A.DiminishingThreshold > 0.0f && B.DiminishingThreshold > 0.0f)
	{
		Result.DiminishingThreshold = FMath::Min(A.DiminishingThreshold, B.DiminishingThreshold);
	}
	else
	{
		Result.DiminishingThreshold = FMath::Max(A.DiminishingThreshold, B.DiminishingThreshold);
	}

	if (A.DiminishingFactor > 0.0f && B.DiminishingFactor > 0.0f)
	{
		Result.DiminishingFactor = FMath::Min(A.DiminishingFactor, B.DiminishingFactor);
	}
	else
	{
		Result.DiminishingFactor = A.DiminishingFactor > 0.0f ? A.DiminishingFactor : B.DiminishingFactor;
	}

	if (Result.Multiplier <= 0.0f)
	{
		Result.Multiplier = 1.0f;
	}
	if (Result.DiminishingFactor <= 0.0f)
	{
		Result.DiminishingFactor = 1.0f;
	}

	return Result;
}

bool UGambitEffectResolver::AreConditionsMet(const UGambitItemEffectDefinition* EffectDefinition, FGambitEffectExecutionContext& Context) const
{
	if (!EffectDefinition)
	{
		return false;
	}

	for (const FGambitEffectConditionDefinition& Condition : EffectDefinition->Conditions)
	{
		if (!IsConditionMet(Condition, Context))
		{
			return false;
		}
	}

	return true;
}

bool UGambitEffectResolver::IsConditionMet(const FGambitEffectConditionDefinition& Condition, FGambitEffectExecutionContext& Context) const
{
	bool bResult = true;
	const EGambitEffectTarget Target = Condition.ConditionTarget == EGambitEffectTarget::Target
		? EGambitEffectTarget::Target
		: EGambitEffectTarget::Source;
	const TArray<FGambitDieRuntimeState>& DiceStates = ResolveDiceSnapshotConst(Context, Target);

	switch (Condition.ConditionType)
	{
	case EGambitEffectConditionType::None:
		bResult = true;
		break;
	case EGambitEffectConditionType::DieValueEquals:
		bResult = DiceStates.ContainsByPredicate([&Condition](const FGambitDieRuntimeState& DieState)
		{
			return DieState.EffectiveValue == Condition.Value;
		});
		break;
	case EGambitEffectConditionType::DieValueGreater:
		bResult = DiceStates.ContainsByPredicate([&Condition](const FGambitDieRuntimeState& DieState)
		{
			return DieState.EffectiveValue > Condition.Value;
		});
		break;
	case EGambitEffectConditionType::DieValueLower:
		bResult = DiceStates.ContainsByPredicate([&Condition](const FGambitDieRuntimeState& DieState)
		{
			return DieState.EffectiveValue < Condition.Value;
		});
		break;
	case EGambitEffectConditionType::HasAtLeastMatchingDice:
	{
		int32 MatchCount = 0;
		for (const FGambitDieRuntimeState& DieState : DiceStates)
		{
			if (DieState.EffectiveValue == Condition.Value)
			{
				MatchCount++;
			}
		}
		bResult = MatchCount >= FMath::Max(1, Condition.Count);
		break;
	}
	case EGambitEffectConditionType::HasCombinationType:
		bResult = Context.CurrentCombinationResult.Combination == Condition.CombinationType
			|| Context.CurrentScoreBreakdown.Combination == Condition.CombinationType;
		break;
	case EGambitEffectConditionType::EvenDiceCount:
		bResult = CompareInt(CountMatchingDiceForCondition(DiceStates, Condition), Condition.Comparison, FMath::Max(0, Condition.Count));
		break;
	case EGambitEffectConditionType::OddDiceCount:
		bResult = CompareInt(CountMatchingDiceForCondition(DiceStates, Condition), Condition.Comparison, FMath::Max(0, Condition.Count));
		break;
	case EGambitEffectConditionType::AllDiceEven:
		bResult = DiceStates.Num() > 0 && CountMatchingDiceForCondition(DiceStates, Condition) == DiceStates.Num();
		break;
	case EGambitEffectConditionType::AllDiceOdd:
		bResult = DiceStates.Num() > 0 && CountMatchingDiceForCondition(DiceStates, Condition) == DiceStates.Num();
		break;
	case EGambitEffectConditionType::AllDiceValueGreaterOrEqual:
		bResult = DiceStates.Num() > 0 && CountMatchingDiceForCondition(DiceStates, Condition) == DiceStates.Num();
		break;
	case EGambitEffectConditionType::AllDiceValueLowerOrEqual:
		bResult = DiceStates.Num() > 0 && CountMatchingDiceForCondition(DiceStates, Condition) == DiceStates.Num();
		break;
	case EGambitEffectConditionType::AllDiceValueBetween:
		bResult = DiceStates.Num() > 0 && CountMatchingDiceForCondition(DiceStates, Condition) == DiceStates.Num();
		break;
	case EGambitEffectConditionType::AllDiceValueInSet:
		bResult = DiceStates.Num() > 0 && CountMatchingDiceForCondition(DiceStates, Condition) == DiceStates.Num();
		break;
	case EGambitEffectConditionType::DiceAverage:
	{
		if (DiceStates.Num() == 0)
		{
			bResult = false;
			break;
		}

		bResult = CompareFloat(CalculateDiceAverage(DiceStates), Condition.Comparison, static_cast<float>(Condition.Value));
		break;
	}
	case EGambitEffectConditionType::SourceDieComparedToAverage:
	{
		const int32 SourceDieIndex = Target == EGambitEffectTarget::Target ? Context.TargetDieHandIndex : Context.SourceDieHandIndex;
		if (!DiceStates.IsValidIndex(SourceDieIndex) || DiceStates.Num() == 0)
		{
			bResult = false;
			break;
		}

		bResult = CompareFloat(
			static_cast<float>(DiceStates[SourceDieIndex].EffectiveValue),
			Condition.Comparison,
			CalculateDiceAverage(DiceStates) + static_cast<float>(Condition.Value));
		break;
	}
	case EGambitEffectConditionType::RawScore:
		bResult = CompareInt(ResolveRawScore(Context), Condition.Comparison, Condition.Value);
		break;
	case EGambitEffectConditionType::FirstDieValue:
	{
		const FGambitDieRuntimeState* FirstDieState = nullptr;
		for (const FGambitDieRuntimeState& DieState : DiceStates)
		{
			if (!FirstDieState
				|| (DieState.HandIndex >= 0 && (FirstDieState->HandIndex < 0 || DieState.HandIndex < FirstDieState->HandIndex)))
			{
				FirstDieState = &DieState;
			}
		}
		bResult = FirstDieState ? CompareInt(FirstDieState->EffectiveValue, Condition.Comparison, Condition.Value) : false;
		break;
	}
	case EGambitEffectConditionType::SourceDieValue:
		bResult = Context.SourceDice.IsValidIndex(Context.SourceDieHandIndex)
			? CompareInt(Context.SourceDice[Context.SourceDieHandIndex].EffectiveValue, Condition.Comparison, Condition.Value)
			: false;
		break;
	case EGambitEffectConditionType::SourceDieLocked:
		bResult = Context.SourceDice.IsValidIndex(Context.SourceDieHandIndex)
			&& Context.SourceDice[Context.SourceDieHandIndex].bLocked;
		break;
	case EGambitEffectConditionType::NoComboOrHighDice:
	{
		EGambitCombinationType Combination = Context.CurrentCombinationResult.Combination;
		if (Combination == EGambitCombinationType::None)
		{
			Combination = Context.CurrentScoreBreakdown.Combination;
		}
		bResult = Combination == EGambitCombinationType::None || Combination == EGambitCombinationType::HighDice;
		break;
	}
	case EGambitEffectConditionType::AllDiceUsedBySelectedCombination:
		bResult = Context.CurrentCombinationResult.bAllDiceUsedBySelectedCombo;
		break;
	case EGambitEffectConditionType::DiceValuesPalindrome:
		bResult = AreDiceValuesPalindrome(DiceStates);
		break;
	case EGambitEffectConditionType::OwnedDiceDistinctTypeCount:
	case EGambitEffectConditionType::OwnedDiceMostRepeatedTypeCount:
	case EGambitEffectConditionType::OwnedDiceRarityCount:
	case EGambitEffectConditionType::OwnedDiceRarityAtLeastCount:
	case EGambitEffectConditionType::OwnedItemRarityCount:
	case EGambitEffectConditionType::OwnedItemRarityAtLeastCount:
	case EGambitEffectConditionType::OwnedModuleRarityCount:
	case EGambitEffectConditionType::OwnedModuleRarityAtLeastCount:
		bResult = CompareInt(ResolveInventoryCountForCondition(Context, Target, Condition), Condition.Comparison, FMath::Max(0, Condition.Count));
		break;
	case EGambitEffectConditionType::RerollsRemaining:
		bResult = CompareInt(FMath::Max(0, Context.RerollLimit - Context.RerollsUsed), Condition.Comparison, Condition.Value);
		break;
	case EGambitEffectConditionType::RerollsUsedEqualsLimit:
		bResult = Context.RerollsUsed == Context.RerollLimit;
		break;
	case EGambitEffectConditionType::LastAffectedDieValueIncreased:
		bResult = Context.LastAffectedDieValueBefore != INDEX_NONE
			&& Context.LastAffectedDieValueAfter != INDEX_NONE
			&& Context.LastAffectedDieValueAfter > Context.LastAffectedDieValueBefore;
		break;
	case EGambitEffectConditionType::SourceDieHasMatchingValue:
		bResult = DoesSourceDieHaveMatchingValue(Context);
		break;
	case EGambitEffectConditionType::GoldThreshold:
		if (const UGambitEconomyComponent* EconomyComponent = ResolveEconomyComponent(Context, Target))
		{
			bResult = CompareInt(EconomyComponent->GetCurrentGold(), Condition.Comparison, Condition.Value);
		}
		else
		{
			bResult = false;
		}
		break;
	case EGambitEffectConditionType::RankCondition:
		bResult = CompareInt(Target == EGambitEffectTarget::Target ? Context.TargetRank : Context.SourceRank, Condition.Comparison, Condition.Value);
		break;
	case EGambitEffectConditionType::LowestGold:
		bResult = IsGoldExtremum(Context, Target, true);
		break;
	case EGambitEffectConditionType::HighestGold:
		bResult = IsGoldExtremum(Context, Target, false);
		break;
	case EGambitEffectConditionType::LastRank:
	{
		const int32 PlayerCount = Context.MatchPlayerCount > 0 ? Context.MatchPlayerCount : Condition.Value;
		const int32 Rank = Target == EGambitEffectTarget::Target ? Context.TargetRank : Context.SourceRank;
		bResult = PlayerCount > 0 && Rank == PlayerCount;
		break;
	}
	case EGambitEffectConditionType::RerollsUsedCondition:
		bResult = CompareInt(Context.RerollsUsed, Condition.Comparison, Condition.Value);
		break;
	case EGambitEffectConditionType::ItemRarity:
		bResult = Context.SourceItem ? CompareRarity(Context.SourceItem->Rarity, Condition.Comparison, Condition.Rarity) : false;
		break;
	case EGambitEffectConditionType::ItemTag:
		bResult = Context.SourceItem ? Context.SourceItem->Tags.Contains(Condition.Tag) : false;
		if (!bResult && Context.SourceDiceDefinition)
		{
			bResult = Context.SourceDiceDefinition->Tags.Contains(Condition.Tag);
		}
		if (!bResult)
		{
			const int32 SelectedDieIndex = ResolveSelectedDieHandIndex(Context, Target);
			if (DiceStates.IsValidIndex(SelectedDieIndex))
			{
				bResult = DoesDieHaveTag(DiceStates[SelectedDieIndex], Condition.Tag);
			}
		}
		if (!bResult && Target == EGambitEffectTarget::Source)
		{
			bResult = DiceStates.ContainsByPredicate([&Condition](const FGambitDieRuntimeState& DieState)
			{
				return DieState.RuntimeTags.Contains(Condition.Tag);
			});
		}
		break;
	case EGambitEffectConditionType::ShopItemRarity:
		bResult = Context.ShopPurchase.ItemDefinition
			? CompareRarity(Context.ShopPurchase.ItemDefinition->Rarity, Condition.Comparison, Condition.Rarity)
			: false;
		break;
	case EGambitEffectConditionType::ShopItemTag:
		bResult = Context.ShopPurchase.ItemDefinition
			? Context.ShopPurchase.ItemDefinition->Tags.Contains(Condition.Tag)
			: false;
		break;
	case EGambitEffectConditionType::ShopPrice:
		bResult = CompareInt(Context.ShopPurchase.ResolvedPrice, Condition.Comparison, Condition.Value);
		break;
	case EGambitEffectConditionType::ShopPurchaseCount:
		bResult = CompareInt(Context.ShopPurchase.PurchasesMadeBefore, Condition.Comparison, Condition.Value);
		break;
	case EGambitEffectConditionType::ShopOfferUsesSharedPool:
		bResult = Condition.Value != 0
			? Context.ShopPurchase.bUsesSharedPool
			: !Context.ShopPurchase.bUsesSharedPool;
		break;
	case EGambitEffectConditionType::RerollValueIncreased:
		bResult = CompareInt(CountRerollDeltasByValueDirection(Context, 1), Condition.Comparison, FMath::Max(1, Condition.Count));
		break;
	case EGambitEffectConditionType::RerollValueDecreased:
		bResult = CompareInt(CountRerollDeltasByValueDirection(Context, -1), Condition.Comparison, FMath::Max(1, Condition.Count));
		break;
	case EGambitEffectConditionType::RerollNoValueChanged:
		bResult = DidRerollChangeNoDiceValues(Context);
		break;
	case EGambitEffectConditionType::AnyDieRerolledAtLeastCount:
		bResult = CompareInt(Context.MaxRerollCountForAnyDieThisRound, Condition.Comparison, FMath::Max(1, Condition.Count));
		break;
	case EGambitEffectConditionType::ShopGlobalPurchaseTypeCount:
		bResult = CompareInt(Context.ShopPurchase.GlobalPurchasesForItemType, Condition.Comparison, FMath::Max(0, Condition.Count));
		break;
	case EGambitEffectConditionType::ShopGlobalPurchaseRarityCount:
		bResult = CompareInt(Context.ShopPurchase.GlobalPurchasesForItemRarity, Condition.Comparison, FMath::Max(0, Condition.Count));
		break;
	case EGambitEffectConditionType::ShopGlobalPurchaseTypeAndRarityCount:
		bResult = CompareInt(Context.ShopPurchase.GlobalPurchasesForItemTypeAndRarity, Condition.Comparison, FMath::Max(0, Condition.Count));
		break;
	case EGambitEffectConditionType::ChancePercentage:
	{
		const float Roll = Context.RandomStream.FRandRange(0.0f, 100.0f);
		bResult = Roll <= FMath::Clamp(Condition.ChancePercent, 0.0f, 100.0f);
		UE_LOG(
			LogGambit,
			Log,
			TEXT("EffectResolver: ChanceCondition Roll=%.2f Chance=%.2f Result=%s"),
			Roll,
			Condition.ChancePercent,
			bResult ? TEXT("Pass") : TEXT("Fail"));
		break;
	}
	default:
		bResult = false;
		break;
	}

	return Condition.bInvert ? !bResult : bResult;
}

bool UGambitEffectResolver::ApplyEffectDefinition(UGambitItemEffectDefinition* EffectDefinition, FGambitEffectExecutionContext& Context) const
{
	if (!EffectDefinition)
	{
		return false;
	}

	if (EffectDefinition->EffectType == EGambitItemEffectType::CopyLastTriggeredEffect)
	{
		UGambitItemEffectDefinition* PreviousEffect = Context.LastTriggeredEffectDefinition.Get();
		if (!PreviousEffect || PreviousEffect == EffectDefinition)
		{
			return false;
		}

		return ApplyEffectDefinition(PreviousEffect, Context);
	}

	if (EffectDefinition->Target == EGambitEffectTarget::SourceAndTarget)
	{
		const bool bSourceTriggered = ApplyEffectToTarget(EffectDefinition, EGambitEffectTarget::Source, Context);
		const bool bTargetTriggered = Context.TargetPlayer && Context.TargetPlayer != Context.SourcePlayer
			? ApplyEffectToTarget(EffectDefinition, EGambitEffectTarget::Target, Context)
			: false;
		return bSourceTriggered || bTargetTriggered;
	}

	return ApplyEffectToTarget(EffectDefinition, EffectDefinition->Target, Context);
}

bool UGambitEffectResolver::ApplyEffectToTarget(
	UGambitItemEffectDefinition* EffectDefinition,
	const EGambitEffectTarget Target,
	FGambitEffectExecutionContext& Context) const
{
	if (!EffectDefinition)
	{
		return false;
	}

	const int32 Amount = ResolveContextualAmountAsInt(EffectDefinition, Context, Target);
	switch (EffectDefinition->EffectType)
	{
	case EGambitItemEffectType::None:
		return false;
	case EGambitItemEffectType::ScoreModifier:
		ApplyScoreModifier(ResolveScoreModifierDelta(Context, Target), EffectDefinition->ScoreModifier);
		AddScoreModifierDebugLine(Context, EffectDefinition->ScoreModifier, GetEffectName(EffectDefinition));
		return true;
	case EGambitItemEffectType::AddScoreFlat:
	{
		const float ScoreBefore = Context.CurrentScoreBreakdown.FinalScore;
		if (IsPostScoreHook(Context.Hook))
		{
			ApplyFlatScoreToBreakdown(Context.CurrentScoreBreakdown, static_cast<float>(Amount));
		}
		else
		{
			ResolveScoreModifierDelta(Context, Target).AdditiveBonus += static_cast<float>(Amount);
		}
		const float ScoreAfter = Context.CurrentScoreBreakdown.FinalScore;
		Context.DebugScoreLines.Add(MakeScoreDebugLine(
			Context,
			EGambitDebugScoreLineType::Additive,
			GetEffectName(EffectDefinition),
			static_cast<float>(Amount),
			0.0f,
			1.0f,
			ScoreBefore,
			ScoreAfter,
			FString::Printf(TEXT("%s adds %+d score"), *GetEffectName(EffectDefinition), Amount)));
		return true;
	}
	case EGambitItemEffectType::MultiplyScore:
	{
		const float Multiplier = ResolveContextualMultiplier(EffectDefinition, Context, Target);
		const float ScoreBefore = Context.CurrentScoreBreakdown.FinalScore;
		if (IsPostScoreHook(Context.Hook))
		{
			ApplyScoreMultiplierToBreakdown(Context.CurrentScoreBreakdown, Multiplier);
		}
		else
		{
			ResolveScoreModifierDelta(Context, Target).Multiplier *= Multiplier;
		}
		const float ScoreAfter = Context.CurrentScoreBreakdown.FinalScore;
		Context.DebugScoreLines.Add(MakeScoreDebugLine(
			Context,
			EGambitDebugScoreLineType::Multiplier,
			GetEffectName(EffectDefinition),
			0.0f,
			0.0f,
			Multiplier,
			ScoreBefore,
			ScoreAfter,
			FString::Printf(TEXT("%s multiplies score by x%0.2f"), *GetEffectName(EffectDefinition), Multiplier)));
		return true;
	}
	case EGambitItemEffectType::MultiplyDiceContribution:
	{
		const float Bonus = ResolveDiceContributionMultiplierBonus(EffectDefinition, Context, Target);
		if (FMath::IsNearlyZero(Bonus))
		{
			return false;
		}

		ResolveScoreModifierDelta(Context, Target).DiceContributionMultiplierBonus += Bonus;
		Context.DebugScoreLines.Add(MakeScoreDebugLine(
			Context,
			EGambitDebugScoreLineType::DiceContribution,
			GetEffectName(EffectDefinition),
			0.0f,
			Bonus,
			1.0f,
			0.0f,
			0.0f,
			FString::Printf(TEXT("%s changes dice contribution by %+0.2f"), *GetEffectName(EffectDefinition), Bonus)));
		return true;
	}
	case EGambitItemEffectType::AddGold:
		if (UGambitEconomyComponent* EconomyComponent = ResolveEconomyComponent(Context, Target))
		{
			const int32 GoldBefore = EconomyComponent->GetCurrentGold();
			const int32 GoldAfter = EconomyComponent->AddGold(Amount);
			Context.DebugGoldLines.Add(MakeGoldDebugLine(
				Context,
				EGambitDebugGoldLineType::Effect,
				Amount,
				GoldBefore,
				GoldAfter,
				FString::Printf(TEXT("%s adds %+d gold to %s"), *GetEffectName(EffectDefinition), Amount, *ResolveDebugTargetName(Context, Target))));
			return true;
		}
		return false;
	case EGambitItemEffectType::SpendGold:
		if (UGambitEconomyComponent* EconomyComponent = ResolveEconomyComponent(Context, Target))
		{
			const int32 Cost = FMath::Max(0, Amount);
			const int32 GoldBefore = EconomyComponent->GetCurrentGold();
			const bool bSpent = EconomyComponent->SpendGold(Cost);
			if (bSpent)
			{
				Context.DebugGoldLines.Add(MakeGoldDebugLine(
					Context,
					EGambitDebugGoldLineType::Effect,
					-Cost,
					GoldBefore,
					EconomyComponent->GetCurrentGold(),
					FString::Printf(TEXT("%s spends %d gold from %s"), *GetEffectName(EffectDefinition), Cost, *ResolveDebugTargetName(Context, Target))));
			}
			return bSpent;
		}
		return false;
	case EGambitItemEffectType::ModifyDieValue:
	{
		TArray<FGambitDieRuntimeState>& DiceStates = ResolveDiceSnapshot(Context, Target);
		const TArray<int32> Indexes = BuildAffectedDieIndexes(EffectDefinition, Context, Target, DiceStates);
		if (Indexes.Num() == 0)
		{
			return false;
		}

		UGambitDiceComponent* DiceComponent = ResolveDiceComponent(Context, Target);
		for (const int32 DieIndex : Indexes)
		{
			if (DiceComponent)
			{
				DiceComponent->ModifyDieValue(DieIndex, Amount);
			}
			else if (DiceStates.IsValidIndex(DieIndex))
			{
				FGambitDieRuntimeState& DieState = DiceStates[DieIndex];
				DieState.EffectiveValue += Amount;
				RefreshRuntimeScoreContribution(DieState);
			}
		}
		RefreshDiceSnapshot(Context, Target);
		return true;
	}
	case EGambitItemEffectType::SetDieValue:
	{
		TArray<FGambitDieRuntimeState>& DiceStates = ResolveDiceSnapshot(Context, Target);
		const TArray<int32> Indexes = BuildAffectedDieIndexes(EffectDefinition, Context, Target, DiceStates);
		if (Indexes.Num() == 0)
		{
			return false;
		}

		const int32 DieValue = ResolveIntScalar(EffectDefinition, TEXT("DieValue"), EffectDefinition->DieValue);
		UGambitDiceComponent* DiceComponent = ResolveDiceComponent(Context, Target);
		for (const int32 DieIndex : Indexes)
		{
			if (DiceComponent)
			{
				DiceComponent->SetDieValue(DieIndex, DieValue);
			}
			else if (DiceStates.IsValidIndex(DieIndex))
			{
				FGambitDieRuntimeState& DieState = DiceStates[DieIndex];
				DieState.EffectiveValue = DieValue;
				RefreshRuntimeScoreContribution(DieState);
			}
		}
		RefreshDiceSnapshot(Context, Target);
		return true;
	}
	case EGambitItemEffectType::LockDie:
	{
		TArray<FGambitDieRuntimeState>& DiceStates = ResolveDiceSnapshot(Context, Target);
		const TArray<int32> Indexes = BuildAffectedDieIndexes(EffectDefinition, Context, Target, DiceStates);
		if (Indexes.Num() == 0)
		{
			return false;
		}

		UGambitDiceComponent* DiceComponent = ResolveDiceComponent(Context, Target);
		for (const int32 DieIndex : Indexes)
		{
			if (DiceComponent)
			{
				DiceComponent->SetDieLocked(DieIndex, true);
			}
			else if (DiceStates.IsValidIndex(DieIndex))
			{
				if (DiceStates[DieIndex].bCanBeLocked)
				{
					DiceStates[DieIndex].bLocked = true;
				}
			}
		}
		RefreshDiceSnapshot(Context, Target);
		return true;
	}
	case EGambitItemEffectType::RerollDie:
	{
		TArray<FGambitDieRuntimeState>& DiceStates = ResolveDiceSnapshot(Context, Target);
		const TArray<int32> Indexes = BuildAffectedDieIndexes(EffectDefinition, Context, Target, DiceStates);
		if (Indexes.Num() == 0)
		{
			return false;
		}

		UGambitDiceComponent* DiceComponent = ResolveDiceComponent(Context, Target);
		bool bRerolledAny = false;
		for (const int32 DieIndex : Indexes)
		{
			const TArray<FGambitDieRuntimeState>& CurrentDiceStates = DiceComponent ? DiceComponent->GetDiceStatesRef() : DiceStates;
			if (!CurrentDiceStates.IsValidIndex(DieIndex))
			{
				continue;
			}

			const int32 ValueBefore = CurrentDiceStates[DieIndex].EffectiveValue;
			const bool bRerolled = DiceComponent
				? DiceComponent->RollDieAtIndex(DieIndex, Context.RandomStream)
				: RerollRuntimeDieState(DiceStates[DieIndex], Context.RandomStream);
			if (!bRerolled)
			{
				continue;
			}

			const TArray<FGambitDieRuntimeState>& UpdatedDiceStates = DiceComponent ? DiceComponent->GetDiceStatesRef() : DiceStates;
			if (UpdatedDiceStates.IsValidIndex(DieIndex))
			{
				Context.LastAffectedDieIndex = DieIndex;
				Context.LastAffectedDieValueBefore = ValueBefore;
				Context.LastAffectedDieValueAfter = UpdatedDiceStates[DieIndex].EffectiveValue;
			}
			bRerolledAny = true;
		}

		RefreshDiceSnapshot(Context, Target);
		return bRerolledAny;
	}
	case EGambitItemEffectType::AddReroll:
	case EGambitItemEffectType::ModifyRerollLimit:
		Context.RerollLimitDelta += Amount;
		return true;
	case EGambitItemEffectType::GrantConsumable:
	{
		UGambitConsumableDefinition* ConsumableDefinition = EffectDefinition->GrantedConsumableDefinition.Get();
		if (!ConsumableDefinition)
		{
			ConsumableDefinition = Context.GrantedConsumable.Get();
		}

		if (UGambitInventoryComponent* InventoryComponent = ResolveInventoryComponent(Context, Target))
		{
			const bool bAdded = InventoryComponent->AddConsumable(ConsumableDefinition);
			if (bAdded)
			{
				Context.GrantedConsumable = ConsumableDefinition;
			}
			return bAdded;
		}
		return false;
	}
	case EGambitItemEffectType::AddTemporaryScoreModifier:
	{
		FGambitScoreModifierContext& TargetModifier = Target == EGambitEffectTarget::Target
			? Context.TargetTemporaryScoreModifierDelta
			: Context.TemporaryScoreModifierDelta;
		FGambitScoreModifierContext DebugModifier = EffectDefinition->ScoreModifier;
		ApplyScoreModifier(TargetModifier, EffectDefinition->ScoreModifier);
		if (Amount != 0)
		{
			TargetModifier.AdditiveBonus += static_cast<float>(Amount);
			DebugModifier.AdditiveBonus += static_cast<float>(Amount);
		}
		const float TemporaryMultiplier = ResolveContextualMultiplier(EffectDefinition, Context, Target);
		if (!FMath::IsNearlyEqual(TemporaryMultiplier, 1.0f))
		{
			TargetModifier.Multiplier *= TemporaryMultiplier;
			DebugModifier.Multiplier *= TemporaryMultiplier;
		}
		AddScoreModifierDebugLine(Context, DebugModifier, FString::Printf(TEXT("%s temporary modifier"), *GetEffectName(EffectDefinition)));
		return true;
	}
	case EGambitItemEffectType::StealScore:
	{
		AGambitPlayerState* SourcePlayer = Context.SourcePlayer.Get();
		AGambitPlayerState* TargetPlayer = Context.TargetPlayer.Get();
		if (!SourcePlayer || !TargetPlayer || SourcePlayer == TargetPlayer)
		{
			return false;
		}

		FGambitScoreBreakdown TargetBreakdown = TargetPlayer->GetLastScoreBreakdown();
		const int32 ActualAmount = FMath::Min(FMath::Max(0, Amount), TargetBreakdown.FinalScore);
		if (ActualAmount <= 0)
		{
			return false;
		}

		TargetBreakdown.FinalScore -= ActualAmount;
		TargetBreakdown.ScoreAfterCap = FMath::Max(0.0f, TargetBreakdown.ScoreAfterCap - static_cast<float>(ActualAmount));
		TargetPlayer->ApplyRoundScore(TargetBreakdown);

		if (Target == EGambitEffectTarget::Source || SourcePlayer == Context.SourcePlayer)
		{
			const float ScoreBefore = Context.CurrentScoreBreakdown.FinalScore;
			ApplyFlatScoreToBreakdown(Context.CurrentScoreBreakdown, static_cast<float>(ActualAmount));
			Context.DebugScoreLines.Add(MakeScoreDebugLine(
				Context,
				EGambitDebugScoreLineType::Additive,
				GetEffectName(EffectDefinition),
				static_cast<float>(ActualAmount),
				0.0f,
				1.0f,
				ScoreBefore,
				Context.CurrentScoreBreakdown.FinalScore,
				FString::Printf(TEXT("%s steals %d score"), *GetEffectName(EffectDefinition), ActualAmount)));
		}
		else
		{
			FGambitScoreBreakdown SourceBreakdown = SourcePlayer->GetLastScoreBreakdown();
			const float ScoreBefore = SourceBreakdown.FinalScore;
			ApplyFlatScoreToBreakdown(SourceBreakdown, static_cast<float>(ActualAmount));
			SourcePlayer->ApplyRoundScore(SourceBreakdown);
			Context.DebugScoreLines.Add(MakeScoreDebugLine(
				Context,
				EGambitDebugScoreLineType::Additive,
				GetEffectName(EffectDefinition),
				static_cast<float>(ActualAmount),
				0.0f,
				1.0f,
				ScoreBefore,
				SourceBreakdown.FinalScore,
				FString::Printf(TEXT("%s steals %d score"), *GetEffectName(EffectDefinition), ActualAmount)));
		}
		return true;
	}
	case EGambitItemEffectType::StealGold:
	{
		UGambitEconomyComponent* SourceEconomy = Context.SourceEconomyComponent.Get();
		UGambitEconomyComponent* TargetEconomy = Context.TargetEconomyComponent.Get();
		if (!SourceEconomy || !TargetEconomy || SourceEconomy == TargetEconomy)
		{
			return false;
		}

		const int32 ActualAmount = FMath::Min(FMath::Max(0, Amount), TargetEconomy->GetCurrentGold());
		if (ActualAmount <= 0 || !TargetEconomy->SpendGold(ActualAmount))
		{
			return false;
		}

		Context.DebugGoldLines.Add(MakeGoldDebugLine(
			Context,
			EGambitDebugGoldLineType::Effect,
			-ActualAmount,
			TargetEconomy->GetCurrentGold() + ActualAmount,
			TargetEconomy->GetCurrentGold(),
			FString::Printf(TEXT("%s steals %d gold from target"), *GetEffectName(EffectDefinition), ActualAmount)));

		const int32 SourceGoldBefore = SourceEconomy->GetCurrentGold();
		SourceEconomy->AddGold(ActualAmount);
		Context.DebugGoldLines.Add(MakeGoldDebugLine(
			Context,
			EGambitDebugGoldLineType::Effect,
			ActualAmount,
			SourceGoldBefore,
			SourceEconomy->GetCurrentGold(),
			FString::Printf(TEXT("%s gains %d stolen gold"), *GetEffectName(EffectDefinition), ActualAmount)));
		return true;
	}
	case EGambitItemEffectType::PreventNegativeEffect:
		Context.bPreventNegativeEffects = true;
		return true;
	case EGambitItemEffectType::DestroyOrRemoveDiceChance:
	{
		const float ChancePercent = ResolveScalar(EffectDefinition, TEXT("ChancePercent"), EffectDefinition->Amount > 0.0f ? EffectDefinition->Amount : 100.0f);
		const float Roll = Context.RandomStream.FRandRange(0.0f, 100.0f);
		if (Roll > FMath::Clamp(ChancePercent, 0.0f, 100.0f))
		{
			UE_LOG(
				LogGambit,
				Log,
				TEXT("EffectResolver: RemoveDiceChance failed Roll=%.2f Chance=%.2f"),
				Roll,
				ChancePercent);
			return false;
		}

		TArray<FGambitDieRuntimeState>& DiceStates = ResolveDiceSnapshot(Context, Target);
		const TArray<int32> Indexes = BuildAffectedDieIndexes(EffectDefinition, Context, Target, DiceStates);
		if (Indexes.Num() == 0)
		{
			return false;
		}

		const int32 DieIndex = Indexes[0];
		if (UGambitDiceComponent* DiceComponent = ResolveDiceComponent(Context, Target))
		{
			DiceComponent->RemoveDieAtIndex(DieIndex);
		}
		else if (DiceStates.IsValidIndex(DieIndex))
		{
			DiceStates.RemoveAt(DieIndex);
		}
		if (UGambitInventoryComponent* InventoryComponent = ResolveInventoryComponent(Context, Target))
		{
			UGambitDiceDefinition* RemovedDieDefinition = nullptr;
			InventoryComponent->RemoveOwnedDieAtIndex(DieIndex, RemovedDieDefinition);
		}
		RefreshDiceSnapshot(Context, Target);
		return true;
	}
	case EGambitItemEffectType::TransformDiceForRound:
	{
		TArray<FGambitDieRuntimeState>& DiceStates = ResolveDiceSnapshot(Context, Target);
		const TArray<int32> Indexes = BuildAffectedDieIndexes(EffectDefinition, Context, Target, DiceStates);
		if (Indexes.Num() == 0)
		{
			return false;
		}

		UGambitDiceComponent* DiceComponent = ResolveDiceComponent(Context, Target);
		UGambitDiceDefinition* DiceDefinition = EffectDefinition->TransformDiceDefinition.Get();
		for (const int32 DieIndex : Indexes)
		{
			if (DiceComponent && DiceDefinition)
			{
				DiceComponent->TransformDieToDefinitionForRound(DieIndex, DiceDefinition, true);
			}
			else if (DiceComponent)
			{
				const int32 Value = DiceStates.IsValidIndex(DieIndex)
					? DiceStates[DieIndex].EffectiveValue
					: ResolveIntScalar(EffectDefinition, TEXT("DieValue"), EffectDefinition->DieValue);
				DiceComponent->TransformDieForRound(
					DieIndex,
					EffectDefinition->TransformDiceType,
					EffectDefinition->TransformMinValue,
					EffectDefinition->TransformMaxValue,
					Value);
			}
			else if (DiceStates.IsValidIndex(DieIndex))
			{
				FGambitDieRuntimeState& DieState = DiceStates[DieIndex];
				if (DiceDefinition)
				{
					DieState.DiceDefinition = DiceDefinition;
					DieState.ComboContributionCount = FMath::Max(0, DiceDefinition->DefaultComboContributionCount);
					DieState.bCountsForScoreSum = DiceDefinition->bCountsForScoreSum;
					DieState.bCountsForCombinations = DiceDefinition->bCountsForCombinations;
					DieState.bCanBeRerolled = DiceDefinition->bCanBeRerolled;
					DieState.bCanBeLocked = DiceDefinition->bCanBeLocked;
					DieState.bDestroyedAfterRound = DiceDefinition->bDestroyedAfterRound;
					DieState.RuntimeTags = DiceDefinition->Tags;
					if (!DieState.bCanBeLocked)
					{
						DieState.bLocked = false;
					}
				}
				RefreshRuntimeScoreContribution(DieState);
			}
		}
		RefreshDiceSnapshot(Context, Target);
		return true;
	}
	case EGambitItemEffectType::AddTemporaryDie:
	{
		UGambitDiceComponent* DiceComponent = ResolveDiceComponent(Context, Target);
		if (!DiceComponent)
		{
			return false;
		}

		const bool bRollImmediately = ResolveScalar(EffectDefinition, TEXT("RollImmediately"), 1.0f) > 0.0f;
		DiceComponent->AddTemporaryDie(EffectDefinition->TransformDiceDefinition.Get(), bRollImmediately, Context.RandomStream);
		RefreshDiceSnapshot(Context, Target);
		return true;
	}
	case EGambitItemEffectType::SetDieScoreContributionValue:
	case EGambitItemEffectType::SetDieComboContributionCount:
	case EGambitItemEffectType::SetDieCountsForScoreSum:
	case EGambitItemEffectType::SetDieCountsForCombinations:
	case EGambitItemEffectType::SetDieCanBeRerolled:
	case EGambitItemEffectType::SetDieCanBeLocked:
	case EGambitItemEffectType::MarkDieDestroyedAfterRound:
	case EGambitItemEffectType::AddDieRuntimeTags:
	case EGambitItemEffectType::RemoveDieRuntimeTags:
	{
		TArray<FGambitDieRuntimeState>& DiceStates = ResolveDiceSnapshot(Context, Target);
		const TArray<int32> Indexes = BuildAffectedDieIndexes(EffectDefinition, Context, Target, DiceStates);
		if (Indexes.Num() == 0)
		{
			return false;
		}

		UGambitDiceComponent* DiceComponent = ResolveDiceComponent(Context, Target);
		for (const int32 DieIndex : Indexes)
		{
			switch (EffectDefinition->EffectType)
			{
			case EGambitItemEffectType::SetDieScoreContributionValue:
				if (DiceComponent)
				{
					DiceComponent->SetDieScoreContributionValue(
						DieIndex,
						ResolveIntScalar(EffectDefinition, TEXT("ScoreContributionValue"), EffectDefinition->ScoreContributionValue));
				}
				else if (DiceStates.IsValidIndex(DieIndex))
				{
					DiceStates[DieIndex].ScoreContributionValue = ResolveIntScalar(EffectDefinition, TEXT("ScoreContributionValue"), EffectDefinition->ScoreContributionValue);
				}
				break;
			case EGambitItemEffectType::SetDieComboContributionCount:
				if (DiceComponent)
				{
					DiceComponent->SetDieComboContributionCount(
						DieIndex,
						ResolveIntScalar(EffectDefinition, TEXT("ComboContributionCount"), EffectDefinition->ComboContributionCount));
				}
				else if (DiceStates.IsValidIndex(DieIndex))
				{
					DiceStates[DieIndex].ComboContributionCount = FMath::Max(0, ResolveIntScalar(EffectDefinition, TEXT("ComboContributionCount"), EffectDefinition->ComboContributionCount));
				}
				break;
			case EGambitItemEffectType::SetDieCountsForScoreSum:
				if (DiceComponent)
				{
					DiceComponent->SetDieCountsForScoreSum(DieIndex, EffectDefinition->bRuntimeBoolValue);
				}
				else if (DiceStates.IsValidIndex(DieIndex))
				{
					DiceStates[DieIndex].bCountsForScoreSum = EffectDefinition->bRuntimeBoolValue;
				}
				break;
			case EGambitItemEffectType::SetDieCountsForCombinations:
				if (DiceComponent)
				{
					DiceComponent->SetDieCountsForCombinations(DieIndex, EffectDefinition->bRuntimeBoolValue);
				}
				else if (DiceStates.IsValidIndex(DieIndex))
				{
					DiceStates[DieIndex].bCountsForCombinations = EffectDefinition->bRuntimeBoolValue;
				}
				break;
			case EGambitItemEffectType::SetDieCanBeRerolled:
				if (DiceComponent)
				{
					DiceComponent->SetDieCanBeRerolled(DieIndex, EffectDefinition->bRuntimeBoolValue);
				}
				else if (DiceStates.IsValidIndex(DieIndex))
				{
					DiceStates[DieIndex].bCanBeRerolled = EffectDefinition->bRuntimeBoolValue;
				}
				break;
			case EGambitItemEffectType::SetDieCanBeLocked:
				if (DiceComponent)
				{
					DiceComponent->SetDieCanBeLocked(DieIndex, EffectDefinition->bRuntimeBoolValue);
				}
				else if (DiceStates.IsValidIndex(DieIndex))
				{
					DiceStates[DieIndex].bCanBeLocked = EffectDefinition->bRuntimeBoolValue;
					if (!EffectDefinition->bRuntimeBoolValue)
					{
						DiceStates[DieIndex].bLocked = false;
					}
				}
				break;
			case EGambitItemEffectType::MarkDieDestroyedAfterRound:
				if (DiceComponent)
				{
					DiceComponent->SetDieDestroyedAfterRound(DieIndex, EffectDefinition->bRuntimeBoolValue);
				}
				else if (DiceStates.IsValidIndex(DieIndex))
				{
					DiceStates[DieIndex].bDestroyedAfterRound = EffectDefinition->bRuntimeBoolValue;
				}
				break;
			case EGambitItemEffectType::AddDieRuntimeTags:
				if (DiceComponent)
				{
					DiceComponent->AddRuntimeTags(DieIndex, EffectDefinition->RuntimeTagsToAdd);
				}
				else if (DiceStates.IsValidIndex(DieIndex))
				{
					for (const FName Tag : EffectDefinition->RuntimeTagsToAdd)
					{
						DiceStates[DieIndex].RuntimeTags.AddUnique(Tag);
					}
				}
				break;
			case EGambitItemEffectType::RemoveDieRuntimeTags:
				if (DiceComponent)
				{
					DiceComponent->RemoveRuntimeTags(DieIndex, EffectDefinition->RuntimeTagsToRemove);
				}
				else if (DiceStates.IsValidIndex(DieIndex))
				{
					for (const FName Tag : EffectDefinition->RuntimeTagsToRemove)
					{
						DiceStates[DieIndex].RuntimeTags.Remove(Tag);
					}
				}
				break;
			default:
				break;
			}
		}

		RefreshDiceSnapshot(Context, Target);
		return true;
	}
	case EGambitItemEffectType::ModifyShopOfferCount:
		Context.ShopOfferCountDelta += Amount;
		return true;
	case EGambitItemEffectType::AddShopDiscountPercent:
		Context.ShopPurchase.DiscountPercent += ResolveScalar(EffectDefinition, TEXT("Percent"), EffectDefinition->Amount);
		UE_LOG(LogGambit, Log, TEXT("EffectResolver: Shop discount applied Percent=%.2f Total=%.2f"), ResolveScalar(EffectDefinition, TEXT("Percent"), EffectDefinition->Amount), Context.ShopPurchase.DiscountPercent);
		return true;
	case EGambitItemEffectType::AddShopSurchargePercent:
		Context.ShopPurchase.SurchargePercent += ResolveScalar(EffectDefinition, TEXT("Percent"), EffectDefinition->Amount);
		UE_LOG(LogGambit, Log, TEXT("EffectResolver: Shop surcharge applied Percent=%.2f Total=%.2f"), ResolveScalar(EffectDefinition, TEXT("Percent"), EffectDefinition->Amount), Context.ShopPurchase.SurchargePercent);
		return true;
	case EGambitItemEffectType::AddShopFlatPriceDelta:
		Context.ShopPurchase.FlatPriceDelta += Amount;
		return true;
	case EGambitItemEffectType::MakeShopOfferFree:
		if (Context.Hook == EGambitEffectHook::PostShopGenerate)
		{
			if (Context.GeneratedShopOffers.Num() == 0)
			{
				return false;
			}

			TArray<int32> PricedOfferIndexes;
			for (int32 OfferIndex = 0; OfferIndex < Context.GeneratedShopOffers.Num(); ++OfferIndex)
			{
				if (!Context.GeneratedShopOffers[OfferIndex].bFree)
				{
					PricedOfferIndexes.Add(OfferIndex);
				}
			}
			if (PricedOfferIndexes.Num() == 0)
			{
				return false;
			}

			const int32 ChosenOfferIndex = PricedOfferIndexes[Context.RandomStream.RandRange(0, PricedOfferIndexes.Num() - 1)];
			FGambitShopOffer& Offer = Context.GeneratedShopOffers[ChosenOfferIndex];
			Offer.bFree = true;
			Offer.FreeReason = GetEffectName(EffectDefinition);
			Offer.Price = 0;
			UE_LOG(LogGambit, Log, TEXT("EffectResolver: Shop generated offer made free OfferId=%d Reason=%s"), Offer.OfferId, *Offer.FreeReason);
			return true;
		}

		Context.ShopPurchase.bFree = true;
		Context.ShopPurchase.FreeReason = GetEffectName(EffectDefinition);
		UE_LOG(LogGambit, Log, TEXT("EffectResolver: Shop offer made free Reason=%s"), *Context.ShopPurchase.FreeReason);
		return true;
	case EGambitItemEffectType::AddShopCashbackPercent:
		Context.ShopPurchase.CashbackPercent += ResolveScalar(EffectDefinition, TEXT("Percent"), EffectDefinition->Amount);
		UE_LOG(LogGambit, Log, TEXT("EffectResolver: Cashback applied Percent=%.2f Total=%.2f"), ResolveScalar(EffectDefinition, TEXT("Percent"), EffectDefinition->Amount), Context.ShopPurchase.CashbackPercent);
		return true;
	case EGambitItemEffectType::AddPurchaseGoldDelta:
		Context.ShopPurchase.GoldDeltaOnPurchase += Amount;
		return true;
	case EGambitItemEffectType::BlockShopPurchase:
		Context.ShopPurchase.bBlockedByEffect = true;
		Context.ShopPurchase.FailureReason = GetEffectName(EffectDefinition);
		return true;
	case EGambitItemEffectType::ModifyDebtLimit:
		if (UGambitEconomyComponent* EconomyComponent = ResolveEconomyComponent(Context, Target))
		{
			EconomyComponent->SetDebtLimitModifier(GetEffectSourceId(EffectDefinition), Amount);
			return true;
		}
		return false;
	case EGambitItemEffectType::ModifyMaxGold:
		if (UGambitEconomyComponent* EconomyComponent = ResolveEconomyComponent(Context, Target))
		{
			EconomyComponent->SetMaxGoldModifier(GetEffectSourceId(EffectDefinition), Amount);
			return true;
		}
		return false;
	case EGambitItemEffectType::ModifyInterestInterval:
		if (UGambitEconomyComponent* EconomyComponent = ResolveEconomyComponent(Context, Target))
		{
			EconomyComponent->SetInterestIntervalModifier(GetEffectSourceId(EffectDefinition), Amount);
			return true;
		}
		return false;
	case EGambitItemEffectType::ModifyMaxInterest:
		if (UGambitEconomyComponent* EconomyComponent = ResolveEconomyComponent(Context, Target))
		{
			EconomyComponent->SetMaxInterestModifier(GetEffectSourceId(EffectDefinition), Amount);
			return true;
		}
		return false;
	case EGambitItemEffectType::ModifyInterestBonus:
		if (UGambitEconomyComponent* EconomyComponent = ResolveEconomyComponent(Context, Target))
		{
			EconomyComponent->SetInterestBonusModifier(GetEffectSourceId(EffectDefinition), Amount);
			return true;
		}
		return false;
	case EGambitItemEffectType::AddRecurringGoldIncome:
		if (UGambitEconomyComponent* EconomyComponent = ResolveEconomyComponent(Context, Target))
		{
			EconomyComponent->AddRecurringGoldIncome(EffectDefinition->EffectId, Amount, ResolveDurationRounds(EffectDefinition));
			return true;
		}
		return false;
	case EGambitItemEffectType::SellItem:
		if (UGambitInventoryComponent* InventoryComponent = ResolveInventoryComponent(Context, Target))
		{
			UGambitItemDefinition* ItemToSell = ResolveReferencedShopItem(EffectDefinition, Context);
			if (!InventoryComponent->RemoveItemDefinition(ItemToSell))
			{
				return false;
			}
			if (UGambitEconomyComponent* EconomyComponent = ResolveEconomyComponent(Context, Target))
			{
				const int32 GoldBefore = EconomyComponent->GetCurrentGold();
				EconomyComponent->AddGold(Amount);
				Context.DebugGoldLines.Add(MakeGoldDebugLine(
					Context,
					EGambitDebugGoldLineType::Effect,
					Amount,
					GoldBefore,
					EconomyComponent->GetCurrentGold(),
					FString::Printf(TEXT("%s sells item for %+d gold"), *GetEffectName(EffectDefinition), Amount)));
			}
			return true;
		}
		return false;
	case EGambitItemEffectType::SellDie:
		if (UGambitInventoryComponent* InventoryComponent = ResolveInventoryComponent(Context, Target))
		{
			UGambitDiceDefinition* RemovedDieDefinition = nullptr;
			if (!InventoryComponent->RemoveOwnedDieAtIndex(ResolveIntScalar(EffectDefinition, TEXT("DieIndex"), EffectDefinition->DieIndex), RemovedDieDefinition))
			{
				return false;
			}
			if (UGambitEconomyComponent* EconomyComponent = ResolveEconomyComponent(Context, Target))
			{
				const int32 GoldBefore = EconomyComponent->GetCurrentGold();
				EconomyComponent->AddGold(Amount);
				Context.DebugGoldLines.Add(MakeGoldDebugLine(
					Context,
					EGambitDebugGoldLineType::Effect,
					Amount,
					GoldBefore,
					EconomyComponent->GetCurrentGold(),
					FString::Printf(TEXT("%s sells die for %+d gold"), *GetEffectName(EffectDefinition), Amount)));
			}
			return true;
		}
		return false;
	case EGambitItemEffectType::AddSharedPoolStock:
		if (Context.SharedPoolComponent)
		{
			if (UGambitItemDefinition* ItemDefinition = ResolveReferencedShopItem(EffectDefinition, Context))
			{
				Context.SharedPoolComponent->AddDynamicStock(ItemDefinition, Amount);
				return true;
			}
		}
		return false;
	case EGambitItemEffectType::SetSharedPoolPurchaseLimit:
		if (Context.SharedPoolComponent)
		{
			if (UGambitItemDefinition* ItemDefinition = ResolveReferencedShopItem(EffectDefinition, Context))
			{
				Context.SharedPoolComponent->SetPurchaseLimit(ItemDefinition, Amount);
				return true;
			}
		}
		return false;
	case EGambitItemEffectType::SetSharedPoolItemUnavailable:
		if (Context.SharedPoolComponent)
		{
			if (UGambitItemDefinition* ItemDefinition = ResolveReferencedShopItem(EffectDefinition, Context))
			{
				Context.SharedPoolComponent->SetItemUnavailable(ItemDefinition);
				return true;
			}
		}
		return false;
	case EGambitItemEffectType::CopyLastTriggeredEffect:
		return false;
	default:
		return false;
	}
}

bool UGambitEffectResolver::CompareInt(const int32 Left, const EGambitEffectComparison Comparison, const int32 Right)
{
	switch (Comparison)
	{
	case EGambitEffectComparison::Equal: return Left == Right;
	case EGambitEffectComparison::NotEqual: return Left != Right;
	case EGambitEffectComparison::Greater: return Left > Right;
	case EGambitEffectComparison::GreaterOrEqual: return Left >= Right;
	case EGambitEffectComparison::Lower: return Left < Right;
	case EGambitEffectComparison::LowerOrEqual: return Left <= Right;
	default: return false;
	}
}

bool UGambitEffectResolver::CompareRarity(
	const EGambitItemRarity Left,
	const EGambitEffectComparison Comparison,
	const EGambitItemRarity Right)
{
	return CompareInt(static_cast<int32>(Left), Comparison, static_cast<int32>(Right));
}

FString UGambitEffectResolver::EffectHookToString(const EGambitEffectHook Hook)
{
	switch (Hook)
	{
	case EGambitEffectHook::RoundStart: return TEXT("RoundStart");
	case EGambitEffectHook::PreRoll: return TEXT("PreRoll");
	case EGambitEffectHook::PostRoll: return TEXT("PostRoll");
	case EGambitEffectHook::PreReroll: return TEXT("PreReroll");
	case EGambitEffectHook::PostReroll: return TEXT("PostReroll");
	case EGambitEffectHook::ConsumableUse: return TEXT("ConsumableUse");
	case EGambitEffectHook::PreCombinationEvaluation: return TEXT("PreCombinationEvaluation");
	case EGambitEffectHook::PostCombinationEvaluation: return TEXT("PostCombinationEvaluation");
	case EGambitEffectHook::PreScoreCalculation: return TEXT("PreScoreCalculation");
	case EGambitEffectHook::ScoreModifier: return TEXT("ScoreModifier");
	case EGambitEffectHook::PostScoreCalculation: return TEXT("PostScoreCalculation");
	case EGambitEffectHook::Reward: return TEXT("Reward");
	case EGambitEffectHook::Ranking: return TEXT("Ranking");
	case EGambitEffectHook::PreShopGenerate: return TEXT("PreShopGenerate");
	case EGambitEffectHook::PostShopGenerate: return TEXT("PostShopGenerate");
	case EGambitEffectHook::PrePriceResolve: return TEXT("PrePriceResolve");
	case EGambitEffectHook::PostPriceResolve: return TEXT("PostPriceResolve");
	case EGambitEffectHook::PrePurchase: return TEXT("PrePurchase");
	case EGambitEffectHook::PostPurchase: return TEXT("PostPurchase");
	case EGambitEffectHook::ShopSkipped: return TEXT("ShopSkipped");
	case EGambitEffectHook::RoundEnd: return TEXT("RoundEnd");
	default: return TEXT("Unknown");
	}
}

FString UGambitEffectResolver::EffectTypeToString(const EGambitItemEffectType EffectType)
{
	switch (EffectType)
	{
	case EGambitItemEffectType::None: return TEXT("None");
	case EGambitItemEffectType::ScoreModifier: return TEXT("ScoreModifier");
	case EGambitItemEffectType::AddScoreFlat: return TEXT("AddScoreFlat");
	case EGambitItemEffectType::MultiplyScore: return TEXT("MultiplyScore");
	case EGambitItemEffectType::MultiplyDiceContribution: return TEXT("MultiplyDiceContribution");
	case EGambitItemEffectType::AddGold: return TEXT("AddGold");
	case EGambitItemEffectType::SpendGold: return TEXT("SpendGold");
	case EGambitItemEffectType::ModifyDieValue: return TEXT("ModifyDieValue");
	case EGambitItemEffectType::SetDieValue: return TEXT("SetDieValue");
	case EGambitItemEffectType::LockDie: return TEXT("LockDie");
	case EGambitItemEffectType::RerollDie: return TEXT("RerollDie");
	case EGambitItemEffectType::AddReroll: return TEXT("AddReroll");
	case EGambitItemEffectType::ModifyRerollLimit: return TEXT("ModifyRerollLimit");
	case EGambitItemEffectType::GrantConsumable: return TEXT("GrantConsumable");
	case EGambitItemEffectType::AddTemporaryScoreModifier: return TEXT("AddTemporaryScoreModifier");
	case EGambitItemEffectType::StealScore: return TEXT("StealScore");
	case EGambitItemEffectType::StealGold: return TEXT("StealGold");
	case EGambitItemEffectType::PreventNegativeEffect: return TEXT("PreventNegativeEffect");
	case EGambitItemEffectType::DestroyOrRemoveDiceChance: return TEXT("DestroyOrRemoveDiceChance");
	case EGambitItemEffectType::TransformDiceForRound: return TEXT("TransformDiceForRound");
	case EGambitItemEffectType::AddTemporaryDie: return TEXT("AddTemporaryDie");
	case EGambitItemEffectType::SetDieScoreContributionValue: return TEXT("SetDieScoreContributionValue");
	case EGambitItemEffectType::SetDieComboContributionCount: return TEXT("SetDieComboContributionCount");
	case EGambitItemEffectType::SetDieCountsForScoreSum: return TEXT("SetDieCountsForScoreSum");
	case EGambitItemEffectType::SetDieCountsForCombinations: return TEXT("SetDieCountsForCombinations");
	case EGambitItemEffectType::SetDieCanBeRerolled: return TEXT("SetDieCanBeRerolled");
	case EGambitItemEffectType::SetDieCanBeLocked: return TEXT("SetDieCanBeLocked");
	case EGambitItemEffectType::MarkDieDestroyedAfterRound: return TEXT("MarkDieDestroyedAfterRound");
	case EGambitItemEffectType::AddDieRuntimeTags: return TEXT("AddDieRuntimeTags");
	case EGambitItemEffectType::RemoveDieRuntimeTags: return TEXT("RemoveDieRuntimeTags");
	case EGambitItemEffectType::ModifyShopOfferCount: return TEXT("ModifyShopOfferCount");
	case EGambitItemEffectType::AddShopDiscountPercent: return TEXT("AddShopDiscountPercent");
	case EGambitItemEffectType::AddShopSurchargePercent: return TEXT("AddShopSurchargePercent");
	case EGambitItemEffectType::AddShopFlatPriceDelta: return TEXT("AddShopFlatPriceDelta");
	case EGambitItemEffectType::MakeShopOfferFree: return TEXT("MakeShopOfferFree");
	case EGambitItemEffectType::AddShopCashbackPercent: return TEXT("AddShopCashbackPercent");
	case EGambitItemEffectType::AddPurchaseGoldDelta: return TEXT("AddPurchaseGoldDelta");
	case EGambitItemEffectType::BlockShopPurchase: return TEXT("BlockShopPurchase");
	case EGambitItemEffectType::ModifyDebtLimit: return TEXT("ModifyDebtLimit");
	case EGambitItemEffectType::ModifyMaxGold: return TEXT("ModifyMaxGold");
	case EGambitItemEffectType::ModifyInterestInterval: return TEXT("ModifyInterestInterval");
	case EGambitItemEffectType::ModifyMaxInterest: return TEXT("ModifyMaxInterest");
	case EGambitItemEffectType::ModifyInterestBonus: return TEXT("ModifyInterestBonus");
	case EGambitItemEffectType::AddRecurringGoldIncome: return TEXT("AddRecurringGoldIncome");
	case EGambitItemEffectType::SellItem: return TEXT("SellItem");
	case EGambitItemEffectType::SellDie: return TEXT("SellDie");
	case EGambitItemEffectType::AddSharedPoolStock: return TEXT("AddSharedPoolStock");
	case EGambitItemEffectType::SetSharedPoolPurchaseLimit: return TEXT("SetSharedPoolPurchaseLimit");
	case EGambitItemEffectType::SetSharedPoolItemUnavailable: return TEXT("SetSharedPoolItemUnavailable");
	case EGambitItemEffectType::CopyLastTriggeredEffect: return TEXT("CopyLastTriggeredEffect");
	default: return TEXT("Unknown");
	}
}
