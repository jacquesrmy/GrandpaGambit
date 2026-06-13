#include "Ranking/Components/GambitRankingComponent.h"

#include "Core/Settings/GambitGameBalanceSettings.h"
#include "Players/States/GambitPlayerState.h"

UGambitRankingComponent::UGambitRankingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

TArray<FGambitRankingEntry> UGambitRankingComponent::BuildRoundRanking(const TArray<AGambitPlayerState*>& PlayerStates) const
{
	TArray<FGambitRankingEntry> RankingEntries;
	RankingEntries.Reserve(PlayerStates.Num());

	for (AGambitPlayerState* PlayerState : PlayerStates)
	{
		if (!PlayerState)
		{
			continue;
		}

		FGambitRankingEntry Entry;
		Entry.PlayerState = PlayerState;
		Entry.RoundScore = PlayerState->GetCurrentRoundScore();
		RankingEntries.Add(Entry);
	}

	RankingEntries.Sort([](const FGambitRankingEntry& A, const FGambitRankingEntry& B)
	{
		if (A.RoundScore == B.RoundScore)
		{
			const AGambitPlayerState* AState = Cast<AGambitPlayerState>(A.PlayerState);
			const AGambitPlayerState* BState = Cast<AGambitPlayerState>(B.PlayerState);
			const int32 AGold = AState ? AState->GetCurrentGold() : 0;
			const int32 BGold = BState ? BState->GetCurrentGold() : 0;
			return AGold > BGold;
		}

		return A.RoundScore > B.RoundScore;
	});

	for (int32 Index = 0; Index < RankingEntries.Num(); ++Index)
	{
		RankingEntries[Index].Rank = Index + 1;
	}

	return RankingEntries;
}

void UGambitRankingComponent::ApplyVictoryPoints(TArray<FGambitRankingEntry>& InOutRanking) const
{
	const UGambitGameBalanceSettings* Settings = UGambitGameBalanceSettings::Get();
	for (FGambitRankingEntry& Entry : InOutRanking)
	{
		Entry.VictoryPointsGranted = Settings->GetVictoryPointsForRank(Entry.Rank);
		if (AGambitPlayerState* PlayerState = Cast<AGambitPlayerState>(Entry.PlayerState))
		{
			PlayerState->AddVictoryPoints(Entry.VictoryPointsGranted);
		}
	}
}
