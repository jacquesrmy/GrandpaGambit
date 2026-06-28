#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#include "Dice/Data/GambitDiceDefinition.h"
#include "Core/Types/GambitDebugTypes.h"
#include "Core/Types/GambitRoundGameplayEventTypes.h"
#include "Core/Types/GambitRoundPhaseRules.h"
#include "Game/Flow/GambitRoundFlowComponent.h"
#include "Game/States/GambitGameState.h"
#include "Players/Components/GambitDiceComponent.h"
#include "Players/States/GambitPlayerState.h"
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

	const TArray<FString> LedgerLines = UGambitPCShellWidget::BuildLedgerFeedbackLines(PlayerState, 10);
	TestTrue(TEXT("effect applied event is visible"), ContainsShellLine(LedgerLines, TEXT("EffectApplied")));
	TestTrue(TEXT("effect prevented event is visible"), ContainsShellLine(LedgerLines, TEXT("EffectPrevented")));
	TestTrue(TEXT("B6.1 defense reason is visible when present"), ContainsShellLine(LedgerLines, TEXT("Gold loss prevented by Safe")));
	TestTrue(TEXT("negative category is visible"), ContainsShellLine(LedgerLines, TEXT("GoldLoss")));
	TestTrue(TEXT("gold changed event is visible"), ContainsShellLine(LedgerLines, TEXT("GoldChanged")));
	TestTrue(TEXT("score modifier event is visible"), ContainsShellLine(LedgerLines, TEXT("ScoreModifierApplied")));
	TestTrue(TEXT("die modified event is visible"), ContainsShellLine(LedgerLines, TEXT("DieModified")));
	TestTrue(TEXT("die removed event is visible"), ContainsShellLine(LedgerLines, TEXT("DieDestroyedOrRemoved")));
	TestFalse(TEXT("unimportant round scored event is not part of ledger summary"), ContainsShellLine(LedgerLines, TEXT("RoundScored")));

	TestWorld.Destroy();
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
