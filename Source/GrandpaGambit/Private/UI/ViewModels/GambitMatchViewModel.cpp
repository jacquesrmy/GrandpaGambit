#include "UI/ViewModels/GambitMatchViewModel.h"

#include "Game/States/GambitGameState.h"
#include "Players/States/GambitPlayerState.h"

void UGambitMatchViewModel::InitializeFromGameState(AGambitGameState* InGameState)
{
	if (BoundGameState == InGameState)
	{
		return;
	}

	if (BoundGameState)
	{
		BoundGameState->OnPhaseChanged.RemoveDynamic(this, &UGambitMatchViewModel::HandlePhaseChanged);
		BoundGameState->OnRoundChanged.RemoveDynamic(this, &UGambitMatchViewModel::HandleRoundChanged);
		BoundGameState->OnRankingUpdated.RemoveDynamic(this, &UGambitMatchViewModel::HandleRankingUpdated);
	}

	BoundGameState = InGameState;
	if (!BoundGameState)
	{
		return;
	}

	RoundIndex = BoundGameState->GetCurrentRoundIndex();
	Phase = BoundGameState->GetCurrentPhase();
	RankingSnapshot = BoundGameState->GetRoundRankingSnapshot();

	BoundGameState->OnPhaseChanged.AddDynamic(this, &UGambitMatchViewModel::HandlePhaseChanged);
	BoundGameState->OnRoundChanged.AddDynamic(this, &UGambitMatchViewModel::HandleRoundChanged);
	BoundGameState->OnRankingUpdated.AddDynamic(this, &UGambitMatchViewModel::HandleRankingUpdated);

	OnViewModelUpdated.Broadcast();
}

void UGambitMatchViewModel::HandlePhaseChanged(EGambitRoundPhase OldPhase, EGambitRoundPhase NewPhase)
{
	Phase = NewPhase;
	OnViewModelUpdated.Broadcast();
}

void UGambitMatchViewModel::HandleRoundChanged(const int32 NewRoundIndex)
{
	RoundIndex = NewRoundIndex;
	OnViewModelUpdated.Broadcast();
}

void UGambitMatchViewModel::HandleRankingUpdated()
{
	if (!BoundGameState)
	{
		return;
	}

	RankingSnapshot = BoundGameState->GetRoundRankingSnapshot();
	OnViewModelUpdated.Broadcast();
}

TArray<FGambitDebugPlayerSnapshot> UGambitMatchViewModel::BuildDebugPlayerSnapshots() const
{
	TArray<FGambitDebugPlayerSnapshot> Snapshots;
	if (!BoundGameState)
	{
		return Snapshots;
	}

	const TArray<AGambitPlayerState*> PlayerStates = BoundGameState->GetGambitPlayerStates();
	Snapshots.Reserve(PlayerStates.Num());
	for (int32 PlayerIndex = 0; PlayerIndex < PlayerStates.Num(); ++PlayerIndex)
	{
		if (const AGambitPlayerState* PlayerState = PlayerStates[PlayerIndex])
		{
			Snapshots.Add(PlayerState->BuildDebugPlayerSnapshot(PlayerIndex));
		}
	}

	return Snapshots;
}
