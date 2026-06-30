#include "UI/ViewModels/GambitMatchViewModel.h"

#include "Engine/World.h"
#include "Game/Flow/GambitRoundFlowComponent.h"
#include "Game/Modes/GambitGameMode.h"
#include "Game/States/GambitGameState.h"
#include "Players/Controllers/GambitPlayerController.h"
#include "Players/States/GambitPlayerState.h"

namespace
{
	bool IsGambitReadyGatedPhase(const EGambitRoundPhase Phase)
	{
		return Phase == EGambitRoundPhase::SelectionReroll
			|| Phase == EGambitRoundPhase::Action
			|| Phase == EGambitRoundPhase::Shop;
	}

	AGambitPlayerController* ResolveControllerForPlayerState(
		const AGambitGameState* GameState,
		const AGambitPlayerState* PlayerState)
	{
		if (!GameState || !PlayerState)
		{
			return nullptr;
		}

		if (AGambitPlayerController* OwningController = Cast<AGambitPlayerController>(PlayerState->GetOwner()))
		{
			return OwningController;
		}

		UWorld* World = GameState->GetWorld();
		if (!World)
		{
			return nullptr;
		}

		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			AGambitPlayerController* PlayerController = Cast<AGambitPlayerController>(It->Get());
			if (PlayerController && PlayerController->GetPlayerState<AGambitPlayerState>() == PlayerState)
			{
				return PlayerController;
			}
		}

		return nullptr;
	}

	UGambitRoundFlowComponent* ResolveRoundFlowForGameState(const AGambitGameState* GameState)
	{
		const UWorld* World = GameState ? GameState->GetWorld() : nullptr;
		const AGambitGameMode* GameMode = World ? World->GetAuthGameMode<AGambitGameMode>() : nullptr;
		return GameMode ? GameMode->GetRoundFlowComponent() : nullptr;
	}
}

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

TArray<FGambitPlayerSnapshot> UGambitMatchViewModel::BuildPlayerSnapshots() const
{
	TArray<FGambitPlayerSnapshot> Snapshots;
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
			Snapshots.Add(PlayerState->BuildPlayerSnapshot(PlayerIndex));
		}
	}

	return Snapshots;
}

TArray<FGambitUIPlayerSnapshot> UGambitMatchViewModel::BuildUIPlayerSnapshots() const
{
	TArray<FGambitUIPlayerSnapshot> Snapshots;
	if (!BoundGameState)
	{
		return Snapshots;
	}

	const TArray<AGambitPlayerState*> PlayerStates = BoundGameState->GetGambitPlayerStates();
	const UGambitRoundFlowComponent* RoundFlow = ResolveRoundFlowForGameState(BoundGameState);
	Snapshots.Reserve(PlayerStates.Num());
	for (int32 PlayerIndex = 0; PlayerIndex < PlayerStates.Num(); ++PlayerIndex)
	{
		AGambitPlayerState* PlayerState = PlayerStates[PlayerIndex];
		if (!PlayerState)
		{
			continue;
		}

		FGambitUIPlayerSnapshot Snapshot;
		Snapshot.Player = PlayerState->BuildPlayerSnapshot(PlayerIndex);
		Snapshot.Actions = RoundFlow
			? RoundFlow->BuildPlayerActionSnapshot(PlayerState)
			: FGambitUIPlayerActionSnapshot();
		Snapshot.Actions.PlayerIndex = PlayerIndex;
		Snapshot.Actions.Phase = Phase;

		if (const AGambitPlayerController* PlayerController = ResolveControllerForPlayerState(BoundGameState, PlayerState))
		{
			Snapshot.Actions.TargetSelection = PlayerController->BuildTargetSelectionSnapshot();
		}

		Snapshots.Add(Snapshot);
	}

	return Snapshots;
}

FGambitUIMatchSnapshot UGambitMatchViewModel::BuildUIMatchSnapshot() const
{
	FGambitUIMatchSnapshot Snapshot;
	if (!BoundGameState)
	{
		return Snapshot;
	}

	Snapshot.LifecycleState = BoundGameState->GetMatchLifecycleState();
	Snapshot.MatchSetup = BoundGameState->GetMatchSetupConfig();
	Snapshot.CurrentRoundIndex = BoundGameState->GetCurrentRoundIndex();
	Snapshot.CurrentPhase = BoundGameState->GetCurrentPhase();
	Snapshot.bIsReadyGatedPhase = IsGambitReadyGatedPhase(Snapshot.CurrentPhase);
	Snapshot.RoundRanking = BoundGameState->GetRoundRankingSnapshot();
	Snapshot.FinalRanking = BoundGameState->GetFinalRankingSnapshot();
	Snapshot.Players = BuildUIPlayerSnapshots();
	return Snapshot;
}
