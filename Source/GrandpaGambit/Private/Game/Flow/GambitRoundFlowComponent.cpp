#include "Game/Flow/GambitRoundFlowComponent.h"

#include "Core/Logging/GambitLog.h"
#include "Core/Interfaces/GambitDiceEvaluatorContract.h"
#include "Core/Interfaces/GambitScoreCalculatorContract.h"
#include "Core/Settings/GambitGameBalanceSettings.h"
#include "Core/Types/GambitRoundFeedbackTypes.h"
#include "Core/Types/GambitRoundPhaseRules.h"
#include "Data/Assets/GambitItemEffectDefinition.h"
#include "Dice/Data/GambitDiceDefinition.h"
#include "Dice/Evaluation/GambitDiceCombinationEvaluator.h"
#include "Game/Flow/GambitRoundEffectPipeline.h"
#include "Game/Flow/GambitTargetSelectionService.h"
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
	FString RoundFlowEnumToString(const TEnum Value)
	{
		if (const UEnum* Enum = StaticEnum<TEnum>())
		{
			return Enum->GetNameStringByValue(static_cast<int64>(Value));
		}

		return TEXT("Unknown");
	}

	FGambitScoreBreakdownLine MakeRoundScoreLine(
		const EGambitScoreBreakdownLineType LineType,
		const EGambitRoundPhase Phase,
		const FString& Label,
		const float AdditiveDelta,
		const float DiceContributionDelta,
		const float MultiplierValue,
		const float ScoreBefore,
		const float ScoreAfter,
		const FString& Summary)
	{
		FGambitScoreBreakdownLine Line;
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

	void RecordScoreBreakdownPresentationLines(
		AGambitPlayerState* PlayerState,
		const EGambitRoundPhase Phase,
		const FGambitScoreBreakdown& ScoreBreakdown)
	{
		if (!PlayerState)
		{
			return;
		}

		PlayerState->AddScoreBreakdownLine(MakeRoundScoreLine(
			EGambitScoreBreakdownLineType::BaseCombination,
			Phase,
			TEXT("Combination base"),
			static_cast<float>(ScoreBreakdown.BaseCombinationScore),
			0.0f,
			1.0f,
			0.0f,
			static_cast<float>(ScoreBreakdown.BaseCombinationScore),
			FString::Printf(TEXT("%s base score: %d"), *RoundFlowCombinationToString(ScoreBreakdown.Combination), ScoreBreakdown.BaseCombinationScore)));

		PlayerState->AddScoreBreakdownLine(MakeRoundScoreLine(
			EGambitScoreBreakdownLineType::DiceSum,
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
			PlayerState->AddScoreBreakdownLine(MakeRoundScoreLine(
				EGambitScoreBreakdownLineType::DiceContribution,
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
			PlayerState->AddScoreBreakdownLine(MakeRoundScoreLine(
				EGambitScoreBreakdownLineType::Additive,
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
			PlayerState->AddScoreBreakdownLine(MakeRoundScoreLine(
				EGambitScoreBreakdownLineType::Multiplier,
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
			PlayerState->AddScoreBreakdownLine(MakeRoundScoreLine(
				EGambitScoreBreakdownLineType::Cap,
				Phase,
				TEXT("Score cap or diminishing"),
				0.0f,
				0.0f,
				1.0f,
				ScoreBreakdown.ScoreBeforeCap,
				ScoreBreakdown.ScoreAfterCap,
				FString::Printf(TEXT("Score adjusted from %0.2f to %0.2f by cap/diminishing"), ScoreBreakdown.ScoreBeforeCap, ScoreBreakdown.ScoreAfterCap)));
		}

		PlayerState->AddScoreBreakdownLine(MakeRoundScoreLine(
			EGambitScoreBreakdownLineType::FinalScore,
			Phase,
			TEXT("Final score"),
			0.0f,
			0.0f,
			1.0f,
			ScoreBreakdown.ScoreAfterCap,
			static_cast<float>(ScoreBreakdown.FinalScore),
			FString::Printf(TEXT("Final rounded score: %d"), ScoreBreakdown.FinalScore)));
	}

	FGambitShopBreakdownLine BuildShopLineFromOffer(
		const FGambitShopOffer& Offer,
		const EGambitRoundPhase Phase,
		const UGambitSharedPoolComponent* SharedPoolComponent)
	{
		FGambitShopBreakdownLine Line;
		Line.LineType = EGambitShopBreakdownLineType::GeneratedOffer;
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
				Line.SharedPoolState = RoundFlowEnumToString(Availability.State);
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

	FGambitShopBreakdownLine BuildShopLineFromPurchaseContext(
		const FGambitShopPurchaseContext& PurchaseContext,
		const EGambitRoundPhase Phase,
		const EGambitShopBreakdownLineType LineType)
	{
		FGambitShopBreakdownLine Line;
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
		Line.SharedPoolState = RoundFlowEnumToString(PurchaseContext.SharedPoolAvailability.State);
		Line.SharedPoolReason = PurchaseContext.SharedPoolAvailability.Reason;
		Line.bPurchaseSucceeded = PurchaseContext.bPurchaseSucceeded;
		Line.bRefused = !PurchaseContext.bPurchaseSucceeded;
		Line.FailureReason = PurchaseContext.FailureReason;
		Line.PresentationLines = PurchaseContext.PresentationLines;

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

	EGambitRoundCommandStatus ResolvePurchaseFailureStatus(const FGambitShopPurchaseContext& PurchaseContext)
	{
		if (PurchaseContext.bBlockedByEffect)
		{
			return EGambitRoundCommandStatus::PurchaseBlocked;
		}

		if (!PurchaseContext.ItemDefinition || PurchaseContext.FailureReason.Contains(TEXT("not found")))
		{
			return EGambitRoundCommandStatus::InvalidShopOffer;
		}

		if (PurchaseContext.FailureReason.Contains(TEXT("Not enough gold")))
		{
			return EGambitRoundCommandStatus::NotEnoughGold;
		}

		if (PurchaseContext.bUsesSharedPool
			&& !PurchaseContext.bHasSharedPoolReservation
			&& !PurchaseContext.SharedPoolAvailability.bAvailable)
		{
			return EGambitRoundCommandStatus::SharedPoolUnavailable;
		}

		if (PurchaseContext.FailureReason.Contains(TEXT("shared pool"), ESearchCase::IgnoreCase)
			|| PurchaseContext.FailureReason.Contains(TEXT("stock"), ESearchCase::IgnoreCase)
			|| PurchaseContext.FailureReason.Contains(TEXT("reserved"), ESearchCase::IgnoreCase)
			|| PurchaseContext.FailureReason.Contains(TEXT("purchase limit"), ESearchCase::IgnoreCase))
		{
			return EGambitRoundCommandStatus::SharedPoolUnavailable;
		}

		if (PurchaseContext.FailureReason.Contains(TEXT("slot"), ESearchCase::IgnoreCase)
			|| PurchaseContext.FailureReason.Contains(TEXT("inventory"), ESearchCase::IgnoreCase)
			|| PurchaseContext.FailureReason.Contains(TEXT("dice item"), ESearchCase::IgnoreCase))
		{
			return EGambitRoundCommandStatus::InventoryFull;
		}

		return EGambitRoundCommandStatus::Failed;
	}

	FGambitShopOfferSnapshot BuildOfferSnapshotFromContext(
		const FGambitShopOffer& Offer,
		const FGambitShopPurchaseContext& PurchaseContext,
		const AGambitPlayerState* PlayerState,
		const EGambitRoundPhase CurrentPhase,
		const int32 PurchasesMadeThisShop)
	{
		FGambitShopOfferSnapshot Snapshot;
		Snapshot.OfferId = Offer.OfferId;
		Snapshot.ItemDefinition = Offer.ItemDefinition;
		Snapshot.BasePrice = Offer.BasePrice;
		Snapshot.ResolvedPrice = PurchaseContext.ItemDefinition ? PurchaseContext.ResolvedPrice : Offer.Price;
		Snapshot.CurrentGold = PlayerState ? PlayerState->GetCurrentGold() : 0;
		Snapshot.SlotState = PlayerState ? PlayerState->GetSlotState() : FGambitPlayerSlotState();
		Snapshot.PurchasesMadeThisShop = PurchasesMadeThisShop;
		Snapshot.bUsesSharedPool = PurchaseContext.ItemDefinition ? PurchaseContext.bUsesSharedPool : Offer.bUsesSharedPool;
		Snapshot.bHasSharedPoolReservation = PurchaseContext.ItemDefinition ? PurchaseContext.bHasSharedPoolReservation : Offer.bHasSharedPoolReservation;
		Snapshot.SharedPoolAvailability = PurchaseContext.SharedPoolAvailability;

		const UGambitItemDefinition* ItemDefinition = PurchaseContext.ItemDefinition
			? PurchaseContext.ItemDefinition.Get()
			: Offer.ItemDefinition.Get();
		if (ItemDefinition)
		{
			Snapshot.ItemId = ItemDefinition->GetResolvedItemId();
			Snapshot.ItemName = FormatRoundFlowItemName(ItemDefinition);
			Snapshot.ItemType = ItemDefinition->ItemType;
			Snapshot.Rarity = ItemDefinition->Rarity;
		}

		if (CurrentPhase != EGambitRoundPhase::Shop)
		{
			Snapshot.bCanPurchase = false;
			Snapshot.UnavailableReason = FString::Printf(
				TEXT("Current phase is %s, expected Shop"),
				*RoundFlowPhaseToString(CurrentPhase));
		}
		else if (PurchasesMadeThisShop > 0)
		{
			Snapshot.bCanPurchase = false;
			Snapshot.UnavailableReason = TEXT("Purchase already made this shop");
		}
		else
		{
			Snapshot.bCanPurchase = PurchaseContext.bCanPurchase;
			Snapshot.UnavailableReason = PurchaseContext.bCanPurchase
				? FString()
				: PurchaseContext.FailureReason;
		}

		return Snapshot;
	}
}

UGambitRoundFlowComponent::UGambitRoundFlowComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	DiceEvaluatorClass = UGambitDiceCombinationEvaluator::StaticClass();
	ScoreCalculatorClass = UGambitScoreCalculator::StaticClass();
	const UGambitGameBalanceSettings* Settings = UGambitGameBalanceSettings::Get();
	ActiveMatchSetup.LocalPlayerCount = Settings->MinLocalPlayers;
	ActiveMatchSetup.RoundCount = Settings->RoundCount;
}

void UGambitRoundFlowComponent::BeginPlay()
{
	Super::BeginPlay();

	MatchRandomStream.Initialize(RandomSeed);
	EnsureRuntimeServices();
}

void UGambitRoundFlowComponent::EnsureRuntimeServices()
{
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

	if (!TargetSelectionService)
	{
		TargetSelectionService = NewObject<UGambitTargetSelectionService>(this);
	}
}

void UGambitRoundFlowComponent::ApplyMatchSetup(const FGambitMatchSetupConfig& NewMatchSetup)
{
	const UGambitGameBalanceSettings* Settings = UGambitGameBalanceSettings::Get();
	ActiveMatchSetup.LocalPlayerCount = Settings->ClampLocalPlayerCount(NewMatchSetup.LocalPlayerCount);
	ActiveMatchSetup.RoundCount = FMath::Max(1, NewMatchSetup.RoundCount);
}

void UGambitRoundFlowComponent::StartMatchFlow()
{
	EnsureRuntimeServices();

	AGambitGameState* GameState = GetGambitGameState();
	if (!GameState)
	{
		UE_LOG(LogGambit, Warning, TEXT("RoundFlow: missing GambitGameState"));
		return;
	}

	const UGambitGameBalanceSettings* Settings = UGambitGameBalanceSettings::Get();
	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	if (ActiveMatchSetup.RoundCount <= 0)
	{
		ActiveMatchSetup.RoundCount = Settings->RoundCount;
	}
	if (ActiveMatchSetup.LocalPlayerCount <= 0)
	{
		ActiveMatchSetup.LocalPlayerCount = Settings->ClampLocalPlayerCount(Players.Num());
	}
	UE_LOG(
		LogGambit,
		Log,
		TEXT("RoundFlow: StartMatchFlow Players=%d RoundCount=%d DefaultDiceCount=%d MaxRerolls=%d ShopOfferCount=%d"),
		Players.Num(),
		ActiveMatchSetup.RoundCount,
		Settings->DefaultActiveDiceCount,
		Settings->MaxRerollsPerRound,
		Settings->ShopOfferCount);

	GameState->SetCurrentRoundIndex(0);
	GameState->SetCurrentPhase(EGambitRoundPhase::None);
	GameState->SetMatchSetupConfig(ActiveMatchSetup);
	GameState->SetFinalRankingSnapshot({});

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
	return RequestSetDieLockedDetailed(PlayerState, DieIndex, bLocked).bSuccess;
}

FGambitRoundCommandResult UGambitRoundFlowComponent::RequestSetDieLockedDetailed(
	AGambitPlayerState* PlayerState,
	const int32 DieIndex,
	const bool bLocked)
{
	AGambitGameState* GameState = GetGambitGameState();
	const EGambitRoundPhase CurrentPhase = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	FGambitRoundCommandResult Result;

	if (!GameState)
	{
		Result = MakeCommandResult(
			PlayerState,
			EGambitRoundCommandStatus::MissingGameState,
			false,
			TEXT("Lock refused: missing game state."),
			DieIndex);
	}
	else if (!PlayerState)
	{
		Result = MakeCommandResult(
			PlayerState,
			EGambitRoundCommandStatus::InvalidPlayer,
			false,
			TEXT("Lock refused: invalid player."),
			DieIndex);
	}
	else if (CurrentPhase != EGambitRoundPhase::SelectionReroll)
	{
		Result = MakeCommandResult(
			PlayerState,
			EGambitRoundCommandStatus::InvalidPhase,
			false,
			FString::Printf(
				TEXT("Lock refused: current phase is %s, expected Selection / Reroll."),
				*RoundFlowPhaseToString(CurrentPhase)),
			DieIndex);
	}
	else
	{
		const TArray<FGambitDieRuntimeState>& DiceStates = PlayerState->GetDiceStatesRef();
		if (!DiceStates.IsValidIndex(DieIndex))
		{
			Result = MakeCommandResult(
				PlayerState,
				EGambitRoundCommandStatus::InvalidDie,
				false,
				FString::Printf(TEXT("Lock refused: die %d does not exist."), DieIndex + 1),
				DieIndex);
		}
		else if (bLocked && !DiceStates[DieIndex].bCanBeLocked)
		{
			Result = MakeCommandResult(
				PlayerState,
				EGambitRoundCommandStatus::DieCannotBeLocked,
				false,
				FString::Printf(TEXT("Lock refused: die %d cannot be locked."), DieIndex + 1),
				DieIndex);
		}
		else
		{
			const bool bSuccess = PlayerState->TrySetDieLocked(DieIndex, bLocked);
			Result = MakeCommandResult(
				PlayerState,
				bSuccess ? EGambitRoundCommandStatus::Success : EGambitRoundCommandStatus::Failed,
				bSuccess,
				bSuccess
					? FString::Printf(TEXT("Die %d %s."), DieIndex + 1, bLocked ? TEXT("locked") : TEXT("unlocked"))
					: FString::Printf(TEXT("Lock refused: die %d could not be updated."), DieIndex + 1),
				DieIndex);
		}
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
		Result.bSuccess ? TEXT("Success") : TEXT("Failure"),
		*DiceString);
	return Result;
}

bool UGambitRoundFlowComponent::RequestReroll(AGambitPlayerState* PlayerState)
{
	return RequestRerollDetailed(PlayerState).bSuccess;
}

FGambitRoundCommandResult UGambitRoundFlowComponent::RequestRerollDetailed(AGambitPlayerState* PlayerState)
{
	AGambitGameState* GameState = GetGambitGameState();
	const EGambitRoundPhase CurrentPhase = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	const int32 UsedBefore = GetRerollCount(PlayerState);
	int32 EffectiveRerollLimit = GetEffectiveRerollLimit(PlayerState);
	FGambitRoundCommandResult Result;

	if (!GameState)
	{
		Result = MakeCommandResult(
			PlayerState,
			EGambitRoundCommandStatus::MissingGameState,
			false,
			TEXT("Reroll refused: missing game state."));
	}
	else if (!PlayerState)
	{
		Result = MakeCommandResult(
			PlayerState,
			EGambitRoundCommandStatus::InvalidPlayer,
			false,
			TEXT("Reroll refused: invalid player."));
	}
	else if (CurrentPhase != EGambitRoundPhase::SelectionReroll)
	{
		Result = MakeCommandResult(
			PlayerState,
			EGambitRoundCommandStatus::InvalidPhase,
			false,
			FString::Printf(
				TEXT("Reroll refused: current phase is %s, expected Selection / Reroll."),
				*RoundFlowPhaseToString(CurrentPhase)));
	}
	else
	{
		FGambitEffectExecutionContext PreRerollContext = CreateEffectContext(EGambitEffectHook::PreReroll, PlayerState);
		ExecuteActiveEffectsAndCommit(PreRerollContext);
		EffectiveRerollLimit = GetEffectiveRerollLimit(PlayerState);

		if (UsedBefore >= EffectiveRerollLimit)
		{
			Result = MakeCommandResult(
				PlayerState,
				EGambitRoundCommandStatus::RerollLimitReached,
				false,
				FString::Printf(TEXT("Reroll refused: limit reached (%d/%d)."), UsedBefore, EffectiveRerollLimit));
		}
		else if (!HasUnlockedRerollableDice(PlayerState))
		{
			Result = MakeCommandResult(
				PlayerState,
				EGambitRoundCommandStatus::NoUnlockedDice,
				false,
				TEXT("Reroll refused: no unlocked rerollable dice."));
		}
		else
		{
			const TArray<FGambitDieRuntimeState> DiceBefore = PlayerState->GetDiceStates();
			if (PlayerState->TryRerollUnlockedDice(MatchRandomStream))
			{
				TArray<FGambitRerollDieDelta> RerollDeltas = BuildRerollDeltas(DiceBefore, PlayerState->GetDiceStatesRef());
				TrackRerollDeltas(PlayerState, RerollDeltas);
				IncrementRerollCount(PlayerState);

				FGambitEffectExecutionContext PostRerollContext = CreateEffectContext(EGambitEffectHook::PostReroll, PlayerState);
				PostRerollContext.RerollDeltas = RerollDeltas;
				ExecuteActiveEffectsAndCommit(PostRerollContext);
				EffectiveRerollLimit = GetEffectiveRerollLimit(PlayerState);

				Result = MakeCommandResult(
					PlayerState,
					EGambitRoundCommandStatus::Success,
					true,
					FString::Printf(TEXT("Reroll used %d/%d."), GetRerollCount(PlayerState), EffectiveRerollLimit));
			}
			else
			{
				Result = MakeCommandResult(
					PlayerState,
					EGambitRoundCommandStatus::Failed,
					false,
					TEXT("Reroll refused: unlocked dice could not be rerolled."));
			}
		}
	}

	const int32 UsedAfter = GetRerollCount(PlayerState);
	Result.RerollsUsed = UsedAfter;
	Result.RerollLimit = EffectiveRerollLimit;
	if (Result.bSuccess && PlayerState)
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
		Result.bSuccess ? TEXT("Success") : TEXT("Failure"),
		*DiceString);
	return Result;
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
	EnsureRuntimeServices();

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

bool UGambitRoundFlowComponent::BuildConsumableTargetSelectionRequest(
	AGambitPlayerState* PlayerState,
	const int32 SlotIndex,
	FGambitTargetSelectionRequest& OutRequest) const
{
	if (!TargetSelectionService)
	{
		OutRequest = FGambitTargetSelectionRequest();
		OutRequest.InvalidReason = TEXT("Target selection failed: missing target selection service.");
		return false;
	}

	const AGambitGameState* GameState = GetGambitGameState();
	const EGambitRoundPhase CurrentPhase = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	const FGambitEffectExecutionContext BaseContext = CreateEffectContext(EGambitEffectHook::ConsumableUse, PlayerState, PlayerState);
	return TargetSelectionService->BuildConsumableTargetSelectionRequest(
		PlayerState,
		SlotIndex,
		GetAllPlayers(),
		CurrentPhase,
		BaseContext,
		OutRequest);
}

bool UGambitRoundFlowComponent::RequestUseConsumableWithTargetSelectionResult(
	AGambitPlayerState* PlayerState,
	const FGambitTargetSelectionResult& TargetSelectionResult)
{
	if (!PlayerState || !TargetSelectionResult.bConfirmed)
	{
		return false;
	}

	return RequestUseConsumableOnTargetSelectedDie(
		PlayerState,
		TargetSelectionResult.SourceConsumableSlotIndex,
		TargetSelectionResult.TargetPlayer ? TargetSelectionResult.TargetPlayer.Get() : PlayerState,
		TargetSelectionResult.TargetDieHandIndex);
}

bool UGambitRoundFlowComponent::RequestPurchaseOffer(AGambitPlayerState* PlayerState, const int32 OfferId)
{
	return RequestPurchaseOfferDetailed(PlayerState, OfferId).bSuccess;
}

FGambitRoundCommandResult UGambitRoundFlowComponent::RequestPurchaseOfferDetailed(
	AGambitPlayerState* PlayerState,
	const int32 OfferId)
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

	FGambitRoundCommandResult Result;
	if (!GameState)
	{
		Result = MakeCommandResult(
			PlayerState,
			EGambitRoundCommandStatus::MissingGameState,
			false,
			TEXT("Purchase refused: missing game state."));
	}
	else if (!PlayerState)
	{
		Result = MakeCommandResult(
			PlayerState,
			EGambitRoundCommandStatus::InvalidPlayer,
			false,
			TEXT("Purchase refused: invalid player."));
	}
	else if (CurrentPhase != EGambitRoundPhase::Shop)
	{
		Result = MakeCommandResult(
			PlayerState,
			EGambitRoundCommandStatus::InvalidPhase,
			false,
			FString::Printf(
				TEXT("Purchase refused: current phase is %s, expected Shop."),
				*RoundFlowPhaseToString(CurrentPhase)));
	}
	else if (!PlayerState->GetShopComponent())
	{
		Result = MakeCommandResult(
			PlayerState,
			EGambitRoundCommandStatus::Failed,
			false,
			TEXT("Purchase refused: missing shop component."));
	}
	else if (PlayerState->GetShopComponent()->GetPurchasesMadeThisShop() > 0)
	{
		Result = MakeCommandResult(
			PlayerState,
			EGambitRoundCommandStatus::PurchaseAlreadyMade,
			false,
			TEXT("Purchase refused: this player already bought an offer this shop."));
	}
	else if (!bFoundOffer)
	{
		Result = MakeCommandResult(
			PlayerState,
			EGambitRoundCommandStatus::InvalidShopOffer,
			false,
			FString::Printf(TEXT("Purchase refused: offer %d was not found."), OfferId));
	}
	else
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
		const bool bSuccess = PlayerState->TryPurchaseOfferWithContext(PurchaseContext, SharedPoolComponent);
		const int32 GoldAfterPurchase = PlayerState->GetCurrentGold();
		if (bSuccess && PurchaseContext.ItemDefinition)
		{
			FGambitGoldBreakdownLine PurchaseGoldLine;
			PurchaseGoldLine.LineType = EGambitGoldBreakdownLineType::Purchase;
			PurchaseGoldLine.Phase = CurrentPhase;
			PurchaseGoldLine.SourceId = PurchaseContext.ItemDefinition->GetResolvedItemId();
			PurchaseGoldLine.SourceName = FormatRoundFlowItemName(PurchaseContext.ItemDefinition.Get());
			PurchaseGoldLine.RequestedDelta = -PurchaseContext.ResolvedPrice;
			PurchaseGoldLine.ActualDelta = GoldAfterPurchase - GoldBeforePurchase;
			PurchaseGoldLine.GoldBefore = GoldBeforePurchase;
			PurchaseGoldLine.GoldAfter = GoldAfterPurchase;
			PurchaseGoldLine.bClamped = PurchaseGoldLine.RequestedDelta != PurchaseGoldLine.ActualDelta;
			PurchaseGoldLine.Summary = FString::Printf(TEXT("Purchase spend for %s: %d gold"), *PurchaseGoldLine.SourceName, PurchaseContext.ResolvedPrice);
			PlayerState->AddGoldBreakdownLine(PurchaseGoldLine);

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
				FGambitGoldBreakdownLine PostPurchaseGoldLine;
				PostPurchaseGoldLine.LineType = EGambitGoldBreakdownLineType::Cashback;
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
				PlayerState->AddGoldBreakdownLine(PostPurchaseGoldLine);
			}
			PlayerState->AddShopBreakdownLine(BuildShopLineFromPurchaseContext(PurchaseContext, CurrentPhase, EGambitShopBreakdownLineType::PurchaseSuccess));

			Result = MakeCommandResult(
				PlayerState,
				EGambitRoundCommandStatus::Success,
				true,
				FString::Printf(
					TEXT("Purchased %s for %d gold. Gold %d -> %d."),
					*FormatRoundFlowItemName(PurchaseContext.ItemDefinition.Get()),
					PurchaseContext.ResolvedPrice,
					GoldBeforePurchase,
					PlayerState->GetCurrentGold()));
		}
		else
		{
			if (PurchaseContext.FailureReason.IsEmpty())
			{
				PurchaseContext.FailureReason = TEXT("Purchase rejected by shop.");
			}
			PlayerState->AddShopBreakdownLine(BuildShopLineFromPurchaseContext(PurchaseContext, CurrentPhase, EGambitShopBreakdownLineType::PurchaseFailure));
			UE_LOG(
				LogGambit,
				Log,
				TEXT("RoundFlow: RequestPurchaseOffer refused Player=%s OfferId=%d Reason=%s"),
				*BuildRoundFlowPlayerLabel(PlayerState, Players),
				OfferId,
				*PurchaseContext.FailureReason);

			Result = MakeCommandResult(
				PlayerState,
				ResolvePurchaseFailureStatus(PurchaseContext),
				false,
				FString::Printf(
					TEXT("Purchase refused: %s"),
					*PurchaseContext.FailureReason));
		}

		Result.OfferId = OfferId;
		Result.GoldBefore = GoldBeforePurchase;
		Result.GoldAfter = PlayerState->GetCurrentGold();
	}

	const FString OfferString = bFoundOffer ? FormatRoundFlowOffer(OfferSnapshot) : FString::Printf(TEXT("OfferId=%d NotFound"), OfferId);
	UE_LOG(
		LogGambit,
		Log,
		TEXT("RoundFlow: RequestPurchaseOffer Player=%s Phase=%s Offer=%s Result=%s GoldRemaining=%d"),
		*BuildRoundFlowPlayerLabel(PlayerState, Players),
		*RoundFlowPhaseToString(CurrentPhase),
		*OfferString,
		Result.bSuccess ? TEXT("Success") : TEXT("Failure"),
		PlayerState ? PlayerState->GetCurrentGold() : 0);
	Result.OfferId = OfferId;
	return Result;
}

TArray<FGambitShopOfferSnapshot> UGambitRoundFlowComponent::BuildShopOfferSnapshots(
	AGambitPlayerState* PlayerState) const
{
	TArray<FGambitShopOfferSnapshot> Snapshots;
	if (!PlayerState)
	{
		return Snapshots;
	}

	const AGambitGameState* GameState = GetGambitGameState();
	const EGambitRoundPhase CurrentPhase = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	const UGambitSharedPoolComponent* SharedPoolComponent = GameState ? GameState->GetSharedPoolComponent() : nullptr;
	const UGambitShopComponent* ShopComponent = PlayerState->GetShopComponent();
	const int32 PurchasesMadeThisShop = ShopComponent ? ShopComponent->GetPurchasesMadeThisShop() : 0;
	const TArray<FGambitShopOffer>& Offers = PlayerState->GetCurrentShopOffersRef();
	Snapshots.Reserve(Offers.Num());

	for (const FGambitShopOffer& Offer : Offers)
	{
		const FGambitShopPurchaseContext PurchaseContext = PlayerState->BuildShopPurchasePreviewContext(Offer.OfferId, SharedPoolComponent);
		Snapshots.Add(BuildOfferSnapshotFromContext(Offer, PurchaseContext, PlayerState, CurrentPhase, PurchasesMadeThisShop));
	}

	return Snapshots;
}

FGambitUIPlayerActionSnapshot UGambitRoundFlowComponent::BuildPlayerActionSnapshot(
	AGambitPlayerState* PlayerState) const
{
	FGambitUIPlayerActionSnapshot Snapshot;

	const AGambitGameState* GameState = GetGambitGameState();
	const EGambitRoundPhase CurrentPhase = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	Snapshot.PlayerIndex = Players.IndexOfByKey(PlayerState);
	Snapshot.Phase = CurrentPhase;
	Snapshot.RerollsUsed = GetRerollCount(PlayerState);
	Snapshot.RerollLimit = GetEffectiveRerollLimit(PlayerState);
	Snapshot.RerollsRemaining = FMath::Max(0, Snapshot.RerollLimit - Snapshot.RerollsUsed);

	const bool bReadyGatedPhase = CurrentPhase == EGambitRoundPhase::SelectionReroll
		|| CurrentPhase == EGambitRoundPhase::Action
		|| CurrentPhase == EGambitRoundPhase::Shop;
	Snapshot.bCanReadyForPhase = GameState && PlayerState && bReadyGatedPhase;
	if (!GameState)
	{
		Snapshot.ReadyUnavailableReason = TEXT("Ready unavailable: missing game state.");
	}
	else if (!PlayerState)
	{
		Snapshot.ReadyUnavailableReason = TEXT("Ready unavailable: invalid player.");
	}
	else if (!bReadyGatedPhase)
	{
		Snapshot.ReadyUnavailableReason = FString::Printf(
			TEXT("Ready unavailable: current phase is %s."),
			*RoundFlowPhaseToString(CurrentPhase));
	}

	if (!GameState)
	{
		Snapshot.RerollUnavailableReason = TEXT("Reroll unavailable: missing game state.");
	}
	else if (!PlayerState)
	{
		Snapshot.RerollUnavailableReason = TEXT("Reroll unavailable: invalid player.");
	}
	else if (CurrentPhase != EGambitRoundPhase::SelectionReroll)
	{
		Snapshot.RerollUnavailableReason = FString::Printf(
			TEXT("Reroll unavailable: current phase is %s, expected Selection / Reroll."),
			*RoundFlowPhaseToString(CurrentPhase));
	}
	else if (Snapshot.RerollsUsed >= Snapshot.RerollLimit)
	{
		Snapshot.RerollUnavailableReason = FString::Printf(
			TEXT("Reroll unavailable: limit reached (%d/%d)."),
			Snapshot.RerollsUsed,
			Snapshot.RerollLimit);
	}
	else if (!HasUnlockedRerollableDice(PlayerState))
	{
		Snapshot.RerollUnavailableReason = TEXT("Reroll unavailable: no unlocked rerollable dice.");
	}
	else
	{
		Snapshot.bCanReroll = true;
	}

	if (!PlayerState)
	{
		return Snapshot;
	}

	const TArray<FGambitDiceSnapshot> DiceSnapshots = PlayerState->BuildDiceSnapshot();
	Snapshot.DiceActions.Reserve(DiceSnapshots.Num());
	for (const FGambitDiceSnapshot& DiceSnapshot : DiceSnapshots)
	{
		FGambitUIDieActionSnapshot DieAction;
		DieAction.Die = DiceSnapshot;

		if (CurrentPhase != EGambitRoundPhase::SelectionReroll)
		{
			DieAction.LockUnavailableReason = FString::Printf(
				TEXT("Lock unavailable: current phase is %s, expected Selection / Reroll."),
				*RoundFlowPhaseToString(CurrentPhase));
		}
		else if (!DiceSnapshot.bLocked && !DiceSnapshot.bCanBeLocked)
		{
			DieAction.LockUnavailableReason = FString::Printf(
				TEXT("Lock unavailable: die %d cannot be locked."),
				DiceSnapshot.HandIndex + 1);
		}
		else
		{
			DieAction.bCanLock = !DiceSnapshot.bLocked;
			DieAction.bCanUnlock = DiceSnapshot.bLocked;
			DieAction.bCanToggleLock = DieAction.bCanLock || DieAction.bCanUnlock;
		}

		if (CurrentPhase != EGambitRoundPhase::SelectionReroll)
		{
			DieAction.RerollUnavailableReason = FString::Printf(
				TEXT("Die reroll unavailable: current phase is %s, expected Selection / Reroll."),
				*RoundFlowPhaseToString(CurrentPhase));
		}
		else if (Snapshot.RerollsUsed >= Snapshot.RerollLimit)
		{
			DieAction.RerollUnavailableReason = FString::Printf(
				TEXT("Die reroll unavailable: limit reached (%d/%d)."),
				Snapshot.RerollsUsed,
				Snapshot.RerollLimit);
		}
		else if (DiceSnapshot.bLocked)
		{
			DieAction.RerollUnavailableReason = FString::Printf(
				TEXT("Die reroll unavailable: die %d is locked."),
				DiceSnapshot.HandIndex + 1);
		}
		else if (!DiceSnapshot.bCanBeRerolled)
		{
			DieAction.RerollUnavailableReason = FString::Printf(
				TEXT("Die reroll unavailable: die %d cannot be rerolled."),
				DiceSnapshot.HandIndex + 1);
		}
		else
		{
			DieAction.bCanBeRerolledByNextReroll = true;
		}

		Snapshot.DiceActions.Add(DieAction);
	}

	const TArray<FGambitConsumableRuntimeSlot>& ConsumableSlots = PlayerState->GetConsumableSlotsRef();
	const TArray<FGambitItemSnapshot> ConsumableSnapshots = PlayerState->BuildConsumableSnapshot();
	Snapshot.ConsumableActions.Reserve(ConsumableSlots.Num());
	for (int32 SlotIndex = 0; SlotIndex < ConsumableSlots.Num(); ++SlotIndex)
	{
		const FGambitConsumableRuntimeSlot& Slot = ConsumableSlots[SlotIndex];
		FGambitUIConsumableActionSnapshot ConsumableAction;
		ConsumableAction.SlotIndex = SlotIndex;
		ConsumableAction.bSlotOccupied = Slot.IsValid();
		if (ConsumableSnapshots.IsValidIndex(SlotIndex))
		{
			ConsumableAction.Consumable = ConsumableSnapshots[SlotIndex];
		}

		if (!Slot.IsValid())
		{
			ConsumableAction.UseUnavailableReason = FString::Printf(
				TEXT("Consumable unavailable: slot %d is empty."),
				SlotIndex + 1);
		}
		else if (CurrentPhase != EGambitRoundPhase::Action)
		{
			ConsumableAction.UseUnavailableReason = FString::Printf(
				TEXT("Consumable unavailable: current phase is %s, expected Action."),
				*RoundFlowPhaseToString(CurrentPhase));
		}
		else if (!Slot.Definition->CanBeUsedDuringPhase(CurrentPhase))
		{
			ConsumableAction.UseUnavailableReason = FString::Printf(
				TEXT("Consumable unavailable: slot %d cannot be used during this phase."),
				SlotIndex + 1);
		}
		else
		{
			ConsumableAction.bCanUse = true;
		}

		if (TargetSelectionService && Slot.IsValid() && Slot.Definition->CanBeUsedDuringPhase(CurrentPhase))
		{
			FGambitTargetSelectionRequest TargetSelectionPreview;
			if (BuildConsumableTargetSelectionRequest(PlayerState, SlotIndex, TargetSelectionPreview))
			{
				ConsumableAction.TargetSelectionPreview = TargetSelectionPreview;
				ConsumableAction.bRequiresTargetSelection = TargetSelectionPreview.bRequiresExplicitSelection;
				ConsumableAction.bHasValidTargetOptions = TargetSelectionPreview.HasValidOptions();
			}
		}

		Snapshot.ConsumableActions.Add(ConsumableAction);
	}

	Snapshot.ShopOffers = BuildShopOfferSnapshots(PlayerState);
	for (const FGambitShopOfferSnapshot& OfferSnapshot : Snapshot.ShopOffers)
	{
		if (OfferSnapshot.bCanPurchase)
		{
			Snapshot.bCanPurchaseAnyOffer = true;
			break;
		}
	}

	if (CurrentPhase != EGambitRoundPhase::Shop)
	{
		Snapshot.ShopUnavailableReason = FString::Printf(
			TEXT("Shop unavailable: current phase is %s, expected Shop."),
			*RoundFlowPhaseToString(CurrentPhase));
	}
	else if (Snapshot.ShopOffers.Num() == 0)
	{
		Snapshot.ShopUnavailableReason = TEXT("Shop unavailable: no offers were generated.");
	}
	else if (!Snapshot.bCanPurchaseAnyOffer)
	{
		Snapshot.ShopUnavailableReason = TEXT("Shop unavailable: no offer can currently be purchased.");
	}

	return Snapshot;
}

int32 UGambitRoundFlowComponent::GetRerollsUsedForPlayer(AGambitPlayerState* PlayerState) const
{
	return GetRerollCount(PlayerState);
}

int32 UGambitRoundFlowComponent::GetMaxRerollsForPlayer(AGambitPlayerState* PlayerState) const
{
	return GetEffectiveRerollLimit(PlayerState);
}

int32 UGambitRoundFlowComponent::GetActiveRoundCount() const
{
	return FMath::Max(1, ActiveMatchSetup.RoundCount);
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
		RecordScoreBreakdownPresentationLines(PlayerState, EGambitRoundPhase::Resolution, ScoreBreakdown);
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
				PlayerState->AddShopBreakdownLine(BuildShopLineFromOffer(Offer, EGambitRoundPhase::Shop, GameState->GetSharedPoolComponent()));
				UE_LOG(LogGambit, Log, TEXT("RoundFlow: %s ShopOffer %s"), *BuildRoundFlowPlayerLabel(PlayerState, Players), *FormatRoundFlowOffer(Offer));
			}
		}
	}

	ResetReadiness();
}

void UGambitRoundFlowComponent::EnterRoundEndPhase()
{
	UE_LOG(LogGambit, Log, TEXT("RoundFlow: EnterRoundEndPhase"));
	AGambitGameState* GameState = GetGambitGameState();
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

	const int32 RoundCount = GetActiveRoundCount();
	if (GameState->GetCurrentRoundIndex() >= RoundCount)
	{
		const TArray<FGambitFinalRankingEntry> FinalRanking = BuildFinalRankingSnapshot(Players);
		GameState->SetFinalRankingSnapshot(FinalRanking);
		GameState->SetMatchLifecycleState(EGambitMatchLifecycleState::MatchComplete);

		if (FinalRanking.Num() > 0)
		{
			UE_LOG(
				LogGambit,
				Log,
				TEXT("RoundFlow: Match complete. Winner=%s VP=%d"),
				*FinalRanking[0].PlayerName,
				FinalRanking[0].TotalVictoryPoints);
		}

		for (const FGambitFinalRankingEntry& Entry : FinalRanking)
		{
			UE_LOG(
				LogGambit,
				Log,
				TEXT("RoundFlow: FinalRank=%d Player=%s TotalVP=%d LastRoundScore=%d Gold=%d"),
				Entry.Rank,
				*Entry.PlayerName,
				Entry.TotalVictoryPoints,
				Entry.LastRoundScore,
				Entry.Gold);
		}

		TransitionToPhase(EGambitRoundPhase::None);
		return;
	}

	UE_LOG(
		LogGambit,
		Log,
		TEXT("RoundFlow: Preparing next round %d of %d"),
		GameState->GetCurrentRoundIndex() + 1,
		RoundCount);
	StartNextRound();
}

TArray<FGambitFinalRankingEntry> UGambitRoundFlowComponent::BuildFinalRankingSnapshot(
	const TArray<AGambitPlayerState*>& Players) const
{
	TArray<AGambitPlayerState*> SortedPlayers = Players;
	SortedPlayers.Sort([](const AGambitPlayerState& A, const AGambitPlayerState& B)
	{
		if (A.GetTotalVictoryPoints() == B.GetTotalVictoryPoints())
		{
			if (A.GetCurrentRoundScore() == B.GetCurrentRoundScore())
			{
				return A.GetPlayerId() < B.GetPlayerId();
			}

			return A.GetCurrentRoundScore() > B.GetCurrentRoundScore();
		}

		return A.GetTotalVictoryPoints() > B.GetTotalVictoryPoints();
	});

	TArray<FGambitFinalRankingEntry> FinalRanking;
	FinalRanking.Reserve(SortedPlayers.Num());
	for (int32 Index = 0; Index < SortedPlayers.Num(); ++Index)
	{
		const AGambitPlayerState* PlayerState = SortedPlayers[Index];
		if (!PlayerState)
		{
			continue;
		}

		FGambitFinalRankingEntry Entry;
		Entry.Rank = FinalRanking.Num() + 1;
		Entry.PlayerState = const_cast<AGambitPlayerState*>(PlayerState);
		Entry.PlayerName = BuildRoundFlowPlayerLabel(PlayerState, Players);
		Entry.TotalVictoryPoints = PlayerState->GetTotalVictoryPoints();
		Entry.LastRoundScore = PlayerState->GetCurrentRoundScore();
		Entry.Gold = PlayerState->GetCurrentGold();
		Entry.bWinner = FinalRanking.Num() == 0;
		FinalRanking.Add(Entry);
	}

	return FinalRanking;
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
	EnsureRuntimeServices();

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

FGambitRoundCommandResult UGambitRoundFlowComponent::MakeCommandResult(
	AGambitPlayerState* PlayerState,
	const EGambitRoundCommandStatus Status,
	const bool bSuccess,
	const FString& Message,
	const int32 DieIndex) const
{
	const AGambitGameState* GameState = GetGambitGameState();
	const TArray<AGambitPlayerState*> Players = GetAllPlayers();

	FGambitRoundCommandResult Result;
	Result.bSuccess = bSuccess;
	Result.Status = Status;
	Result.Phase = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	Result.PlayerIndex = Players.IndexOfByKey(PlayerState);
	Result.DieIndex = DieIndex;
	Result.RerollsUsed = GetRerollCount(PlayerState);
	Result.RerollLimit = GetEffectiveRerollLimit(PlayerState);
	Result.Message = Message;
	return Result;
}

bool UGambitRoundFlowComponent::HasUnlockedRerollableDice(const AGambitPlayerState* PlayerState) const
{
	if (!PlayerState)
	{
		return false;
	}

	for (const FGambitDieRuntimeState& DieState : PlayerState->GetDiceStatesRef())
	{
		if (!DieState.bLocked && DieState.bCanBeRerolled)
		{
			return true;
		}
	}

	return false;
}

AGambitGameState* UGambitRoundFlowComponent::GetGambitGameState() const
{
	const AGameModeBase* GameMode = Cast<AGameModeBase>(GetOwner());
	if (GameMode)
	{
		return GameMode->GetGameState<AGambitGameState>();
	}

	return GetTypedOuter<AGambitGameState>();
}

TArray<AGambitPlayerState*> UGambitRoundFlowComponent::GetAllPlayers() const
{
	if (const AGambitGameState* GameState = GetGambitGameState())
	{
		return GameState->GetGambitPlayerStates();
	}

	return {};
}
