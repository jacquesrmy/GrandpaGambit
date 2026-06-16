#include "Items/Effects/GambitEffectTargetResolver.h"

#include "Data/Assets/GambitItemEffectDefinition.h"
#include "Engine/World.h"
#include "Game/States/GambitGameState.h"
#include "Items/Effects/GambitEffectTargetRules.h"
#include "Players/Components/GambitDiceComponent.h"
#include "Players/Components/GambitEconomyComponent.h"
#include "Players/Components/GambitInventoryComponent.h"
#include "Players/States/GambitPlayerState.h"
#include "Shop/Components/GambitShopComponent.h"

namespace
{
	FGambitEffectTargetResolveResult MakeFailure(const FName TargetRuleId, const FString& FailureReason)
	{
		FGambitEffectTargetResolveResult Result;
		Result.TargetRuleId = TargetRuleId;
		Result.FailureReason = FailureReason;
		return Result;
	}

	EGambitEffectTarget NormalizeTargetSide(const EGambitEffectTarget TargetSide)
	{
		return TargetSide == EGambitEffectTarget::Target
			? EGambitEffectTarget::Target
			: EGambitEffectTarget::Source;
	}

	bool IsSourceDieRule(const FName TargetRuleId)
	{
		return TargetRuleId == GambitEffectTargetRules::SourceSelectedDie
			|| TargetRuleId == GambitEffectTargetRules::SourceRandomDie
			|| TargetRuleId == GambitEffectTargetRules::SourceBestDie
			|| TargetRuleId == GambitEffectTargetRules::SourceLowestDie
			|| TargetRuleId == GambitEffectTargetRules::SourceAllDice;
	}

	bool IsTargetDieRule(const FName TargetRuleId)
	{
		return TargetRuleId == GambitEffectTargetRules::TargetSelectedDie
			|| TargetRuleId == GambitEffectTargetRules::TargetRandomDie
			|| TargetRuleId == GambitEffectTargetRules::TargetBestDie
			|| TargetRuleId == GambitEffectTargetRules::TargetLowestDie
			|| TargetRuleId == GambitEffectTargetRules::TargetAllDice;
	}

	bool IsRandomDieRule(const FName TargetRuleId)
	{
		return TargetRuleId == GambitEffectTargetRules::SourceRandomDie
			|| TargetRuleId == GambitEffectTargetRules::TargetRandomDie;
	}

	bool IsBestDieRule(const FName TargetRuleId)
	{
		return TargetRuleId == GambitEffectTargetRules::SourceBestDie
			|| TargetRuleId == GambitEffectTargetRules::TargetBestDie;
	}

	bool IsLowestDieRule(const FName TargetRuleId)
	{
		return TargetRuleId == GambitEffectTargetRules::SourceLowestDie
			|| TargetRuleId == GambitEffectTargetRules::TargetLowestDie;
	}

	bool IsAllDiceRule(const FName TargetRuleId)
	{
		return TargetRuleId == GambitEffectTargetRules::SourceAllDice
			|| TargetRuleId == GambitEffectTargetRules::TargetAllDice;
	}

	EGambitEffectTarget ResolveTargetSideForRule(const FName TargetRuleId, const EGambitEffectTarget RequestedTarget)
	{
		if (IsSourceDieRule(TargetRuleId))
		{
			return EGambitEffectTarget::Source;
		}

		if (IsTargetDieRule(TargetRuleId)
			|| GambitEffectTargetRules::IsPlayerRule(TargetRuleId))
		{
			return EGambitEffectTarget::Target;
		}

		return NormalizeTargetSide(RequestedTarget);
	}

	AGambitPlayerState* ResolveTargetResolverPlayer(const FGambitEffectExecutionContext& Context, const EGambitEffectTarget TargetSide)
	{
		return TargetSide == EGambitEffectTarget::Target ? Context.TargetPlayer.Get() : Context.SourcePlayer.Get();
	}

	UGambitDiceComponent* ResolveTargetResolverDiceComponent(const FGambitEffectExecutionContext& Context, const EGambitEffectTarget TargetSide)
	{
		if (UGambitDiceComponent* DiceComponent = TargetSide == EGambitEffectTarget::Target
			? Context.TargetDiceComponent.Get()
			: Context.SourceDiceComponent.Get())
		{
			return DiceComponent;
		}

		if (AGambitPlayerState* Player = ResolveTargetResolverPlayer(Context, TargetSide))
		{
			return Player->GetDiceComponent();
		}

		return nullptr;
	}

	UGambitEconomyComponent* ResolveTargetResolverEconomyComponent(const FGambitEffectExecutionContext& Context, const EGambitEffectTarget TargetSide)
	{
		if (UGambitEconomyComponent* EconomyComponent = TargetSide == EGambitEffectTarget::Target
			? Context.TargetEconomyComponent.Get()
			: Context.SourceEconomyComponent.Get())
		{
			return EconomyComponent;
		}

		if (AGambitPlayerState* Player = ResolveTargetResolverPlayer(Context, TargetSide))
		{
			return Player->GetEconomyComponent();
		}

		return nullptr;
	}

	UGambitInventoryComponent* ResolveTargetResolverInventoryComponent(const FGambitEffectExecutionContext& Context, const EGambitEffectTarget TargetSide)
	{
		if (UGambitInventoryComponent* InventoryComponent = TargetSide == EGambitEffectTarget::Target
			? Context.TargetInventoryComponent.Get()
			: Context.SourceInventoryComponent.Get())
		{
			return InventoryComponent;
		}

		if (AGambitPlayerState* Player = ResolveTargetResolverPlayer(Context, TargetSide))
		{
			return Player->GetInventoryComponent();
		}

		return nullptr;
	}

	UGambitShopComponent* ResolveTargetResolverShopComponent(const FGambitEffectExecutionContext& Context, const EGambitEffectTarget TargetSide)
	{
		if (UGambitShopComponent* ShopComponent = TargetSide == EGambitEffectTarget::Target
			? Context.TargetShopComponent.Get()
			: Context.SourceShopComponent.Get())
		{
			return ShopComponent;
		}

		if (AGambitPlayerState* Player = ResolveTargetResolverPlayer(Context, TargetSide))
		{
			return Player->GetShopComponent();
		}

		return nullptr;
	}

	UGambitDiceComponent* ResolveDiceComponentForPlayer(
		const FGambitEffectExecutionContext& Context,
		const AGambitPlayerState* Player,
		const EGambitEffectTarget TargetSide)
	{
		if (Player && Player == Context.SourcePlayer.Get() && Context.SourceDiceComponent)
		{
			return Context.SourceDiceComponent.Get();
		}

		if (Player && Player == Context.TargetPlayer.Get() && Context.TargetDiceComponent)
		{
			return Context.TargetDiceComponent.Get();
		}

		return Player ? Player->GetDiceComponent() : ResolveTargetResolverDiceComponent(Context, TargetSide);
	}

	UGambitEconomyComponent* ResolveEconomyComponentForPlayer(
		const FGambitEffectExecutionContext& Context,
		const AGambitPlayerState* Player,
		const EGambitEffectTarget TargetSide)
	{
		if (Player && Player == Context.SourcePlayer.Get() && Context.SourceEconomyComponent)
		{
			return Context.SourceEconomyComponent.Get();
		}

		if (Player && Player == Context.TargetPlayer.Get() && Context.TargetEconomyComponent)
		{
			return Context.TargetEconomyComponent.Get();
		}

		return Player ? Player->GetEconomyComponent() : ResolveTargetResolverEconomyComponent(Context, TargetSide);
	}

	UGambitInventoryComponent* ResolveInventoryComponentForPlayer(
		const FGambitEffectExecutionContext& Context,
		const AGambitPlayerState* Player,
		const EGambitEffectTarget TargetSide)
	{
		if (Player && Player == Context.SourcePlayer.Get() && Context.SourceInventoryComponent)
		{
			return Context.SourceInventoryComponent.Get();
		}

		if (Player && Player == Context.TargetPlayer.Get() && Context.TargetInventoryComponent)
		{
			return Context.TargetInventoryComponent.Get();
		}

		return Player ? Player->GetInventoryComponent() : ResolveTargetResolverInventoryComponent(Context, TargetSide);
	}

	UGambitShopComponent* ResolveShopComponentForPlayer(
		const FGambitEffectExecutionContext& Context,
		const AGambitPlayerState* Player,
		const EGambitEffectTarget TargetSide)
	{
		if (Player && Player == Context.SourcePlayer.Get() && Context.SourceShopComponent)
		{
			return Context.SourceShopComponent.Get();
		}

		if (Player && Player == Context.TargetPlayer.Get() && Context.TargetShopComponent)
		{
			return Context.TargetShopComponent.Get();
		}

		return Player ? Player->GetShopComponent() : ResolveTargetResolverShopComponent(Context, TargetSide);
	}

	TArray<FGambitDieRuntimeState> ResolveDiceSnapshotForPlayer(
		const FGambitEffectExecutionContext& Context,
		const AGambitPlayerState* Player,
		const EGambitEffectTarget TargetSide)
	{
		if (Player && Player == Context.SourcePlayer.Get())
		{
			return Context.SourceDice;
		}

		if (Player && Player == Context.TargetPlayer.Get())
		{
			return Context.TargetDice;
		}

		if (Player)
		{
			return Player->GetDiceStates();
		}

		return TargetSide == EGambitEffectTarget::Target ? Context.TargetDice : Context.SourceDice;
	}

	float ResolveTargetResolverScalar(const UGambitItemEffectDefinition* EffectDefinition, const FName ParameterName, const float Fallback)
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

	int32 ResolveTargetResolverIntScalar(const UGambitItemEffectDefinition* EffectDefinition, const FName ParameterName, const int32 Fallback)
	{
		return FMath::RoundToInt(ResolveTargetResolverScalar(EffectDefinition, ParameterName, static_cast<float>(Fallback)));
	}

	bool HasExplicitDieIndex(const UGambitItemEffectDefinition* EffectDefinition)
	{
		if (!EffectDefinition)
		{
			return false;
		}

		return EffectDefinition->ScalarParameters.Contains(TEXT("DieIndex"))
			|| EffectDefinition->DieIndex != INDEX_NONE;
	}

	bool RequiresDiceIndexes(const UGambitItemEffectDefinition* EffectDefinition)
	{
		return EffectDefinition
			&& (EffectDefinition->bAffectAllDice
				|| HasExplicitDieIndex(EffectDefinition)
				|| GambitEffectTargetRules::IsDieRule(EffectDefinition->TargetRuleId)
				|| GambitEffectTargetRules::IsFirstRerolledDieRule(EffectDefinition->TargetRuleId));
	}

	int32 FindBestDieIndex(const TArray<FGambitDieRuntimeState>& DiceStates, const bool bLowest)
	{
		int32 BestIndex = INDEX_NONE;
		for (int32 Index = 0; Index < DiceStates.Num(); ++Index)
		{
			if (BestIndex == INDEX_NONE)
			{
				BestIndex = Index;
				continue;
			}

			const FGambitDieRuntimeState& Candidate = DiceStates[Index];
			const FGambitDieRuntimeState& CurrentBest = DiceStates[BestIndex];
			const bool bBetterValue = bLowest
				? Candidate.EffectiveValue < CurrentBest.EffectiveValue
				: Candidate.EffectiveValue > CurrentBest.EffectiveValue;
			const bool bTieWithLowerHandIndex = Candidate.EffectiveValue == CurrentBest.EffectiveValue
				&& Candidate.HandIndex < CurrentBest.HandIndex;

			if (bBetterValue || bTieWithLowerHandIndex)
			{
				BestIndex = Index;
			}
		}

		return BestIndex;
	}

	bool BuildAffectedDieIndexes(
		const UGambitItemEffectDefinition* EffectDefinition,
		const FGambitEffectExecutionContext& Context,
		const EGambitEffectTarget TargetSide,
		const TArray<FGambitDieRuntimeState>& DiceStates,
		TArray<int32>& OutIndexes,
		FString& OutFailureReason)
	{
		const FName TargetRuleId = EffectDefinition ? EffectDefinition->TargetRuleId : NAME_None;
		if (DiceStates.Num() == 0)
		{
			if (RequiresDiceIndexes(EffectDefinition))
			{
				OutFailureReason = FString::Printf(
					TEXT("TargetRuleId '%s' requires dice but no dice states are available."),
					*TargetRuleId.ToString());
				return false;
			}

			return true;
		}

		if ((EffectDefinition && EffectDefinition->bAffectAllDice) || IsAllDiceRule(TargetRuleId))
		{
			OutIndexes.Reserve(DiceStates.Num());
			for (int32 Index = 0; Index < DiceStates.Num(); ++Index)
			{
				OutIndexes.Add(Index);
			}
			return true;
		}

		if (EffectDefinition && GambitEffectTargetRules::IsSelectedDieRule(TargetRuleId))
		{
			const int32 SelectedDieIndex = GambitEffectTargetResolver::ResolveSelectedDieHandIndex(Context, TargetSide, TargetRuleId);
			if (!DiceStates.IsValidIndex(SelectedDieIndex))
			{
				OutFailureReason = FString::Printf(TEXT("Selected die index %d is invalid."), SelectedDieIndex);
				return false;
			}

			OutIndexes.Add(SelectedDieIndex);
			return true;
		}

		if (EffectDefinition && GambitEffectTargetRules::IsFirstRerolledDieRule(TargetRuleId))
		{
			const int32 FirstRerolledDieIndex = Context.FirstRerolledDieHandIndexThisRound;
			if (!DiceStates.IsValidIndex(FirstRerolledDieIndex))
			{
				OutFailureReason = FString::Printf(TEXT("First rerolled die index %d is invalid."), FirstRerolledDieIndex);
				return false;
			}

			OutIndexes.Add(FirstRerolledDieIndex);
			return true;
		}

		if (IsRandomDieRule(TargetRuleId))
		{
			FRandomStream RandomStream = Context.RandomStream;
			OutIndexes.Add(RandomStream.RandRange(0, DiceStates.Num() - 1));
			return true;
		}

		if (IsBestDieRule(TargetRuleId) || IsLowestDieRule(TargetRuleId))
		{
			const int32 BestIndex = FindBestDieIndex(DiceStates, IsLowestDieRule(TargetRuleId));
			if (!DiceStates.IsValidIndex(BestIndex))
			{
				OutFailureReason = FString::Printf(TEXT("TargetRuleId '%s' could not select a valid die."), *TargetRuleId.ToString());
				return false;
			}

			OutIndexes.Add(BestIndex);
			return true;
		}

		const int32 RequestedIndex = ResolveTargetResolverIntScalar(EffectDefinition, TEXT("DieIndex"), EffectDefinition ? EffectDefinition->DieIndex : INDEX_NONE);
		if (DiceStates.IsValidIndex(RequestedIndex))
		{
			OutIndexes.Add(RequestedIndex);
			return true;
		}

		if (TargetSide == EGambitEffectTarget::Source && DiceStates.IsValidIndex(Context.SourceDieHandIndex))
		{
			OutIndexes.Add(Context.SourceDieHandIndex);
			return true;
		}

		OutIndexes.Add(0);
		return true;
	}

	bool ShouldResolveDiceIndexes(const UGambitItemEffectDefinition* EffectDefinition, const TArray<FGambitDieRuntimeState>& DiceStates)
	{
		return DiceStates.Num() > 0 || RequiresDiceIndexes(EffectDefinition);
	}

	void AddUniquePlayer(TArray<AGambitPlayerState*>& Players, AGambitPlayerState* Player)
	{
		if (Player)
		{
			Players.AddUnique(Player);
		}
	}

	void AddPlayersFromWorld(TArray<AGambitPlayerState*>& Players, const AGambitPlayerState* AnchorPlayer)
	{
		const UWorld* World = AnchorPlayer ? AnchorPlayer->GetWorld() : nullptr;
		const AGambitGameState* GameState = World ? World->GetGameState<AGambitGameState>() : nullptr;
		if (!GameState)
		{
			return;
		}

		for (AGambitPlayerState* PlayerState : GameState->GetGambitPlayerStates())
		{
			AddUniquePlayer(Players, PlayerState);
		}
	}

	TArray<AGambitPlayerState*> BuildStablePlayerOrder(const FGambitEffectExecutionContext& Context)
	{
		TArray<AGambitPlayerState*> Players;
		for (const TObjectPtr<AGambitPlayerState>& PlayerState : Context.MatchPlayerStates)
		{
			AddUniquePlayer(Players, PlayerState.Get());
		}

		AddPlayersFromWorld(Players, Context.SourcePlayer.Get());
		AddPlayersFromWorld(Players, Context.TargetPlayer.Get());
		AddUniquePlayer(Players, Context.SourcePlayer.Get());
		AddUniquePlayer(Players, Context.TargetPlayer.Get());
		return Players;
	}

	TArray<AGambitPlayerState*> BuildOpponents(const FGambitEffectExecutionContext& Context, FString& OutFailureReason)
	{
		TArray<AGambitPlayerState*> Opponents;
		AGambitPlayerState* SourcePlayer = Context.SourcePlayer.Get();
		if (!SourcePlayer)
		{
			OutFailureReason = TEXT("Opponent target rule requires SourcePlayer.");
			return Opponents;
		}

		for (AGambitPlayerState* PlayerState : BuildStablePlayerOrder(Context))
		{
			if (PlayerState && PlayerState != SourcePlayer)
			{
				Opponents.AddUnique(PlayerState);
			}
		}

		if (Opponents.Num() == 0)
		{
			OutFailureReason = TEXT("Opponent target rule found no player different from SourcePlayer.");
		}
		return Opponents;
	}

	int32 FindPlayerIndex(const TArray<AGambitPlayerState*>& Players, const AGambitPlayerState* Player)
	{
		for (int32 Index = 0; Index < Players.Num(); ++Index)
		{
			if (Players[Index] == Player)
			{
				return Index;
			}
		}

		return INDEX_NONE;
	}

	AGambitPlayerState* SelectLeadingPlayer(const TArray<AGambitPlayerState*>& Players)
	{
		AGambitPlayerState* BestPlayer = nullptr;
		for (AGambitPlayerState* Player : Players)
		{
			if (!Player)
			{
				continue;
			}

			if (!BestPlayer
				|| Player->GetTotalVictoryPoints() > BestPlayer->GetTotalVictoryPoints()
				|| (Player->GetTotalVictoryPoints() == BestPlayer->GetTotalVictoryPoints()
					&& Player->GetCurrentRoundScore() > BestPlayer->GetCurrentRoundScore()))
			{
				BestPlayer = Player;
			}
		}

		return BestPlayer;
	}

	AGambitPlayerState* SelectGoldPlayer(const TArray<AGambitPlayerState*>& Players, const bool bPoorest)
	{
		AGambitPlayerState* BestPlayer = nullptr;
		for (AGambitPlayerState* Player : Players)
		{
			if (!Player)
			{
				continue;
			}

			if (!BestPlayer
				|| (bPoorest && Player->GetCurrentGold() < BestPlayer->GetCurrentGold())
				|| (!bPoorest && Player->GetCurrentGold() > BestPlayer->GetCurrentGold()))
			{
				BestPlayer = Player;
			}
		}

		return BestPlayer;
	}

	AGambitPlayerState* SelectLowestScorePlayer(const TArray<AGambitPlayerState*>& Players)
	{
		AGambitPlayerState* BestPlayer = nullptr;
		for (AGambitPlayerState* Player : Players)
		{
			if (!Player)
			{
				continue;
			}

			if (!BestPlayer || Player->GetCurrentRoundScore() < BestPlayer->GetCurrentRoundScore())
			{
				BestPlayer = Player;
			}
		}

		return BestPlayer;
	}

	FGambitResolvedEffectTarget MakeResolvedTargetForPlayer(
		const FGambitEffectExecutionContext& Context,
		AGambitPlayerState* Player,
		const EGambitEffectTarget TargetSide,
		const FName TargetRuleId,
		const TArray<int32>& DiceHandIndexes)
	{
		const EGambitEffectTarget ResolvedSide = Player && Player == Context.SourcePlayer.Get()
			? EGambitEffectTarget::Source
			: TargetSide;

		FGambitResolvedEffectTarget ResolvedTarget;
		ResolvedTarget.TargetSide = ResolvedSide;
		ResolvedTarget.Player = Player ? Player : ResolveTargetResolverPlayer(Context, TargetSide);
		ResolvedTarget.DiceComponent = ResolveDiceComponentForPlayer(Context, Player, TargetSide);
		ResolvedTarget.EconomyComponent = ResolveEconomyComponentForPlayer(Context, Player, TargetSide);
		ResolvedTarget.InventoryComponent = ResolveInventoryComponentForPlayer(Context, Player, TargetSide);
		ResolvedTarget.ShopComponent = ResolveShopComponentForPlayer(Context, Player, TargetSide);
		ResolvedTarget.DiceHandIndexes = DiceHandIndexes;
		ResolvedTarget.TargetRuleId = TargetRuleId;
		return ResolvedTarget;
	}

	FGambitEffectTargetResolveResult ResolvePlayerRuleTargets(
		const UGambitItemEffectDefinition* EffectDefinition,
		const FGambitEffectExecutionContext& Context,
		const EGambitEffectTarget RequestedTarget)
	{
		const FName TargetRuleId = EffectDefinition ? EffectDefinition->TargetRuleId : NAME_None;
		const EGambitEffectTarget TargetSide = ResolveTargetSideForRule(TargetRuleId, RequestedTarget);
		TArray<AGambitPlayerState*> TargetPlayers;
		FString FailureReason;

		if (TargetRuleId == GambitEffectTargetRules::TargetOpponent)
		{
			AGambitPlayerState* SourcePlayer = Context.SourcePlayer.Get();
			AGambitPlayerState* TargetPlayer = Context.TargetPlayer.Get();
			if (!TargetPlayer || TargetPlayer == SourcePlayer)
			{
				return MakeFailure(TargetRuleId, TEXT("target.opponent requires a valid TargetPlayer different from SourcePlayer."));
			}

			TargetPlayers.Add(TargetPlayer);
		}
		else if (TargetRuleId == GambitEffectTargetRules::AllOpponents)
		{
			TargetPlayers = BuildOpponents(Context, FailureReason);
		}
		else if (TargetRuleId == GambitEffectTargetRules::RandomOpponent)
		{
			const TArray<AGambitPlayerState*> Opponents = BuildOpponents(Context, FailureReason);
			if (Opponents.Num() > 0)
			{
				FRandomStream RandomStream = Context.RandomStream;
				TargetPlayers.Add(Opponents[RandomStream.RandRange(0, Opponents.Num() - 1)]);
			}
		}
		else if (TargetRuleId == GambitEffectTargetRules::LeftOpponent
			|| TargetRuleId == GambitEffectTargetRules::RightOpponent
			|| TargetRuleId == GambitEffectTargetRules::OppositePlayer)
		{
			const TArray<AGambitPlayerState*> Players = BuildStablePlayerOrder(Context);
			AGambitPlayerState* SourcePlayer = Context.SourcePlayer.Get();
			const int32 SourceIndex = FindPlayerIndex(Players, SourcePlayer);
			if (!SourcePlayer || SourceIndex == INDEX_NONE)
			{
				FailureReason = FString::Printf(TEXT("TargetRuleId '%s' requires SourcePlayer in stable match player order."), *TargetRuleId.ToString());
			}
			else if (TargetRuleId == GambitEffectTargetRules::OppositePlayer)
			{
				if (Players.Num() != 4)
				{
					FailureReason = TEXT("opposite_player requires exactly four players in stable match player order.");
				}
				else
				{
					TargetPlayers.Add(Players[(SourceIndex + 2) % Players.Num()]);
				}
			}
			else if (Players.Num() < 2)
			{
				FailureReason = FString::Printf(TEXT("TargetRuleId '%s' requires at least two players."), *TargetRuleId.ToString());
			}
			else
			{
				const int32 Direction = TargetRuleId == GambitEffectTargetRules::LeftOpponent ? -1 : 1;
				const int32 TargetIndex = (SourceIndex + Direction + Players.Num()) % Players.Num();
				if (Players.IsValidIndex(TargetIndex) && Players[TargetIndex] != SourcePlayer)
				{
					TargetPlayers.Add(Players[TargetIndex]);
				}
			}
		}
		else
		{
			const TArray<AGambitPlayerState*> Players = BuildStablePlayerOrder(Context);
			if (Players.Num() == 0)
			{
				FailureReason = FString::Printf(TEXT("TargetRuleId '%s' requires at least one player."), *TargetRuleId.ToString());
			}
			else if (TargetRuleId == GambitEffectTargetRules::LeadingPlayer)
			{
				TargetPlayers.Add(SelectLeadingPlayer(Players));
			}
			else if (TargetRuleId == GambitEffectTargetRules::RichestPlayer)
			{
				TargetPlayers.Add(SelectGoldPlayer(Players, false));
			}
			else if (TargetRuleId == GambitEffectTargetRules::PoorestPlayer)
			{
				TargetPlayers.Add(SelectGoldPlayer(Players, true));
			}
			else if (TargetRuleId == GambitEffectTargetRules::LowestScorePlayer)
			{
				TargetPlayers.Add(SelectLowestScorePlayer(Players));
			}
		}

		TargetPlayers.Remove(nullptr);
		if (TargetPlayers.Num() == 0)
		{
			return MakeFailure(
				TargetRuleId,
				FailureReason.IsEmpty()
					? FString::Printf(TEXT("TargetRuleId '%s' resolved no valid player target."), *TargetRuleId.ToString())
					: FailureReason);
		}

		FGambitEffectTargetResolveResult Result;
		Result.bSuccess = true;
		Result.TargetRuleId = TargetRuleId;
		for (AGambitPlayerState* TargetPlayer : TargetPlayers)
		{
			const TArray<int32> EmptyDiceIndexes;
			Result.Targets.Add(MakeResolvedTargetForPlayer(Context, TargetPlayer, TargetSide, TargetRuleId, EmptyDiceIndexes));
		}
		return Result;
	}

	FGambitEffectTargetResolveResult ResolveSingleTarget(
		const UGambitItemEffectDefinition* EffectDefinition,
		const FGambitEffectExecutionContext& Context,
		const EGambitEffectTarget RequestedTarget)
	{
		if (!EffectDefinition)
		{
			return MakeFailure(NAME_None, TEXT("EffectDefinition is null."));
		}

		const FName TargetRuleId = EffectDefinition->TargetRuleId;
		if (!TargetRuleId.IsNone() && !GambitEffectTargetRules::IsKnownRule(TargetRuleId))
		{
			return MakeFailure(
				TargetRuleId,
				FString::Printf(TEXT("Unknown TargetRuleId '%s'."), *TargetRuleId.ToString()));
		}

		if (GambitEffectTargetRules::IsPlayerRule(TargetRuleId))
		{
			return ResolvePlayerRuleTargets(EffectDefinition, Context, RequestedTarget);
		}

		const EGambitEffectTarget TargetSide = ResolveTargetSideForRule(TargetRuleId, RequestedTarget);
		AGambitPlayerState* TargetPlayer = ResolveTargetResolverPlayer(Context, TargetSide);
		const TArray<FGambitDieRuntimeState> DiceStates = ResolveDiceSnapshotForPlayer(Context, TargetPlayer, TargetSide);

		TArray<int32> DiceHandIndexes;
		if (ShouldResolveDiceIndexes(EffectDefinition, DiceStates))
		{
			FString FailureReason;
			if (!BuildAffectedDieIndexes(EffectDefinition, Context, TargetSide, DiceStates, DiceHandIndexes, FailureReason))
			{
				return MakeFailure(TargetRuleId, FailureReason);
			}
		}

		FGambitEffectTargetResolveResult Result;
		Result.bSuccess = true;
		Result.TargetRuleId = TargetRuleId;
		Result.Targets.Add(MakeResolvedTargetForPlayer(Context, TargetPlayer, TargetSide, TargetRuleId, DiceHandIndexes));
		return Result;
	}

	void AppendResolvedTargets(
		FGambitEffectTargetResolveResult& AggregateResult,
		const FGambitEffectTargetResolveResult& SingleResult)
	{
		if (!SingleResult.bSuccess)
		{
			if (AggregateResult.FailureReason.IsEmpty())
			{
				AggregateResult.FailureReason = SingleResult.FailureReason;
			}
			return;
		}

		AggregateResult.Targets.Append(SingleResult.Targets);
		AggregateResult.bSuccess = AggregateResult.Targets.Num() > 0;
	}
}

namespace GambitEffectTargetResolver
{
	FGambitEffectTargetResolveResult ResolveEffectTargets(
		const UGambitItemEffectDefinition* EffectDefinition,
		const FGambitEffectExecutionContext& Context)
	{
		if (!EffectDefinition)
		{
			return MakeFailure(NAME_None, TEXT("EffectDefinition is null."));
		}

		if (!EffectDefinition->TargetRuleId.IsNone()
			&& GambitEffectTargetRules::IsPlayerRule(EffectDefinition->TargetRuleId))
		{
			return ResolveSingleTarget(EffectDefinition, Context, EffectDefinition->Target);
		}

		FGambitEffectTargetResolveResult Result;
		Result.TargetRuleId = EffectDefinition->TargetRuleId;

		if (EffectDefinition->Target == EGambitEffectTarget::SourceAndTarget)
		{
			AppendResolvedTargets(Result, ResolveSingleTarget(EffectDefinition, Context, EGambitEffectTarget::Source));
			if (Context.TargetPlayer && Context.TargetPlayer != Context.SourcePlayer)
			{
				AppendResolvedTargets(Result, ResolveSingleTarget(EffectDefinition, Context, EGambitEffectTarget::Target));
			}

			if (!Result.bSuccess && Result.FailureReason.IsEmpty())
			{
				Result.FailureReason = TEXT("SourceAndTarget resolved no valid target.");
			}

			return Result;
		}

		return ResolveSingleTarget(EffectDefinition, Context, EffectDefinition->Target);
	}

	FGambitEffectTargetResolveResult ResolveEffectTarget(
		const UGambitItemEffectDefinition* EffectDefinition,
		const FGambitEffectExecutionContext& Context,
		const EGambitEffectTarget RequestedTarget)
	{
		return ResolveSingleTarget(EffectDefinition, Context, RequestedTarget);
	}

	FGambitEffectTargetResolveResult ResolveContextTarget(
		const FGambitEffectExecutionContext& Context,
		const EGambitEffectTarget RequestedTarget)
	{
		const EGambitEffectTarget TargetSide = NormalizeTargetSide(RequestedTarget);
		AGambitPlayerState* TargetPlayer = ResolveTargetResolverPlayer(Context, TargetSide);

		FGambitEffectTargetResolveResult Result;
		Result.bSuccess = true;
		Result.TargetRuleId = NAME_None;
		const TArray<int32> EmptyDiceIndexes;
		Result.Targets.Add(MakeResolvedTargetForPlayer(Context, TargetPlayer, TargetSide, NAME_None, EmptyDiceIndexes));
		return Result;
	}

	int32 ResolveSelectedDieHandIndex(
		const FGambitEffectExecutionContext& Context,
		const EGambitEffectTarget RequestedTarget,
		const FName TargetRuleId)
	{
		if (TargetRuleId == GambitEffectTargetRules::SourceSelectedDie)
		{
			return Context.SourceDieHandIndex;
		}

		if (TargetRuleId == GambitEffectTargetRules::TargetSelectedDie)
		{
			return Context.TargetDieHandIndex;
		}

		return NormalizeTargetSide(RequestedTarget) == EGambitEffectTarget::Target
			? Context.TargetDieHandIndex
			: Context.SourceDieHandIndex;
	}
}
