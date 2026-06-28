#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

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

	return true;
}

#endif
