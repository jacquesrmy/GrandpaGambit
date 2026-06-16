#include "Core/Settings/GambitGameBalanceSettings.h"

namespace
{
	const TArray<FGambitCombinationScoreEntry>& GetDefaultCombinationBaseScores()
	{
		static const TArray<FGambitCombinationScoreEntry> DefaultScores = {
			{EGambitCombinationType::HighDice, 5},
			{EGambitCombinationType::Pair, 12},
			{EGambitCombinationType::TwoPair, 18},
			{EGambitCombinationType::ThreeOfAKind, 25},
			{EGambitCombinationType::StraightSmall, 30},
			{EGambitCombinationType::FullHouse, 40},
			{EGambitCombinationType::FourOfAKind, 45},
			{EGambitCombinationType::StraightLarge, 55},
			{EGambitCombinationType::FiveOfAKind, 70}
		};
		return DefaultScores;
	}

	bool IsScoreBearingCombination(const EGambitCombinationType Combination)
	{
		return Combination != EGambitCombinationType::None;
	}

	int32 GetDefaultCombinationBaseScore(const EGambitCombinationType Combination)
	{
		for (const FGambitCombinationScoreEntry& Entry : GetDefaultCombinationBaseScores())
		{
			if (Entry.Combination == Combination)
			{
				return Entry.BaseScore;
			}
		}

		return 0;
	}

	FString CombinationToString(const EGambitCombinationType Combination)
	{
		const UEnum* Enum = StaticEnum<EGambitCombinationType>();
		return Enum ? Enum->GetNameStringByValue(static_cast<int64>(Combination)) : TEXT("Unknown");
	}
}

UGambitGameBalanceSettings::UGambitGameBalanceSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("GrandpaGambit");

	CombinationBaseScores = GetDefaultCombinationBaseScores();

	VictoryPointTable = {
		{1, 5},
		{2, 3},
		{3, 2},
		{4, 1}
	};
}

const UGambitGameBalanceSettings* UGambitGameBalanceSettings::Get()
{
	return GetDefault<UGambitGameBalanceSettings>();
}

int32 UGambitGameBalanceSettings::GetVictoryPointsForRank(const int32 Rank) const
{
	for (const FGambitVictoryPointReward& Reward : VictoryPointTable)
	{
		if (Reward.Rank == Rank)
		{
			return Reward.VictoryPoints;
		}
	}

	return 0;
}

int32 UGambitGameBalanceSettings::GetInterestGoldBonus(const int32 CurrentGold) const
{
	if (InterestIntervalGold <= 0)
	{
		return 0;
	}

	return FMath::Clamp(CurrentGold / InterestIntervalGold, 0, MaxInterestGold);
}

bool UGambitGameBalanceSettings::IsSupportedLocalPlayerCount(const int32 LocalPlayerCount) const
{
	const int32 ClampedMinPlayers = FMath::Clamp(MinLocalPlayers, 1, 4);
	const int32 ClampedMaxPlayers = FMath::Clamp(MaxLocalPlayers, ClampedMinPlayers, 4);
	return LocalPlayerCount >= ClampedMinPlayers && LocalPlayerCount <= ClampedMaxPlayers;
}

int32 UGambitGameBalanceSettings::ClampLocalPlayerCount(const int32 RequestedLocalPlayerCount) const
{
	const int32 ClampedMinPlayers = FMath::Clamp(MinLocalPlayers, 1, 4);
	const int32 ClampedMaxPlayers = FMath::Clamp(MaxLocalPlayers, ClampedMinPlayers, 4);
	return FMath::Clamp(RequestedLocalPlayerCount, ClampedMinPlayers, ClampedMaxPlayers);
}

int32 UGambitGameBalanceSettings::GetRoundGoldRewardFromScore(const int32 RoundScore) const
{
	const int32 GoldStep = FMath::Max(1, ScorePerGoldStep);
	const int32 UnclampedReward = FMath::Max(0, BaseRoundGoldReward) + RoundScore / GoldStep;
	return FMath::Clamp(UnclampedReward, 0, FMath::Max(1, MaxRoundGoldReward));
}

int32 UGambitGameBalanceSettings::GetBaseScoreForCombination(const EGambitCombinationType Combination) const
{
	if (!IsScoreBearingCombination(Combination))
	{
		return 0;
	}

	for (const FGambitCombinationScoreEntry& Entry : CombinationBaseScores)
	{
		if (Entry.Combination == Combination && Entry.BaseScore >= 0)
		{
			return Entry.BaseScore;
		}
	}

	return GetDefaultCombinationBaseScore(Combination);
}

void UGambitGameBalanceSettings::ValidateCombinationBaseScores(TArray<FString>& OutWarnings) const
{
	TSet<uint8> SeenCombinations;
	for (const FGambitCombinationScoreEntry& Entry : CombinationBaseScores)
	{
		if (!IsScoreBearingCombination(Entry.Combination))
		{
			OutWarnings.Add(TEXT("CombinationBaseScores contains None; it is ignored and always resolves to 0."));
			continue;
		}

		const uint8 CombinationKey = static_cast<uint8>(Entry.Combination);
		if (SeenCombinations.Contains(CombinationKey))
		{
			OutWarnings.Add(FString::Printf(
				TEXT("CombinationBaseScores contains duplicate entry %s; the first valid entry is used."),
				*CombinationToString(Entry.Combination)));
		}
		SeenCombinations.Add(CombinationKey);

		if (Entry.BaseScore < 0)
		{
			OutWarnings.Add(FString::Printf(
				TEXT("CombinationBaseScores entry %s has a negative score; the built-in default fallback is used."),
				*CombinationToString(Entry.Combination)));
		}
	}

	for (const FGambitCombinationScoreEntry& DefaultEntry : GetDefaultCombinationBaseScores())
	{
		if (!SeenCombinations.Contains(static_cast<uint8>(DefaultEntry.Combination)))
		{
			OutWarnings.Add(FString::Printf(
				TEXT("CombinationBaseScores is missing %s; the built-in default score %d is used."),
				*CombinationToString(DefaultEntry.Combination),
				DefaultEntry.BaseScore));
		}
	}
}
