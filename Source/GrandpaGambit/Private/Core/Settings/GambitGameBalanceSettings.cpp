#include "Core/Settings/GambitGameBalanceSettings.h"

UGambitGameBalanceSettings::UGambitGameBalanceSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("GrandpaGambit");

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
