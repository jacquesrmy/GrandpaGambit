#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#include "Dice/Data/GambitDiceDefinition.h"
#include "Core/Types/GambitDebugTypes.h"
#include "Core/Types/GambitRoundGameplayEventTypes.h"
#include "Core/Types/GambitRoundPhaseRules.h"
#include "Data/Assets/GambitItemEffectDefinition.h"
#include "Game/Flow/GambitRoundFlowComponent.h"
#include "Game/States/GambitGameState.h"
#include "Items/Consumables/GambitConsumableDefinition.h"
#include "Items/Data/GambitItemDefinition.h"
#include "Items/Modules/GambitModuleDefinition.h"
#include "Managers/SharedPool/GambitSharedPoolComponent.h"
#include "Players/Components/GambitDiceComponent.h"
#include "Players/Components/GambitEconomyComponent.h"
#include "Players/Components/GambitInventoryComponent.h"
#include "Players/Controllers/GambitPlayerController.h"
#include "Players/States/GambitPlayerState.h"
#include "Shop/Components/GambitShopComponent.h"
#include "UI/Widgets/GambitPCShellWidget.h"

namespace
{
	UGambitDiceDefinition* MakePCShellFlowTestDie(UObject* Outer, const TCHAR* DiceId, const TArray<int32>& Faces)
	{
		UGambitDiceDefinition* DiceDefinition = NewObject<UGambitDiceDefinition>(Outer);
		DiceDefinition->DiceId = FName(DiceId);
		DiceDefinition->Faces = Faces;
		return DiceDefinition;
	}

	UGambitConsumableDefinition* MakePCShellFlowTestConsumable(
		UObject* Outer,
		const TCHAR* ItemId,
		const TCHAR* DisplayName,
		const TArray<EGambitRoundPhase>& UsablePhases)
	{
		UGambitConsumableDefinition* ConsumableDefinition = NewObject<UGambitConsumableDefinition>(Outer);
		ConsumableDefinition->ItemId = FName(ItemId);
		ConsumableDefinition->DisplayName = FText::FromString(DisplayName);
		ConsumableDefinition->UsablePhases = UsablePhases;
		return ConsumableDefinition;
	}

	UGambitConsumableDefinition* MakePCShellFlowSelfGoldConsumable(
		UObject* Outer,
		const TCHAR* ItemId,
		const TCHAR* DisplayName,
		const float Amount)
	{
		UGambitItemEffectDefinition* EffectDefinition = NewObject<UGambitItemEffectDefinition>(Outer);
		EffectDefinition->Hook = EGambitEffectHook::ConsumableUse;
		EffectDefinition->EffectType = EGambitItemEffectType::AddGold;
		EffectDefinition->Target = EGambitEffectTarget::Source;
		EffectDefinition->Amount = Amount;
		EffectDefinition->EffectId = FName(*FString::Printf(TEXT("effect.test.%s.gold"), ItemId));

		UGambitConsumableDefinition* ConsumableDefinition = MakePCShellFlowTestConsumable(
			Outer,
			ItemId,
			DisplayName,
			{ EGambitRoundPhase::Action });
		ConsumableDefinition->EffectDefinitions.Add(EffectDefinition);
		return ConsumableDefinition;
	}

	UGambitModuleDefinition* MakePCShellFlowTestModule(
		UObject* Outer,
		const TCHAR* ItemId,
		const TCHAR* DisplayName,
		const int32 Cost,
		const EGambitItemRarity Rarity = EGambitItemRarity::Common)
	{
		UGambitModuleDefinition* ModuleDefinition = NewObject<UGambitModuleDefinition>(Outer);
		ModuleDefinition->ItemId = FName(ItemId);
		ModuleDefinition->DisplayName = FText::FromString(DisplayName);
		ModuleDefinition->Cost = Cost;
		ModuleDefinition->Rarity = Rarity;
		return ModuleDefinition;
	}

	FGambitShopOffer MakePCShellFlowTestOffer(
		const int32 OfferId,
		UGambitItemDefinition* ItemDefinition,
		const int32 Price,
		const bool bUsesSharedPool = false)
	{
		FGambitShopOffer Offer;
		Offer.OfferId = OfferId;
		Offer.ItemDefinition = ItemDefinition;
		Offer.BasePrice = Price;
		Offer.Price = Price;
		Offer.bUsesSharedPool = bUsesSharedPool;
		return Offer;
	}

	struct FGambitPCShellFlowWorld
	{
		UWorld* World = nullptr;
		AGambitGameState* GameState = nullptr;
		UGambitRoundFlowComponent* RoundFlow = nullptr;
		TArray<AGambitPlayerState*> Players;

		void Destroy()
		{
			if (World)
			{
				World->DestroyWorld(false);
				World = nullptr;
			}
		}
	};

	bool BuildPCShellFlowWorld(FAutomationTestBase& Test, const int32 PlayerCount, FGambitPCShellFlowWorld& OutWorld)
	{
		OutWorld.World = UWorld::CreateWorld(EWorldType::Game, false);
		if (!Test.TestNotNull(TEXT("PC shell test world is created"), OutWorld.World))
		{
			return false;
		}

		OutWorld.GameState = OutWorld.World->SpawnActor<AGambitGameState>();
		if (!Test.TestNotNull(TEXT("PC shell test GameState is created"), OutWorld.GameState))
		{
			OutWorld.Destroy();
			return false;
		}

		OutWorld.RoundFlow = NewObject<UGambitRoundFlowComponent>(OutWorld.GameState);
		if (!Test.TestNotNull(TEXT("PC shell test RoundFlow is created"), OutWorld.RoundFlow))
		{
			OutWorld.Destroy();
			return false;
		}

		OutWorld.Players.Reserve(PlayerCount);
		for (int32 PlayerIndex = 0; PlayerIndex < PlayerCount; ++PlayerIndex)
		{
			AGambitPlayerState* PlayerState = OutWorld.World->SpawnActor<AGambitPlayerState>();
			if (!Test.TestNotNull(FString::Printf(TEXT("PC shell test player %d is created"), PlayerIndex), PlayerState))
			{
				OutWorld.Destroy();
				return false;
			}

			PlayerState->SetPlayerId(PlayerIndex);
			PlayerState->SetPlayerName(FString::Printf(TEXT("Player %d"), PlayerIndex + 1));
			PlayerState->InitializeForMatch();
			OutWorld.GameState->AddPlayerState(PlayerState);
			OutWorld.Players.Add(PlayerState);
		}

		return true;
	}

	void FreeOneConsumableSlotIfNeeded(AGambitPlayerState* PlayerState)
	{
		UGambitInventoryComponent* InventoryComponent = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
		if (!InventoryComponent)
		{
			return;
		}

		if (!InventoryComponent->HasAvailableConsumableSlot() && InventoryComponent->GetConsumableSlotsUsed() > 0)
		{
			UGambitConsumableDefinition* RemovedConsumableDefinition = nullptr;
			InventoryComponent->RemoveConsumableAtSlot(0, RemovedConsumableDefinition);
		}
	}

	int32 FindConsumableSlotIndex(
		const AGambitPlayerState* PlayerState,
		const UGambitConsumableDefinition* ConsumableDefinition)
	{
		if (!PlayerState || !ConsumableDefinition)
		{
			return INDEX_NONE;
		}

		return PlayerState->GetConsumableSlotsRef().IndexOfByPredicate(
			[ConsumableDefinition](const FGambitConsumableRuntimeSlot& Slot)
			{
				return Slot.Definition == ConsumableDefinition;
			});
	}

	void MarkAllPlayersReady(
		UGambitRoundFlowComponent* RoundFlow,
		const TArray<AGambitPlayerState*>& Players)
	{
		if (!RoundFlow)
		{
			return;
		}

		for (AGambitPlayerState* PlayerState : Players)
		{
			RoundFlow->SetPlayerReady(PlayerState, true);
		}
	}

	bool ContainsShellLine(const TArray<FString>& Lines, const FString& Needle)
	{
		return Lines.ContainsByPredicate([&Needle](const FString& Line)
		{
			return Line.Contains(Needle);
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitPCShellRoundFlowSetupTest,
	"GrandpaGambit.PCShell.RoundFlowSetup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitPCShellRoundFlowSetupTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitRoundFlowComponent* RoundFlow = NewObject<UGambitRoundFlowComponent>();
	TestNotNull(TEXT("Round flow component can be created for setup smoke"), RoundFlow);

	FGambitMatchSetupConfig MatchSetup;
	MatchSetup.LocalPlayerCount = 4;
	MatchSetup.RoundCount = 3;
	RoundFlow->ApplyMatchSetup(MatchSetup);

	TestEqual(TEXT("Configured player count is retained"), RoundFlow->GetActiveMatchSetup().LocalPlayerCount, 4);
	TestEqual(TEXT("Configured round count is retained"), RoundFlow->GetActiveRoundCount(), 3);

	MatchSetup.LocalPlayerCount = 99;
	MatchSetup.RoundCount = 0;
	RoundFlow->ApplyMatchSetup(MatchSetup);

	TestEqual(TEXT("Player count clamps to supported max"), RoundFlow->GetActiveMatchSetup().LocalPlayerCount, 4);
	TestEqual(TEXT("Round count clamps to at least one"), RoundFlow->GetActiveRoundCount(), 1);

	for (const int32 SupportedPlayerCount : {2, 3, 4})
	{
		MatchSetup.LocalPlayerCount = SupportedPlayerCount;
		MatchSetup.RoundCount = SupportedPlayerCount + 1;
		RoundFlow->ApplyMatchSetup(MatchSetup);

		TestEqual(
			FString::Printf(TEXT("Configured player count %d is retained"), SupportedPlayerCount),
			RoundFlow->GetActiveMatchSetup().LocalPlayerCount,
			SupportedPlayerCount);
		TestEqual(
			FString::Printf(TEXT("Configured round count for %d players is retained"), SupportedPlayerCount),
			RoundFlow->GetActiveRoundCount(),
			SupportedPlayerCount + 1);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitPCShellRoundHudDiceComponentTest,
	"GrandpaGambit.PCShell.RoundHud.DiceLockPreservesReroll",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitPCShellRoundHudDiceComponentTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UObject* TestOuter = GetTransientPackage();
	UGambitDiceDefinition* AlwaysFour = MakePCShellFlowTestDie(TestOuter, TEXT("dice.test.shell_four"), { 4 });
	UGambitDiceDefinition* AlwaysSix = MakePCShellFlowTestDie(TestOuter, TEXT("dice.test.shell_six"), { 6 });

	UGambitDiceComponent* DiceComponent = NewObject<UGambitDiceComponent>();
	TArray<TObjectPtr<UGambitDiceDefinition>> OwnedDice;
	OwnedDice.Add(AlwaysFour);
	OwnedDice.Add(AlwaysSix);
	DiceComponent->InitializeDicePool(OwnedDice);

	FRandomStream RandomStream(991);
	DiceComponent->RollAll(RandomStream);
	TestTrue(TEXT("first die can be locked"), DiceComponent->SetDieLocked(0, true));

	const int32 LockedValueBefore = DiceComponent->GetDiceStatesRef()[0].EffectiveValue;
	TestTrue(TEXT("unlocked dice reroll succeeds while one die is locked"), DiceComponent->RollUnlocked(RandomStream));
	TestEqual(TEXT("locked die keeps value through unlocked reroll"), DiceComponent->GetDiceStatesRef()[0].EffectiveValue, LockedValueBefore);
	TestTrue(TEXT("locked die remains locked after unlocked reroll"), DiceComponent->GetDiceStatesRef()[0].bLocked);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitPCShellRoundHudCommandTest,
	"GrandpaGambit.PCShell.RoundHud.Commands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitPCShellRoundHudCommandTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FGambitPCShellFlowWorld TestWorld;
	if (!BuildPCShellFlowWorld(*this, 2, TestWorld))
	{
		return false;
	}

	UGambitRoundFlowComponent* RoundFlow = TestWorld.RoundFlow;
	AGambitPlayerState* PlayerState = TestWorld.Players.IsValidIndex(0) ? TestWorld.Players[0] : nullptr;
	if (!TestNotNull(TEXT("round flow exists for HUD command smoke"), RoundFlow)
		|| !TestNotNull(TEXT("player exists for HUD command smoke"), PlayerState))
	{
		TestWorld.Destroy();
		return false;
	}

	TestWorld.GameState->SetCurrentPhase(EGambitRoundPhase::SelectionReroll);

	const FGambitRoundCommandResult LockResult = RoundFlow->RequestSetDieLockedDetailed(PlayerState, 0, true);
	TestTrue(TEXT("lock command succeeds during Selection/Reroll"), LockResult.bSuccess);
	TestEqual(TEXT("lock command reports success"), LockResult.Status, EGambitRoundCommandStatus::Success);
	TestTrue(TEXT("die is locked after command"), PlayerState->GetDiceStatesRef()[0].bLocked);
	const int32 LockedValueBefore = PlayerState->GetDiceStatesRef()[0].EffectiveValue;

	const FGambitRoundCommandResult FirstReroll = RoundFlow->RequestRerollDetailed(PlayerState);
	TestTrue(TEXT("reroll command succeeds during Selection/Reroll"), FirstReroll.bSuccess);
	TestEqual(TEXT("first reroll consumes one reroll"), RoundFlow->GetRerollsUsedForPlayer(PlayerState), 1);
	TestEqual(TEXT("locked die keeps value after round flow reroll"), PlayerState->GetDiceStatesRef()[0].EffectiveValue, LockedValueBefore);
	TestTrue(TEXT("locked die stays locked after round flow reroll"), PlayerState->GetDiceStatesRef()[0].bLocked);

	const int32 RerollLimit = RoundFlow->GetMaxRerollsForPlayer(PlayerState);
	for (int32 RerollIndex = RoundFlow->GetRerollsUsedForPlayer(PlayerState); RerollIndex < RerollLimit; ++RerollIndex)
	{
		const FGambitRoundCommandResult AdditionalReroll = RoundFlow->RequestRerollDetailed(PlayerState);
		TestTrue(FString::Printf(TEXT("reroll %d succeeds before limit"), RerollIndex + 1), AdditionalReroll.bSuccess);
	}

	const FGambitRoundCommandResult LimitedReroll = RoundFlow->RequestRerollDetailed(PlayerState);
	TestFalse(TEXT("reroll command is refused at limit"), LimitedReroll.bSuccess);
	TestEqual(TEXT("reroll limit refusal is typed"), LimitedReroll.Status, EGambitRoundCommandStatus::RerollLimitReached);
	TestEqual(TEXT("reroll count remains capped"), RoundFlow->GetRerollsUsedForPlayer(PlayerState), RerollLimit);

	TestWorld.GameState->SetCurrentPhase(EGambitRoundPhase::Action);
	const FGambitRoundCommandResult LockOutOfPhase = RoundFlow->RequestSetDieLockedDetailed(PlayerState, 1, true);
	TestFalse(TEXT("lock command is refused outside Selection/Reroll"), LockOutOfPhase.bSuccess);
	TestEqual(TEXT("lock outside phase refusal is typed"), LockOutOfPhase.Status, EGambitRoundCommandStatus::InvalidPhase);

	const FGambitRoundCommandResult RerollOutOfPhase = RoundFlow->RequestRerollDetailed(PlayerState);
	TestFalse(TEXT("reroll command is refused outside Selection/Reroll"), RerollOutOfPhase.bSuccess);
	TestEqual(TEXT("reroll outside phase refusal is typed"), RerollOutOfPhase.Status, EGambitRoundCommandStatus::InvalidPhase);

	TestWorld.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitPCShellRoundHudActionConsumableFeedbackTest,
	"GrandpaGambit.PCShell.RoundHud.ActionConsumableFeedback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitPCShellRoundHudActionConsumableFeedbackTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FGambitPCShellFlowWorld TestWorld;
	if (!BuildPCShellFlowWorld(*this, 2, TestWorld))
	{
		return false;
	}

	AGambitPlayerState* PlayerState = TestWorld.Players.IsValidIndex(0) ? TestWorld.Players[0] : nullptr;
	if (!TestNotNull(TEXT("player exists for consumable feedback smoke"), PlayerState))
	{
		TestWorld.Destroy();
		return false;
	}

	UGambitConsumableDefinition* ConsumableDefinition = MakePCShellFlowTestConsumable(
		GetTransientPackage(),
		TEXT("consumable.test.action_feedback"),
		TEXT("Action Coffee"),
		{ EGambitRoundPhase::Action });
	TestTrue(TEXT("player receives action consumable"), PlayerState->GetInventoryComponent()->AddConsumable(ConsumableDefinition));

	const TArray<FString> ActionLines = UGambitPCShellWidget::BuildConsumableFeedbackLines(PlayerState, EGambitRoundPhase::Action);
	TestTrue(TEXT("consumable section is visible"), ContainsShellLine(ActionLines, TEXT("Consumables:")));
	TestTrue(TEXT("consumable display name is visible"), ContainsShellLine(ActionLines, TEXT("Action Coffee")));
	TestTrue(TEXT("consumable id is visible"), ContainsShellLine(ActionLines, TEXT("consumable.test.action_feedback")));
	TestTrue(TEXT("stack count is visible"), ContainsShellLine(ActionLines, TEXT("Stack 1")));
	TestTrue(TEXT("usable state is visible during action"), ContainsShellLine(ActionLines, TEXT("Ready")));

	const TArray<FString> RewardLines = UGambitPCShellWidget::BuildConsumableFeedbackLines(PlayerState, EGambitRoundPhase::Reward);
	TestTrue(TEXT("unusable phase state is visible"), ContainsShellLine(RewardLines, TEXT("Unavailable in phase")));

	TestWorld.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitPCShellRoundHudTargetSelectionFeedbackTest,
	"GrandpaGambit.PCShell.RoundHud.TargetSelectionFeedback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitPCShellRoundHudTargetSelectionFeedbackTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FGambitPCShellFlowWorld TestWorld;
	if (!BuildPCShellFlowWorld(*this, 2, TestWorld))
	{
		return false;
	}

	AGambitPlayerState* PlayerOne = TestWorld.Players.IsValidIndex(0) ? TestWorld.Players[0] : nullptr;
	AGambitPlayerState* PlayerTwo = TestWorld.Players.IsValidIndex(1) ? TestWorld.Players[1] : nullptr;
	if (!TestNotNull(TEXT("player one exists for target selection HUD smoke"), PlayerOne)
		|| !TestNotNull(TEXT("player two exists for target selection HUD smoke"), PlayerTwo))
	{
		TestWorld.Destroy();
		return false;
	}

	UGambitConsumableDefinition* ConsumableDefinition = MakePCShellFlowTestConsumable(
		GetTransientPackage(),
		TEXT("consumable.test.target_feedback"),
		TEXT("Target Coffee"),
		{ EGambitRoundPhase::Action });

	FGambitTargetSelectionRequest Request;
	Request.RequestId = FGuid::NewGuid();
	Request.RequestingPlayer = PlayerOne;
	Request.SourceConsumableSlotIndex = 0;
	Request.SourceItemDefinition = ConsumableDefinition;
	Request.SourceItemId = ConsumableDefinition->GetResolvedItemId();
	Request.TargetRuleId = TEXT("target.opponent");
	Request.SelectionType = EGambitTargetSelectionType::Player;
	Request.CurrentPhase = EGambitRoundPhase::Action;
	Request.bRequiresExplicitSelection = true;
	Request.bHasValidOptions = true;
	Request.DebugText = TEXT("Target Coffee requires one opponent.");

	FGambitTargetSelectionOption Option;
	Option.OptionId = 7;
	Option.SelectionType = EGambitTargetSelectionType::Player;
	Option.TargetPlayer = PlayerTwo;
	Option.TargetPlayerIndex = 1;
	Option.TargetRuleId = TEXT("target.opponent");
	Option.bValid = true;
	Option.Label = TEXT("P2 Player 2");
	Option.DebugText = TEXT("Validated by target.opponent");
	Request.Options.Add(Option);

	AGambitPlayerController* PlayerController = TestWorld.World->SpawnActor<AGambitPlayerController>();
	if (!TestNotNull(TEXT("controller exists for target selection HUD smoke"), PlayerController))
	{
		TestWorld.Destroy();
		return false;
	}

	TestTrue(TEXT("controller starts target selection"), PlayerController->StartTargetSelection(Request));
	const TArray<FString> Lines = UGambitPCShellWidget::BuildTargetSelectionFeedbackLines(PlayerController);
	TestTrue(TEXT("target selection header is visible"), ContainsShellLine(Lines, TEXT("Target Selection:")));
	TestTrue(TEXT("target request summary is visible"), ContainsShellLine(Lines, TEXT("Target Coffee requires one opponent")));
	TestTrue(TEXT("selected option is visible"), ContainsShellLine(Lines, TEXT("Selected 7")));
	TestTrue(TEXT("target option label is visible"), ContainsShellLine(Lines, TEXT("P2 Player 2")));
	TestTrue(TEXT("target rule is visible"), ContainsShellLine(Lines, TEXT("target.opponent")));
	TestTrue(TEXT("selection requested event is recorded"), PlayerOne->HasEventThisRound(EGambitRoundGameplayEventType::TargetSelectionRequested));

	TestWorld.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitPCShellRoundHudResolutionFeedbackTest,
	"GrandpaGambit.PCShell.RoundHud.ResolutionFeedback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitPCShellRoundHudResolutionFeedbackTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FGambitPCShellFlowWorld TestWorld;
	if (!BuildPCShellFlowWorld(*this, 2, TestWorld))
	{
		return false;
	}

	AGambitPlayerState* PlayerState = TestWorld.Players.IsValidIndex(0) ? TestWorld.Players[0] : nullptr;
	if (!TestNotNull(TEXT("player exists for resolution feedback smoke"), PlayerState))
	{
		TestWorld.Destroy();
		return false;
	}

	FGambitScoreBreakdown ProducedBreakdown;
	ProducedBreakdown.Combination = EGambitCombinationType::FullHouse;
	ProducedBreakdown.BaseCombinationScore = 25;
	ProducedBreakdown.DiceSum = 18;
	ProducedBreakdown.DiceContributionMultiplierBonus = 3.0f;
	ProducedBreakdown.AdjustedDiceSum = 21.0f;
	ProducedBreakdown.RawScore = 43;
	ProducedBreakdown.AdditiveBonus = 7.0f;
	ProducedBreakdown.Multiplier = 2.0f;
	ProducedBreakdown.ScoreBeforeCap = 100.0f;
	ProducedBreakdown.ScoreAfterCap = 90.0f;
	ProducedBreakdown.FinalScore = 777;
	PlayerState->ApplyRoundScore(ProducedBreakdown);

	FGambitRoundGameplayEvent ScoredEvent;
	ScoredEvent.EventType = EGambitRoundGameplayEventType::RoundScored;
	ScoredEvent.Outcome = EGambitRoundGameplayEventOutcome::Applied;
	ScoredEvent.Reason = TEXT("Round score finalized at 777");
	ScoredEvent.NumericDelta = 777.0f;
	PlayerState->AddRoundEvent(ScoredEvent);

	FGambitDebugScoreLine ScoreLine;
	ScoreLine.LineType = EGambitDebugScoreLineType::Additive;
	ScoreLine.SourceName = TEXT("Lucky module");
	ScoreLine.Summary = TEXT("Lucky module adds +7 score");
	PlayerState->AddDebugScoreLine(ScoreLine);

	const TArray<FString> Lines = UGambitPCShellWidget::BuildScoreFeedbackLines(PlayerState);
	TestTrue(TEXT("resolution line is visible once scoring is finalized"), ContainsShellLine(Lines, TEXT("Resolution:")));
	TestTrue(TEXT("combination result is visible"), ContainsShellLine(Lines, TEXT("Full House")));
	TestTrue(TEXT("raw score is read from produced breakdown"), ContainsShellLine(Lines, TEXT("Raw 43")));
	TestTrue(TEXT("final score is read from produced breakdown"), ContainsShellLine(Lines, TEXT("Final 777")));
	TestTrue(TEXT("score effect breakdown line is visible"), ContainsShellLine(Lines, TEXT("Lucky module adds +7 score")));
	TestFalse(TEXT("HUD helper does not replace final score with raw score"), ContainsShellLine(Lines, TEXT("Final 43")));

	TestWorld.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitPCShellRoundHudRewardRankingFeedbackTest,
	"GrandpaGambit.PCShell.RoundHud.RewardRankingFeedback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitPCShellRoundHudRewardRankingFeedbackTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FGambitPCShellFlowWorld TestWorld;
	if (!BuildPCShellFlowWorld(*this, 2, TestWorld))
	{
		return false;
	}

	AGambitPlayerState* PlayerOne = TestWorld.Players.IsValidIndex(0) ? TestWorld.Players[0] : nullptr;
	AGambitPlayerState* PlayerTwo = TestWorld.Players.IsValidIndex(1) ? TestWorld.Players[1] : nullptr;
	if (!TestNotNull(TEXT("player one exists for reward/ranking smoke"), PlayerOne)
		|| !TestNotNull(TEXT("player two exists for reward/ranking smoke"), PlayerTwo))
	{
		TestWorld.Destroy();
		return false;
	}

	FGambitDebugGoldLine BaseRewardLine;
	BaseRewardLine.LineType = EGambitDebugGoldLineType::BaseReward;
	BaseRewardLine.ActualDelta = 4;
	BaseRewardLine.RequestedDelta = 4;
	BaseRewardLine.GoldBefore = 10;
	BaseRewardLine.GoldAfter = 14;
	BaseRewardLine.Summary = TEXT("Base reward from round score: +4 gold");
	PlayerOne->AddDebugGoldLine(BaseRewardLine);

	FGambitDebugGoldLine InterestLine;
	InterestLine.LineType = EGambitDebugGoldLineType::Interest;
	InterestLine.ActualDelta = 1;
	InterestLine.RequestedDelta = 1;
	InterestLine.GoldBefore = 14;
	InterestLine.GoldAfter = 15;
	InterestLine.Summary = TEXT("Interest from saved gold: +1 gold");
	PlayerOne->AddDebugGoldLine(InterestLine);

	PlayerOne->AddVictoryPoints(5);
	PlayerTwo->AddVictoryPoints(3);

	TArray<FGambitRankingEntry> Ranking;
	FGambitRankingEntry FirstEntry;
	FirstEntry.Rank = 1;
	FirstEntry.PlayerState = PlayerOne;
	FirstEntry.RoundScore = 90;
	FirstEntry.VictoryPointsGranted = 5;
	Ranking.Add(FirstEntry);

	FGambitRankingEntry SecondEntry;
	SecondEntry.Rank = 2;
	SecondEntry.PlayerState = PlayerTwo;
	SecondEntry.RoundScore = 40;
	SecondEntry.VictoryPointsGranted = 3;
	Ranking.Add(SecondEntry);
	TestWorld.GameState->SetRoundRankingSnapshot(Ranking);

	const TArray<FString> RewardLines = UGambitPCShellWidget::BuildRewardFeedbackLines(PlayerOne, 5);
	TestTrue(TEXT("reward summary exposes total gold gained"), ContainsShellLine(RewardLines, TEXT("Gold +5")));
	TestTrue(TEXT("reward summary exposes VP gained when available"), ContainsShellLine(RewardLines, TEXT("VP +5")));
	TestTrue(TEXT("base reward line is visible"), ContainsShellLine(RewardLines, TEXT("Base reward from round score")));
	TestTrue(TEXT("interest reward line is visible"), ContainsShellLine(RewardLines, TEXT("Interest from saved gold")));

	const TArray<FString> RankingLines = UGambitPCShellWidget::BuildRankingFeedbackLines(TestWorld.GameState);
	TestTrue(TEXT("round ranking header is visible"), ContainsShellLine(RankingLines, TEXT("Round Ranking")));
	TestTrue(TEXT("rank one is visible"), ContainsShellLine(RankingLines, TEXT("#1 Player 1")));
	TestTrue(TEXT("rank one score is visible"), ContainsShellLine(RankingLines, TEXT("Score 90")));
	TestTrue(TEXT("rank one VP reward is visible"), ContainsShellLine(RankingLines, TEXT("VP +5")));

	TestWorld.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitPCShellRoundHudLedgerFeedbackTest,
	"GrandpaGambit.PCShell.RoundHud.LedgerFeedback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitPCShellRoundHudLedgerFeedbackTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FGambitPCShellFlowWorld TestWorld;
	if (!BuildPCShellFlowWorld(*this, 2, TestWorld))
	{
		return false;
	}

	AGambitPlayerState* PlayerState = TestWorld.Players.IsValidIndex(0) ? TestWorld.Players[0] : nullptr;
	if (!TestNotNull(TEXT("player exists for ledger feedback smoke"), PlayerState))
	{
		TestWorld.Destroy();
		return false;
	}

	FGambitRoundGameplayEvent IgnoredScoredEvent;
	IgnoredScoredEvent.EventType = EGambitRoundGameplayEventType::RoundScored;
	IgnoredScoredEvent.Reason = TEXT("Round scored should not be in important ledger summary");
	PlayerState->AddRoundEvent(IgnoredScoredEvent);

	FGambitRoundGameplayEvent AppliedEvent;
	AppliedEvent.EventType = EGambitRoundGameplayEventType::EffectApplied;
	AppliedEvent.Outcome = EGambitRoundGameplayEventOutcome::Applied;
	AppliedEvent.Reason = TEXT("Module applied effect");
	PlayerState->AddRoundEvent(AppliedEvent);

	FGambitRoundGameplayEvent ConsumableUsedEvent;
	ConsumableUsedEvent.EventType = EGambitRoundGameplayEventType::ConsumableUsed;
	ConsumableUsedEvent.Outcome = EGambitRoundGameplayEventOutcome::Applied;
	ConsumableUsedEvent.Reason = TEXT("Consumable used in Action phase");
	PlayerState->AddRoundEvent(ConsumableUsedEvent);

	FGambitRoundGameplayEvent PreventedEvent;
	PreventedEvent.EventType = EGambitRoundGameplayEventType::EffectPrevented;
	PreventedEvent.Outcome = EGambitRoundGameplayEventOutcome::Prevented;
	PreventedEvent.Reason = TEXT("Gold loss prevented by Safe");
	PreventedEvent.NegativeCategoryIds = { FName(TEXT("GoldLoss")) };
	PlayerState->AddRoundEvent(PreventedEvent);

	FGambitRoundGameplayEvent GoldChangedEvent;
	GoldChangedEvent.EventType = EGambitRoundGameplayEventType::GoldChanged;
	GoldChangedEvent.Outcome = EGambitRoundGameplayEventOutcome::Applied;
	GoldChangedEvent.Reason = TEXT("Reward changed gold");
	GoldChangedEvent.NumericDelta = 3.0f;
	PlayerState->AddRoundEvent(GoldChangedEvent);

	FGambitRoundGameplayEvent ScoreModifierEvent;
	ScoreModifierEvent.EventType = EGambitRoundGameplayEventType::ScoreModifierApplied;
	ScoreModifierEvent.Outcome = EGambitRoundGameplayEventOutcome::Applied;
	ScoreModifierEvent.Reason = TEXT("Multiplier applied");
	ScoreModifierEvent.NumericDelta = 2.0f;
	PlayerState->AddRoundEvent(ScoreModifierEvent);

	FGambitRoundGameplayEvent DieModifiedEvent;
	DieModifiedEvent.EventType = EGambitRoundGameplayEventType::DieModified;
	DieModifiedEvent.Outcome = EGambitRoundGameplayEventOutcome::Applied;
	DieModifiedEvent.Reason = TEXT("Die value modified");
	DieModifiedEvent.TargetDieHandIndex = 1;
	PlayerState->AddRoundEvent(DieModifiedEvent);

	FGambitRoundGameplayEvent DieRemovedEvent;
	DieRemovedEvent.EventType = EGambitRoundGameplayEventType::DieDestroyedOrRemoved;
	DieRemovedEvent.Outcome = EGambitRoundGameplayEventOutcome::Applied;
	DieRemovedEvent.Reason = TEXT("Die removed");
	DieRemovedEvent.TargetDieHandIndex = 2;
	PlayerState->AddRoundEvent(DieRemovedEvent);

	FGambitRoundGameplayEvent TargetInvalidEvent;
	TargetInvalidEvent.EventType = EGambitRoundGameplayEventType::TargetSelectionInvalid;
	TargetInvalidEvent.Outcome = EGambitRoundGameplayEventOutcome::Failed;
	TargetInvalidEvent.Reason = TEXT("Target option invalid");
	PlayerState->AddRoundEvent(TargetInvalidEvent);

	const TArray<FString> LedgerLines = UGambitPCShellWidget::BuildLedgerFeedbackLines(PlayerState, 10);
	TestTrue(TEXT("effect applied event is visible"), ContainsShellLine(LedgerLines, TEXT("EffectApplied")));
	TestTrue(TEXT("consumable used event is visible"), ContainsShellLine(LedgerLines, TEXT("ConsumableUsed")));
	TestTrue(TEXT("effect prevented event is visible"), ContainsShellLine(LedgerLines, TEXT("EffectPrevented")));
	TestTrue(TEXT("B6.1 defense reason is visible when present"), ContainsShellLine(LedgerLines, TEXT("Gold loss prevented by Safe")));
	TestTrue(TEXT("negative category is visible"), ContainsShellLine(LedgerLines, TEXT("GoldLoss")));
	TestTrue(TEXT("gold changed event is visible"), ContainsShellLine(LedgerLines, TEXT("GoldChanged")));
	TestTrue(TEXT("score modifier event is visible"), ContainsShellLine(LedgerLines, TEXT("ScoreModifierApplied")));
	TestTrue(TEXT("die modified event is visible"), ContainsShellLine(LedgerLines, TEXT("DieModified")));
	TestTrue(TEXT("die removed event is visible"), ContainsShellLine(LedgerLines, TEXT("DieDestroyedOrRemoved")));
	TestTrue(TEXT("target invalid event is visible"), ContainsShellLine(LedgerLines, TEXT("TargetSelectionInvalid")));
	TestFalse(TEXT("unimportant round scored event is not part of ledger summary"), ContainsShellLine(LedgerLines, TEXT("RoundScored")));

	TestWorld.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitPCShellRoundHudShopPurchaseTest,
	"GrandpaGambit.PCShell.RoundHud.ShopPurchase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitPCShellRoundHudShopPurchaseTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FGambitPCShellFlowWorld TestWorld;
	if (!BuildPCShellFlowWorld(*this, 4, TestWorld))
	{
		return false;
	}

	AGambitPlayerState* Buyer = TestWorld.Players.IsValidIndex(0) ? TestWorld.Players[0] : nullptr;
	AGambitPlayerState* LowGoldPlayer = TestWorld.Players.IsValidIndex(1) ? TestWorld.Players[1] : nullptr;
	AGambitPlayerState* SharedPoolPlayer = TestWorld.Players.IsValidIndex(2) ? TestWorld.Players[2] : nullptr;
	AGambitPlayerState* FullInventoryPlayer = TestWorld.Players.IsValidIndex(3) ? TestWorld.Players[3] : nullptr;
	if (!TestNotNull(TEXT("buyer exists for shop purchase smoke"), Buyer)
		|| !TestNotNull(TEXT("low-gold player exists for shop refusal smoke"), LowGoldPlayer)
		|| !TestNotNull(TEXT("shared-pool player exists for shop refusal smoke"), SharedPoolPlayer)
		|| !TestNotNull(TEXT("full-inventory player exists for shop refusal smoke"), FullInventoryPlayer))
	{
		TestWorld.Destroy();
		return false;
	}

	UObject* TestOuter = GetTransientPackage();
	UGambitModuleDefinition* CoffeeModule = MakePCShellFlowTestModule(TestOuter, TEXT("module.test.shop_coffee"), TEXT("Shop Coffee"), 5);
	UGambitModuleDefinition* TeaModule = MakePCShellFlowTestModule(TestOuter, TEXT("module.test.shop_tea"), TEXT("Shop Tea"), 7, EGambitItemRarity::Uncommon);
	UGambitModuleDefinition* BiscuitModule = MakePCShellFlowTestModule(TestOuter, TEXT("module.test.shop_biscuit"), TEXT("Shop Biscuit"), 9, EGambitItemRarity::Rare);

	TestWorld.GameState->SetCurrentPhase(EGambitRoundPhase::Shop);
	Buyer->GetEconomyComponent()->AddGold(20);
	Buyer->GetShopComponent()->SetCurrentOffers({
		MakePCShellFlowTestOffer(0, CoffeeModule, CoffeeModule->Cost),
		MakePCShellFlowTestOffer(1, TeaModule, TeaModule->Cost),
		MakePCShellFlowTestOffer(2, BiscuitModule, BiscuitModule->Cost)
	});

	const TArray<FGambitShopOfferSnapshot> BuyerSnapshotsBefore = TestWorld.RoundFlow->BuildShopOfferSnapshots(Buyer);
	TestEqual(TEXT("shop snapshot exposes three offers"), BuyerSnapshotsBefore.Num(), 3);
	const TArray<FString> ShopLinesBefore = UGambitPCShellWidget::BuildShopFeedbackLines(Buyer, BuyerSnapshotsBefore);
	TestTrue(TEXT("shop header exposes current gold"), ContainsShellLine(ShopLinesBefore, TEXT("Shop: Gold 20")));
	TestTrue(TEXT("offer name is visible"), ContainsShellLine(ShopLinesBefore, TEXT("Shop Coffee")));
	TestTrue(TEXT("offer id is visible"), ContainsShellLine(ShopLinesBefore, TEXT("module.test.shop_coffee")));
	TestTrue(TEXT("offer type is visible"), ContainsShellLine(ShopLinesBefore, TEXT("Type Module")));
	TestTrue(TEXT("offer rarity is visible"), ContainsShellLine(ShopLinesBefore, TEXT("Rarity Common")));
	TestTrue(TEXT("offer cost is visible"), ContainsShellLine(ShopLinesBefore, TEXT("Cost 5 gold")));
	TestTrue(TEXT("buyable state is visible"), ContainsShellLine(ShopLinesBefore, TEXT("Buyable")));

	const FGambitRoundCommandResult PurchaseResult = TestWorld.RoundFlow->RequestPurchaseOfferDetailed(Buyer, 0);
	TestTrue(TEXT("shop offer purchase succeeds during Shop"), PurchaseResult.bSuccess);
	TestEqual(TEXT("shop purchase reports success"), PurchaseResult.Status, EGambitRoundCommandStatus::Success);
	TestEqual(TEXT("purchase spends the offer cost"), Buyer->GetCurrentGold(), 15);
	TestTrue(TEXT("purchased module is active"), Buyer->GetActiveModulesRef().Contains(CoffeeModule));
	TestTrue(TEXT("purchased module definition is owned"), Buyer->GetInventoryComponent()->HasItemDefinition(CoffeeModule));
	if (const FGambitInventoryItemInstance* PurchasedInstance = Buyer->GetInventoryComponent()->FindFirstItemInstanceByDefinition(CoffeeModule))
	{
		TestEqual(TEXT("runtime instance records shop purchase source"), PurchasedInstance->SourcePurchaseId, FName(TEXT("shop.offer.0")));
	}
	else
	{
		AddError(TEXT("shop purchase did not create a runtime inventory instance"));
	}

	const TArray<FString> InventoryLinesAfter = UGambitPCShellWidget::BuildInventorySummaryLines(Buyer);
	TestTrue(TEXT("inventory summary exposes module count after purchase"), ContainsShellLine(InventoryLinesAfter, TEXT("Modules")));
	TestTrue(TEXT("inventory summary includes purchased module"), ContainsShellLine(InventoryLinesAfter, TEXT("Shop Coffee")));

	const TArray<FGambitShopOfferSnapshot> BuyerSnapshotsAfter = TestWorld.RoundFlow->BuildShopOfferSnapshots(Buyer);
	TestTrue(TEXT("remaining offers are shown as unavailable after purchase"), BuyerSnapshotsAfter.ContainsByPredicate([](const FGambitShopOfferSnapshot& Snapshot)
	{
		return !Snapshot.bCanPurchase && Snapshot.UnavailableReason.Contains(TEXT("Purchase already made"));
	}));
	const FGambitRoundCommandResult SecondPurchaseResult = TestWorld.RoundFlow->RequestPurchaseOfferDetailed(Buyer, 1);
	TestFalse(TEXT("second purchase in same shop is refused"), SecondPurchaseResult.bSuccess);
	TestEqual(TEXT("second purchase refusal is typed"), SecondPurchaseResult.Status, EGambitRoundCommandStatus::PurchaseAlreadyMade);
	TestEqual(TEXT("second purchase does not spend gold"), Buyer->GetCurrentGold(), 15);

	UGambitModuleDefinition* ExpensiveModule = MakePCShellFlowTestModule(TestOuter, TEXT("module.test.expensive"), TEXT("Expensive"), 50);
	LowGoldPlayer->GetShopComponent()->SetCurrentOffers({ MakePCShellFlowTestOffer(0, ExpensiveModule, ExpensiveModule->Cost) });
	const FGambitRoundCommandResult InvalidOfferResult = TestWorld.RoundFlow->RequestPurchaseOfferDetailed(LowGoldPlayer, 9);
	TestFalse(TEXT("invalid offer is refused"), InvalidOfferResult.bSuccess);
	TestEqual(TEXT("invalid offer refusal is typed"), InvalidOfferResult.Status, EGambitRoundCommandStatus::InvalidShopOffer);

	const FGambitRoundCommandResult NotEnoughGoldResult = TestWorld.RoundFlow->RequestPurchaseOfferDetailed(LowGoldPlayer, 0);
	TestFalse(TEXT("not enough gold is refused"), NotEnoughGoldResult.bSuccess);
	TestEqual(TEXT("not enough gold refusal is typed"), NotEnoughGoldResult.Status, EGambitRoundCommandStatus::NotEnoughGold);
	TestFalse(TEXT("not enough gold does not add item"), LowGoldPlayer->GetInventoryComponent()->HasItemDefinition(ExpensiveModule));

	UGambitModuleDefinition* SharedModule = MakePCShellFlowTestModule(TestOuter, TEXT("module.test.shared_empty"), TEXT("Shared Empty"), 3);
	SharedModule->bUsesSharedPool = true;
	SharedModule->SharedPoolMaxStock = 1;
	SharedModule->SharedPoolPurchaseLimit = 1;
	FGambitSharedStockEntry SharedStock;
	SharedStock.ItemDefinition = SharedModule;
	SharedStock.MaxStock = 1;
	SharedStock.GlobalPurchaseLimit = 1;
	UGambitSharedPoolComponent* SharedPool = TestWorld.GameState->GetSharedPoolComponent();
	TestNotNull(TEXT("shared pool exists for shop refusal smoke"), SharedPool);
	if (SharedPool)
	{
		SharedPool->InitializeSharedStock({ SharedStock });
		TestTrue(TEXT("test consumes shared stock before purchase"), SharedPool->ConsumeItemStock(SharedModule));
	}
	SharedPoolPlayer->GetEconomyComponent()->AddGold(20);
	SharedPoolPlayer->GetShopComponent()->SetCurrentOffers({ MakePCShellFlowTestOffer(0, SharedModule, SharedModule->Cost, true) });
	const FGambitRoundCommandResult SharedPoolResult = TestWorld.RoundFlow->RequestPurchaseOfferDetailed(SharedPoolPlayer, 0);
	TestFalse(TEXT("unavailable shared pool offer is refused"), SharedPoolResult.bSuccess);
	TestEqual(TEXT("shared pool refusal is typed"), SharedPoolResult.Status, EGambitRoundCommandStatus::SharedPoolUnavailable);

	while (FullInventoryPlayer->GetInventoryComponent()->HasAvailableModuleSlot())
	{
		const int32 FillIndex = FullInventoryPlayer->GetActiveModulesRef().Num();
		UGambitModuleDefinition* FillerModule = MakePCShellFlowTestModule(
			TestOuter,
			*FString::Printf(TEXT("module.test.slot_fill_%d"), FillIndex),
			*FString::Printf(TEXT("Slot Fill %d"), FillIndex),
			1);
		const bool bAddedFiller = FullInventoryPlayer->GetInventoryComponent()->AddModule(FillerModule);
		TestTrue(TEXT("filler module can be added while slots remain"), bAddedFiller);
		if (!bAddedFiller)
		{
			break;
		}
	}
	UGambitModuleDefinition* OverflowModule = MakePCShellFlowTestModule(TestOuter, TEXT("module.test.overflow"), TEXT("Overflow"), 1);
	FullInventoryPlayer->GetEconomyComponent()->AddGold(20);
	FullInventoryPlayer->GetShopComponent()->SetCurrentOffers({ MakePCShellFlowTestOffer(0, OverflowModule, OverflowModule->Cost) });
	const FGambitRoundCommandResult FullInventoryResult = TestWorld.RoundFlow->RequestPurchaseOfferDetailed(FullInventoryPlayer, 0);
	TestFalse(TEXT("full module slots refuse purchase"), FullInventoryResult.bSuccess);
	TestEqual(TEXT("full inventory refusal is typed"), FullInventoryResult.Status, EGambitRoundCommandStatus::InventoryFull);

	TestWorld.GameState->SetCurrentPhase(EGambitRoundPhase::Action);
	const FGambitRoundCommandResult OutOfPhaseResult = TestWorld.RoundFlow->RequestPurchaseOfferDetailed(LowGoldPlayer, 0);
	TestFalse(TEXT("out-of-shop purchase is refused"), OutOfPhaseResult.bSuccess);
	TestEqual(TEXT("out-of-shop purchase refusal is typed"), OutOfPhaseResult.Status, EGambitRoundCommandStatus::InvalidPhase);

	TestWorld.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitPCShellShopContinueFlowTest,
	"GrandpaGambit.PCShell.ShopContinueFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitPCShellShopContinueFlowTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	for (const int32 PlayerCount : {2, 3, 4})
	{
		FGambitPCShellFlowWorld TestWorld;
		if (!BuildPCShellFlowWorld(*this, PlayerCount, TestWorld))
		{
			return false;
		}

		FGambitMatchSetupConfig MatchSetup;
		MatchSetup.LocalPlayerCount = PlayerCount;
		MatchSetup.RoundCount = 2;
		TestWorld.RoundFlow->ApplyMatchSetup(MatchSetup);
		TestWorld.RoundFlow->StartMatchFlow();
		TestEqual(
			FString::Printf(TEXT("%d-player match enters Selection/Reroll after automatic roll"), PlayerCount),
			TestWorld.GameState->GetCurrentPhase(),
			EGambitRoundPhase::SelectionReroll);

		for (AGambitPlayerState* PlayerState : TestWorld.Players)
		{
			TestWorld.RoundFlow->SetPlayerReady(PlayerState, true);
		}
		TestEqual(
			FString::Printf(TEXT("%d-player match advances to Action"), PlayerCount),
			TestWorld.GameState->GetCurrentPhase(),
			EGambitRoundPhase::Action);

		for (AGambitPlayerState* PlayerState : TestWorld.Players)
		{
			TestWorld.RoundFlow->SetPlayerReady(PlayerState, true);
		}
		TestEqual(
			FString::Printf(TEXT("%d-player match reaches Shop after auto resolution/reward/ranking"), PlayerCount),
			TestWorld.GameState->GetCurrentPhase(),
			EGambitRoundPhase::Shop);

		for (AGambitPlayerState* PlayerState : TestWorld.Players)
		{
			TestWorld.RoundFlow->SetPlayerReady(PlayerState, true);
		}
		TestEqual(
			FString::Printf(TEXT("%d-player shop continue reaches next round Selection/Reroll"), PlayerCount),
			TestWorld.GameState->GetCurrentPhase(),
			EGambitRoundPhase::SelectionReroll);
		TestEqual(
			FString::Printf(TEXT("%d-player shop continue increments round index"), PlayerCount),
			TestWorld.GameState->GetCurrentRoundIndex(),
			2);

		TestWorld.Destroy();
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitPCShellFullPlayableLoop2To4Test,
	"GrandpaGambit.PCShell.FullPlayableLoop2To4",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitPCShellFullPlayableLoop2To4Test::RunTest(const FString& Parameters)
{
	(void)Parameters;

	for (const int32 PlayerCount : {2, 3, 4})
	{
		FGambitPCShellFlowWorld TestWorld;
		if (!BuildPCShellFlowWorld(*this, PlayerCount, TestWorld))
		{
			return false;
		}

		FGambitMatchSetupConfig MatchSetup;
		MatchSetup.LocalPlayerCount = PlayerCount;
		MatchSetup.RoundCount = 2;
		TestWorld.RoundFlow->ApplyMatchSetup(MatchSetup);
		TestWorld.GameState->SetMatchLifecycleState(EGambitMatchLifecycleState::InMatch);
		TestWorld.RoundFlow->StartMatchFlow();

		TestEqual(
			FString::Printf(TEXT("%d-player smoke starts round one"), PlayerCount),
			TestWorld.GameState->GetCurrentRoundIndex(),
			1);
		TestEqual(
			FString::Printf(TEXT("%d-player smoke enters Selection/Reroll after Roll"), PlayerCount),
			TestWorld.GameState->GetCurrentPhase(),
			EGambitRoundPhase::SelectionReroll);
		TestEqual(
			FString::Printf(TEXT("%d-player setup is published to GameState"), PlayerCount),
			TestWorld.GameState->GetMatchSetupConfig().LocalPlayerCount,
			PlayerCount);

		AGambitPlayerState* Buyer = TestWorld.Players.IsValidIndex(0) ? TestWorld.Players[0] : nullptr;
		if (!TestNotNull(FString::Printf(TEXT("%d-player smoke buyer exists"), PlayerCount), Buyer))
		{
			TestWorld.Destroy();
			return false;
		}
		TestTrue(
			FString::Printf(TEXT("%d-player smoke buyer has rolled dice"), PlayerCount),
			Buyer->GetDiceStatesRef().Num() > 1);

		const FGambitRoundCommandResult LockResult = TestWorld.RoundFlow->RequestSetDieLockedDetailed(Buyer, 0, true);
		TestTrue(FString::Printf(TEXT("%d-player smoke lock succeeds"), PlayerCount), LockResult.bSuccess);
		TestTrue(FString::Printf(TEXT("%d-player smoke die is locked"), PlayerCount), Buyer->GetDiceStatesRef()[0].bLocked);

		const FGambitRoundCommandResult UnlockResult = TestWorld.RoundFlow->RequestSetDieLockedDetailed(Buyer, 0, false);
		TestTrue(FString::Printf(TEXT("%d-player smoke unlock succeeds"), PlayerCount), UnlockResult.bSuccess);
		TestFalse(FString::Printf(TEXT("%d-player smoke die is unlocked"), PlayerCount), Buyer->GetDiceStatesRef()[0].bLocked);

		const FGambitRoundCommandResult RelockResult = TestWorld.RoundFlow->RequestSetDieLockedDetailed(Buyer, 0, true);
		TestTrue(FString::Printf(TEXT("%d-player smoke relock succeeds"), PlayerCount), RelockResult.bSuccess);
		const int32 LockedValueBeforeReroll = Buyer->GetDiceStatesRef()[0].EffectiveValue;
		const FGambitRoundCommandResult RerollResult = TestWorld.RoundFlow->RequestRerollDetailed(Buyer);
		TestTrue(FString::Printf(TEXT("%d-player smoke reroll succeeds"), PlayerCount), RerollResult.bSuccess);
		TestEqual(
			FString::Printf(TEXT("%d-player smoke locked die keeps value through reroll"), PlayerCount),
			Buyer->GetDiceStatesRef()[0].EffectiveValue,
			LockedValueBeforeReroll);

		FreeOneConsumableSlotIfNeeded(Buyer);
		const FString DirectConsumableId = FString::Printf(TEXT("consumable.test.direct_%d"), PlayerCount);
		UGambitConsumableDefinition* DirectConsumable = MakePCShellFlowSelfGoldConsumable(
			GetTransientPackage(),
			*DirectConsumableId,
			TEXT("Direct Coffee"),
			2.0f);
		TestTrue(
			FString::Printf(TEXT("%d-player smoke direct consumable is added"), PlayerCount),
			Buyer->GetInventoryComponent()->AddConsumable(DirectConsumable));
		const int32 DirectConsumableSlot = FindConsumableSlotIndex(Buyer, DirectConsumable);
		TestTrue(
			FString::Printf(TEXT("%d-player smoke direct consumable slot is found"), PlayerCount),
			DirectConsumableSlot != INDEX_NONE);

		MarkAllPlayersReady(TestWorld.RoundFlow, TestWorld.Players);
		TestEqual(
			FString::Printf(TEXT("%d-player smoke reaches Action"), PlayerCount),
			TestWorld.GameState->GetCurrentPhase(),
			EGambitRoundPhase::Action);

		const int32 GoldBeforeConsumable = Buyer->GetCurrentGold();
		TestTrue(
			FString::Printf(TEXT("%d-player smoke direct consumable succeeds"), PlayerCount),
			TestWorld.RoundFlow->RequestUseConsumable(Buyer, DirectConsumableSlot));
		TestEqual(
			FString::Printf(TEXT("%d-player smoke direct consumable applies gold"), PlayerCount),
			Buyer->GetCurrentGold(),
			GoldBeforeConsumable + 2);
		TestEqual(
			FString::Printf(TEXT("%d-player smoke direct consumable slot is consumed"), PlayerCount),
			FindConsumableSlotIndex(Buyer, DirectConsumable),
			INDEX_NONE);

		MarkAllPlayersReady(TestWorld.RoundFlow, TestWorld.Players);
		TestEqual(
			FString::Printf(TEXT("%d-player smoke reaches Shop after resolution/reward/ranking"), PlayerCount),
			TestWorld.GameState->GetCurrentPhase(),
			EGambitRoundPhase::Shop);
		TestEqual(
			FString::Printf(TEXT("%d-player smoke records round ranking"), PlayerCount),
			TestWorld.GameState->GetRoundRankingSnapshotRef().Num(),
			PlayerCount);

		for (AGambitPlayerState* PlayerState : TestWorld.Players)
		{
			TestTrue(
				FString::Printf(TEXT("%d-player smoke scored player %s"), PlayerCount, *PlayerState->GetPlayerName()),
				PlayerState->HasEventThisRound(EGambitRoundGameplayEventType::RoundScored));
		}

		FreeOneConsumableSlotIfNeeded(Buyer);
		const FString ShopConsumableAId = FString::Printf(TEXT("consumable.test.shop_%d_a"), PlayerCount);
		const FString ShopConsumableBId = FString::Printf(TEXT("consumable.test.shop_%d_b"), PlayerCount);
		UGambitConsumableDefinition* ShopConsumableA = MakePCShellFlowTestConsumable(
			GetTransientPackage(),
			*ShopConsumableAId,
			TEXT("Shop Coffee A"),
			{ EGambitRoundPhase::Action });
		UGambitConsumableDefinition* ShopConsumableB = MakePCShellFlowTestConsumable(
			GetTransientPackage(),
			*ShopConsumableBId,
			TEXT("Shop Coffee B"),
			{ EGambitRoundPhase::Action });
		Buyer->GetEconomyComponent()->AddGold(10);
		Buyer->GetShopComponent()->SetCurrentOffers({
			MakePCShellFlowTestOffer(0, ShopConsumableA, 1),
			MakePCShellFlowTestOffer(1, ShopConsumableB, 1)
		});

		const TArray<FGambitShopOfferSnapshot> ShopSnapshotsBeforePurchase = TestWorld.RoundFlow->BuildShopOfferSnapshots(Buyer);
		TestEqual(
			FString::Printf(TEXT("%d-player smoke exposes injected shop offers"), PlayerCount),
			ShopSnapshotsBeforePurchase.Num(),
			2);
		TestTrue(
			FString::Printf(TEXT("%d-player smoke has a buyable shop offer"), PlayerCount),
			ShopSnapshotsBeforePurchase.ContainsByPredicate([](const FGambitShopOfferSnapshot& Snapshot)
			{
				return Snapshot.bCanPurchase;
			}));

		const int32 GoldBeforePurchase = Buyer->GetCurrentGold();
		const FGambitRoundCommandResult PurchaseResult = TestWorld.RoundFlow->RequestPurchaseOfferDetailed(Buyer, 0);
		TestTrue(FString::Printf(TEXT("%d-player smoke shop purchase succeeds"), PlayerCount), PurchaseResult.bSuccess);
		TestEqual(
			FString::Printf(TEXT("%d-player smoke shop purchase spends gold"), PlayerCount),
			Buyer->GetCurrentGold(),
			GoldBeforePurchase - 1);
		TestTrue(
			FString::Printf(TEXT("%d-player smoke purchased consumable is owned"), PlayerCount),
			Buyer->GetInventoryComponent()->HasItemDefinition(ShopConsumableA));

		const FGambitRoundCommandResult SecondPurchaseResult = TestWorld.RoundFlow->RequestPurchaseOfferDetailed(Buyer, 1);
		TestFalse(FString::Printf(TEXT("%d-player smoke second shop purchase is refused"), PlayerCount), SecondPurchaseResult.bSuccess);
		TestEqual(
			FString::Printf(TEXT("%d-player smoke second purchase refusal is typed"), PlayerCount),
			SecondPurchaseResult.Status,
			EGambitRoundCommandStatus::PurchaseAlreadyMade);

		MarkAllPlayersReady(TestWorld.RoundFlow, TestWorld.Players);
		TestEqual(
			FString::Printf(TEXT("%d-player smoke advances to round two"), PlayerCount),
			TestWorld.GameState->GetCurrentRoundIndex(),
			2);
		TestEqual(
			FString::Printf(TEXT("%d-player smoke round two starts at Selection/Reroll"), PlayerCount),
			TestWorld.GameState->GetCurrentPhase(),
			EGambitRoundPhase::SelectionReroll);
		TestEqual(
			FString::Printf(TEXT("%d-player smoke remains in match before final round end"), PlayerCount),
			TestWorld.GameState->GetMatchLifecycleState(),
			EGambitMatchLifecycleState::InMatch);

		MarkAllPlayersReady(TestWorld.RoundFlow, TestWorld.Players);
		TestEqual(
			FString::Printf(TEXT("%d-player smoke reaches second Action"), PlayerCount),
			TestWorld.GameState->GetCurrentPhase(),
			EGambitRoundPhase::Action);
		MarkAllPlayersReady(TestWorld.RoundFlow, TestWorld.Players);
		TestEqual(
			FString::Printf(TEXT("%d-player smoke reaches final Shop"), PlayerCount),
			TestWorld.GameState->GetCurrentPhase(),
			EGambitRoundPhase::Shop);
		MarkAllPlayersReady(TestWorld.RoundFlow, TestWorld.Players);

		TestEqual(
			FString::Printf(TEXT("%d-player smoke final match clears phase"), PlayerCount),
			TestWorld.GameState->GetCurrentPhase(),
			EGambitRoundPhase::None);
		TestEqual(
			FString::Printf(TEXT("%d-player smoke final match completes"), PlayerCount),
			TestWorld.GameState->GetMatchLifecycleState(),
			EGambitMatchLifecycleState::MatchComplete);
		TestEqual(
			FString::Printf(TEXT("%d-player smoke final ranking has all players"), PlayerCount),
			TestWorld.GameState->GetFinalRankingSnapshotRef().Num(),
			PlayerCount);
		if (TestWorld.GameState->GetFinalRankingSnapshotRef().Num() > 0)
		{
			TestTrue(
				FString::Printf(TEXT("%d-player smoke final winner is flagged"), PlayerCount),
				TestWorld.GameState->GetFinalRankingSnapshotRef()[0].bWinner);
		}

		TestWorld.Destroy();
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitPCShellPhaseCompletionTest,
	"GrandpaGambit.PCShell.PhaseCompletionPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitPCShellPhaseCompletionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TestTrue(TEXT("Shell can start round flow from no phase"), FGambitRoundPhaseRules::IsTransitionAllowed(EGambitRoundPhase::None, EGambitRoundPhase::Roll));
	TestTrue(TEXT("Roll advances to selection/reroll"), FGambitRoundPhaseRules::IsTransitionAllowed(EGambitRoundPhase::Roll, EGambitRoundPhase::SelectionReroll));
	TestTrue(TEXT("Selection/reroll advances to action"), FGambitRoundPhaseRules::IsTransitionAllowed(EGambitRoundPhase::SelectionReroll, EGambitRoundPhase::Action));
	TestTrue(TEXT("Action advances to resolution"), FGambitRoundPhaseRules::IsTransitionAllowed(EGambitRoundPhase::Action, EGambitRoundPhase::Resolution));
	TestTrue(TEXT("Resolution advances to reward"), FGambitRoundPhaseRules::IsTransitionAllowed(EGambitRoundPhase::Resolution, EGambitRoundPhase::Reward));
	TestTrue(TEXT("Reward advances to ranking"), FGambitRoundPhaseRules::IsTransitionAllowed(EGambitRoundPhase::Reward, EGambitRoundPhase::Ranking));
	TestTrue(TEXT("Ranking advances to shop"), FGambitRoundPhaseRules::IsTransitionAllowed(EGambitRoundPhase::Ranking, EGambitRoundPhase::Shop));
	TestTrue(TEXT("Shop advances to round end"), FGambitRoundPhaseRules::IsTransitionAllowed(EGambitRoundPhase::Shop, EGambitRoundPhase::RoundEnd));
	TestTrue(TEXT("Final round end can clear phase for match complete"), FGambitRoundPhaseRules::IsTransitionAllowed(EGambitRoundPhase::RoundEnd, EGambitRoundPhase::None));
	TestFalse(TEXT("Shell must not skip shop directly to match complete"), FGambitRoundPhaseRules::IsTransitionAllowed(EGambitRoundPhase::Shop, EGambitRoundPhase::None));

	return true;
}

#endif
