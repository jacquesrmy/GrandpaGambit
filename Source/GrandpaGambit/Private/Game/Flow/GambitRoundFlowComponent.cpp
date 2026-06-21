#include "Game/Flow/GambitRoundFlowComponent.h"

#include "Core/Logging/GambitLog.h"
#include "Core/Interfaces/GambitDiceEvaluatorContract.h"
#include "Core/Interfaces/GambitScoreCalculatorContract.h"
#include "Core/Settings/GambitGameBalanceSettings.h"
#include "Core/Types/GambitDebugTypes.h"
#include "Core/Types/GambitRoundPhaseRules.h"
#include "Data/Assets/GambitItemEffectDefinition.h"
#include "Dice/Data/GambitDiceDefinition.h"
#include "Dice/Evaluation/GambitDiceCombinationEvaluator.h"
#include "Game/Flow/GambitRoundEffectPipeline.h"
#include "Game/States/GambitGameState.h"
#include "GameFramework/GameModeBase.h"
#include "Items/Consumables/GambitConsumableDefinition.h"
#include "Items/Data/GambitItemDefinition.h"
#include "Items/Effects/GambitEffectResolver.h"
#include "Items/Effects/GambitEffectTargetRules.h"
#include "Items/Modules/GambitModuleDefinition.h"
#include "Managers/SharedPool/GambitSharedPoolComponent.h"
#include "Players/Components/GambitDiceComponent.h"
#include "Players/Components/GambitEconomyComponent.h"
#include "Players/Components/GambitInventoryComponent.h"
#include "Players/Components/GambitPlayerRoundStateComponent.h"
#include "Players/States/GambitPlayerState.h"
#include "Ranking/Components/GambitRankingComponent.h"
#include "Scoring/Calculators/GambitScoreCalculator.h"
#include "Scoring/Calculators/GambitScoreModifierMath.h"
#include "Shop/Components/GambitShopComponent.h"

namespace
{
	FString RoundFlowPhaseToString(const EGambitRoundPhase Phase)
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

	FString RoundFlowCombinationToString(const EGambitCombinationType Combination)
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

	FString FormatRoundFlowDiceValues(const TArray<FGambitDieRuntimeState>& DiceStates)
	{
		TArray<FString> Values;
		Values.Reserve(DiceStates.Num());

		for (const FGambitDieRuntimeState& DieState : DiceStates)
		{
			Values.Add(DieState.bLocked
				? FString::Printf(TEXT("%dL"), DieState.EffectiveValue)
				: FString::FromInt(DieState.EffectiveValue));
		}

		return FString::Printf(TEXT("[%s]"), *FString::Join(Values, TEXT(", ")));
	}

	FString FormatRoundFlowItemName(const UGambitItemDefinition* ItemDefinition)
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

	bool ConsumableRequiresSelectedDie(const UGambitConsumableDefinition* ConsumableDefinition)
	{
		if (!ConsumableDefinition)
		{
			return false;
		}

		return ConsumableDefinition->EffectDefinitions.ContainsByPredicate(
			[](const TObjectPtr<UGambitItemEffectDefinition>& EffectDefinition)
			{
				return EffectDefinition && GambitEffectTargetRules::RequiresSelectedDie(EffectDefinition->TargetRuleId);
			});
	}

	FString FormatRoundFlowOffer(const FGambitShopOffer& Offer)
	{
		const UGambitItemDefinition* ItemDefinition = Offer.ItemDefinition;
		const FString ItemId = ItemDefinition ? ItemDefinition->GetResolvedItemId().ToString() : TEXT("None");
		return FString::Printf(
			TEXT("OfferId=%d Item=%s ItemId=%s BasePrice=%d Price=%d SharedPool=%s Reserved=%s"),
			Offer.OfferId,
			*FormatRoundFlowItemName(ItemDefinition),
			*ItemId,
			Offer.BasePrice,
			Offer.Price,
			Offer.bUsesSharedPool ? TEXT("Yes") : TEXT("No"),
			Offer.bHasSharedPoolReservation ? TEXT("Yes") : TEXT("No"));
	}

	FString BuildRoundFlowPlayerLabel(const AGambitPlayerState* PlayerState, const TArray<AGambitPlayerState*>& Players)
	{
		const int32 PlayerIndex = Players.IndexOfByKey(PlayerState);
		const FString PlayerName = PlayerState ? PlayerState->GetPlayerName() : FString();
		const FString ResolvedName = PlayerName.IsEmpty() ? TEXT("Unnamed") : PlayerName;
		return FString::Printf(TEXT("P%d %s"), PlayerIndex, *ResolvedName);
	}

	void LogPlayerRoundSnapshot(const AGambitPlayerState* PlayerState, const TArray<AGambitPlayerState*>& Players)
	{
		if (!PlayerState)
		{
			return;
		}

		const FGambitPlayerSlotState SlotState = PlayerState->GetSlotState();
		const TArray<FGambitDieRuntimeState>& DiceStates = PlayerState->GetDiceStatesRef();
		UE_LOG(
			LogGambit,
			Log,
			TEXT("RoundFlow: %s Gold=%d VP=%d Score=%d DiceCount=%d Modules=%d/%d Consumables=%d/%d Dice=%s"),
			*BuildRoundFlowPlayerLabel(PlayerState, Players),
			PlayerState->GetCurrentGold(),
			PlayerState->GetTotalVictoryPoints(),
			PlayerState->GetCurrentRoundScore(),
			DiceStates.Num(),
			SlotState.ModuleSlotsUsed,
			SlotState.ModuleSlotsCapacity,
			SlotState.ConsumableSlotsUsed,
			SlotState.ConsumableSlotsCapacity,
			*FormatRoundFlowDiceValues(DiceStates));
	}

	template <typename TEnum>
	FString RoundFlowEnumToDebugString(const TEnum Value)
	{
		if (const UEnum* Enum = StaticEnum<TEnum>())
		{
			return Enum->GetNameStringByValue(static_cast<int64>(Value));
		}

		return TEXT("Unknown");
	}

	FGambitDebugScoreLine MakeRoundScoreLine(
		const EGambitDebugScoreLineType LineType,
		const EGambitRoundPhase Phase,
		const FString& Label,
		const float AdditiveDelta,
		const float DiceContributionDelta,
		const float MultiplierValue,
		const float ScoreBefore,
		const float ScoreAfter,
		const FString& Summary)
	{
		FGambitDebugScoreLine Line;
		Line.LineType = LineType;
		Line.Phase = Phase;
		Line.SourceId = TEXT("score.calculator");
		Line.SourceName = TEXT("Score calculator");
		Line.Label = Label;
		Line.AdditiveDelta = AdditiveDelta;
		Line.DiceContributionDelta = DiceContributionDelta;
		Line.MultiplierValue = MultiplierValue;
		Line.ScoreBefore = ScoreBefore;
		Line.ScoreAfter = ScoreAfter;
		Line.Summary = Summary;
		return Line;
	}

	void RecordScoreBreakdownDebugLines(
		AGambitPlayerState* PlayerState,
		const EGambitRoundPhase Phase,
		const FGambitScoreBreakdown& ScoreBreakdown)
	{
		if (!PlayerState)
		{
			return;
		}

		PlayerState->AddDebugScoreLine(MakeRoundScoreLine(
			EGambitDebugScoreLineType::BaseCombination,
			Phase,
			TEXT("Combination base"),
			static_cast<float>(ScoreBreakdown.BaseCombinationScore),
			0.0f,
			1.0f,
			0.0f,
			static_cast<float>(ScoreBreakdown.BaseCombinationScore),
			FString::Printf(TEXT("%s base score: %d"), *RoundFlowCombinationToString(ScoreBreakdown.Combination), ScoreBreakdown.BaseCombinationScore)));

		PlayerState->AddDebugScoreLine(MakeRoundScoreLine(
			EGambitDebugScoreLineType::DiceSum,
			Phase,
			TEXT("Dice sum"),
			static_cast<float>(ScoreBreakdown.DiceSum),
			0.0f,
			1.0f,
			0.0f,
			static_cast<float>(ScoreBreakdown.DiceSum),
			FString::Printf(TEXT("Dice score contribution sum: %d"), ScoreBreakdown.DiceSum)));

		if (!FMath::IsNearlyZero(ScoreBreakdown.DiceContributionMultiplierBonus))
		{
			PlayerState->AddDebugScoreLine(MakeRoundScoreLine(
				EGambitDebugScoreLineType::DiceContribution,
				Phase,
				TEXT("Dice contribution bonus"),
				0.0f,
				ScoreBreakdown.DiceContributionMultiplierBonus,
				1.0f,
				static_cast<float>(ScoreBreakdown.DiceSum),
				ScoreBreakdown.AdjustedDiceSum,
				FString::Printf(TEXT("Dice contribution adjusted by %+0.2f"), ScoreBreakdown.DiceContributionMultiplierBonus)));
		}

		if (!FMath::IsNearlyZero(ScoreBreakdown.AdditiveBonus))
		{
			PlayerState->AddDebugScoreLine(MakeRoundScoreLine(
				EGambitDebugScoreLineType::Additive,
				Phase,
				TEXT("Total additive bonus"),
				ScoreBreakdown.AdditiveBonus,
				0.0f,
				1.0f,
				static_cast<float>(ScoreBreakdown.RawScore),
				static_cast<float>(ScoreBreakdown.RawScore) + ScoreBreakdown.AdditiveBonus,
				FString::Printf(TEXT("Total additive score bonus: %+0.2f"), ScoreBreakdown.AdditiveBonus)));
		}

		if (!FMath::IsNearlyEqual(ScoreBreakdown.Multiplier, 1.0f))
		{
			PlayerState->AddDebugScoreLine(MakeRoundScoreLine(
				EGambitDebugScoreLineType::Multiplier,
				Phase,
				TEXT("Total multiplier"),
				0.0f,
				0.0f,
				ScoreBreakdown.Multiplier,
				0.0f,
				ScoreBreakdown.ScoreBeforeCap,
				FString::Printf(TEXT("Total score multiplier: x%0.2f"), ScoreBreakdown.Multiplier)));
		}

		if (!FMath::IsNearlyEqual(ScoreBreakdown.ScoreBeforeCap, ScoreBreakdown.ScoreAfterCap))
		{
			PlayerState->AddDebugScoreLine(MakeRoundScoreLine(
				EGambitDebugScoreLineType::Cap,
				Phase,
				TEXT("Score cap or diminishing"),
				0.0f,
				0.0f,
				1.0f,
				ScoreBreakdown.ScoreBeforeCap,
				ScoreBreakdown.ScoreAfterCap,
				FString::Printf(TEXT("Score adjusted from %0.2f to %0.2f by cap/diminishing"), ScoreBreakdown.ScoreBeforeCap, ScoreBreakdown.ScoreAfterCap)));
		}

		PlayerState->AddDebugScoreLine(MakeRoundScoreLine(
			EGambitDebugScoreLineType::FinalScore,
			Phase,
			TEXT("Final score"),
			0.0f,
			0.0f,
			1.0f,
			ScoreBreakdown.ScoreAfterCap,
			static_cast<float>(ScoreBreakdown.FinalScore),
			FString::Printf(TEXT("Final rounded score: %d"), ScoreBreakdown.FinalScore)));
	}

	FGambitDebugShopLine BuildShopLineFromOffer(
		const FGambitShopOffer& Offer,
		const EGambitRoundPhase Phase,
		const UGambitSharedPoolComponent* SharedPoolComponent)
	{
		FGambitDebugShopLine Line;
		Line.LineType = EGambitDebugShopLineType::GeneratedOffer;
		Line.Phase = Phase;
		Line.OfferId = Offer.OfferId;
		Line.BasePrice = Offer.BasePrice;
		Line.PriceBeforeModifiers = Offer.BasePrice;
		Line.ResolvedPrice = Offer.Price;
		Line.bFree = Offer.bFree;
		Line.FreeReason = Offer.FreeReason;
		Line.bUsesSharedPool = Offer.bUsesSharedPool;
		Line.bHasSharedPoolReservation = Offer.bHasSharedPoolReservation;

		if (const UGambitItemDefinition* ItemDefinition = Offer.ItemDefinition)
		{
			Line.ItemId = ItemDefinition->GetResolvedItemId();
			Line.ItemName = FormatRoundFlowItemName(ItemDefinition);
			Line.ItemType = ItemDefinition->ItemType;
			Line.Rarity = ItemDefinition->Rarity;
			if (ItemDefinition->bUsesSharedPool && SharedPoolComponent)
			{
				const FGambitSharedPoolAvailabilityResult Availability = SharedPoolComponent->QueryItemAvailability(ItemDefinition);
				Line.SharedPoolState = RoundFlowEnumToDebugString(Availability.State);
				Line.SharedPoolReason = Availability.Reason;
			}
		}

		Line.Summary = FString::Printf(
			TEXT("Offer %d: %s Price=%d SharedPool=%s"),
			Line.OfferId,
			*Line.ItemName,
			Line.ResolvedPrice,
			Line.bUsesSharedPool ? TEXT("Yes") : TEXT("No"));
		return Line;
	}

	FGambitDebugShopLine BuildShopLineFromPurchaseContext(
		const FGambitShopPurchaseContext& PurchaseContext,
		const EGambitRoundPhase Phase,
		const EGambitDebugShopLineType LineType)
	{
		FGambitDebugShopLine Line;
		Line.LineType = LineType;
		Line.Phase = Phase;
		Line.OfferId = PurchaseContext.OfferId;
		Line.BasePrice = PurchaseContext.BasePrice;
		Line.PriceBeforeModifiers = PurchaseContext.PriceBeforeModifiers;
		Line.ResolvedPrice = PurchaseContext.ResolvedPrice;
		Line.DiscountPercent = PurchaseContext.DiscountPercent;
		Line.SurchargePercent = PurchaseContext.SurchargePercent;
		Line.FlatPriceDelta = PurchaseContext.FlatPriceDelta;
		Line.bFree = PurchaseContext.bFree;
		Line.FreeReason = PurchaseContext.FreeReason;
		Line.CashbackPercent = PurchaseContext.CashbackPercent;
		Line.CashbackGold = PurchaseContext.CashbackGold;
		Line.GoldDeltaOnPurchase = PurchaseContext.GoldDeltaOnPurchase;
		Line.bUsesSharedPool = PurchaseContext.bUsesSharedPool;
		Line.bHasSharedPoolReservation = PurchaseContext.bHasSharedPoolReservation;
		Line.SharedPoolState = RoundFlowEnumToDebugString(PurchaseContext.SharedPoolAvailability.State);
		Line.SharedPoolReason = PurchaseContext.SharedPoolAvailability.Reason;
		Line.bPurchaseSucceeded = PurchaseContext.bPurchaseSucceeded;
		Line.bRefused = !PurchaseContext.bPurchaseSucceeded;
		Line.FailureReason = PurchaseContext.FailureReason;
		Line.DebugLines = PurchaseContext.DebugLines;

		if (const UGambitItemDefinition* ItemDefinition = PurchaseContext.ItemDefinition.Get())
		{
			Line.ItemId = ItemDefinition->GetResolvedItemId();
			Line.ItemName = FormatRoundFlowItemName(ItemDefinition);
			Line.ItemType = ItemDefinition->ItemType;
			Line.Rarity = ItemDefinition->Rarity;
		}

		Line.Summary = PurchaseContext.bPurchaseSucceeded
			? FString::Printf(TEXT("Purchased %s for %d gold"), *Line.ItemName, Line.ResolvedPrice)
			: FString::Printf(TEXT("Purchase refused for %s: %s"), *Line.ItemName, *Line.FailureReason);
		return Line;
	}

	int32 ResolveRoundFlowPlayerId(const AGambitPlayerState* PlayerState)
	{
		return PlayerState ? PlayerState->GetPlayerId() : INDEX_NONE;
	}

	FGambitRoundGameplayEvent MakeRoundFlowEvent(
		const AGambitGameState* GameState,
		AGambitPlayerState* SourcePlayer,
		AGambitPlayerState* TargetPlayer,
		const EGambitRoundGameplayEventType EventType,
		const EGambitRoundGameplayEventOutcome Outcome,
		const float NumericDelta,
		const FString& Reason)
	{
		FGambitRoundGameplayEvent Event;
		Event.EventType = EventType;
		Event.Outcome = Outcome;
		Event.RoundNumber = GameState ? GameState->GetCurrentRoundIndex() : 0;
		Event.Phase = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
		Event.SourcePlayerId = ResolveRoundFlowPlayerId(SourcePlayer);
		Event.TargetPlayerId = ResolveRoundFlowPlayerId(TargetPlayer ? TargetPlayer : SourcePlayer);
		Event.NumericDelta = NumericDelta;
		Event.Reason = Reason;
		return Event;
	}
}

UGambitRoundFlowComponent::UGambitRoundFlowComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	DiceEvaluatorClass = UGambitDiceCombinationEvaluator::StaticClass();
	ScoreCalculatorClass = UGambitScoreCalculator::StaticClass();
}

void UGambitRoundFlowComponent::BeginPlay()
{
	Super::BeginPlay();

	MatchRandomStream.Initialize(RandomSeed);
	if (!DiceEvaluator)
	{
		DiceEvaluator = NewObject<UGambitDiceEvaluatorContract>(this, DiceEvaluatorClass);
	}
	if (!ScoreCalculator)
	{
		ScoreCalculator = NewObject<UGambitScoreCalculatorContract>(this, ScoreCalculatorClass);
	}
	if (!EffectResolver)
	{
		EffectResolver = NewObject<UGambitEffectResolver>(this);
	}
	if (!EffectPipeline)
	{
		EffectPipeline = NewObject<UGambitRoundEffectPipeline>(this);
	}
	EffectPipeline->SetEffectResolver(EffectResolver);
}

void UGambitRoundFlowComponent::StartMatchFlow()
{
	AGambitGameState* GameState = GetGambitGameState();
	if (!GameState)
	{
		UE_LOG(LogGambit, Warning, TEXT("RoundFlow: missing GambitGameState"));
		return;
	}

	const UGambitGameBalanceSettings* Settings = UGambitGameBalanceSettings::Get();
	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	UE_LOG(
		LogGambit,
		Log,
		TEXT("RoundFlow: StartMatchFlow Players=%d RoundCount=%d DefaultDiceCount=%d MaxRerolls=%d ShopOfferCount=%d"),
		Players.Num(),
		Settings->RoundCount,
		Settings->DefaultActiveDiceCount,
		Settings->MaxRerollsPerRound,
		Settings->ShopOfferCount);

	GameState->SetCurrentRoundIndex(0);
	GameState->SetCurrentPhase(EGambitRoundPhase::None);

	for (AGambitPlayerState* PlayerState : Players)
	{
		if (PlayerState)
		{
			PlayerState->InitializeForMatch();
		}
	}

	if (UGambitSharedPoolComponent* SharedPool = GameState->GetSharedPoolComponent())
	{
		SharedPool->InitializeSharedStock({});
	}

	StartNextRound();
}

void UGambitRoundFlowComponent::SetPlayerReady(AGambitPlayerState* PlayerState, const bool bReady)
{
	if (!PlayerState)
	{
		UE_LOG(LogGambit, Log, TEXT("RoundFlow: SetPlayerReady failed, PlayerState is null"));
		return;
	}

	PlayerReadyMap.Add(PlayerState, bReady);
	const AGambitGameState* GameState = GetGambitGameState();
	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	const FString PhaseString = GameState ? RoundFlowPhaseToString(GameState->GetCurrentPhase()) : FString(TEXT("MissingGameState"));
	UE_LOG(
		LogGambit,
		Log,
		TEXT("RoundFlow: SetPlayerReady Player=%s Phase=%s Ready=%s"),
		*BuildRoundFlowPlayerLabel(PlayerState, Players),
		*PhaseString,
		bReady ? TEXT("true") : TEXT("false"));

	if (GameState && GameState->GetCurrentPhase() == EGambitRoundPhase::Shop && bReady)
	{
		FGambitEffectExecutionContext ShopSkippedContext = CreateEffectContext(EGambitEffectHook::ShopSkipped, PlayerState);
		ExecuteActiveEffectsAndCommit(ShopSkippedContext);
		PlayerState->SkipShop(GameState->GetSharedPoolComponent());
	UE_LOG(LogGambit, Log, TEXT("RoundFlow: %s ShopSkipped"), *BuildRoundFlowPlayerLabel(PlayerState, Players));
	}

	AdvanceWhenAllPlayersReady();
}

bool UGambitRoundFlowComponent::RequestSetDieLocked(AGambitPlayerState* PlayerState, const int32 DieIndex, const bool bLocked)
{
	AGambitGameState* GameState = GetGambitGameState();
	const EGambitRoundPhase CurrentPhase = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	bool bSuccess = false;
	if (GameState && CurrentPhase == EGambitRoundPhase::SelectionReroll && PlayerState)
	{
		bSuccess = PlayerState->TrySetDieLocked(DieIndex, bLocked);
	}

	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	const FString DiceString = PlayerState ? FormatRoundFlowDiceValues(PlayerState->GetDiceStatesRef()) : FString(TEXT("[]"));
	UE_LOG(
		LogGambit,
		Log,
		TEXT("RoundFlow: RequestSetDieLocked Player=%s Phase=%s DieIndex=%d Locked=%s Result=%s Dice=%s"),
		*BuildRoundFlowPlayerLabel(PlayerState, Players),
		*RoundFlowPhaseToString(CurrentPhase),
		DieIndex,
		bLocked ? TEXT("true") : TEXT("false"),
		bSuccess ? TEXT("Success") : TEXT("Failure"),
		*DiceString);
	return bSuccess;
}

bool UGambitRoundFlowComponent::RequestReroll(AGambitPlayerState* PlayerState)
{
	AGambitGameState* GameState = GetGambitGameState();
	const EGambitRoundPhase CurrentPhase = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	const int32 UsedBefore = GetRerollCount(PlayerState);
	bool bSuccess = false;
	int32 EffectiveRerollLimit = GetEffectiveRerollLimit(PlayerState);

	if (GameState && CurrentPhase == EGambitRoundPhase::SelectionReroll && PlayerState)
	{
		FGambitEffectExecutionContext PreRerollContext = CreateEffectContext(EGambitEffectHook::PreReroll, PlayerState);
		ExecuteActiveEffectsAndCommit(PreRerollContext);
		EffectiveRerollLimit = GetEffectiveRerollLimit(PlayerState);
	}

	if (GameState && CurrentPhase == EGambitRoundPhase::SelectionReroll && PlayerState && UsedBefore < EffectiveRerollLimit)
	{
		const TArray<FGambitDieRuntimeState> DiceBefore = PlayerState->GetDiceStates();
		if (PlayerState->TryRerollUnlockedDice(MatchRandomStream))
		{
			TArray<FGambitRerollDieDelta> RerollDeltas = BuildRerollDeltas(DiceBefore, PlayerState->GetDiceStatesRef());
			TrackRerollDeltas(PlayerState, RerollDeltas);
			IncrementRerollCount(PlayerState);
			bSuccess = true;

			FGambitEffectExecutionContext PostRerollContext = CreateEffectContext(EGambitEffectHook::PostReroll, PlayerState);
			PostRerollContext.RerollDeltas = RerollDeltas;
			ExecuteActiveEffectsAndCommit(PostRerollContext);
			EffectiveRerollLimit = GetEffectiveRerollLimit(PlayerState);
		}
	}

	const int32 UsedAfter = GetRerollCount(PlayerState);
	if (bSuccess && PlayerState)
	{
		PlayerState->AddRoundEvent(MakeRoundFlowEvent(
			GameState,
			PlayerState,
			PlayerState,
			EGambitRoundGameplayEventType::RerollUsed,
			EGambitRoundGameplayEventOutcome::Applied,
			static_cast<float>(UsedAfter),
			FString::Printf(TEXT("Reroll used %d/%d"), UsedAfter, EffectiveRerollLimit)));
	}

	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	const FString DiceString = PlayerState ? FormatRoundFlowDiceValues(PlayerState->GetDiceStatesRef()) : FString(TEXT("[]"));
	UE_LOG(
		LogGambit,
		Log,
		TEXT("RoundFlow: RequestReroll Player=%s Phase=%s RerollsUsed=%d/%d Result=%s Dice=%s"),
		*BuildRoundFlowPlayerLabel(PlayerState, Players),
		*RoundFlowPhaseToString(CurrentPhase),
		UsedAfter,
		EffectiveRerollLimit,
		bSuccess ? TEXT("Success") : TEXT("Failure"),
		*DiceString);
	return bSuccess;
}

bool UGambitRoundFlowComponent::RequestUseConsumable(AGambitPlayerState* PlayerState, const int32 SlotIndex)
{
	return RequestUseConsumableOnTarget(PlayerState, SlotIndex, PlayerState);
}

bool UGambitRoundFlowComponent::RequestUseConsumableOnTarget(
	AGambitPlayerState* PlayerState,
	const int32 SlotIndex,
	AGambitPlayerState* TargetPlayerState)
{
	return RequestUseConsumableOnTargetSelectedDie(PlayerState, SlotIndex, TargetPlayerState, INDEX_NONE);
}

bool UGambitRoundFlowComponent::RequestUseConsumableOnTargetSelectedDie(
	AGambitPlayerState* PlayerState,
	const int32 SlotIndex,
	AGambitPlayerState* TargetPlayerState,
	const int32 SelectedDieIndex)
{
	AGambitGameState* GameState = GetGambitGameState();
	const EGambitRoundPhase CurrentPhase = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	bool bSuccess = false;
	if (GameState && PlayerState)
	{
		UGambitConsumableDefinition* ConsumableDefinition = nullptr;
		const TArray<FGambitConsumableRuntimeSlot>& ConsumableSlots = PlayerState->GetConsumableSlotsRef();
		const FGambitConsumableRuntimeSlot* Slot = ConsumableSlots.IsValidIndex(SlotIndex) ? &ConsumableSlots[SlotIndex] : nullptr;
		const bool bTargetsOpponent = TargetPlayerState && TargetPlayerState != PlayerState;
		if (Slot
			&& Slot->Definition
			&& Slot->Definition->CanBeUsedDuringPhase(CurrentPhase)
			&& (!bTargetsOpponent || Slot->Definition->bCanTargetOpponent))
		{
			const AGambitPlayerState* SelectedDieOwner = bTargetsOpponent ? TargetPlayerState : PlayerState;
			const bool bSelectedDieValid = SelectedDieOwner && SelectedDieOwner->GetDiceStatesRef().IsValidIndex(SelectedDieIndex);
			if (ConsumableRequiresSelectedDie(Slot->Definition) && !bSelectedDieValid)
			{
				UE_LOG(
					LogGambit,
					Log,
					TEXT("RoundFlow: RequestUseConsumable refused, selected die invalid Slot=%d SelectedDie=%d"),
					SlotIndex,
					SelectedDieIndex);
			}
			else
			{
				if (PlayerState->ConsumeConsumableDefinitionAtSlot(SlotIndex, ConsumableDefinition) && ConsumableDefinition)
				{
					FGambitEffectExecutionContext Context = CreateEffectContext(EGambitEffectHook::ConsumableUse, PlayerState, TargetPlayerState ? TargetPlayerState : PlayerState);
					if (bTargetsOpponent)
					{
						Context.TargetDieHandIndex = SelectedDieIndex;
					}
					else
					{
						Context.SourceDieHandIndex = SelectedDieIndex;
						Context.TargetDieHandIndex = SelectedDieIndex;
					}
					if (EffectPipeline)
					{
						EffectPipeline->ExecuteItemEffects(ConsumableDefinition, Context);
						EffectPipeline->CommitEffectContext(Context, BuildEffectCommitRequest());
					}
					bSuccess = true;
				}
			}
		}
	}

	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	UE_LOG(
		LogGambit,
		Log,
		TEXT("RoundFlow: RequestUseConsumable Player=%s Target=%s Phase=%s Slot=%d Result=%s"),
		*BuildRoundFlowPlayerLabel(PlayerState, Players),
		*BuildRoundFlowPlayerLabel(TargetPlayerState, Players),
		*RoundFlowPhaseToString(CurrentPhase),
		SlotIndex,
		bSuccess ? TEXT("Success") : TEXT("Failure"));
	return bSuccess;
}

bool UGambitRoundFlowComponent::RequestPurchaseOffer(AGambitPlayerState* PlayerState, const int32 OfferId)
{
	AGambitGameState* GameState = GetGambitGameState();
	const EGambitRoundPhase CurrentPhase = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	FGambitShopOffer OfferSnapshot;
	bool bFoundOffer = false;
	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	if (PlayerState)
	{
		if (const FGambitShopOffer* FoundOffer = PlayerState->GetCurrentShopOffersRef().FindByPredicate([OfferId](const FGambitShopOffer& Offer)
		{
			return Offer.OfferId == OfferId;
		}))
		{
			OfferSnapshot = *FoundOffer;
			bFoundOffer = true;
		}
	}

	bool bSuccess = false;
	if (GameState && CurrentPhase == EGambitRoundPhase::Shop && PlayerState)
	{
		UGambitSharedPoolComponent* SharedPoolComponent = GameState->GetSharedPoolComponent();
		FGambitShopPurchaseContext PurchaseContext = PlayerState->BuildShopPurchaseContext(OfferId, SharedPoolComponent);

		FGambitEffectExecutionContext PrePriceContext = CreateEffectContext(EGambitEffectHook::PrePriceResolve, PlayerState);
		PrePriceContext.ShopPurchase = PurchaseContext;
		ExecuteActiveEffectsAndCommit(PrePriceContext);
		PurchaseContext = PrePriceContext.ShopPurchase;
		PlayerState->ResolveShopPurchasePrice(PurchaseContext);

		FGambitEffectExecutionContext PostPriceContext = CreateEffectContext(EGambitEffectHook::PostPriceResolve, PlayerState);
		PostPriceContext.ShopPurchase = PurchaseContext;
		ExecuteActiveEffectsAndCommit(PostPriceContext);
		PurchaseContext = PostPriceContext.ShopPurchase;
		PlayerState->ResolveShopPurchasePrice(PurchaseContext);

		FGambitEffectExecutionContext PrePurchaseContext = CreateEffectContext(EGambitEffectHook::PrePurchase, PlayerState);
		PrePurchaseContext.ShopPurchase = PurchaseContext;
		ExecuteActiveEffectsAndCommit(PrePurchaseContext);
		PurchaseContext = PrePurchaseContext.ShopPurchase;

		const int32 GoldBeforePurchase = PlayerState->GetCurrentGold();
		bSuccess = PlayerState->TryPurchaseOfferWithContext(PurchaseContext, SharedPoolComponent);
		const int32 GoldAfterPurchase = PlayerState->GetCurrentGold();
		if (bSuccess && PurchaseContext.ItemDefinition)
		{
			FGambitDebugGoldLine PurchaseGoldLine;
			PurchaseGoldLine.LineType = EGambitDebugGoldLineType::Purchase;
			PurchaseGoldLine.Phase = CurrentPhase;
			PurchaseGoldLine.SourceId = PurchaseContext.ItemDefinition->GetResolvedItemId();
			PurchaseGoldLine.SourceName = FormatRoundFlowItemName(PurchaseContext.ItemDefinition.Get());
			PurchaseGoldLine.RequestedDelta = -PurchaseContext.ResolvedPrice;
			PurchaseGoldLine.ActualDelta = GoldAfterPurchase - GoldBeforePurchase;
			PurchaseGoldLine.GoldBefore = GoldBeforePurchase;
			PurchaseGoldLine.GoldAfter = GoldAfterPurchase;
			PurchaseGoldLine.bClamped = PurchaseGoldLine.RequestedDelta != PurchaseGoldLine.ActualDelta;
			PurchaseGoldLine.Summary = FString::Printf(TEXT("Purchase spend for %s: %d gold"), *PurchaseGoldLine.SourceName, PurchaseContext.ResolvedPrice);
			PlayerState->AddDebugGoldLine(PurchaseGoldLine);

			FGambitEffectExecutionContext PostPurchaseContext = CreateEffectContext(EGambitEffectHook::PostPurchase, PlayerState);
			PostPurchaseContext.ShopPurchase = PurchaseContext;
			if (EffectPipeline)
			{
				EffectPipeline->ExecuteActiveSourceEffects(PostPurchaseContext);
			}
			if (!Cast<UGambitModuleDefinition>(PurchaseContext.ItemDefinition.Get()))
			{
				if (EffectPipeline)
				{
					EffectPipeline->ExecuteItemEffects(PurchaseContext.ItemDefinition.Get(), PostPurchaseContext);
				}
			}
			if (EffectPipeline)
			{
				EffectPipeline->CommitEffectContext(PostPurchaseContext, BuildEffectCommitRequest());
			}
			PurchaseContext = PostPurchaseContext.ShopPurchase;
			const int32 GoldBeforePostPurchase = PlayerState->GetCurrentGold();
			PlayerState->ApplyPostPurchaseAdjustments(PurchaseContext);
			const int32 GoldAfterPostPurchase = PlayerState->GetCurrentGold();
			if (GoldAfterPostPurchase != GoldBeforePostPurchase)
			{
				FGambitDebugGoldLine PostPurchaseGoldLine;
				PostPurchaseGoldLine.LineType = EGambitDebugGoldLineType::Cashback;
				PostPurchaseGoldLine.Phase = CurrentPhase;
				PostPurchaseGoldLine.SourceId = PurchaseContext.ItemDefinition->GetResolvedItemId();
				PostPurchaseGoldLine.SourceName = FormatRoundFlowItemName(PurchaseContext.ItemDefinition.Get());
				PostPurchaseGoldLine.RequestedDelta = PurchaseContext.CashbackGold + PurchaseContext.GoldDeltaOnPurchase;
				PostPurchaseGoldLine.ActualDelta = GoldAfterPostPurchase - GoldBeforePostPurchase;
				PostPurchaseGoldLine.GoldBefore = GoldBeforePostPurchase;
				PostPurchaseGoldLine.GoldAfter = GoldAfterPostPurchase;
				PostPurchaseGoldLine.bClamped = PostPurchaseGoldLine.RequestedDelta != PostPurchaseGoldLine.ActualDelta;
				PostPurchaseGoldLine.Summary = FString::Printf(
					TEXT("Post-purchase gold adjustment for %s: cashback=%d delta=%d"),
					*PostPurchaseGoldLine.SourceName,
					PurchaseContext.CashbackGold,
					PurchaseContext.GoldDeltaOnPurchase);
				PlayerState->AddDebugGoldLine(PostPurchaseGoldLine);
			}
			PlayerState->AddDebugShopLine(BuildShopLineFromPurchaseContext(PurchaseContext, CurrentPhase, EGambitDebugShopLineType::PurchaseSuccess));
		}
		else if (!PurchaseContext.FailureReason.IsEmpty())
		{
			PlayerState->AddDebugShopLine(BuildShopLineFromPurchaseContext(PurchaseContext, CurrentPhase, EGambitDebugShopLineType::PurchaseFailure));
			UE_LOG(
				LogGambit,
				Log,
				TEXT("RoundFlow: RequestPurchaseOffer refused Player=%s OfferId=%d Reason=%s"),
				*BuildRoundFlowPlayerLabel(PlayerState, Players),
				OfferId,
				*PurchaseContext.FailureReason);
		}
	}

	const FString OfferString = bFoundOffer ? FormatRoundFlowOffer(OfferSnapshot) : FString::Printf(TEXT("OfferId=%d NotFound"), OfferId);
	UE_LOG(
		LogGambit,
		Log,
		TEXT("RoundFlow: RequestPurchaseOffer Player=%s Phase=%s Offer=%s Result=%s GoldRemaining=%d"),
		*BuildRoundFlowPlayerLabel(PlayerState, Players),
		*RoundFlowPhaseToString(CurrentPhase),
		*OfferString,
		bSuccess ? TEXT("Success") : TEXT("Failure"),
		PlayerState ? PlayerState->GetCurrentGold() : 0);
	return bSuccess;
}

int32 UGambitRoundFlowComponent::GetRerollsUsedForPlayer(AGambitPlayerState* PlayerState) const
{
	return GetRerollCount(PlayerState);
}

int32 UGambitRoundFlowComponent::GetMaxRerollsForPlayer(AGambitPlayerState* PlayerState) const
{
	return GetEffectiveRerollLimit(PlayerState);
}

bool UGambitRoundFlowComponent::TransitionToPhase(const EGambitRoundPhase NewPhase)
{
	AGambitGameState* GameState = GetGambitGameState();
	if (!GameState)
	{
		return false;
	}

	const EGambitRoundPhase CurrentPhase = GameState->GetCurrentPhase();
	if (!IsTransitionAllowed(CurrentPhase, NewPhase))
	{
		UE_LOG(LogGambit, Warning, TEXT("RoundFlow: invalid transition %s -> %s"), *RoundFlowPhaseToString(CurrentPhase), *RoundFlowPhaseToString(NewPhase));
		return false;
	}

	GameState->SetCurrentPhase(NewPhase);
	UE_LOG(
		LogGambit,
		Log,
		TEXT("RoundFlow: Round=%d PhaseTransition Old=%s New=%s"),
		GameState->GetCurrentRoundIndex(),
		*RoundFlowPhaseToString(CurrentPhase),
		*RoundFlowPhaseToString(NewPhase));
	OnPhaseEntered.Broadcast(NewPhase);

	switch (NewPhase)
	{
	case EGambitRoundPhase::Roll:
		EnterRollPhase();
		break;
	case EGambitRoundPhase::SelectionReroll:
		EnterSelectionRerollPhase();
		break;
	case EGambitRoundPhase::Action:
		EnterActionPhase();
		break;
	case EGambitRoundPhase::Resolution:
		EnterResolutionPhase();
		break;
	case EGambitRoundPhase::Reward:
		EnterRewardPhase();
		break;
	case EGambitRoundPhase::Ranking:
		EnterRankingPhase();
		break;
	case EGambitRoundPhase::Shop:
		EnterShopPhase();
		break;
	case EGambitRoundPhase::RoundEnd:
		EnterRoundEndPhase();
		break;
	default:
		break;
	}

	return true;
}

bool UGambitRoundFlowComponent::IsTransitionAllowed(const EGambitRoundPhase CurrentPhase, const EGambitRoundPhase NextPhase) const
{
	return FGambitRoundPhaseRules::IsTransitionAllowed(CurrentPhase, NextPhase);
}

void UGambitRoundFlowComponent::StartNextRound()
{
	AGambitGameState* GameState = GetGambitGameState();
	if (!GameState)
	{
		return;
	}

	GameState->SetCurrentRoundIndex(GameState->GetCurrentRoundIndex() + 1);
	ResetReadiness();
	RerollsUsedByPlayer.Reset();
	RerollLimitDeltaByPlayer.Reset();
	FirstRerolledDieInstanceByPlayer.Reset();
	RerollCountByDieInstanceByPlayer.Reset();

	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	for (AGambitPlayerState* PlayerState : Players)
	{
		if (PlayerState)
		{
			PlayerState->ResetRoundState();
			RerollsUsedByPlayer.Add(PlayerState, 0);
		}
	}

	UE_LOG(LogGambit, Log, TEXT("RoundFlow: === ROUND %d START ==="), GameState->GetCurrentRoundIndex());
	for (AGambitPlayerState* PlayerState : Players)
	{
		FGambitEffectExecutionContext Context = CreateEffectContext(EGambitEffectHook::RoundStart, PlayerState);
		ExecuteActiveEffectsAndCommit(Context);
		LogPlayerRoundSnapshot(PlayerState, Players);
	}

	TransitionToPhase(EGambitRoundPhase::Roll);
}

void UGambitRoundFlowComponent::EnterRollPhase()
{
	UE_LOG(LogGambit, Log, TEXT("RoundFlow: EnterRollPhase"));
	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	for (AGambitPlayerState* PlayerState : Players)
	{
		if (PlayerState)
		{
			FGambitEffectExecutionContext PreRollContext = CreateEffectContext(EGambitEffectHook::PreRoll, PlayerState);
			ExecuteActiveEffectsAndCommit(PreRollContext);

			PlayerState->RollAllDice(MatchRandomStream);

			FGambitEffectExecutionContext PostRollContext = CreateEffectContext(EGambitEffectHook::PostRoll, PlayerState);
			ExecuteActiveEffectsAndCommit(PostRollContext);

			UE_LOG(
				LogGambit,
				Log,
				TEXT("RoundFlow: %s Dice: %s"),
				*BuildRoundFlowPlayerLabel(PlayerState, Players),
				*FormatRoundFlowDiceValues(PlayerState->GetDiceStatesRef()));
		}
	}

	TransitionToPhase(EGambitRoundPhase::SelectionReroll);
}

void UGambitRoundFlowComponent::EnterSelectionRerollPhase()
{
	UE_LOG(LogGambit, Log, TEXT("RoundFlow: EnterSelectionRerollPhase waiting for player lock/reroll decisions or ready"));
	ResetReadiness();
}

void UGambitRoundFlowComponent::EnterActionPhase()
{
	UE_LOG(LogGambit, Log, TEXT("RoundFlow: EnterActionPhase waiting for consumables or ready"));
	ResetReadiness();
}

void UGambitRoundFlowComponent::EnterResolutionPhase()
{
	UE_LOG(LogGambit, Log, TEXT("RoundFlow: EnterResolutionPhase"));
	if (!DiceEvaluator || !ScoreCalculator)
	{
		UE_LOG(LogGambit, Log, TEXT("RoundFlow: EnterResolutionPhase failed, evaluator or score calculator is missing"));
		return;
	}

	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	for (AGambitPlayerState* PlayerState : Players)
	{
		if (!PlayerState)
		{
			continue;
		}

		FGambitEffectExecutionContext PreCombinationContext = CreateEffectContext(EGambitEffectHook::PreCombinationEvaluation, PlayerState);
		ExecuteActiveEffectsAndCommit(PreCombinationContext);

		const FGambitDiceCombinationResult CombinationResult = DiceEvaluator->EvaluateDice(PlayerState->GetDiceStatesRef());

		FGambitEffectExecutionContext PostCombinationContext = CreateEffectContext(EGambitEffectHook::PostCombinationEvaluation, PlayerState);
		PostCombinationContext.CurrentCombinationResult = CombinationResult;
		ExecuteActiveEffectsAndCommit(PostCombinationContext);

		FGambitEffectExecutionContext PreScoreContext = CreateEffectContext(EGambitEffectHook::PreScoreCalculation, PlayerState);
		PreScoreContext.CurrentCombinationResult = CombinationResult;
		ExecuteActiveEffectsAndCommit(PreScoreContext);

		const FGambitScoreModifierContext Modifier = BuildScoreModifierForResolution(PlayerState, CombinationResult);
		FGambitScoreBreakdown ScoreBreakdown = ScoreCalculator->CalculateScore(CombinationResult, Modifier);

		FGambitEffectExecutionContext PostScoreContext = CreateEffectContext(EGambitEffectHook::PostScoreCalculation, PlayerState);
		PostScoreContext.CurrentCombinationResult = CombinationResult;
		PostScoreContext.CurrentScoreModifier = Modifier;
		PostScoreContext.CurrentScoreBreakdown = ScoreBreakdown;
		ExecuteActiveEffectsAndCommit(PostScoreContext);
		ScoreBreakdown = PostScoreContext.CurrentScoreBreakdown;

		PlayerState->ApplyRoundScore(ScoreBreakdown);
		PlayerState->AddRoundEvent(MakeRoundFlowEvent(
			GetGambitGameState(),
			PlayerState,
			PlayerState,
			EGambitRoundGameplayEventType::RoundScored,
			EGambitRoundGameplayEventOutcome::Applied,
			static_cast<float>(ScoreBreakdown.FinalScore),
			FString::Printf(TEXT("Round score finalized at %d"), ScoreBreakdown.FinalScore)));
		RecordScoreBreakdownDebugLines(PlayerState, EGambitRoundPhase::Resolution, ScoreBreakdown);
		UE_LOG(
			LogGambit,
			Log,
			TEXT("RoundFlow: %s Resolution Combination=%s DiceSum=%d DiceContributionBonus=%.2f Base=%d Additive=%.2f Multiplier=%.2f BeforeCap=%.2f AfterCap=%.2f Final=%d"),
			*BuildRoundFlowPlayerLabel(PlayerState, Players),
			*RoundFlowCombinationToString(ScoreBreakdown.Combination),
			ScoreBreakdown.DiceSum,
			ScoreBreakdown.DiceContributionMultiplierBonus,
			ScoreBreakdown.BaseCombinationScore,
			ScoreBreakdown.AdditiveBonus,
			ScoreBreakdown.Multiplier,
			ScoreBreakdown.ScoreBeforeCap,
			ScoreBreakdown.ScoreAfterCap,
			ScoreBreakdown.FinalScore);
	}

	TransitionToPhase(EGambitRoundPhase::Reward);
}

void UGambitRoundFlowComponent::EnterRewardPhase()
{
	UE_LOG(LogGambit, Log, TEXT("RoundFlow: EnterRewardPhase"));
	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	for (AGambitPlayerState* PlayerState : Players)
	{
		if (!PlayerState)
		{
			continue;
		}

		const int32 BaseGoldReward = ComputeBaseGoldReward(PlayerState);
		const int32 Interest = PlayerState->ApplyRoundGoldReward(BaseGoldReward);
		FGambitEffectExecutionContext RewardContext = CreateEffectContext(EGambitEffectHook::Reward, PlayerState);
		RewardContext.CurrentScoreBreakdown = PlayerState->GetLastScoreBreakdown();
		RewardContext.BaseGoldReward = BaseGoldReward;
		RewardContext.InterestGoldReward = Interest;
		ExecuteActiveEffectsAndCommit(RewardContext);
		UE_LOG(
			LogGambit,
			Log,
			TEXT("RoundFlow: %s Reward BaseGold=%d Interest=%d GoldFinal=%d"),
			*BuildRoundFlowPlayerLabel(PlayerState, Players),
			BaseGoldReward,
			Interest,
			PlayerState->GetCurrentGold());
	}

	TransitionToPhase(EGambitRoundPhase::Ranking);
}

void UGambitRoundFlowComponent::EnterRankingPhase()
{
	UE_LOG(LogGambit, Log, TEXT("RoundFlow: EnterRankingPhase"));
	AGambitGameState* GameState = GetGambitGameState();
	if (!GameState)
	{
		return;
	}

	UGambitRankingComponent* RankingComponent = GameState->GetRankingComponent();
	if (!RankingComponent)
	{
		return;
	}

	TArray<FGambitRankingEntry> Ranking = RankingComponent->BuildRoundRanking(GetAllPlayers());
	for (const FGambitRankingEntry& Entry : Ranking)
	{
		if (AGambitPlayerState* RankedPlayer = Cast<AGambitPlayerState>(Entry.PlayerState))
		{
			FGambitEffectExecutionContext RankingContext = CreateEffectContext(EGambitEffectHook::Ranking, RankedPlayer);
			RankingContext.SourceRank = Entry.Rank;
			RankingContext.CurrentScoreBreakdown = RankedPlayer->GetLastScoreBreakdown();
			ExecuteActiveEffectsAndCommit(RankingContext);
		}
	}
	RankingComponent->ApplyVictoryPoints(Ranking);
	GameState->SetRoundRankingSnapshot(Ranking);

	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	for (const FGambitRankingEntry& Entry : Ranking)
	{
		const AGambitPlayerState* PlayerState = Cast<AGambitPlayerState>(Entry.PlayerState);
		UE_LOG(
			LogGambit,
			Log,
			TEXT("RoundFlow: Ranking Rank=%d Player=%s RoundScore=%d VPGained=%d TotalVP=%d"),
			Entry.Rank,
			*BuildRoundFlowPlayerLabel(PlayerState, Players),
			Entry.RoundScore,
			Entry.VictoryPointsGranted,
			PlayerState ? PlayerState->GetTotalVictoryPoints() : 0);
	}

	TransitionToPhase(EGambitRoundPhase::Shop);
}

void UGambitRoundFlowComponent::EnterShopPhase()
{
	UE_LOG(LogGambit, Log, TEXT("RoundFlow: EnterShopPhase"));
	AGambitGameState* GameState = GetGambitGameState();
	if (!GameState)
	{
		return;
	}

	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	for (AGambitPlayerState* PlayerState : Players)
	{
		if (PlayerState)
		{
			const int32 BaseOfferCount = PlayerState->GetShopComponent()
				? PlayerState->GetShopComponent()->GetConfiguredOfferCount()
				: UGambitGameBalanceSettings::Get()->ShopOfferCount;

			FGambitEffectExecutionContext PreShopGenerateContext = CreateEffectContext(EGambitEffectHook::PreShopGenerate, PlayerState);
			PreShopGenerateContext.ShopOfferCount = BaseOfferCount;
			ExecuteActiveEffectsAndCommit(PreShopGenerateContext);

			const int32 OfferCount = FMath::Max(0, PreShopGenerateContext.ShopOfferCount + PreShopGenerateContext.ShopOfferCountDelta);
			PlayerState->RefreshShopOffersWithCount(MatchRandomStream, GameState->GetSharedPoolComponent(), OfferCount);

			FGambitEffectExecutionContext PostShopGenerateContext = CreateEffectContext(EGambitEffectHook::PostShopGenerate, PlayerState);
			PostShopGenerateContext.ShopOfferCount = OfferCount;
			PostShopGenerateContext.GeneratedShopOffers = PlayerState->GetCurrentShopOffers();
			ExecuteActiveEffectsAndCommit(PostShopGenerateContext);
			if (UGambitShopComponent* ShopComponent = PlayerState->GetShopComponent())
			{
				ShopComponent->SetCurrentOffers(PostShopGenerateContext.GeneratedShopOffers);
			}

			const TArray<FGambitShopOffer>& Offers = PlayerState->GetCurrentShopOffersRef();
			if (Offers.Num() == 0)
			{
				UE_LOG(LogGambit, Log, TEXT("RoundFlow: %s ShopOffers=None"), *BuildRoundFlowPlayerLabel(PlayerState, Players));
			}

			for (const FGambitShopOffer& Offer : Offers)
			{
				PlayerState->AddDebugShopLine(BuildShopLineFromOffer(Offer, EGambitRoundPhase::Shop, GameState->GetSharedPoolComponent()));
				UE_LOG(LogGambit, Log, TEXT("RoundFlow: %s ShopOffer %s"), *BuildRoundFlowPlayerLabel(PlayerState, Players), *FormatRoundFlowOffer(Offer));
			}
		}
	}

	ResetReadiness();
}

void UGambitRoundFlowComponent::EnterRoundEndPhase()
{
	UE_LOG(LogGambit, Log, TEXT("RoundFlow: EnterRoundEndPhase"));
	const AGambitGameState* GameState = GetGambitGameState();
	if (!GameState)
	{
		return;
	}

	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	for (AGambitPlayerState* PlayerState : Players)
	{
		FGambitEffectExecutionContext RoundEndContext = CreateEffectContext(EGambitEffectHook::RoundEnd, PlayerState);
		RoundEndContext.CurrentScoreBreakdown = PlayerState ? PlayerState->GetLastScoreBreakdown() : FGambitScoreBreakdown();
		ExecuteActiveEffectsAndCommit(RoundEndContext);

		const int32 DestroyedDiceCount = PlayerState->CommitDestroyedDiceAfterRound();
		if (DestroyedDiceCount > 0)
		{
			UE_LOG(
				LogGambit,
				Log,
				TEXT("RoundFlow: %s DestroyedDiceCommitted=%d OwnedDiceRemaining=%d"),
				*BuildRoundFlowPlayerLabel(PlayerState, Players),
				DestroyedDiceCount,
				PlayerState->GetSlotState().OwnedDiceCount);
		}
	}

	const UGambitGameBalanceSettings* Settings = UGambitGameBalanceSettings::Get();
	if (GameState->GetCurrentRoundIndex() >= Settings->RoundCount)
	{
		TArray<AGambitPlayerState*> FinalRanking = Players;
		FinalRanking.Sort([](const AGambitPlayerState& A, const AGambitPlayerState& B)
		{
			if (A.GetTotalVictoryPoints() == B.GetTotalVictoryPoints())
			{
				return A.GetCurrentRoundScore() > B.GetCurrentRoundScore();
			}

			return A.GetTotalVictoryPoints() > B.GetTotalVictoryPoints();
		});

		if (FinalRanking.Num() > 0 && FinalRanking[0])
		{
			UE_LOG(LogGambit, Log, TEXT("RoundFlow: Match complete. Winner=%s VP=%d"), *BuildRoundFlowPlayerLabel(FinalRanking[0], Players), FinalRanking[0]->GetTotalVictoryPoints());
		}

		for (int32 Index = 0; Index < FinalRanking.Num(); ++Index)
		{
			const AGambitPlayerState* PlayerState = FinalRanking[Index];
			UE_LOG(
				LogGambit,
				Log,
				TEXT("RoundFlow: FinalRank=%d Player=%s TotalVP=%d LastRoundScore=%d Gold=%d"),
				Index + 1,
				*BuildRoundFlowPlayerLabel(PlayerState, Players),
				PlayerState ? PlayerState->GetTotalVictoryPoints() : 0,
				PlayerState ? PlayerState->GetCurrentRoundScore() : 0,
				PlayerState ? PlayerState->GetCurrentGold() : 0);
		}

		TransitionToPhase(EGambitRoundPhase::None);
		return;
	}

	UE_LOG(
		LogGambit,
		Log,
		TEXT("RoundFlow: Preparing next round %d of %d"),
		GameState->GetCurrentRoundIndex() + 1,
		Settings->RoundCount);
	StartNextRound();
}

void UGambitRoundFlowComponent::AdvanceWhenAllPlayersReady()
{
	AGambitGameState* GameState = GetGambitGameState();
	if (!GameState || !AreAllPlayersReady())
	{
		return;
	}

	switch (GameState->GetCurrentPhase())
	{
	case EGambitRoundPhase::SelectionReroll:
		TransitionToPhase(EGambitRoundPhase::Action);
		break;
	case EGambitRoundPhase::Action:
		TransitionToPhase(EGambitRoundPhase::Resolution);
		break;
	case EGambitRoundPhase::Shop:
		TransitionToPhase(EGambitRoundPhase::RoundEnd);
		break;
	default:
		break;
	}
}

bool UGambitRoundFlowComponent::AreAllPlayersReady() const
{
	for (AGambitPlayerState* PlayerState : GetAllPlayers())
	{
		const bool* bReady = PlayerReadyMap.Find(PlayerState);
		if (!bReady || !(*bReady))
		{
			return false;
		}
	}

	return true;
}

void UGambitRoundFlowComponent::ResetReadiness()
{
	PlayerReadyMap.Reset();
	for (AGambitPlayerState* PlayerState : GetAllPlayers())
	{
		PlayerReadyMap.Add(PlayerState, false);
	}
}

FGambitRoundEffectContextRequest UGambitRoundFlowComponent::BuildEffectContextRequest(
	const EGambitEffectHook Hook,
	AGambitPlayerState* SourcePlayer,
	AGambitPlayerState* TargetPlayer) const
{
	const AGambitGameState* GameState = GetGambitGameState();
	AGambitPlayerState* EffectiveTarget = TargetPlayer ? TargetPlayer : SourcePlayer;

	FGambitRoundEffectContextRequest Request;
	Request.Hook = Hook;
	Request.CurrentPhase = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	Request.RoundNumber = GameState ? GameState->GetCurrentRoundIndex() : 0;
	Request.SourcePlayer = SourcePlayer;
	Request.TargetPlayer = TargetPlayer ? TargetPlayer : SourcePlayer;
	Request.SharedPoolComponent = GameState ? GameState->GetSharedPoolComponent() : nullptr;
	Request.MatchPlayers = GetAllPlayers();
	Request.RandomStream = MatchRandomStream;
	Request.RerollsUsed = GetRerollCount(SourcePlayer);
	Request.RerollLimit = GetEffectiveRerollLimit(SourcePlayer);
	Request.SourceRank = GetRankForPlayer(SourcePlayer);
	Request.TargetRank = GetRankForPlayer(EffectiveTarget);
	Request.FirstRerolledDieHandIndexThisRound = GetFirstRerolledDieHandIndex(SourcePlayer);
	Request.MaxRerollCountForAnyDieThisRound = GetMaxRerollCountForAnyDie(SourcePlayer);
	return Request;
}

FGambitRoundEffectCommitRequest UGambitRoundFlowComponent::BuildEffectCommitRequest()
{
	FGambitRoundEffectCommitRequest Request;
	Request.MatchRandomStream = &MatchRandomStream;
	Request.RerollLimitDeltaByPlayer = &RerollLimitDeltaByPlayer;
	return Request;
}

FGambitEffectExecutionContext UGambitRoundFlowComponent::CreateEffectContext(
	const EGambitEffectHook Hook,
	AGambitPlayerState* SourcePlayer,
	AGambitPlayerState* TargetPlayer) const
{
	return EffectPipeline
		? EffectPipeline->MakeEffectContext(BuildEffectContextRequest(Hook, SourcePlayer, TargetPlayer))
		: FGambitEffectExecutionContext();
}

void UGambitRoundFlowComponent::ExecuteActiveEffectsAndCommit(FGambitEffectExecutionContext& Context)
{
	if (!EffectPipeline)
	{
		return;
	}

	EffectPipeline->ExecuteActiveSourceEffects(Context);
	EffectPipeline->CommitEffectContext(Context, BuildEffectCommitRequest());
}

FGambitScoreModifierContext UGambitRoundFlowComponent::BuildScoreModifierForResolution(
	AGambitPlayerState* PlayerState,
	const FGambitDiceCombinationResult& CombinationResult)
{
	if (EffectPipeline)
	{
		FGambitRoundScoreModifierEffectRequest Request;
		Request.ContextRequest = BuildEffectContextRequest(EGambitEffectHook::ScoreModifier, PlayerState);
		Request.CommitRequest = BuildEffectCommitRequest();
		Request.CombinationResult = CombinationResult;
		return EffectPipeline->BuildScoreModifierFromEffects(Request).ScoreModifier;
	}

	FGambitScoreModifierContext Modifier = PlayerState
		? PlayerState->GetTemporaryScoreModifier()
		: FGambitScoreModifierMath::MakeNeutral();
	return FGambitScoreModifierMath::Normalize(Modifier);
}

int32 UGambitRoundFlowComponent::GetRerollCount(AGambitPlayerState* PlayerState) const
{
	if (const int32* Count = RerollsUsedByPlayer.Find(PlayerState))
	{
		return *Count;
	}

	return 0;
}

void UGambitRoundFlowComponent::IncrementRerollCount(AGambitPlayerState* PlayerState)
{
	int32& Count = RerollsUsedByPlayer.FindOrAdd(PlayerState);
	Count++;
}

TArray<FGambitRerollDieDelta> UGambitRoundFlowComponent::BuildRerollDeltas(
	const TArray<FGambitDieRuntimeState>& DiceBefore,
	const TArray<FGambitDieRuntimeState>& DiceAfter) const
{
	TArray<FGambitRerollDieDelta> Deltas;
	for (int32 Index = 0; Index < DiceBefore.Num(); ++Index)
	{
		const FGambitDieRuntimeState& BeforeState = DiceBefore[Index];
		if (BeforeState.bLocked || !BeforeState.bCanBeRerolled || !DiceAfter.IsValidIndex(Index))
		{
			continue;
		}

		const FGambitDieRuntimeState& AfterState = DiceAfter[Index];
		if (AfterState.InstanceId != BeforeState.InstanceId)
		{
			continue;
		}

		FGambitRerollDieDelta Delta;
		Delta.DieInstanceId = BeforeState.InstanceId;
		Delta.HandIndex = AfterState.HandIndex;
		Delta.ValueBefore = BeforeState.EffectiveValue;
		Delta.ValueAfter = AfterState.EffectiveValue;
		Deltas.Add(Delta);
	}

	return Deltas;
}

void UGambitRoundFlowComponent::TrackRerollDeltas(
	AGambitPlayerState* PlayerState,
	TArray<FGambitRerollDieDelta>& RerollDeltas)
{
	if (!PlayerState || RerollDeltas.Num() == 0)
	{
		return;
	}

	TMap<int32, int32>& CountsByDieInstance = RerollCountByDieInstanceByPlayer.FindOrAdd(PlayerState);
	if (!FirstRerolledDieInstanceByPlayer.Contains(PlayerState))
	{
		FirstRerolledDieInstanceByPlayer.Add(PlayerState, INDEX_NONE);
	}
	int32& FirstRerolledDieInstance = FirstRerolledDieInstanceByPlayer.FindChecked(PlayerState);

	for (FGambitRerollDieDelta& Delta : RerollDeltas)
	{
		if (Delta.DieInstanceId == INDEX_NONE)
		{
			continue;
		}

		if (FirstRerolledDieInstance == INDEX_NONE)
		{
			FirstRerolledDieInstance = Delta.DieInstanceId;
		}

		int32& CountForDie = CountsByDieInstance.FindOrAdd(Delta.DieInstanceId);
		CountForDie++;
		Delta.RerollCountForDieThisRound = CountForDie;
		Delta.bFirstRerolledDieThisRound = Delta.DieInstanceId == FirstRerolledDieInstance;
	}
}

int32 UGambitRoundFlowComponent::GetFirstRerolledDieHandIndex(AGambitPlayerState* PlayerState) const
{
	if (!PlayerState)
	{
		return INDEX_NONE;
	}

	const int32* FirstRerolledDieInstance = FirstRerolledDieInstanceByPlayer.Find(PlayerState);
	if (!FirstRerolledDieInstance || *FirstRerolledDieInstance == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	const TArray<FGambitDieRuntimeState>& DiceStates = PlayerState->GetDiceStatesRef();
	for (const FGambitDieRuntimeState& DieState : DiceStates)
	{
		if (DieState.InstanceId == *FirstRerolledDieInstance)
		{
			return DieState.HandIndex;
		}
	}

	return INDEX_NONE;
}

int32 UGambitRoundFlowComponent::GetMaxRerollCountForAnyDie(AGambitPlayerState* PlayerState) const
{
	const TMap<int32, int32>* CountsByDieInstance = RerollCountByDieInstanceByPlayer.Find(PlayerState);
	if (!CountsByDieInstance)
	{
		return 0;
	}

	int32 MaxCount = 0;
	for (const TPair<int32, int32>& Pair : *CountsByDieInstance)
	{
		MaxCount = FMath::Max(MaxCount, Pair.Value);
	}

	return MaxCount;
}

int32 UGambitRoundFlowComponent::GetEffectiveRerollLimit(AGambitPlayerState* PlayerState) const
{
	const int32 BaseLimit = UGambitGameBalanceSettings::Get()->MaxRerollsPerRound;
	const int32* Delta = RerollLimitDeltaByPlayer.Find(PlayerState);
	return FMath::Max(0, BaseLimit + (Delta ? *Delta : 0));
}

int32 UGambitRoundFlowComponent::GetRankForPlayer(AGambitPlayerState* PlayerState) const
{
	const AGambitGameState* GameState = GetGambitGameState();
	if (!GameState || !PlayerState)
	{
		return INDEX_NONE;
	}

	for (const FGambitRankingEntry& Entry : GameState->GetRoundRankingSnapshotRef())
	{
		if (Entry.PlayerState == PlayerState)
		{
			return Entry.Rank;
		}
	}

	return INDEX_NONE;
}

int32 UGambitRoundFlowComponent::ComputeBaseGoldReward(const AGambitPlayerState* PlayerState) const
{
	if (!PlayerState)
	{
		return 0;
	}

	const int32 Score = PlayerState->GetCurrentRoundScore();
	return UGambitGameBalanceSettings::Get()->GetRoundGoldRewardFromScore(Score);
}

AGambitGameState* UGambitRoundFlowComponent::GetGambitGameState() const
{
	const AGameModeBase* GameMode = Cast<AGameModeBase>(GetOwner());
	return GameMode ? GameMode->GetGameState<AGambitGameState>() : nullptr;
}

TArray<AGambitPlayerState*> UGambitRoundFlowComponent::GetAllPlayers() const
{
	if (const AGambitGameState* GameState = GetGambitGameState())
	{
		return GameState->GetGambitPlayerStates();
	}

	return {};
}
