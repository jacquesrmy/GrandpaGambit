#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/Types/GambitRoundPhaseRules.h"
#include "Game/Flow/GambitRoundFlowComponent.h"

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
