#include "Debug/GambitDebugAutoPlayer.h"

#include "Core/Logging/GambitLog.h"
#include "Dice/Evaluation/GambitDiceCombinationEvaluator.h"
#include "Game/Flow/GambitRoundFlowComponent.h"
#include "Game/Modes/GambitGameMode.h"
#include "Game/States/GambitGameState.h"
#include "Items/Consumables/GambitConsumableDefinition.h"
#include "Items/Data/GambitItemDefinition.h"
#include "Items/DiceItems/GambitDiceItemDefinition.h"
#include "Items/Effects/GambitEffectResolver.h"
#include "Items/Modules/GambitModuleDefinition.h"
#include "Managers/SharedPool/GambitSharedPoolComponent.h"
#include "Players/States/GambitPlayerState.h"
#include "Scoring/Calculators/GambitScoreCalculator.h"

namespace
{
	constexpr int32 ConsumableScoreThreshold = 40;

	FString PhaseToString(const EGambitRoundPhase Phase)
	{
		switch (Phase)
		{
		case EGambitRoundPhase::None: return TEXT("None");
		case EGambitRoundPhase::Roll: return TEXT("Roll");
		case EGambitRoundPhase::SelectionReroll: return TEXT("SelectionReroll");
		case EGambitRoundPhase::Action: return TEXT("Action");
		case EGambitRoundPhase::Resolution: return TEXT("Resolution");
		case EGambitRoundPhase::Reward: return TEXT("Reward");
		case EGambitRoundPhase::Ranking: return TEXT("Ranking");
		case EGambitRoundPhase::Shop: return TEXT("Shop");
		case EGambitRoundPhase::RoundEnd: return TEXT("RoundEnd");
		default: return TEXT("Unknown");
		}
	}

	FString CombinationToString(const EGambitCombinationType Combination)
	{
		switch (Combination)
		{
		case EGambitCombinationType::None: return TEXT("None");
		case EGambitCombinationType::HighDice: return TEXT("HighDice");
		case EGambitCombinationType::Pair: return TEXT("Pair");
		case EGambitCombinationType::TwoPair: return TEXT("TwoPair");
		case EGambitCombinationType::ThreeOfAKind: return TEXT("ThreeOfAKind");
		case EGambitCombinationType::StraightSmall: return TEXT("StraightSmall");
		case EGambitCombinationType::FullHouse: return TEXT("FullHouse");
		case EGambitCombinationType::FourOfAKind: return TEXT("FourOfAKind");
		case EGambitCombinationType::StraightLarge: return TEXT("StraightLarge");
		case EGambitCombinationType::FiveOfAKind: return TEXT("FiveOfAKind");
		default: return TEXT("Unknown");
		}
	}

	FString ItemTypeToString(const EGambitItemType ItemType)
	{
		switch (ItemType)
		{
		case EGambitItemType::None: return TEXT("None");
		case EGambitItemType::Dice: return TEXT("Dice");
		case EGambitItemType::Module: return TEXT("Module");
		case EGambitItemType::Consumable: return TEXT("Consumable");
		default: return TEXT("Unknown");
		}
	}

	FString FormatDiceValues(const TArray<FGambitDieRuntimeState>& DiceStates)
	{
		TArray<FString> Values;
		Values.Reserve(DiceStates.Num());

		for (const FGambitDieRuntimeState& DieState : DiceStates)
		{
			Values.Add(DieState.bLocked
				? FString::Printf(TEXT("%dL"), DieState.EffectiveValue)
				: FString::FromInt(DieState.EffectiveValue));
		}

		return FString::Printf(TEXT("[%s]"), *FString::Join(Values, TEXT(",")));
	}

	FString FormatIndexList(const TArray<int32>& Indices)
	{
		TArray<FString> Values;
		Values.Reserve(Indices.Num());

		for (const int32 Index : Indices)
		{
			Values.Add(FString::FromInt(Index));
		}

		return FString::Printf(TEXT("[%s]"), *FString::Join(Values, TEXT(",")));
	}

	FString FormatItemName(const UGambitItemDefinition* ItemDefinition)
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

	FString FormatModifier(const FGambitScoreModifierContext& Modifier)
	{
		return FString::Printf(
			TEXT("Add=%.2f Mult=%.2f Cap=%.2f DiminishThreshold=%.2f DiminishFactor=%.2f"),
			Modifier.AdditiveBonus,
			Modifier.Multiplier,
			Modifier.ScoreCap,
			Modifier.DiminishingThreshold,
			Modifier.DiminishingFactor);
	}

	FGambitScoreModifierContext ResolveItemScoreModifier(
		const UGambitItemDefinition* ItemDefinition,
		const EGambitEffectHook Hook)
	{
		FGambitScoreModifierContext Modifier;
		if (!ItemDefinition)
		{
			return Modifier;
		}

		UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
		FGambitEffectExecutionContext Context;
		Context.Hook = Hook;
		Resolver->ExecuteItemEffects(const_cast<UGambitItemDefinition*>(ItemDefinition), Context);
		return Hook == EGambitEffectHook::ConsumableUse
			? Context.TemporaryScoreModifierDelta
			: Context.ScoreModifierDelta;
	}

	AGambitGameState* ResolveGameState(AGambitGameMode* GameMode)
	{
		return GameMode ? GameMode->GetGameState<AGambitGameState>() : nullptr;
	}

	struct FGambitDebugLockTarget
	{
		int32 Value = INDEX_NONE;
		int32 Count = 0;
		FString Reason = TEXT("NoDice");

		bool IsValid() const
		{
			return Value != INDEX_NONE;
		}
	};

	FString BuildDuplicateReason(const int32 Count)
	{
		if (Count >= 5)
		{
			return TEXT("FiveOfAKindFound");
		}
		if (Count >= 4)
		{
			return TEXT("FourOfAKindFound");
		}
		if (Count >= 3)
		{
			return TEXT("TripleFound");
		}

		return TEXT("PairFound");
	}

	FGambitDebugLockTarget ChooseLockTarget(const TArray<FGambitDieRuntimeState>& DiceStates)
	{
		FGambitDebugLockTarget Target;
		if (DiceStates.Num() == 0)
		{
			return Target;
		}

		TMap<int32, int32> CountsByValue;
		for (const FGambitDieRuntimeState& DieState : DiceStates)
		{
			if (DieState.bCountsForCombinations)
			{
				CountsByValue.FindOrAdd(DieState.EffectiveValue) += FMath::Max(1, DieState.ComboContributionCount);
			}
		}

		for (const TPair<int32, int32>& Entry : CountsByValue)
		{
			if (Entry.Value < 2)
			{
				continue;
			}

			const bool bBetterCount = Entry.Value > Target.Count;
			const bool bSameCountHigherValue = Entry.Value == Target.Count && Entry.Key > Target.Value;
			if (bBetterCount || bSameCountHigherValue)
			{
				Target.Value = Entry.Key;
				Target.Count = Entry.Value;
				Target.Reason = BuildDuplicateReason(Entry.Value);
			}
		}

		if (Target.IsValid())
		{
			return Target;
		}

		for (const FGambitDieRuntimeState& DieState : DiceStates)
		{
			if (DieState.EffectiveValue > Target.Value)
			{
				Target.Value = DieState.EffectiveValue;
				Target.Count = 1;
				Target.Reason = TEXT("HighestUnique");
			}
		}

		return Target;
	}

	int32 CountUnlockedDice(const TArray<FGambitDieRuntimeState>& DiceStates)
	{
		int32 Count = 0;
		for (const FGambitDieRuntimeState& DieState : DiceStates)
		{
			if (!DieState.bLocked && DieState.bCanBeRerolled)
			{
				Count++;
			}
		}

		return Count;
	}

	TArray<int32> ApplyLockTarget(AGambitGameMode* GameMode, AGambitPlayerState* PlayerState, const FGambitDebugLockTarget& Target)
	{
		TArray<int32> LockedIndices;
		if (!GameMode || !PlayerState || !Target.IsValid())
		{
			return LockedIndices;
		}

		const TArray<FGambitDieRuntimeState> DiceSnapshot = PlayerState->GetDiceStates();
		for (int32 Index = 0; Index < DiceSnapshot.Num(); ++Index)
		{
			const bool bShouldLock = DiceSnapshot[Index].EffectiveValue == Target.Value && DiceSnapshot[Index].bCanBeLocked;
			if (DiceSnapshot[Index].bLocked != bShouldLock)
			{
				GameMode->RequestSetDieLocked(PlayerState, Index, bShouldLock);
			}

			if (bShouldLock)
			{
				LockedIndices.Add(Index);
			}
		}

		return LockedIndices;
	}

	const UGambitConsumableDefinition* GetFirstUsableConsumable(
		const AGambitPlayerState* PlayerState,
		int32& OutSlotIndex)
	{
		OutSlotIndex = INDEX_NONE;
		if (!PlayerState)
		{
			return nullptr;
		}

		const TArray<FGambitConsumableRuntimeSlot>& ConsumableSlots = PlayerState->GetConsumableSlotsRef();
		for (int32 SlotIndex = 0; SlotIndex < ConsumableSlots.Num(); ++SlotIndex)
		{
			if (ConsumableSlots[SlotIndex].IsValid())
			{
				OutSlotIndex = SlotIndex;
				return ConsumableSlots[SlotIndex].Definition;
			}
		}

		return nullptr;
	}

	bool IsSharedPoolAvailable(const FGambitShopOffer& Offer, const UGambitSharedPoolComponent* SharedPoolComponent)
	{
		const UGambitItemDefinition* ItemDefinition = Offer.ItemDefinition;
		if (!ItemDefinition)
		{
			return false;
		}

		if (!ItemDefinition->bUsesSharedPool)
		{
			return true;
		}

		return SharedPoolComponent && SharedPoolComponent->IsItemAvailable(ItemDefinition);
	}

	bool IsOfferSlotAllowed(const FGambitShopOffer& Offer, const FGambitPlayerSlotState& SlotState)
	{
		if (Cast<UGambitModuleDefinition>(Offer.ItemDefinition.Get()))
		{
			return SlotState.ModuleSlotsUsed < SlotState.ModuleSlotsCapacity;
		}

		if (Cast<UGambitConsumableDefinition>(Offer.ItemDefinition.Get()))
		{
			return SlotState.ConsumableSlotsUsed < SlotState.ConsumableSlotsCapacity;
		}

		if (const UGambitDiceItemDefinition* DiceItemDefinition = Cast<UGambitDiceItemDefinition>(Offer.ItemDefinition.Get()))
		{
			return DiceItemDefinition->GrantedDiceDefinition != nullptr;
		}

		return false;
	}

	int32 GetShopPriority(const FGambitShopOffer& Offer)
	{
		if (Cast<UGambitModuleDefinition>(Offer.ItemDefinition.Get()))
		{
			return 0;
		}
		if (Cast<UGambitDiceItemDefinition>(Offer.ItemDefinition.Get()))
		{
			return 1;
		}
		if (Cast<UGambitConsumableDefinition>(Offer.ItemDefinition.Get()))
		{
			return 2;
		}

		return MAX_int32;
	}

	FGambitScoreModifierContext GetOfferModifier(const FGambitShopOffer& Offer)
	{
		if (UGambitModuleDefinition* ModuleDefinition = Cast<UGambitModuleDefinition>(Offer.ItemDefinition.Get()))
		{
			return ResolveItemScoreModifier(ModuleDefinition, EGambitEffectHook::ScoreModifier);
		}

		if (UGambitConsumableDefinition* ConsumableDefinition = Cast<UGambitConsumableDefinition>(Offer.ItemDefinition.Get()))
		{
			return ResolveItemScoreModifier(ConsumableDefinition, EGambitEffectHook::ConsumableUse);
		}

		return FGambitScoreModifierContext();
	}

	struct FGambitDebugShopCandidate
	{
		FGambitShopOffer Offer;
		int32 Priority = MAX_int32;
		float Multiplier = 1.0f;
		float AdditiveBonus = 0.0f;
		FString Reason = TEXT("None");

		bool IsValid() const
		{
			return Offer.ItemDefinition != nullptr;
		}
	};

	FString BuildShopReason(const FGambitShopOffer& Offer, const FGambitScoreModifierContext& Modifier)
	{
		if (Cast<UGambitModuleDefinition>(Offer.ItemDefinition.Get()))
		{
			if (Modifier.Multiplier > 1.0f)
			{
				return TEXT("BestModuleMultiplier");
			}
			if (Modifier.AdditiveBonus > 0.0f)
			{
				return TEXT("BestModuleAdditive");
			}

			return TEXT("BestModuleCost");
		}

		if (Cast<UGambitConsumableDefinition>(Offer.ItemDefinition.Get()))
		{
			if (Modifier.Multiplier > 1.0f)
			{
				return TEXT("BestConsumableMultiplier");
			}
			if (Modifier.AdditiveBonus > 0.0f)
			{
				return TEXT("BestConsumableAdditive");
			}

			return TEXT("BestConsumableCost");
		}

		if (Cast<UGambitDiceItemDefinition>(Offer.ItemDefinition.Get()))
		{
			return TEXT("AffordableDice");
		}

		return TEXT("UnsupportedItem");
	}

	bool IsBetterShopCandidate(const FGambitDebugShopCandidate& Candidate, const FGambitDebugShopCandidate& CurrentBest)
	{
		if (!CurrentBest.IsValid())
		{
			return true;
		}

		if (Candidate.Priority != CurrentBest.Priority)
		{
			return Candidate.Priority < CurrentBest.Priority;
		}

		const bool bCandidateHasMultiplier = Candidate.Multiplier > 1.0f;
		const bool bBestHasMultiplier = CurrentBest.Multiplier > 1.0f;
		if (bCandidateHasMultiplier != bBestHasMultiplier)
		{
			return bCandidateHasMultiplier;
		}

		if (bCandidateHasMultiplier && !FMath::IsNearlyEqual(Candidate.Multiplier, CurrentBest.Multiplier))
		{
			return Candidate.Multiplier > CurrentBest.Multiplier;
		}

		const bool bCandidateHasAdditive = Candidate.AdditiveBonus > 0.0f;
		const bool bBestHasAdditive = CurrentBest.AdditiveBonus > 0.0f;
		if (bCandidateHasAdditive != bBestHasAdditive)
		{
			return bCandidateHasAdditive;
		}

		if (bCandidateHasAdditive && !FMath::IsNearlyEqual(Candidate.AdditiveBonus, CurrentBest.AdditiveBonus))
		{
			return Candidate.AdditiveBonus > CurrentBest.AdditiveBonus;
		}

		if (Candidate.Offer.Price != CurrentBest.Offer.Price)
		{
			return Candidate.Offer.Price < CurrentBest.Offer.Price;
		}

		return Candidate.Offer.OfferId < CurrentBest.Offer.OfferId;
	}

	FGambitDebugShopCandidate BuildShopCandidate(const FGambitShopOffer& Offer)
	{
		FGambitDebugShopCandidate Candidate;
		Candidate.Offer = Offer;
		Candidate.Priority = GetShopPriority(Offer);

		const FGambitScoreModifierContext Modifier = GetOfferModifier(Offer);
		Candidate.Multiplier = Modifier.Multiplier;
		Candidate.AdditiveBonus = Modifier.AdditiveBonus;
		Candidate.Reason = BuildShopReason(Offer, Modifier);
		return Candidate;
	}
}

void UGambitDebugAutoPlayer::DecideRerolls(AGambitGameMode* GameMode)
{
	EnsureScoringServices();

	AGambitGameState* GameState = ResolveGameState(GameMode);
	const EGambitRoundPhase CurrentPhase = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	if (!GameMode || !GameState || CurrentPhase != EGambitRoundPhase::SelectionReroll)
	{
		UE_LOG(LogGambit, Log, TEXT("DebugAI: AIDecideRerolls skipped Phase=%s"), *PhaseToString(CurrentPhase));
		return;
	}

	const TArray<AGambitPlayerState*> Players = GameState->GetGambitPlayerStates();
	for (int32 PlayerIndex = 0; PlayerIndex < Players.Num(); ++PlayerIndex)
	{
		DecideRerollsForPlayer(GameMode, Players[PlayerIndex], PlayerIndex);
	}
}

void UGambitDebugAutoPlayer::DecideRerollsForPlayer(AGambitGameMode* GameMode, AGambitPlayerState* PlayerState, const int32 PlayerIndex)
{
	EnsureScoringServices();

	AGambitGameState* GameState = ResolveGameState(GameMode);
	const EGambitRoundPhase CurrentPhase = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	if (!GameMode || !GameState || !PlayerState || CurrentPhase != EGambitRoundPhase::SelectionReroll)
	{
		UE_LOG(LogGambit, Log, TEXT("DebugAI: AIDecideRerollsForPlayer skipped PlayerIndex=%d Phase=%s"), PlayerIndex, *PhaseToString(CurrentPhase));
		return;
	}

	UGambitRoundFlowComponent* RoundFlow = GameMode->GetRoundFlowComponent();
	if (!RoundFlow || !DiceEvaluator)
	{
		UE_LOG(LogGambit, Log, TEXT("DebugAI: AIDecideRerollsForPlayer failed, missing RoundFlow or DiceEvaluator"));
		return;
	}

	const FGambitDiceCombinationResult CombinationBefore = DiceEvaluator->EvaluateDice(PlayerState->GetDiceStatesRef());
	UE_LOG(
		LogGambit,
		Log,
		TEXT("DebugAI: P%d Selection DiceBefore=%s CombinationBefore=%s RerollsUsed=%d/%d"),
		PlayerIndex,
		*FormatDiceValues(PlayerState->GetDiceStatesRef()),
		*CombinationToString(CombinationBefore.Combination),
		RoundFlow->GetRerollsUsedForPlayer(PlayerState),
		RoundFlow->GetMaxRerollsForPlayer(PlayerState));

	FGambitDebugLockTarget LockTarget = ChooseLockTarget(PlayerState->GetDiceStatesRef());
	TArray<int32> LockedIndices = ApplyLockTarget(GameMode, PlayerState, LockTarget);
	UE_LOG(
		LogGambit,
		Log,
		TEXT("DebugAI: P%d Selection LockValue=%d LockedDice=%s Reason=%s Dice=%s"),
		PlayerIndex,
		LockTarget.Value,
		*FormatIndexList(LockedIndices),
		*LockTarget.Reason,
		*FormatDiceValues(PlayerState->GetDiceStatesRef()));

	int32 RerollCountThisDecision = 0;
	while (RoundFlow->GetRerollsUsedForPlayer(PlayerState) < RoundFlow->GetMaxRerollsForPlayer(PlayerState))
	{
		const int32 UnlockedDiceCount = CountUnlockedDice(PlayerState->GetDiceStatesRef());
		if (UnlockedDiceCount <= 0)
		{
			UE_LOG(LogGambit, Log, TEXT("DebugAI: P%d Reroll Skipped Reason=NoUnlockedDice"), PlayerIndex);
			break;
		}

		const int32 UsedBefore = RoundFlow->GetRerollsUsedForPlayer(PlayerState);
		const bool bRerolled = GameMode->RequestReroll(PlayerState);
		const int32 UsedAfter = RoundFlow->GetRerollsUsedForPlayer(PlayerState);
		RerollCountThisDecision++;

		const FGambitDiceCombinationResult CombinationAfterReroll = DiceEvaluator->EvaluateDice(PlayerState->GetDiceStatesRef());
		UE_LOG(
			LogGambit,
			Log,
			TEXT("DebugAI: P%d Reroll Used=%d/%d Result=%s DiceAfter=%s CombinationAfter=%s"),
			PlayerIndex,
			UsedAfter,
			RoundFlow->GetMaxRerollsForPlayer(PlayerState),
			bRerolled ? TEXT("Success") : TEXT("Failure"),
			*FormatDiceValues(PlayerState->GetDiceStatesRef()),
			*CombinationToString(CombinationAfterReroll.Combination));

		if (!bRerolled || UsedAfter <= UsedBefore)
		{
			break;
		}

		LockTarget = ChooseLockTarget(PlayerState->GetDiceStatesRef());
		LockedIndices = ApplyLockTarget(GameMode, PlayerState, LockTarget);
		UE_LOG(
			LogGambit,
			Log,
			TEXT("DebugAI: P%d Selection RelockValue=%d LockedDice=%s Reason=%s Dice=%s"),
			PlayerIndex,
			LockTarget.Value,
			*FormatIndexList(LockedIndices),
			*LockTarget.Reason,
			*FormatDiceValues(PlayerState->GetDiceStatesRef()));
	}

	if (RerollCountThisDecision == 0)
	{
		UE_LOG(
			LogGambit,
			Log,
			TEXT("DebugAI: P%d Reroll Skipped RerollsUsed=%d/%d DiceAfter=%s"),
			PlayerIndex,
			RoundFlow->GetRerollsUsedForPlayer(PlayerState),
			RoundFlow->GetMaxRerollsForPlayer(PlayerState),
			*FormatDiceValues(PlayerState->GetDiceStatesRef()));
	}
}

void UGambitDebugAutoPlayer::DecideActions(AGambitGameMode* GameMode)
{
	EnsureScoringServices();

	AGambitGameState* GameState = ResolveGameState(GameMode);
	const EGambitRoundPhase CurrentPhase = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	if (!GameMode || !GameState || CurrentPhase != EGambitRoundPhase::Action)
	{
		UE_LOG(LogGambit, Log, TEXT("DebugAI: AIDecideActions skipped Phase=%s"), *PhaseToString(CurrentPhase));
		return;
	}

	const TArray<AGambitPlayerState*> Players = GameState->GetGambitPlayerStates();
	for (int32 PlayerIndex = 0; PlayerIndex < Players.Num(); ++PlayerIndex)
	{
		DecideActionsForPlayer(GameMode, Players[PlayerIndex], PlayerIndex);
	}
}

void UGambitDebugAutoPlayer::DecideActionsForPlayer(AGambitGameMode* GameMode, AGambitPlayerState* PlayerState, const int32 PlayerIndex)
{
	EnsureScoringServices();

	AGambitGameState* GameState = ResolveGameState(GameMode);
	const EGambitRoundPhase CurrentPhase = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	if (!GameMode || !GameState || !PlayerState || CurrentPhase != EGambitRoundPhase::Action)
	{
		UE_LOG(LogGambit, Log, TEXT("DebugAI: AIDecideActionsForPlayer skipped PlayerIndex=%d Phase=%s"), PlayerIndex, *PhaseToString(CurrentPhase));
		return;
	}

	if (!DiceEvaluator || !ScoreCalculator)
	{
		UE_LOG(LogGambit, Log, TEXT("DebugAI: AIDecideActionsForPlayer failed, missing scoring services"));
		return;
	}

	const FGambitDiceCombinationResult CombinationBefore = DiceEvaluator->EvaluateDice(PlayerState->GetDiceStatesRef());
	const FGambitScoreBreakdown ScoreBefore = ScoreCalculator->CalculateScore(
		CombinationBefore,
		PlayerState->BuildCombinedScoreModifier());
	const bool bHasNoActiveModules = PlayerState->GetActiveModulesRef().Num() == 0;

	int32 SlotIndex = INDEX_NONE;
	const UGambitConsumableDefinition* ConsumableDefinition = GetFirstUsableConsumable(PlayerState, SlotIndex);
	const bool bShouldUseConsumable = ConsumableDefinition
		&& (ScoreBefore.FinalScore < ConsumableScoreThreshold || bHasNoActiveModules);

	UE_LOG(
		LogGambit,
		Log,
		TEXT("DebugAI: P%d Action EstimatedScore=%d Combination=%s Threshold=%d FirstConsumableSlot=%d ActiveModules=%d"),
		PlayerIndex,
		ScoreBefore.FinalScore,
		*CombinationToString(ScoreBefore.Combination),
		ConsumableScoreThreshold,
		SlotIndex,
		PlayerState->GetActiveModulesRef().Num());

	if (!bShouldUseConsumable)
	{
		UE_LOG(
			LogGambit,
			Log,
			TEXT("DebugAI: P%d Action UseConsumable=None Reason=%s"),
			PlayerIndex,
			ConsumableDefinition ? TEXT("ScoreAboveThreshold") : TEXT("NoUsableConsumable"));
		return;
	}

	const bool bUsed = GameMode->RequestUseConsumable(PlayerState, SlotIndex);
	const FGambitDiceCombinationResult CombinationAfter = DiceEvaluator->EvaluateDice(PlayerState->GetDiceStatesRef());
	const FGambitScoreBreakdown ScoreAfter = ScoreCalculator->CalculateScore(
		CombinationAfter,
		PlayerState->BuildCombinedScoreModifier());

	UE_LOG(
		LogGambit,
		Log,
		TEXT("DebugAI: P%d Action EstimatedScore=%d UseConsumable Slot=%d Item=%s ResolvedModifier=%s Result=%s ScoreAfter=%d"),
		PlayerIndex,
		ScoreBefore.FinalScore,
		SlotIndex,
		*FormatItemName(ConsumableDefinition),
		ConsumableDefinition ? *FormatModifier(ResolveItemScoreModifier(ConsumableDefinition, EGambitEffectHook::ConsumableUse)) : TEXT("None"),
		bUsed ? TEXT("Success") : TEXT("Failure"),
		ScoreAfter.FinalScore);
}

void UGambitDebugAutoPlayer::DecideShop(AGambitGameMode* GameMode)
{
	AGambitGameState* GameState = ResolveGameState(GameMode);
	const EGambitRoundPhase CurrentPhase = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	if (!GameMode || !GameState || CurrentPhase != EGambitRoundPhase::Shop)
	{
		UE_LOG(LogGambit, Log, TEXT("DebugAI: AIDecideShop skipped Phase=%s"), *PhaseToString(CurrentPhase));
		return;
	}

	const TArray<AGambitPlayerState*> Players = GameState->GetGambitPlayerStates();
	for (int32 PlayerIndex = 0; PlayerIndex < Players.Num(); ++PlayerIndex)
	{
		DecideShopForPlayer(GameMode, Players[PlayerIndex], PlayerIndex);
	}
}

void UGambitDebugAutoPlayer::DecideShopForPlayer(AGambitGameMode* GameMode, AGambitPlayerState* PlayerState, const int32 PlayerIndex)
{
	AGambitGameState* GameState = ResolveGameState(GameMode);
	const EGambitRoundPhase CurrentPhase = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	if (!GameMode || !GameState || !PlayerState || CurrentPhase != EGambitRoundPhase::Shop)
	{
		UE_LOG(LogGambit, Log, TEXT("DebugAI: AIDecideShopForPlayer skipped PlayerIndex=%d Phase=%s"), PlayerIndex, *PhaseToString(CurrentPhase));
		return;
	}

	UGambitSharedPoolComponent* SharedPoolComponent = GameState->GetSharedPoolComponent();
	const int32 Gold = PlayerState->GetCurrentGold();
	const FGambitPlayerSlotState SlotState = PlayerState->GetSlotState();
	const TArray<FGambitShopOffer>& Offers = PlayerState->GetCurrentShopOffersRef();
	UE_LOG(
		LogGambit,
		Log,
		TEXT("DebugAI: P%d Shop Gold=%d Offers=%d Modules=%d/%d Consumables=%d/%d"),
		PlayerIndex,
		Gold,
		Offers.Num(),
		SlotState.ModuleSlotsUsed,
		SlotState.ModuleSlotsCapacity,
		SlotState.ConsumableSlotsUsed,
		SlotState.ConsumableSlotsCapacity);

	FGambitDebugShopCandidate BestCandidate;
	for (const FGambitShopOffer& Offer : Offers)
	{
		const UGambitItemDefinition* ItemDefinition = Offer.ItemDefinition;
		const bool bAffordable = Offer.Price <= Gold;
		const bool bSharedAvailable = IsSharedPoolAvailable(Offer, SharedPoolComponent);
		const bool bSlotAllowed = IsOfferSlotAllowed(Offer, SlotState);
		UE_LOG(
			LogGambit,
			Log,
			TEXT("DebugAI: P%d Shop OfferId=%d Item=%s Type=%s Price=%d Affordable=%s SharedAvailable=%s SlotAllowed=%s"),
			PlayerIndex,
			Offer.OfferId,
			*FormatItemName(ItemDefinition),
			ItemDefinition ? *ItemTypeToString(ItemDefinition->ItemType) : TEXT("None"),
			Offer.Price,
			bAffordable ? TEXT("Yes") : TEXT("No"),
			bSharedAvailable ? TEXT("Yes") : TEXT("No"),
			bSlotAllowed ? TEXT("Yes") : TEXT("No"));

		if (!ItemDefinition || !bAffordable || !bSharedAvailable || !bSlotAllowed)
		{
			continue;
		}

		const FGambitDebugShopCandidate Candidate = BuildShopCandidate(Offer);
		if (Candidate.Priority == MAX_int32)
		{
			continue;
		}

		if (IsBetterShopCandidate(Candidate, BestCandidate))
		{
			BestCandidate = Candidate;
		}
	}

	if (!BestCandidate.IsValid())
	{
		UE_LOG(LogGambit, Log, TEXT("DebugAI: P%d Shop Chosen=None Reason=NoAffordableUsefulOffer"), PlayerIndex);
		return;
	}

	const FGambitShopOffer OfferSnapshot = BestCandidate.Offer;
	const bool bPurchased = GameMode->RequestPurchaseOffer(PlayerState, OfferSnapshot.OfferId);
	const FGambitPlayerSlotState SlotStateAfter = PlayerState->GetSlotState();
	UE_LOG(
		LogGambit,
		Log,
		TEXT("DebugAI: P%d Shop Chosen OfferId=%d Item=%s Type=%s Reason=%s Gold=%d Result=%s GoldAfter=%d Modules=%d/%d Consumables=%d/%d DiceOwned=%d"),
		PlayerIndex,
		OfferSnapshot.OfferId,
		*FormatItemName(OfferSnapshot.ItemDefinition),
		OfferSnapshot.ItemDefinition ? *ItemTypeToString(OfferSnapshot.ItemDefinition->ItemType) : TEXT("None"),
		*BestCandidate.Reason,
		Gold,
		bPurchased ? TEXT("Success") : TEXT("Failure"),
		PlayerState->GetCurrentGold(),
		SlotStateAfter.ModuleSlotsUsed,
		SlotStateAfter.ModuleSlotsCapacity,
		SlotStateAfter.ConsumableSlotsUsed,
		SlotStateAfter.ConsumableSlotsCapacity,
		SlotStateAfter.OwnedDiceCount);
}

void UGambitDebugAutoPlayer::EnsureScoringServices()
{
	if (!DiceEvaluator)
	{
		DiceEvaluator = NewObject<UGambitDiceCombinationEvaluator>(this);
	}

	if (!ScoreCalculator)
	{
		ScoreCalculator = NewObject<UGambitScoreCalculator>(this);
	}
}
