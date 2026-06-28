#include "Game/States/GambitGameState.h"

#include "Managers/SharedPool/GambitSharedPoolComponent.h"
#include "Players/States/GambitPlayerState.h"
#include "Ranking/Components/GambitRankingComponent.h"

AGambitGameState::AGambitGameState()
{
	SharedPoolComponent = CreateDefaultSubobject<UGambitSharedPoolComponent>(TEXT("SharedPoolComponent"));
	RankingComponent = CreateDefaultSubobject<UGambitRankingComponent>(TEXT("RankingComponent"));
}

void AGambitGameState::SetCurrentPhase(const EGambitRoundPhase NewPhase)
{
	if (CurrentPhase == NewPhase)
	{
		return;
	}

	const EGambitRoundPhase OldPhase = CurrentPhase;
	CurrentPhase = NewPhase;
	OnPhaseChanged.Broadcast(OldPhase, CurrentPhase);
}

void AGambitGameState::SetCurrentRoundIndex(const int32 NewRoundIndex)
{
	if (CurrentRoundIndex == NewRoundIndex)
	{
		return;
	}

	CurrentRoundIndex = NewRoundIndex;
	OnRoundChanged.Broadcast(CurrentRoundIndex);
}

void AGambitGameState::SetRoundRankingSnapshot(const TArray<FGambitRankingEntry>& NewRanking)
{
	RoundRankingSnapshot = NewRanking;
	OnRankingUpdated.Broadcast();
}

void AGambitGameState::SetMatchLifecycleState(const EGambitMatchLifecycleState NewState)
{
	if (MatchLifecycleState == NewState)
	{
		return;
	}

	const EGambitMatchLifecycleState OldState = MatchLifecycleState;
	MatchLifecycleState = NewState;
	OnMatchLifecycleChanged.Broadcast(OldState, MatchLifecycleState);
}

void AGambitGameState::SetMatchSetupConfig(const FGambitMatchSetupConfig& NewSetup)
{
	if (MatchSetupConfig.LocalPlayerCount == NewSetup.LocalPlayerCount
		&& MatchSetupConfig.RoundCount == NewSetup.RoundCount)
	{
		return;
	}

	MatchSetupConfig = NewSetup;
	OnMatchSetupChanged.Broadcast(MatchSetupConfig);
}

void AGambitGameState::SetFinalRankingSnapshot(const TArray<FGambitFinalRankingEntry>& NewFinalRanking)
{
	FinalRankingSnapshot = NewFinalRanking;
	OnFinalRankingUpdated.Broadcast();
}

TArray<AGambitPlayerState*> AGambitGameState::GetGambitPlayerStates() const
{
	TArray<AGambitPlayerState*> Result;
	Result.Reserve(PlayerArray.Num());

	for (APlayerState* PlayerState : PlayerArray)
	{
		if (AGambitPlayerState* GambitPlayerState = Cast<AGambitPlayerState>(PlayerState))
		{
			Result.Add(GambitPlayerState);
		}
	}

	return Result;
}
