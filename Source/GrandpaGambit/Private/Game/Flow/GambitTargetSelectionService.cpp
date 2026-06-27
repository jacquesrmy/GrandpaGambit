#include "Game/Flow/GambitTargetSelectionService.h"

#include "Data/Assets/GambitItemEffectDefinition.h"
#include "Items/Consumables/GambitConsumableDefinition.h"
#include "Items/Data/GambitItemDefinition.h"
#include "Items/Effects/GambitEffectTargetResolver.h"
#include "Items/Effects/GambitEffectTargetRules.h"
#include "Players/States/GambitPlayerState.h"

namespace
{
	struct FGambitTargetSelectionRequirement
	{
		UGambitItemEffectDefinition* EffectDefinition = nullptr;
		bool bRequiresPlayerChoice = false;
		bool bRequiresDieChoice = false;
	};

	FName ResolveItemId(const UGambitItemDefinition* ItemDefinition)
	{
		return ItemDefinition ? ItemDefinition->GetResolvedItemId() : NAME_None;
	}

	FName ResolveEffectId(const UGambitItemEffectDefinition* EffectDefinition)
	{
		if (!EffectDefinition)
		{
			return NAME_None;
		}

		return !EffectDefinition->EffectId.IsNone()
			? EffectDefinition->EffectId
			: FName(*EffectDefinition->GetName());
	}

	FString ResolveItemName(const UGambitItemDefinition* ItemDefinition)
	{
		if (!ItemDefinition)
		{
			return TEXT("None");
		}

		return ItemDefinition->DisplayName.IsEmpty()
			? ItemDefinition->GetResolvedItemId().ToString()
			: ItemDefinition->DisplayName.ToString();
	}

	TArray<AGambitPlayerState*> BuildStablePlayerList(
		AGambitPlayerState* RequestingPlayer,
		const TArray<AGambitPlayerState*>& MatchPlayers)
	{
		TArray<AGambitPlayerState*> Players;
		for (AGambitPlayerState* PlayerState : MatchPlayers)
		{
			if (PlayerState)
			{
				Players.AddUnique(PlayerState);
			}
		}

		if (RequestingPlayer)
		{
			Players.AddUnique(RequestingPlayer);
		}

		return Players;
	}

	int32 FindPlayerIndex(const AGambitPlayerState* PlayerState, const TArray<AGambitPlayerState*>& MatchPlayers)
	{
		for (int32 PlayerIndex = 0; PlayerIndex < MatchPlayers.Num(); ++PlayerIndex)
		{
			if (MatchPlayers[PlayerIndex] == PlayerState)
			{
				return PlayerIndex;
			}
		}

		return INDEX_NONE;
	}

	FString BuildPlayerLabel(const AGambitPlayerState* PlayerState, const TArray<AGambitPlayerState*>& MatchPlayers)
	{
		const int32 PlayerIndex = FindPlayerIndex(PlayerState, MatchPlayers);
		if (PlayerState && !PlayerState->GetPlayerName().IsEmpty())
		{
			return PlayerIndex != INDEX_NONE
				? FString::Printf(TEXT("P%d %s"), PlayerIndex, *PlayerState->GetPlayerName())
				: PlayerState->GetPlayerName();
		}

		return PlayerIndex != INDEX_NONE
			? FString::Printf(TEXT("P%d"), PlayerIndex)
			: TEXT("Player");
	}

	FString BuildDieLabel(const FGambitDieRuntimeState& DieState, const int32 FallbackIndex)
	{
		const int32 HandIndex = DieState.HandIndex != INDEX_NONE ? DieState.HandIndex : FallbackIndex;
		return FString::Printf(TEXT("Die %d value %d"), HandIndex, DieState.EffectiveValue);
	}

	bool IsTargetSelectedDieChoice(const UGambitItemEffectDefinition* EffectDefinition)
	{
		if (!EffectDefinition)
		{
			return false;
		}

		return EffectDefinition->TargetRuleId == GambitEffectTargetRules::TargetSelectedDie
			|| (EffectDefinition->TargetRuleId == GambitEffectTargetRules::SelectedDie
				&& EffectDefinition->Target == EGambitEffectTarget::Target);
	}

	bool RequiresPlayerChoice(
		const UGambitConsumableDefinition* ConsumableDefinition,
		const UGambitItemEffectDefinition* EffectDefinition)
	{
		if (!EffectDefinition)
		{
			return false;
		}

		const FName TargetRuleId = EffectDefinition->TargetRuleId;
		if (TargetRuleId == GambitEffectTargetRules::TargetOpponent
			|| GambitEffectTargetRules::RequiresExplicitTargetPlayer(TargetRuleId))
		{
			return true;
		}

		return ConsumableDefinition
			&& ConsumableDefinition->bCanTargetOpponent
			&& IsTargetSelectedDieChoice(EffectDefinition);
	}

	bool RequiresDieChoice(const UGambitItemEffectDefinition* EffectDefinition)
	{
		return EffectDefinition
			&& GambitEffectTargetRules::RequiresSelectedDie(EffectDefinition->TargetRuleId);
	}

	EGambitTargetSelectionType BuildSelectionType(const bool bRequiresPlayerChoice, const bool bRequiresDieChoice)
	{
		if (bRequiresPlayerChoice && bRequiresDieChoice)
		{
			return EGambitTargetSelectionType::PlayerAndDie;
		}

		if (bRequiresPlayerChoice)
		{
			return EGambitTargetSelectionType::Player;
		}

		if (bRequiresDieChoice)
		{
			return EGambitTargetSelectionType::Die;
		}

		return EGambitTargetSelectionType::None;
	}

	bool TryBuildSelectionRequirement(
		const UGambitConsumableDefinition* ConsumableDefinition,
		FGambitTargetSelectionRequirement& OutRequirement)
	{
		if (!ConsumableDefinition)
		{
			return false;
		}

		for (TObjectPtr<UGambitItemEffectDefinition> EffectDefinition : ConsumableDefinition->EffectDefinitions)
		{
			if (!EffectDefinition || EffectDefinition->Hook != EGambitEffectHook::ConsumableUse)
			{
				continue;
			}

			const bool bRequiresPlayerChoice = RequiresPlayerChoice(ConsumableDefinition, EffectDefinition.Get());
			const bool bRequiresDieChoice = RequiresDieChoice(EffectDefinition.Get());
			if (!bRequiresPlayerChoice && !bRequiresDieChoice)
			{
				continue;
			}

			OutRequirement.EffectDefinition = EffectDefinition.Get();
			OutRequirement.bRequiresPlayerChoice = bRequiresPlayerChoice;
			OutRequirement.bRequiresDieChoice = bRequiresDieChoice;
			return true;
		}

		return false;
	}

	bool IsCandidatePlayerAllowed(
		const UGambitConsumableDefinition* ConsumableDefinition,
		const UGambitItemEffectDefinition* EffectDefinition,
		const AGambitPlayerState* RequestingPlayer,
		const AGambitPlayerState* CandidatePlayer)
	{
		if (!CandidatePlayer)
		{
			return false;
		}

		if (EffectDefinition && EffectDefinition->TargetRuleId == GambitEffectTargetRules::TargetOpponent)
		{
			return CandidatePlayer != RequestingPlayer
				&& ConsumableDefinition
				&& ConsumableDefinition->bCanTargetOpponent;
		}

		if (CandidatePlayer == RequestingPlayer)
		{
			return true;
		}

		return ConsumableDefinition && ConsumableDefinition->bCanTargetOpponent;
	}

	void RefreshContextPlayerCaches(
		FGambitEffectExecutionContext& Context,
		AGambitPlayerState* RequestingPlayer,
		AGambitPlayerState* TargetPlayer)
	{
		Context.SourcePlayer = RequestingPlayer;
		Context.TargetPlayer = TargetPlayer ? TargetPlayer : RequestingPlayer;

		if (RequestingPlayer)
		{
			Context.SourceDiceComponent = RequestingPlayer->GetDiceComponent();
			Context.SourceEconomyComponent = RequestingPlayer->GetEconomyComponent();
			Context.SourceInventoryComponent = RequestingPlayer->GetInventoryComponent();
			Context.SourceShopComponent = RequestingPlayer->GetShopComponent();
			Context.SourceDice = RequestingPlayer->GetDiceStates();
		}

		if (AGambitPlayerState* EffectiveTarget = Context.TargetPlayer.Get())
		{
			Context.TargetDiceComponent = EffectiveTarget->GetDiceComponent();
			Context.TargetEconomyComponent = EffectiveTarget->GetEconomyComponent();
			Context.TargetInventoryComponent = EffectiveTarget->GetInventoryComponent();
			Context.TargetShopComponent = EffectiveTarget->GetShopComponent();
			Context.TargetDice = EffectiveTarget->GetDiceStates();
		}
	}

	void ApplyExplicitDieChoiceToContext(
		FGambitEffectExecutionContext& Context,
		const UGambitItemEffectDefinition* EffectDefinition,
		const int32 DieHandIndex)
	{
		if (DieHandIndex == INDEX_NONE)
		{
			return;
		}

		if (IsTargetSelectedDieChoice(EffectDefinition))
		{
			Context.TargetDieHandIndex = DieHandIndex;
			if (Context.TargetPlayer == Context.SourcePlayer)
			{
				Context.SourceDieHandIndex = DieHandIndex;
			}
			return;
		}

		Context.SourceDieHandIndex = DieHandIndex;
		if (Context.TargetPlayer == Context.SourcePlayer)
		{
			Context.TargetDieHandIndex = DieHandIndex;
		}
	}

	bool ValidateOptionWithTargetRules(
		const FGambitEffectExecutionContext& BaseContext,
		AGambitPlayerState* RequestingPlayer,
		const UGambitItemEffectDefinition* EffectDefinition,
		FGambitTargetSelectionOption& Option)
	{
		if (!EffectDefinition)
		{
			Option.DebugText = TEXT("Missing effect definition.");
			return false;
		}

		FGambitEffectExecutionContext TestContext = BaseContext;
		RefreshContextPlayerCaches(TestContext, RequestingPlayer, Option.TargetPlayer.Get());
		ApplyExplicitDieChoiceToContext(TestContext, EffectDefinition, Option.TargetDieHandIndex);

		const FGambitEffectTargetResolveResult ResolveResult = GambitEffectTargetResolver::ResolveEffectTargets(
			EffectDefinition,
			TestContext);
		if (!ResolveResult.bSuccess)
		{
			Option.DebugText = ResolveResult.FailureReason;
			return false;
		}

		Option.PreviewDieHandIndexes.Reset();
		for (const FGambitResolvedEffectTarget& ResolvedTarget : ResolveResult.Targets)
		{
			for (const int32 DieHandIndex : ResolvedTarget.DiceHandIndexes)
			{
				Option.PreviewDieHandIndexes.AddUnique(DieHandIndex);
			}
		}

		Option.DebugText = Option.PreviewDieHandIndexes.Num() > 0
			? FString::Printf(TEXT("Validated by %s; preview dice [%s]"),
				*Option.TargetRuleId.ToString(),
				*FString::JoinBy(Option.PreviewDieHandIndexes, TEXT(","), [](const int32 Value)
				{
					return FString::FromInt(Value);
				}))
			: FString::Printf(TEXT("Validated by %s"), *Option.TargetRuleId.ToString());
		return true;
	}

	void AddValidatedOption(
		FGambitTargetSelectionRequest& Request,
		const FGambitEffectExecutionContext& BaseContext,
		AGambitPlayerState* RequestingPlayer,
		const UGambitItemEffectDefinition* EffectDefinition,
		FGambitTargetSelectionOption&& Option)
	{
		Option.OptionId = Request.Options.Num();
		Option.TargetRuleId = EffectDefinition ? EffectDefinition->TargetRuleId : NAME_None;
		Option.bValid = ValidateOptionWithTargetRules(BaseContext, RequestingPlayer, EffectDefinition, Option);
		if (Option.bValid)
		{
			Request.Options.Add(MoveTemp(Option));
		}
	}

	TArray<AGambitPlayerState*> BuildCandidatePlayers(
		const UGambitConsumableDefinition* ConsumableDefinition,
		const UGambitItemEffectDefinition* EffectDefinition,
		AGambitPlayerState* RequestingPlayer,
		const TArray<AGambitPlayerState*>& MatchPlayers)
	{
		TArray<AGambitPlayerState*> Candidates;
		for (AGambitPlayerState* PlayerState : BuildStablePlayerList(RequestingPlayer, MatchPlayers))
		{
			if (IsCandidatePlayerAllowed(ConsumableDefinition, EffectDefinition, RequestingPlayer, PlayerState))
			{
				Candidates.AddUnique(PlayerState);
			}
		}

		return Candidates;
	}

	const TArray<FGambitDieRuntimeState>& ResolveExplicitDieOwnerDice(
		const FGambitTargetSelectionRequirement& Requirement,
		AGambitPlayerState* RequestingPlayer,
		AGambitPlayerState* CandidateTargetPlayer)
	{
		static const TArray<FGambitDieRuntimeState> EmptyDice;
		const bool bTargetDieChoice = IsTargetSelectedDieChoice(Requirement.EffectDefinition);
		const AGambitPlayerState* DieOwner = bTargetDieChoice
			? (CandidateTargetPlayer ? CandidateTargetPlayer : RequestingPlayer)
			: RequestingPlayer;
		return DieOwner ? DieOwner->GetDiceStatesRef() : EmptyDice;
	}

	void BuildPlayerOptions(
		FGambitTargetSelectionRequest& Request,
		const FGambitTargetSelectionRequirement& Requirement,
		const UGambitConsumableDefinition* ConsumableDefinition,
		AGambitPlayerState* RequestingPlayer,
		const TArray<AGambitPlayerState*>& MatchPlayers,
		const FGambitEffectExecutionContext& BaseContext)
	{
		for (AGambitPlayerState* CandidatePlayer : BuildCandidatePlayers(ConsumableDefinition, Requirement.EffectDefinition, RequestingPlayer, MatchPlayers))
		{
			FGambitTargetSelectionOption Option;
			Option.SelectionType = EGambitTargetSelectionType::Player;
			Option.TargetPlayer = CandidatePlayer;
			Option.TargetPlayerIndex = FindPlayerIndex(CandidatePlayer, MatchPlayers);
			Option.Label = BuildPlayerLabel(CandidatePlayer, MatchPlayers);
			AddValidatedOption(Request, BaseContext, RequestingPlayer, Requirement.EffectDefinition, MoveTemp(Option));
		}
	}

	void BuildDieOptions(
		FGambitTargetSelectionRequest& Request,
		const FGambitTargetSelectionRequirement& Requirement,
		AGambitPlayerState* RequestingPlayer,
		const TArray<AGambitPlayerState*>& MatchPlayers,
		const FGambitEffectExecutionContext& BaseContext)
	{
		const TArray<FGambitDieRuntimeState>& DiceStates = ResolveExplicitDieOwnerDice(Requirement, RequestingPlayer, RequestingPlayer);
		for (int32 DieIndex = 0; DieIndex < DiceStates.Num(); ++DieIndex)
		{
			const FGambitDieRuntimeState& DieState = DiceStates[DieIndex];
			FGambitTargetSelectionOption Option;
			Option.SelectionType = EGambitTargetSelectionType::Die;
			Option.TargetPlayer = RequestingPlayer;
			Option.TargetPlayerIndex = FindPlayerIndex(RequestingPlayer, MatchPlayers);
			Option.TargetDieHandIndex = DieState.HandIndex != INDEX_NONE ? DieState.HandIndex : DieIndex;
			Option.TargetDieInstanceId = DieState.InstanceId;
			Option.Label = BuildDieLabel(DieState, DieIndex);
			AddValidatedOption(Request, BaseContext, RequestingPlayer, Requirement.EffectDefinition, MoveTemp(Option));
		}
	}

	void BuildPlayerAndDieOptions(
		FGambitTargetSelectionRequest& Request,
		const FGambitTargetSelectionRequirement& Requirement,
		const UGambitConsumableDefinition* ConsumableDefinition,
		AGambitPlayerState* RequestingPlayer,
		const TArray<AGambitPlayerState*>& MatchPlayers,
		const FGambitEffectExecutionContext& BaseContext)
	{
		for (AGambitPlayerState* CandidatePlayer : BuildCandidatePlayers(ConsumableDefinition, Requirement.EffectDefinition, RequestingPlayer, MatchPlayers))
		{
			const TArray<FGambitDieRuntimeState>& DiceStates = ResolveExplicitDieOwnerDice(Requirement, RequestingPlayer, CandidatePlayer);
			for (int32 DieIndex = 0; DieIndex < DiceStates.Num(); ++DieIndex)
			{
				const FGambitDieRuntimeState& DieState = DiceStates[DieIndex];
				FGambitTargetSelectionOption Option;
				Option.SelectionType = EGambitTargetSelectionType::PlayerAndDie;
				Option.TargetPlayer = CandidatePlayer;
				Option.TargetPlayerIndex = FindPlayerIndex(CandidatePlayer, MatchPlayers);
				Option.TargetDieHandIndex = DieState.HandIndex != INDEX_NONE ? DieState.HandIndex : DieIndex;
				Option.TargetDieInstanceId = DieState.InstanceId;
				Option.Label = FString::Printf(
					TEXT("%s %s"),
					*BuildPlayerLabel(CandidatePlayer, MatchPlayers),
					*BuildDieLabel(DieState, DieIndex));
				AddValidatedOption(Request, BaseContext, RequestingPlayer, Requirement.EffectDefinition, MoveTemp(Option));
			}
		}
	}
}

bool UGambitTargetSelectionService::BuildConsumableTargetSelectionRequest(
	AGambitPlayerState* RequestingPlayer,
	const int32 SlotIndex,
	const TArray<AGambitPlayerState*>& MatchPlayers,
	const EGambitRoundPhase CurrentPhase,
	const FGambitEffectExecutionContext& BaseContext,
	FGambitTargetSelectionRequest& OutRequest) const
{
	OutRequest = FGambitTargetSelectionRequest();
	OutRequest.RequestId = FGuid::NewGuid();
	OutRequest.RequestingPlayer = RequestingPlayer;
	OutRequest.RequestingPlayerIndex = FindPlayerIndex(RequestingPlayer, MatchPlayers);
	OutRequest.SourceConsumableSlotIndex = SlotIndex;
	OutRequest.CurrentPhase = CurrentPhase;

	if (!RequestingPlayer)
	{
		OutRequest.InvalidReason = TEXT("Target selection failed: requesting player is missing.");
		return false;
	}

	const TArray<FGambitConsumableRuntimeSlot>& ConsumableSlots = RequestingPlayer->GetConsumableSlotsRef();
	if (!ConsumableSlots.IsValidIndex(SlotIndex) || !ConsumableSlots[SlotIndex].IsValid())
	{
		OutRequest.InvalidReason = FString::Printf(TEXT("Target selection failed: consumable slot %d is invalid."), SlotIndex);
		return false;
	}

	const FGambitConsumableRuntimeSlot& ConsumableSlot = ConsumableSlots[SlotIndex];
	UGambitConsumableDefinition* ConsumableDefinition = ConsumableSlot.Definition.Get();
	OutRequest.SourceInventoryInstanceId = ConsumableSlot.InventoryInstanceId;
	OutRequest.SourceItemDefinition = ConsumableDefinition;
	OutRequest.SourceItemId = ResolveItemId(ConsumableDefinition);

	FGambitTargetSelectionRequirement Requirement;
	if (!TryBuildSelectionRequirement(ConsumableDefinition, Requirement))
	{
		OutRequest.DebugText = FString::Printf(TEXT("%s does not require explicit target selection."), *ResolveItemName(ConsumableDefinition));
		return false;
	}

	OutRequest.SourceEffectDefinition = Requirement.EffectDefinition;
	OutRequest.EffectId = ResolveEffectId(Requirement.EffectDefinition);
	OutRequest.TargetRuleId = Requirement.EffectDefinition ? Requirement.EffectDefinition->TargetRuleId : NAME_None;
	OutRequest.SelectionType = BuildSelectionType(Requirement.bRequiresPlayerChoice, Requirement.bRequiresDieChoice);
	OutRequest.RequestedEffectTarget = Requirement.EffectDefinition ? Requirement.EffectDefinition->Target : EGambitEffectTarget::Source;
	OutRequest.bRequiresExplicitSelection = true;

	if (!ConsumableDefinition->CanBeUsedDuringPhase(CurrentPhase))
	{
		OutRequest.InvalidReason = FString::Printf(
			TEXT("%s cannot be used during this phase."),
			*ResolveItemName(ConsumableDefinition));
		OutRequest.DebugText = OutRequest.InvalidReason;
		return true;
	}

	switch (OutRequest.SelectionType)
	{
	case EGambitTargetSelectionType::Player:
		BuildPlayerOptions(OutRequest, Requirement, ConsumableDefinition, RequestingPlayer, MatchPlayers, BaseContext);
		break;
	case EGambitTargetSelectionType::Die:
		BuildDieOptions(OutRequest, Requirement, RequestingPlayer, MatchPlayers, BaseContext);
		break;
	case EGambitTargetSelectionType::PlayerAndDie:
		BuildPlayerAndDieOptions(OutRequest, Requirement, ConsumableDefinition, RequestingPlayer, MatchPlayers, BaseContext);
		break;
	default:
		break;
	}

	OutRequest.bHasValidOptions = OutRequest.Options.Num() > 0;
	if (!OutRequest.bHasValidOptions)
	{
		OutRequest.InvalidReason = FString::Printf(
			TEXT("%s requires a target for rule '%s', but no valid option is available."),
			*ResolveItemName(ConsumableDefinition),
			*OutRequest.TargetRuleId.ToString());
	}

	OutRequest.DebugText = OutRequest.bHasValidOptions
		? FString::Printf(
			TEXT("%s requires %d target option(s) for rule '%s'."),
			*ResolveItemName(ConsumableDefinition),
			OutRequest.Options.Num(),
			*OutRequest.TargetRuleId.ToString())
		: OutRequest.InvalidReason;
	return true;
}

bool UGambitTargetSelectionService::ConsumableRequiresExplicitTargetSelection(const UGambitConsumableDefinition* ConsumableDefinition)
{
	FGambitTargetSelectionRequirement Requirement;
	return TryBuildSelectionRequirement(ConsumableDefinition, Requirement);
}
