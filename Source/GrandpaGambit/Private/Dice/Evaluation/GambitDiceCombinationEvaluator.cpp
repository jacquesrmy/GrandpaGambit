#include "Dice/Evaluation/GambitDiceCombinationEvaluator.h"

#include "Core/Settings/GambitGameBalanceSettings.h"

namespace
{
	int32 GetMaxStraightLength(const TArray<int32>& UniqueSortedValues)
	{
		if (UniqueSortedValues.Num() == 0)
		{
			return 0;
		}

		int32 CurrentStraightLength = 1;
		int32 MaxStraightLength = 1;
		for (int32 Index = 1; Index < UniqueSortedValues.Num(); ++Index)
		{
			if (UniqueSortedValues[Index] == UniqueSortedValues[Index - 1] + 1)
			{
				CurrentStraightLength++;
			}
			else
			{
				CurrentStraightLength = 1;
			}

			MaxStraightLength = FMath::Max(MaxStraightLength, CurrentStraightLength);
		}

		return MaxStraightLength;
	}

	TArray<int32> GetFirstIndexesForValue(const TArray<FGambitDieRuntimeState>& DiceStates, const int32 Value, const int32 WantedCount)
	{
		TArray<int32> Indexes;
		for (int32 DieIndex = 0; DieIndex < DiceStates.Num() && Indexes.Num() < WantedCount; ++DieIndex)
		{
			const FGambitDieRuntimeState& DieState = DiceStates[DieIndex];
			if (!DieState.bRemovedFromRound
				&& DieState.bCountsForCombinations
				&& DieState.ComboContributionCount > 0
				&& DieState.EffectiveValue == Value)
			{
				Indexes.Add(DieIndex);
			}
		}

		return Indexes;
	}

	void AddFirstIndexesForValue(TArray<int32>& InOutIndexes, const TArray<FGambitDieRuntimeState>& DiceStates, const int32 Value, const int32 WantedCount)
	{
		for (const int32 DieIndex : GetFirstIndexesForValue(DiceStates, Value, WantedCount))
		{
			InOutIndexes.AddUnique(DieIndex);
		}
	}

	int32 FindBestValueWithMinimumCount(const TMap<int32, int32>& ValueCounts, const int32 MinimumCount)
	{
		int32 BestValue = INDEX_NONE;
		int32 BestCount = 0;
		for (const TPair<int32, int32>& Pair : ValueCounts)
		{
			if (Pair.Value < MinimumCount)
			{
				continue;
			}

			if (Pair.Value > BestCount || (Pair.Value == BestCount && Pair.Key > BestValue))
			{
				BestValue = Pair.Key;
				BestCount = Pair.Value;
			}
		}

		return BestValue;
	}

	TArray<int32> FindBestPairValues(const TMap<int32, int32>& ValueCounts, const int32 WantedPairCount)
	{
		TArray<int32> PairValues;
		for (const TPair<int32, int32>& Pair : ValueCounts)
		{
			if (Pair.Value >= 2)
			{
				PairValues.Add(Pair.Key);
			}
		}
		PairValues.Sort([](const int32 Left, const int32 Right)
		{
			return Left > Right;
		});
		if (PairValues.Num() > WantedPairCount)
		{
			PairValues.SetNum(WantedPairCount);
		}
		return PairValues;
	}

	TArray<int32> FindBestStraightValues(const TArray<int32>& UniqueSortedValues, const int32 MinimumLength)
	{
		TArray<int32> BestRun;
		TArray<int32> CurrentRun;
		for (const int32 Value : UniqueSortedValues)
		{
			if (CurrentRun.Num() == 0 || Value == CurrentRun.Last() + 1)
			{
				CurrentRun.Add(Value);
			}
			else
			{
				if (CurrentRun.Num() >= BestRun.Num())
				{
					BestRun = CurrentRun;
				}
				CurrentRun.Reset();
				CurrentRun.Add(Value);
			}
		}

		if (CurrentRun.Num() >= BestRun.Num())
		{
			BestRun = CurrentRun;
		}

		if (BestRun.Num() > MinimumLength)
		{
			BestRun.RemoveAt(0, BestRun.Num() - MinimumLength);
		}

		return BestRun.Num() >= MinimumLength ? BestRun : TArray<int32>();
	}
}

FGambitDiceCombinationResult UGambitDiceCombinationEvaluator::EvaluateDice(const TArray<FGambitDieRuntimeState>& DiceStates) const
{
	FGambitDiceCombinationResult Result;
	if (DiceStates.Num() == 0)
	{
		return Result;
	}

	TArray<int32> ExpandedCombinationValues;
	TArray<int32> UniqueCombinationValues;
	TMap<int32, int32> ValueCounts;
	int32 TotalCombinationContributions = 0;

	for (const FGambitDieRuntimeState& DieState : DiceStates)
	{
		if (DieState.bRemovedFromRound)
		{
			continue;
		}

		if (DieState.bCountsForScoreSum)
		{
			Result.DiceSum += DieState.ScoreContributionValue;
		}

		if (!DieState.bCountsForCombinations)
		{
			continue;
		}

		const int32 ContributionCount = FMath::Max(0, DieState.ComboContributionCount);
		if (ContributionCount == 0)
		{
			continue;
		}

		ValueCounts.FindOrAdd(DieState.EffectiveValue) += ContributionCount;
		TotalCombinationContributions += ContributionCount;
		UniqueCombinationValues.AddUnique(DieState.EffectiveValue);

		for (int32 CountIndex = 0; CountIndex < ContributionCount; ++CountIndex)
		{
			ExpandedCombinationValues.Add(DieState.EffectiveValue);
		}
	}

	ExpandedCombinationValues.Sort();
	UniqueCombinationValues.Sort();
	Result.MatchedValues = ExpandedCombinationValues;

	const UGambitGameBalanceSettings* BalanceSettings = UGambitGameBalanceSettings::Get();
	if (TotalCombinationContributions <= 0)
	{
		Result.BaseCombinationScore = BalanceSettings->GetBaseScoreForCombination(Result.Combination);
		Result.RawScore = Result.BaseCombinationScore + Result.DiceSum;
		return Result;
	}

	int32 MaxCount = 0;
	int32 ExactPairCount = 0;
	bool bHasThree = false;
	bool bHasTwo = false;

	for (const TPair<int32, int32>& Pair : ValueCounts)
	{
		MaxCount = FMath::Max(MaxCount, Pair.Value);
		Result.PairCount += Pair.Value / 2;
		if (Pair.Value >= 3)
		{
			bHasThree = true;
		}
		if (Pair.Value >= 2)
		{
			bHasTwo = true;
		}
		if (Pair.Value == 2)
		{
			ExactPairCount++;
		}
	}

	const int32 MaxStraightLength = GetMaxStraightLength(UniqueCombinationValues);
	if (MaxCount >= 5)
	{
		Result.Combination = EGambitCombinationType::FiveOfAKind;
	}
	else if (MaxStraightLength >= TotalCombinationContributions && TotalCombinationContributions >= 4)
	{
		Result.Combination = EGambitCombinationType::StraightLarge;
	}
	else if (MaxCount >= 4)
	{
		Result.Combination = EGambitCombinationType::FourOfAKind;
	}
	else if (bHasThree && (ExactPairCount >= 1 || (bHasTwo && ValueCounts.Num() == 2)))
	{
		Result.Combination = EGambitCombinationType::FullHouse;
	}
	else if (MaxStraightLength >= 4)
	{
		Result.Combination = EGambitCombinationType::StraightSmall;
	}
	else if (MaxCount >= 3)
	{
		Result.Combination = EGambitCombinationType::ThreeOfAKind;
	}
	else if (ExactPairCount >= 2)
	{
		Result.Combination = EGambitCombinationType::TwoPair;
	}
	else if (MaxCount >= 2)
	{
		Result.Combination = EGambitCombinationType::Pair;
	}
	else
	{
		Result.Combination = EGambitCombinationType::HighDice;
	}

	Result.BaseCombinationScore = BalanceSettings->GetBaseScoreForCombination(Result.Combination);
	switch (Result.Combination)
	{
	case EGambitCombinationType::FiveOfAKind:
		AddFirstIndexesForValue(Result.MatchedDieIndexes, DiceStates, FindBestValueWithMinimumCount(ValueCounts, 5), 5);
		break;
	case EGambitCombinationType::FourOfAKind:
		AddFirstIndexesForValue(Result.MatchedDieIndexes, DiceStates, FindBestValueWithMinimumCount(ValueCounts, 4), 4);
		break;
	case EGambitCombinationType::FullHouse:
	{
		const int32 TripleValue = FindBestValueWithMinimumCount(ValueCounts, 3);
		AddFirstIndexesForValue(Result.MatchedDieIndexes, DiceStates, TripleValue, 3);

		TArray<int32> PairValues = FindBestPairValues(ValueCounts, 1);
		if (PairValues.Num() == 0 || PairValues[0] == TripleValue)
		{
			for (const TPair<int32, int32>& Pair : ValueCounts)
			{
				if (Pair.Key != TripleValue && Pair.Value >= 2)
				{
					PairValues.Reset();
					PairValues.Add(Pair.Key);
					break;
				}
			}
		}
		if (PairValues.Num() > 0)
		{
			AddFirstIndexesForValue(Result.MatchedDieIndexes, DiceStates, PairValues[0], 2);
		}
		break;
	}
	case EGambitCombinationType::ThreeOfAKind:
		AddFirstIndexesForValue(Result.MatchedDieIndexes, DiceStates, FindBestValueWithMinimumCount(ValueCounts, 3), 3);
		break;
	case EGambitCombinationType::TwoPair:
		for (const int32 PairValue : FindBestPairValues(ValueCounts, 2))
		{
			AddFirstIndexesForValue(Result.MatchedDieIndexes, DiceStates, PairValue, 2);
		}
		break;
	case EGambitCombinationType::Pair:
		AddFirstIndexesForValue(Result.MatchedDieIndexes, DiceStates, FindBestValueWithMinimumCount(ValueCounts, 2), 2);
		break;
	case EGambitCombinationType::StraightSmall:
	case EGambitCombinationType::StraightLarge:
		for (const int32 StraightValue : FindBestStraightValues(
			UniqueCombinationValues,
			Result.Combination == EGambitCombinationType::StraightLarge ? TotalCombinationContributions : 4))
		{
			AddFirstIndexesForValue(Result.MatchedDieIndexes, DiceStates, StraightValue, 1);
		}
		break;
	case EGambitCombinationType::HighDice:
		for (int32 DieIndex = 0; DieIndex < DiceStates.Num(); ++DieIndex)
		{
			if (!DiceStates[DieIndex].bRemovedFromRound
				&& DiceStates[DieIndex].bCountsForCombinations
				&& DiceStates[DieIndex].ComboContributionCount > 0)
			{
				Result.MatchedDieIndexes.Add(DieIndex);
			}
		}
		break;
	default:
		break;
	}

	Result.MatchedDieIndexes.Sort();
	int32 ComboEligibleDieCount = 0;
	for (const FGambitDieRuntimeState& DieState : DiceStates)
	{
		if (!DieState.bRemovedFromRound && DieState.bCountsForCombinations && DieState.ComboContributionCount > 0)
		{
			ComboEligibleDieCount++;
		}
	}
	Result.bAllDiceUsedBySelectedCombo = Result.MatchedDieIndexes.Num() == ComboEligibleDieCount;
	Result.RawScore = Result.BaseCombinationScore + Result.DiceSum;
	return Result;
}
