#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"

#include "Data/Assets/GambitItemEffectDefinition.h"
#include "Game/Flow/GambitRoundFlowComponent.h"
#include "Game/Flow/GambitTargetSelectionService.h"
#include "Game/States/GambitGameState.h"
#include "Items/Consumables/GambitConsumableDefinition.h"
#include "Items/Effects/GambitEffectResolver.h"
#include "Items/Effects/GambitEffectTargetRules.h"
#include "Players/Components/GambitDiceComponent.h"
#include "Players/Components/GambitEconomyComponent.h"
#include "Players/Components/GambitInventoryComponent.h"
#include "Players/Controllers/GambitPlayerController.h"
#include "Players/States/GambitPlayerState.h"

namespace
{
	UGambitItemEffectDefinition* MakeTargetSelectionEffect(
		UObject* Outer,
		const EGambitItemEffectType EffectType,
		const FName TargetRuleId,
		const EGambitEffectTarget Target = EGambitEffectTarget::Target)
	{
		UGambitItemEffectDefinition* EffectDefinition = NewObject<UGambitItemEffectDefinition>(Outer);
		EffectDefinition->Hook = EGambitEffectHook::ConsumableUse;
		EffectDefinition->EffectType = EffectType;
		EffectDefinition->Target = Target;
		EffectDefinition->TargetRuleId = TargetRuleId;
		EffectDefinition->EffectId = FName(*FString::Printf(TEXT("effect.test.%s"), *TargetRuleId.ToString()));
		return EffectDefinition;
	}

	UGambitConsumableDefinition* MakeTargetSelectionConsumable(
		UObject* Outer,
		const TCHAR* ItemId,
		UGambitItemEffectDefinition* EffectDefinition,
		const bool bCanTargetOpponent = false)
	{
		UGambitConsumableDefinition* ConsumableDefinition = NewObject<UGambitConsumableDefinition>(Outer);
		ConsumableDefinition->ItemId = FName(ItemId);
		ConsumableDefinition->bCanTargetOpponent = bCanTargetOpponent;
		ConsumableDefinition->UsablePhases.Add(EGambitRoundPhase::Action);
		ConsumableDefinition->EffectDefinitions.Add(EffectDefinition);
		return ConsumableDefinition;
	}

	UGambitConsumableDefinition* MakeSelfGoldConsumable(
		UObject* Outer,
		const TCHAR* ItemId,
		const float Amount)
	{
		UGambitItemEffectDefinition* EffectDefinition = NewObject<UGambitItemEffectDefinition>(Outer);
		EffectDefinition->Hook = EGambitEffectHook::ConsumableUse;
		EffectDefinition->EffectType = EGambitItemEffectType::AddGold;
		EffectDefinition->Target = EGambitEffectTarget::Source;
		EffectDefinition->Amount = Amount;
		EffectDefinition->EffectId = FName(*FString::Printf(TEXT("effect.test.%s.gold"), ItemId));

		UGambitConsumableDefinition* ConsumableDefinition = NewObject<UGambitConsumableDefinition>(Outer);
		ConsumableDefinition->ItemId = FName(ItemId);
		ConsumableDefinition->UsablePhases.Add(EGambitRoundPhase::Action);
		ConsumableDefinition->EffectDefinitions.Add(EffectDefinition);
		return ConsumableDefinition;
	}

	TArray<AGambitPlayerState*> SpawnInitializedPlayers(UWorld* World, const int32 PlayerCount)
	{
		TArray<AGambitPlayerState*> Players;
		if (!World)
		{
			return Players;
		}

		Players.Reserve(PlayerCount);
		for (int32 PlayerIndex = 0; PlayerIndex < PlayerCount; ++PlayerIndex)
		{
			AGambitPlayerState* PlayerState = World->SpawnActor<AGambitPlayerState>();
			if (PlayerState)
			{
				PlayerState->InitializeForMatch();
				PlayerState->SetPlayerId(PlayerIndex);
				PlayerState->SetPlayerName(FString::Printf(TEXT("Player %d"), PlayerIndex));
			}
			Players.Add(PlayerState);
		}
		return Players;
	}

	FGambitEffectExecutionContext MakeTargetSelectionContext(
		AGambitPlayerState* SourcePlayer,
		AGambitPlayerState* TargetPlayer,
		const TArray<AGambitPlayerState*>& Players)
	{
		FGambitEffectExecutionContext Context;
		Context.Hook = EGambitEffectHook::ConsumableUse;
		Context.CurrentPhase = EGambitRoundPhase::Action;
		Context.RoundNumber = 1;
		Context.SourcePlayer = SourcePlayer;
		Context.TargetPlayer = TargetPlayer ? TargetPlayer : SourcePlayer;
		Context.SourceDiceComponent = SourcePlayer ? SourcePlayer->GetDiceComponent() : nullptr;
		Context.TargetDiceComponent = TargetPlayer ? TargetPlayer->GetDiceComponent() : nullptr;
		Context.SourceEconomyComponent = SourcePlayer ? SourcePlayer->GetEconomyComponent() : nullptr;
		Context.TargetEconomyComponent = TargetPlayer ? TargetPlayer->GetEconomyComponent() : nullptr;
		Context.SourceInventoryComponent = SourcePlayer ? SourcePlayer->GetInventoryComponent() : nullptr;
		Context.TargetInventoryComponent = TargetPlayer ? TargetPlayer->GetInventoryComponent() : nullptr;
		Context.SourceDice = SourcePlayer ? SourcePlayer->GetDiceStates() : TArray<FGambitDieRuntimeState>();
		Context.TargetDice = TargetPlayer ? TargetPlayer->GetDiceStates() : TArray<FGambitDieRuntimeState>();
		for (AGambitPlayerState* PlayerState : Players)
		{
			Context.MatchPlayerStates.Add(PlayerState);
		}
		Context.MatchPlayerCount = Players.Num();
		return Context;
	}

	AGambitGameState* SpawnTargetSelectionGameState(UWorld* World, const TArray<AGambitPlayerState*>& Players)
	{
		AGambitGameState* GameState = World ? World->SpawnActor<AGambitGameState>() : nullptr;
		if (!GameState)
		{
			return nullptr;
		}

		for (AGambitPlayerState* PlayerState : Players)
		{
			if (PlayerState)
			{
				GameState->AddPlayerState(PlayerState);
			}
		}
		GameState->SetCurrentRoundIndex(1);
		GameState->SetCurrentPhase(EGambitRoundPhase::Action);
		return GameState;
	}

	FGambitTargetSelectionResult BuildConfirmedTargetSelectionResult(
		const FGambitTargetSelectionRequest& Request,
		const FGambitTargetSelectionOption& Option)
	{
		FGambitTargetSelectionResult Result;
		Result.RequestId = Request.RequestId;
		Result.bConfirmed = true;
		Result.SourceConsumableSlotIndex = Request.SourceConsumableSlotIndex;
		Result.SelectedOptionId = Option.OptionId;
		Result.TargetPlayer = Option.TargetPlayer;
		Result.TargetPlayerIndex = Option.TargetPlayerIndex;
		Result.TargetDieHandIndex = Option.TargetDieHandIndex;
		Result.TargetDieInstanceId = Option.TargetDieInstanceId;
		Result.TargetRuleId = Option.TargetRuleId;
		return Result;
	}

	int32 CountTargetSelectionRoundEvents(
		const FGambitEffectExecutionContext& Context,
		const EGambitRoundGameplayEventType EventType)
	{
		return Context.RoundEvents.FilterByPredicate([EventType](const FGambitRoundGameplayEvent& Event)
		{
			return Event.EventType == EventType;
		}).Num();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitTargetSelectionOpponentOptionsTest,
	"GrandpaGambit.TargetSelection.OpponentOptions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitTargetSelectionOpponentOptionsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("test world is created"), TestWorld))
	{
		return false;
	}

	TArray<AGambitPlayerState*> Players = SpawnInitializedPlayers(TestWorld, 3);
	AGambitPlayerState* SourcePlayer = Players[0];

	UGambitItemEffectDefinition* EffectDefinition = MakeTargetSelectionEffect(
		GetTransientPackage(),
		EGambitItemEffectType::StealGold,
		GambitEffectTargetRules::TargetOpponent);
	UGambitConsumableDefinition* ConsumableDefinition = MakeTargetSelectionConsumable(
		GetTransientPackage(),
		TEXT("item.test.bribe"),
		EffectDefinition,
		true);
	const int32 ConsumableSlotIndex = SourcePlayer->GetConsumableSlotsRef().Num();
	TestTrue(TEXT("source receives test consumable"), SourcePlayer->GetInventoryComponent()->AddConsumable(ConsumableDefinition));

	UGambitTargetSelectionService* SelectionService = NewObject<UGambitTargetSelectionService>();
	FGambitTargetSelectionRequest Request;
	const bool bBuilt = SelectionService->BuildConsumableTargetSelectionRequest(
		SourcePlayer,
		ConsumableSlotIndex,
		Players,
		EGambitRoundPhase::Action,
		MakeTargetSelectionContext(SourcePlayer, SourcePlayer, Players),
		Request);

	TestTrue(TEXT("opponent target selection request is built"), bBuilt);
	TestTrue(TEXT("opponent request requires explicit selection"), Request.bRequiresExplicitSelection);
	TestEqual(TEXT("opponent request type is player"), Request.SelectionType, EGambitTargetSelectionType::Player);
	TestEqual(TEXT("opponent request proposes both opponents"), Request.Options.Num(), 2);
	TestFalse(TEXT("source is not proposed as opponent"), Request.Options.ContainsByPredicate([SourcePlayer](const FGambitTargetSelectionOption& Option)
	{
		return Option.TargetPlayer == SourcePlayer;
	}));
	TestFalse(TEXT("no opponent option is invalid"), Request.Options.ContainsByPredicate([](const FGambitTargetSelectionOption& Option)
	{
		return !Option.bValid;
	}));

	AGambitPlayerController* PlayerController = TestWorld->SpawnActor<AGambitPlayerController>();
	if (!TestNotNull(TEXT("controller exists for pending opponent selection"), PlayerController))
	{
		TestWorld->DestroyWorld(false);
		return false;
	}
	TestTrue(TEXT("controller starts pending opponent selection"), PlayerController->StartTargetSelection(Request));
	TestTrue(TEXT("controller keeps pending opponent selection"), PlayerController->HasPendingTargetSelection());
	TestEqual(
		TEXT("pending opponent selection keeps service-built options"),
		PlayerController->GetPendingTargetSelectionRequest().Options.Num(),
		Request.Options.Num());
	TestTrue(TEXT("pending opponent selection auto-selects a valid option"), PlayerController->GetPendingTargetSelectionSelectedOptionId() != INDEX_NONE);
	TestTrue(TEXT("selection request feedback is visible"), SourcePlayer->HasEventThisRound(EGambitRoundGameplayEventType::TargetSelectionRequested));

	TestWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitTargetSelectionDieOptionsAndCancelTest,
	"GrandpaGambit.TargetSelection.DieOptionsAndCancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitTargetSelectionDieOptionsAndCancelTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("test world is created"), TestWorld))
	{
		return false;
	}

	TArray<AGambitPlayerState*> Players = SpawnInitializedPlayers(TestWorld, 2);
	AGambitPlayerState* SourcePlayer = Players[0];
	SourcePlayer->GetDiceComponent()->SetDieValue(0, 2);
	SourcePlayer->GetDiceComponent()->SetDieValue(1, 5);

	UGambitItemEffectDefinition* EffectDefinition = MakeTargetSelectionEffect(
		GetTransientPackage(),
		EGambitItemEffectType::ModifyDieValue,
		GambitEffectTargetRules::SourceSelectedDie,
		EGambitEffectTarget::Source);
	UGambitConsumableDefinition* ConsumableDefinition = MakeTargetSelectionConsumable(
		GetTransientPackage(),
		TEXT("item.test.polish_die"),
		EffectDefinition);
	const int32 ConsumableSlotIndex = SourcePlayer->GetConsumableSlotsRef().Num();
	TestTrue(TEXT("source receives die consumable"), SourcePlayer->GetInventoryComponent()->AddConsumable(ConsumableDefinition));

	UGambitTargetSelectionService* SelectionService = NewObject<UGambitTargetSelectionService>();
	FGambitTargetSelectionRequest Request;
	const bool bBuilt = SelectionService->BuildConsumableTargetSelectionRequest(
		SourcePlayer,
		ConsumableSlotIndex,
		Players,
		EGambitRoundPhase::Action,
		MakeTargetSelectionContext(SourcePlayer, SourcePlayer, Players),
		Request);

	TestTrue(TEXT("die target selection request is built"), bBuilt);
	TestEqual(TEXT("die request type"), Request.SelectionType, EGambitTargetSelectionType::Die);
	TestEqual(TEXT("die request proposes one option per source die"), Request.Options.Num(), SourcePlayer->GetDiceStatesRef().Num());
	TestFalse(TEXT("die options are not opponent-owned"), Request.Options.ContainsByPredicate([SourcePlayer](const FGambitTargetSelectionOption& Option)
	{
		return Option.TargetPlayer != SourcePlayer;
	}));

	const int32 ConsumableCountBeforeCancel = SourcePlayer->GetConsumableSlotsRef().Num();
	AGambitPlayerController* PlayerController = TestWorld->SpawnActor<AGambitPlayerController>();
	TestTrue(TEXT("controller starts pending selection"), PlayerController->StartTargetSelection(Request));
	TestTrue(TEXT("controller has pending selection"), PlayerController->HasPendingTargetSelection());
	TestTrue(TEXT("controller cancels pending selection"), PlayerController->RequestCancelTargetSelection());
	TestFalse(TEXT("controller cleared pending selection"), PlayerController->HasPendingTargetSelection());
	TestEqual(TEXT("cancel does not consume item"), SourcePlayer->GetConsumableSlotsRef().Num(), ConsumableCountBeforeCancel);
	TestTrue(TEXT("cancel feedback is visible"), SourcePlayer->HasEventThisRound(EGambitRoundGameplayEventType::TargetSelectionCancelled));

	TestWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitTargetSelectionRoundFlowSelfConsumableActionUseTest,
	"GrandpaGambit.TargetSelection.RoundFlow.SelfConsumableActionUse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitTargetSelectionRoundFlowSelfConsumableActionUseTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("test world is created"), TestWorld))
	{
		return false;
	}

	TArray<AGambitPlayerState*> Players = SpawnInitializedPlayers(TestWorld, 2);
	AGambitPlayerState* SourcePlayer = Players[0];
	AGambitGameState* GameState = SpawnTargetSelectionGameState(TestWorld, Players);
	UGambitRoundFlowComponent* RoundFlow = NewObject<UGambitRoundFlowComponent>(GameState);
	if (!TestNotNull(TEXT("round flow exists for self consumable use"), RoundFlow)
		|| !TestNotNull(TEXT("source player exists for self consumable use"), SourcePlayer)
		|| !TestNotNull(TEXT("game state exists for self consumable use"), GameState))
	{
		TestWorld->DestroyWorld(false);
		return false;
	}

	UGambitConsumableDefinition* ConsumableDefinition = MakeSelfGoldConsumable(
		GetTransientPackage(),
		TEXT("item.test.self_gold_action"),
		3.0f);
	const int32 ConsumableSlotIndex = SourcePlayer->GetConsumableSlotsRef().Num();
	TestTrue(TEXT("source receives self consumable"), SourcePlayer->GetInventoryComponent()->AddConsumable(ConsumableDefinition));
	const int32 ConsumableCountBeforeUse = SourcePlayer->GetConsumableSlotsRef().Num();

	GameState->SetCurrentPhase(EGambitRoundPhase::Action);
	TestTrue(TEXT("self consumable succeeds during Action phase"), RoundFlow->RequestUseConsumable(SourcePlayer, ConsumableSlotIndex));
	TestEqual(TEXT("self consumable grants gold through existing effect pipeline"), SourcePlayer->GetEconomyComponent()->GetCurrentGold(), 3);
	TestEqual(TEXT("successful self consumable consumes one slot"), SourcePlayer->GetConsumableSlotsRef().Num(), ConsumableCountBeforeUse - 1);
	TestTrue(TEXT("self consumable used event is visible"), SourcePlayer->HasEventThisRound(EGambitRoundGameplayEventType::ConsumableUsed));
	TestTrue(TEXT("self consumable effect applied event is visible"), SourcePlayer->HasEventThisRound(EGambitRoundGameplayEventType::EffectApplied));
	TestTrue(TEXT("self consumable gold feedback is visible"), SourcePlayer->HasEventThisRound(EGambitRoundGameplayEventType::GoldChanged));

	UGambitConsumableDefinition* OutOfPhaseConsumable = MakeSelfGoldConsumable(
		GetTransientPackage(),
		TEXT("item.test.self_gold_out_of_phase"),
		5.0f);
	TestTrue(TEXT("source receives out-of-phase consumable"), SourcePlayer->GetInventoryComponent()->AddConsumable(OutOfPhaseConsumable));
	const int32 OutOfPhaseSlotIndex = SourcePlayer->GetConsumableSlotsRef().Num() - 1;
	const int32 ConsumableCountBeforeRefusal = SourcePlayer->GetConsumableSlotsRef().Num();
	GameState->SetCurrentPhase(EGambitRoundPhase::SelectionReroll);
	TestFalse(TEXT("self consumable is refused outside Action phase"), RoundFlow->RequestUseConsumable(SourcePlayer, OutOfPhaseSlotIndex));
	TestEqual(TEXT("out-of-phase refusal does not consume item"), SourcePlayer->GetConsumableSlotsRef().Num(), ConsumableCountBeforeRefusal);
	TestEqual(TEXT("out-of-phase refusal does not apply gold"), SourcePlayer->GetEconomyComponent()->GetCurrentGold(), 3);

	TestWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitTargetSelectionRoundFlowConfirmedOpponentConsumableTest,
	"GrandpaGambit.TargetSelection.RoundFlow.ConfirmedOpponentConsumable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitTargetSelectionRoundFlowConfirmedOpponentConsumableTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("test world is created"), TestWorld))
	{
		return false;
	}

	TArray<AGambitPlayerState*> Players = SpawnInitializedPlayers(TestWorld, 2);
	AGambitPlayerState* SourcePlayer = Players[0];
	AGambitPlayerState* TargetPlayer = Players[1];
	AGambitGameState* GameState = SpawnTargetSelectionGameState(TestWorld, Players);
	UGambitRoundFlowComponent* RoundFlow = NewObject<UGambitRoundFlowComponent>(GameState);
	if (!TestNotNull(TEXT("round flow exists for confirmed opponent consumable"), RoundFlow)
		|| !TestNotNull(TEXT("source player exists for confirmed opponent consumable"), SourcePlayer)
		|| !TestNotNull(TEXT("target player exists for confirmed opponent consumable"), TargetPlayer)
		|| !TestNotNull(TEXT("game state exists for confirmed opponent consumable"), GameState))
	{
		TestWorld->DestroyWorld(false);
		return false;
	}

	TargetPlayer->GetEconomyComponent()->AddGold(10);

	UGambitItemEffectDefinition* EffectDefinition = MakeTargetSelectionEffect(
		GetTransientPackage(),
		EGambitItemEffectType::StealGold,
		GambitEffectTargetRules::TargetOpponent);
	EffectDefinition->Amount = 4.0f;
	EffectDefinition->bNegativeEffect = true;
	EffectDefinition->NegativeEffectCategories.Add(EGambitNegativeEffectCategory::GoldSteal);
	UGambitConsumableDefinition* ConsumableDefinition = MakeTargetSelectionConsumable(
		GetTransientPackage(),
		TEXT("item.test.confirmed_bribe"),
		EffectDefinition,
		true);
	const int32 ConsumableSlotIndex = SourcePlayer->GetConsumableSlotsRef().Num();
	TestTrue(TEXT("source receives target consumable"), SourcePlayer->GetInventoryComponent()->AddConsumable(ConsumableDefinition));
	const int32 ConsumableCountBeforeUse = SourcePlayer->GetConsumableSlotsRef().Num();

	UGambitTargetSelectionService* SelectionService = NewObject<UGambitTargetSelectionService>();
	FGambitTargetSelectionRequest Request;
	TestTrue(TEXT("round-flow target request options are built by service"), SelectionService->BuildConsumableTargetSelectionRequest(
		SourcePlayer,
		ConsumableSlotIndex,
		Players,
		EGambitRoundPhase::Action,
		MakeTargetSelectionContext(SourcePlayer, SourcePlayer, Players),
		Request));

	const FGambitTargetSelectionOption* TargetOption = Request.Options.FindByPredicate([TargetPlayer](const FGambitTargetSelectionOption& Option)
	{
		return Option.bValid && Option.TargetPlayer == TargetPlayer;
	});
	if (!TestNotNull(TEXT("confirmed target option exists for opponent"), TargetOption))
	{
		TestWorld->DestroyWorld(false);
		return false;
	}

	const FGambitTargetSelectionResult Result = BuildConfirmedTargetSelectionResult(Request, *TargetOption);
	TestTrue(TEXT("confirmed opponent consumable applies through round flow"), RoundFlow->RequestUseConsumableWithTargetSelectionResult(SourcePlayer, Result));
	TestEqual(TEXT("confirmed target loses gold"), TargetPlayer->GetEconomyComponent()->GetCurrentGold(), 6);
	TestEqual(TEXT("confirmed source gains stolen gold"), SourcePlayer->GetEconomyComponent()->GetCurrentGold(), 4);
	TestEqual(TEXT("confirmed target consumable is consumed after successful confirmation"), SourcePlayer->GetConsumableSlotsRef().Num(), ConsumableCountBeforeUse - 1);
	TestTrue(TEXT("confirmed use records consumable used"), SourcePlayer->HasEventThisRound(EGambitRoundGameplayEventType::ConsumableUsed));
	TestTrue(TEXT("confirmed use records effect applied"), SourcePlayer->HasEventThisRound(EGambitRoundGameplayEventType::EffectApplied));
	TestTrue(TEXT("confirmed use records gold changed"), SourcePlayer->HasEventThisRound(EGambitRoundGameplayEventType::GoldChanged));

	TestWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitTargetSelectionAppliesAndReportsFeedbackTest,
	"GrandpaGambit.TargetSelection.AppliesAndReportsFeedback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitTargetSelectionAppliesAndReportsFeedbackTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("test world is created"), TestWorld))
	{
		return false;
	}

	TArray<AGambitPlayerState*> Players = SpawnInitializedPlayers(TestWorld, 2);
	AGambitPlayerState* SourcePlayer = Players[0];
	AGambitPlayerState* TargetPlayer = Players[1];
	TargetPlayer->GetEconomyComponent()->AddGold(10);

	UGambitItemEffectDefinition* EffectDefinition = MakeTargetSelectionEffect(
		GetTransientPackage(),
		EGambitItemEffectType::StealGold,
		GambitEffectTargetRules::TargetOpponent);
	EffectDefinition->Amount = 4.0f;
	EffectDefinition->bNegativeEffect = true;
	EffectDefinition->NegativeEffectCategories.Add(EGambitNegativeEffectCategory::GoldSteal);
	UGambitConsumableDefinition* ConsumableDefinition = MakeTargetSelectionConsumable(
		GetTransientPackage(),
		TEXT("item.test.bribe_feedback"),
		EffectDefinition,
		true);
	const int32 ConsumableSlotIndex = SourcePlayer->GetConsumableSlotsRef().Num();
	TestTrue(TEXT("source receives feedback consumable"), SourcePlayer->GetInventoryComponent()->AddConsumable(ConsumableDefinition));

	UGambitTargetSelectionService* SelectionService = NewObject<UGambitTargetSelectionService>();
	FGambitTargetSelectionRequest Request;
	TestTrue(TEXT("feedback request is built"), SelectionService->BuildConsumableTargetSelectionRequest(
		SourcePlayer,
		ConsumableSlotIndex,
		Players,
		EGambitRoundPhase::Action,
		MakeTargetSelectionContext(SourcePlayer, SourcePlayer, Players),
		Request));
	TestTrue(TEXT("feedback request has an opponent option"), Request.Options.Num() > 0);
	if (Request.Options.Num() == 0)
	{
		AddError(FString::Printf(TEXT("No target options were built. InvalidReason=%s"), *Request.InvalidReason));
		TestWorld->DestroyWorld(false);
		return false;
	}
	TargetPlayer = Request.Options[0].TargetPlayer.Get();

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	FGambitEffectExecutionContext AppliedContext = MakeTargetSelectionContext(SourcePlayer, TargetPlayer, Players);
	Resolver->ExecuteItemEffects(ConsumableDefinition, AppliedContext);
	TestEqual(TEXT("confirmed target loses gold"), TargetPlayer->GetEconomyComponent()->GetCurrentGold(), 6);
	TestEqual(TEXT("source gains stolen gold"), SourcePlayer->GetEconomyComponent()->GetCurrentGold(), 4);
	TestTrue(TEXT("effect applied event is visible"), CountTargetSelectionRoundEvents(AppliedContext, EGambitRoundGameplayEventType::EffectApplied) > 0);
	TestTrue(TEXT("consumable used event is visible"), CountTargetSelectionRoundEvents(AppliedContext, EGambitRoundGameplayEventType::ConsumableUsed) > 0);
	TestTrue(TEXT("gold changed feedback is visible"), CountTargetSelectionRoundEvents(AppliedContext, EGambitRoundGameplayEventType::GoldChanged) > 0);

	SourcePlayer->GetEconomyComponent()->InitializeForMatch();
	TargetPlayer->GetEconomyComponent()->InitializeForMatch();
	TargetPlayer->GetEconomyComponent()->AddGold(10);

	FGambitEffectExecutionContext BlockedContext = MakeTargetSelectionContext(SourcePlayer, TargetPlayer, Players);
	BlockedContext.NegativeEffectProtection.AddProtection(
		{ EGambitNegativeEffectCategory::GoldSteal },
		false,
		1,
		TEXT("effect.test.shield"),
		TEXT("Test Shield"));
	Resolver->ExecuteItemEffects(ConsumableDefinition, BlockedContext);
	TestEqual(TEXT("blocked target keeps gold"), TargetPlayer->GetEconomyComponent()->GetCurrentGold(), 10);
	TestEqual(TEXT("blocked source does not gain gold"), SourcePlayer->GetEconomyComponent()->GetCurrentGold(), 0);
	TestTrue(TEXT("prevented feedback event is visible"), CountTargetSelectionRoundEvents(BlockedContext, EGambitRoundGameplayEventType::EffectPrevented) > 0);
	TestTrue(TEXT("prevented feedback event is visible"), BlockedContext.RoundFeedbackEvents.ContainsByPredicate([](const FGambitRoundFeedbackEvent& Event)
	{
		return Event.bPrevented;
	}));

	TestWorld->DestroyWorld(false);
	return true;
}

#endif
