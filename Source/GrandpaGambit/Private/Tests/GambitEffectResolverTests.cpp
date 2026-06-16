#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#include "Core/Settings/GambitGameBalanceSettings.h"
#include "Data/Validation/GambitDataValidation.h"
#include "Data/Assets/GambitItemEffectDefinition.h"
#include "Dice/Data/GambitDiceDefinition.h"
#include "Dice/Evaluation/GambitDiceCombinationEvaluator.h"
#include "Items/Consumables/GambitConsumableDefinition.h"
#include "Items/Data/GambitItemDefinition.h"
#include "Items/DiceItems/GambitDiceItemDefinition.h"
#include "Items/Effects/GambitEffectTargetRules.h"
#include "Items/Effects/GambitEffectTargetResolver.h"
#include "Items/Effects/GambitEffectResolver.h"
#include "Items/Modules/GambitModuleDefinition.h"
#include "Players/Components/GambitDiceComponent.h"
#include "Players/Components/GambitEconomyComponent.h"
#include "Players/Components/GambitInventoryComponent.h"
#include "Players/States/GambitPlayerState.h"
#include "Scoring/Calculators/GambitScoreCalculator.h"
#include "Scoring/Calculators/GambitScoreModifierMath.h"
#include "Managers/SharedPool/GambitSharedPoolComponent.h"
#include "Shop/Components/GambitShopComponent.h"
#include "Shop/Data/GambitShopLootTable.h"
#include "Shop/Models/GambitShopOfferGenerator.h"

namespace
{
	FGambitDieRuntimeState MakeTestDie(
		const int32 EffectiveValue,
		const int32 ComboContributionCount = 1,
		const bool bCountsForScoreSum = true,
		const bool bCountsForCombinations = true,
		const int32 HandIndex = 0)
	{
		FGambitDieRuntimeState DieState;
		DieState.InstanceId = HandIndex;
		DieState.HandIndex = HandIndex;
		DieState.RawValue = EffectiveValue;
		DieState.EffectiveValue = EffectiveValue;
		DieState.ScoreContributionValue = EffectiveValue;
		DieState.ComboContributionCount = ComboContributionCount;
		DieState.bCountsForScoreSum = bCountsForScoreSum;
		DieState.bCountsForCombinations = bCountsForCombinations;
		return DieState;
	}

	UGambitDiceDefinition* MakeDiceDefinition(
		UObject* Outer,
		const TCHAR* DiceId,
		const TArray<int32>& Faces,
		const int32 ComboContributionCount = 1,
		const bool bCountsForScoreSum = true,
		const bool bCountsForCombinations = true)
	{
		UGambitDiceDefinition* DiceDefinition = NewObject<UGambitDiceDefinition>(Outer);
		DiceDefinition->DiceId = FName(DiceId);
		DiceDefinition->Faces = Faces;
		DiceDefinition->DefaultComboContributionCount = ComboContributionCount;
		DiceDefinition->bCountsForScoreSum = bCountsForScoreSum;
		DiceDefinition->bCountsForCombinations = bCountsForCombinations;
		return DiceDefinition;
	}

	UGambitItemEffectDefinition* MakeEffectDefinition(
		UObject* Outer,
		const EGambitEffectHook Hook,
		const EGambitItemEffectType EffectType)
	{
		UGambitItemEffectDefinition* EffectDefinition = NewObject<UGambitItemEffectDefinition>(Outer);
		EffectDefinition->Hook = Hook;
		EffectDefinition->EffectType = EffectType;
		return EffectDefinition;
	}

	UGambitItemEffectDefinition* MakePreventNegativeEffectDefinition(
		UObject* Outer,
		const EGambitEffectHook Hook,
		const TArray<EGambitNegativeEffectCategory>& PreventedCategories)
	{
		UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(Outer, Hook, EGambitItemEffectType::PreventNegativeEffect);
		EffectDefinition->EffectId = TEXT("effect.test.prevent_negative");
		EffectDefinition->PreventedNegativeEffectCategories = PreventedCategories;
		return EffectDefinition;
	}

	UGambitItemEffectDefinition* MakeStealGoldEffectDefinition(
		UObject* Outer,
		const bool bAddGoldStealCategory)
	{
		UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(Outer, EGambitEffectHook::ConsumableUse, EGambitItemEffectType::StealGold);
		EffectDefinition->EffectId = TEXT("effect.test.steal_gold");
		EffectDefinition->Amount = 4.0f;
		EffectDefinition->bNegativeEffect = true;
		if (bAddGoldStealCategory)
		{
			EffectDefinition->NegativeEffectCategories.Add(EGambitNegativeEffectCategory::GoldSteal);
		}
		return EffectDefinition;
	}

	struct FGambitGoldStealProtectionTestContext
	{
		FGambitEffectExecutionContext EffectContext;
		UGambitEconomyComponent* SourceEconomy = nullptr;
		UGambitEconomyComponent* TargetEconomy = nullptr;
		int32 SourceGoldBefore = 0;
		int32 TargetGoldBefore = 0;
	};

	FGambitGoldStealProtectionTestContext MakeGoldStealProtectionTestContext()
	{
		FGambitGoldStealProtectionTestContext TestContext;
		TestContext.SourceEconomy = NewObject<UGambitEconomyComponent>();
		TestContext.TargetEconomy = NewObject<UGambitEconomyComponent>();
		TestContext.SourceEconomy->InitializeForMatch();
		TestContext.TargetEconomy->InitializeForMatch();
		TestContext.SourceGoldBefore = TestContext.SourceEconomy->GetCurrentGold();
		TestContext.TargetEconomy->AddGold(10);
		TestContext.TargetGoldBefore = TestContext.TargetEconomy->GetCurrentGold();

		TestContext.EffectContext.Hook = EGambitEffectHook::ConsumableUse;
		TestContext.EffectContext.SourceEconomyComponent = TestContext.SourceEconomy;
		TestContext.EffectContext.TargetEconomyComponent = TestContext.TargetEconomy;
		return TestContext;
	}

	int32 CountPreventedDebugEvents(const FGambitEffectExecutionContext& Context)
	{
		return Context.DebugEffectEvents.FilterByPredicate([](const FGambitDebugEffectEvent& Event)
		{
			return Event.bPrevented;
		}).Num();
	}

	UGambitItemDefinition* MakeShopItem(UObject* Outer, const TCHAR* ItemId, const int32 Cost)
	{
		UGambitItemDefinition* ItemDefinition = NewObject<UGambitItemDefinition>(Outer);
		ItemDefinition->ItemId = FName(ItemId);
		ItemDefinition->Cost = Cost;
		ItemDefinition->ItemType = EGambitItemType::Module;
		return ItemDefinition;
	}

	FGambitShopPurchaseContext MakePriceContext(UGambitItemDefinition* ItemDefinition, const int32 Price)
	{
		FGambitShopPurchaseContext Context;
		Context.OfferId = 0;
		Context.ItemDefinition = ItemDefinition;
		Context.BasePrice = Price;
		Context.PriceBeforeModifiers = Price;
		Context.ResolvedPrice = Price;
		return Context;
	}

	bool HasValidationIssue(
		const TArray<FGambitDataValidationIssue>& Issues,
		const EGambitDataValidationSeverity Severity,
		const TCHAR* MessageFragment)
	{
		return Issues.ContainsByPredicate([Severity, MessageFragment](const FGambitDataValidationIssue& Issue)
		{
			return Issue.Severity == Severity && Issue.Message.Contains(MessageFragment);
		});
	}

	TArray<AGambitPlayerState*> SpawnTestPlayers(UWorld* World, const int32 PlayerCount)
	{
		TArray<AGambitPlayerState*> Players;
		if (!World)
		{
			return Players;
		}

		Players.Reserve(PlayerCount);
		for (int32 PlayerIndex = 0; PlayerIndex < PlayerCount; ++PlayerIndex)
		{
			Players.Add(World->SpawnActor<AGambitPlayerState>());
		}
		return Players;
	}

	void AddMatchPlayers(FGambitEffectExecutionContext& Context, const TArray<AGambitPlayerState*>& Players)
	{
		for (AGambitPlayerState* Player : Players)
		{
			Context.MatchPlayerStates.Add(Player);
		}
		Context.MatchPlayerCount = Players.Num();
	}

	void ApplyTestRoundScore(AGambitPlayerState* Player, const int32 Score)
	{
		if (!Player)
		{
			return;
		}

		FGambitScoreBreakdown ScoreBreakdown;
		ScoreBreakdown.RawScore = Score;
		ScoreBreakdown.FinalScore = static_cast<float>(Score);
		Player->ApplyRoundScore(ScoreBreakdown);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitEffectTargetRulesRecognitionTest,
	"GrandpaGambit.Effects.TargetRules.Recognition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitEffectTargetRulesRecognitionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TestTrue(TEXT("selected_die is known"), GambitEffectTargetRules::IsKnownRule(GambitEffectTargetRules::SelectedDie));
	TestTrue(TEXT("selected_die is a selected die rule"), GambitEffectTargetRules::IsSelectedDieRule(GambitEffectTargetRules::SelectedDie));
	TestTrue(TEXT("selected_die requires selected die input"), GambitEffectTargetRules::RequiresSelectedDie(GambitEffectTargetRules::SelectedDie));

	TestTrue(TEXT("source.selected_die is known"), GambitEffectTargetRules::IsKnownRule(GambitEffectTargetRules::SourceSelectedDie));
	TestTrue(TEXT("source.selected_die is a selected die rule"), GambitEffectTargetRules::IsSelectedDieRule(GambitEffectTargetRules::SourceSelectedDie));
	TestTrue(TEXT("source.selected_die requires selected die input"), GambitEffectTargetRules::RequiresSelectedDie(GambitEffectTargetRules::SourceSelectedDie));

	TestTrue(TEXT("target.selected_die is known"), GambitEffectTargetRules::IsKnownRule(GambitEffectTargetRules::TargetSelectedDie));
	TestTrue(TEXT("target.selected_die is a selected die rule"), GambitEffectTargetRules::IsSelectedDieRule(GambitEffectTargetRules::TargetSelectedDie));
	TestTrue(TEXT("target.selected_die requires selected die input"), GambitEffectTargetRules::RequiresSelectedDie(GambitEffectTargetRules::TargetSelectedDie));

	TestTrue(TEXT("first_rerolled_die is known"), GambitEffectTargetRules::IsKnownRule(GambitEffectTargetRules::FirstRerolledDie));
	TestTrue(TEXT("first_rerolled_die is a first rerolled die rule"), GambitEffectTargetRules::IsFirstRerolledDieRule(GambitEffectTargetRules::FirstRerolledDie));

	TestTrue(TEXT("first_rerolled_die_this_round is known"), GambitEffectTargetRules::IsKnownRule(GambitEffectTargetRules::FirstRerolledDieThisRound));
	TestTrue(TEXT("first_rerolled_die_this_round is a first rerolled die rule"), GambitEffectTargetRules::IsFirstRerolledDieRule(GambitEffectTargetRules::FirstRerolledDieThisRound));

	TestTrue(TEXT("target.opponent is known"), GambitEffectTargetRules::IsKnownRule(GambitEffectTargetRules::TargetOpponent));
	TestTrue(TEXT("target.opponent is an opponent rule"), GambitEffectTargetRules::IsOpponentRule(GambitEffectTargetRules::TargetOpponent));
	TestFalse(TEXT("target.opponent does not require selected die input"), GambitEffectTargetRules::RequiresSelectedDie(GambitEffectTargetRules::TargetOpponent));

	TestFalse(TEXT("empty TargetRuleId is not a known rule value"), GambitEffectTargetRules::IsKnownRule(NAME_None));
	TestFalse(TEXT("unknown target rule is not known"), GambitEffectTargetRules::IsKnownRule(TEXT("unknown.target_rule")));

	const TArray<FName> KnownRuleIds = GambitEffectTargetRules::GetKnownRuleIds();
	TestEqual(TEXT("known authoring rules count"), KnownRuleIds.Num(), 23);
	TestTrue(TEXT("known rules include selected_die"), KnownRuleIds.Contains(GambitEffectTargetRules::SelectedDie));
	TestTrue(TEXT("known rules include source.selected_die"), KnownRuleIds.Contains(GambitEffectTargetRules::SourceSelectedDie));
	TestTrue(TEXT("known rules include target.selected_die"), KnownRuleIds.Contains(GambitEffectTargetRules::TargetSelectedDie));
	TestTrue(TEXT("known rules include first_rerolled_die"), KnownRuleIds.Contains(GambitEffectTargetRules::FirstRerolledDie));
	TestTrue(TEXT("known rules include first_rerolled_die_this_round"), KnownRuleIds.Contains(GambitEffectTargetRules::FirstRerolledDieThisRound));
	TestTrue(TEXT("known rules include target.opponent"), KnownRuleIds.Contains(GambitEffectTargetRules::TargetOpponent));
	TestTrue(TEXT("known rules include all_opponents"), KnownRuleIds.Contains(GambitEffectTargetRules::AllOpponents));
	TestTrue(TEXT("known rules include leading_player"), KnownRuleIds.Contains(GambitEffectTargetRules::LeadingPlayer));
	TestTrue(TEXT("known rules include richest_player"), KnownRuleIds.Contains(GambitEffectTargetRules::RichestPlayer));
	TestTrue(TEXT("known rules include source.best_die"), KnownRuleIds.Contains(GambitEffectTargetRules::SourceBestDie));
	TestTrue(TEXT("known rules include target.all_dice"), KnownRuleIds.Contains(GambitEffectTargetRules::TargetAllDice));

	GambitEffectTargetRules::FRuleMetadata AllOpponentsMetadata;
	TestTrue(TEXT("all_opponents metadata exists"), GambitEffectTargetRules::GetRuleMetadata(GambitEffectTargetRules::AllOpponents, AllOpponentsMetadata));
	TestTrue(TEXT("all_opponents is a player rule"), GambitEffectTargetRules::IsPlayerRule(GambitEffectTargetRules::AllOpponents));
	TestTrue(TEXT("all_opponents is multi-target"), GambitEffectTargetRules::IsMultiTargetRule(GambitEffectTargetRules::AllOpponents));
	TestEqual(TEXT("all_opponents metadata category is MultiTarget"), AllOpponentsMetadata.Category, GambitEffectTargetRules::ERuleCategory::MultiTarget);
	TestTrue(TEXT("target.best_die requires dice"), GambitEffectTargetRules::RequiresDice(GambitEffectTargetRules::TargetBestDie));
	TestTrue(TEXT("target.best_die is a die rule"), GambitEffectTargetRules::IsDieRule(GambitEffectTargetRules::TargetBestDie));
	TestTrue(TEXT("target.best_die requires an explicit target player"), GambitEffectTargetRules::RequiresExplicitTargetPlayer(GambitEffectTargetRules::TargetBestDie));

	const TArray<FName> AuthorableRuleIds = GambitEffectTargetRules::GetAuthorableRuleIds();
	TestTrue(TEXT("authorable rules include empty None option"), AuthorableRuleIds.Contains(NAME_None));
	for (const FName KnownRuleId : KnownRuleIds)
	{
		TestTrue(
			FString::Printf(TEXT("authorable rules include %s"), *KnownRuleId.ToString()),
			AuthorableRuleIds.Contains(KnownRuleId));
	}
	for (const FName AuthorableRuleId : AuthorableRuleIds)
	{
		if (!AuthorableRuleId.IsNone())
		{
			TestTrue(
				FString::Printf(TEXT("authorable rule %s is known"), *AuthorableRuleId.ToString()),
				GambitEffectTargetRules::IsKnownRule(AuthorableRuleId));
		}
	}

	const TArray<FName> DropdownOptions = UGambitItemEffectDefinition::GetTargetRuleIdOptions();
	TestEqual(TEXT("TargetRuleId dropdown option count matches authorable rules"), DropdownOptions.Num(), AuthorableRuleIds.Num());
	for (const FName AuthorableRuleId : AuthorableRuleIds)
	{
		TestTrue(
			FString::Printf(TEXT("TargetRuleId dropdown includes %s"), *AuthorableRuleId.ToString()),
			DropdownOptions.Contains(AuthorableRuleId));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitEffectTargetResolverResolveTargetsTest,
	"GrandpaGambit.Effects.TargetResolver.ResolveTargets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitEffectTargetResolverResolveTargetsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("test world is created"), TestWorld))
	{
		return false;
	}

	AGambitPlayerState* SourcePlayer = TestWorld->SpawnActor<AGambitPlayerState>();
	AGambitPlayerState* TargetPlayer = TestWorld->SpawnActor<AGambitPlayerState>();
	const bool bPlayersCreated = TestNotNull(TEXT("source player is created"), SourcePlayer)
		&& TestNotNull(TEXT("target player is created"), TargetPlayer);
	if (!bPlayersCreated)
	{
		TestWorld->DestroyWorld(false);
		return false;
	}

	FGambitEffectExecutionContext Context;
	Context.SourcePlayer = SourcePlayer;
	Context.TargetPlayer = TargetPlayer;
	Context.SourceDice = {
		MakeTestDie(2, 1, true, true, 0),
		MakeTestDie(4, 1, true, true, 1),
	};
	Context.TargetDice = {
		MakeTestDie(5, 1, true, true, 0),
		MakeTestDie(6, 1, true, true, 1),
	};
	Context.SourceDieHandIndex = 1;
	Context.TargetDieHandIndex = 0;
	Context.FirstRerolledDieHandIndexThisRound = 1;

	UGambitItemEffectDefinition* SourceEffect = MakeEffectDefinition(
		GetTransientPackage(),
		EGambitEffectHook::ConsumableUse,
		EGambitItemEffectType::ModifyDieValue);
	SourceEffect->Target = EGambitEffectTarget::Source;

	const FGambitEffectTargetResolveResult SourceResult = GambitEffectTargetResolver::ResolveEffectTargets(SourceEffect, Context);
	TestTrue(TEXT("source target resolves successfully without TargetRuleId"), SourceResult.bSuccess);
	TestEqual(TEXT("source target resolves one target"), SourceResult.Targets.Num(), 1);
	TestEqual(TEXT("source target side is Source"), SourceResult.Targets[0].TargetSide, EGambitEffectTarget::Source);

	UGambitItemEffectDefinition* TargetEffect = MakeEffectDefinition(
		GetTransientPackage(),
		EGambitEffectHook::ConsumableUse,
		EGambitItemEffectType::ModifyDieValue);
	TargetEffect->Target = EGambitEffectTarget::Target;

	const FGambitEffectTargetResolveResult TargetResult = GambitEffectTargetResolver::ResolveEffectTargets(TargetEffect, Context);
	TestTrue(TEXT("target resolves successfully without TargetRuleId"), TargetResult.bSuccess);
	TestEqual(TEXT("target resolves one target"), TargetResult.Targets.Num(), 1);
	TestEqual(TEXT("target side is Target"), TargetResult.Targets[0].TargetSide, EGambitEffectTarget::Target);

	UGambitItemEffectDefinition* SourceAndTargetEffect = MakeEffectDefinition(
		GetTransientPackage(),
		EGambitEffectHook::ConsumableUse,
		EGambitItemEffectType::ModifyDieValue);
	SourceAndTargetEffect->Target = EGambitEffectTarget::SourceAndTarget;

	const FGambitEffectTargetResolveResult SourceAndTargetResult = GambitEffectTargetResolver::ResolveEffectTargets(SourceAndTargetEffect, Context);
	TestTrue(TEXT("SourceAndTarget resolves successfully"), SourceAndTargetResult.bSuccess);
	TestEqual(TEXT("SourceAndTarget resolves two targets"), SourceAndTargetResult.Targets.Num(), 2);
	TestEqual(TEXT("SourceAndTarget first side is Source"), SourceAndTargetResult.Targets[0].TargetSide, EGambitEffectTarget::Source);
	TestEqual(TEXT("SourceAndTarget second side is Target"), SourceAndTargetResult.Targets[1].TargetSide, EGambitEffectTarget::Target);

	UGambitItemEffectDefinition* SelectedDieEffect = MakeEffectDefinition(
		GetTransientPackage(),
		EGambitEffectHook::ConsumableUse,
		EGambitItemEffectType::ModifyDieValue);
	SelectedDieEffect->TargetRuleId = GambitEffectTargetRules::SelectedDie;

	const FGambitEffectTargetResolveResult SelectedDieResult = GambitEffectTargetResolver::ResolveEffectTargets(SelectedDieEffect, Context);
	TestTrue(TEXT("selected_die resolves successfully"), SelectedDieResult.bSuccess);
	TestTrue(TEXT("selected_die resolves a die index"), SelectedDieResult.Targets.IsValidIndex(0) && SelectedDieResult.Targets[0].DiceHandIndexes.IsValidIndex(0));
	if (SelectedDieResult.Targets.IsValidIndex(0) && SelectedDieResult.Targets[0].DiceHandIndexes.IsValidIndex(0))
	{
		TestEqual(TEXT("selected_die resolves selected source die index"), SelectedDieResult.Targets[0].DiceHandIndexes[0], 1);
	}

	UGambitItemEffectDefinition* SourceSelectedDieEffect = MakeEffectDefinition(
		GetTransientPackage(),
		EGambitEffectHook::ConsumableUse,
		EGambitItemEffectType::ModifyDieValue);
	SourceSelectedDieEffect->Target = EGambitEffectTarget::Target;
	SourceSelectedDieEffect->TargetRuleId = GambitEffectTargetRules::SourceSelectedDie;

	const FGambitEffectTargetResolveResult SourceSelectedDieResult = GambitEffectTargetResolver::ResolveEffectTargets(SourceSelectedDieEffect, Context);
	TestTrue(TEXT("source.selected_die resolves successfully"), SourceSelectedDieResult.bSuccess);
	TestEqual(TEXT("source.selected_die resolves source side"), SourceSelectedDieResult.Targets[0].TargetSide, EGambitEffectTarget::Source);
	TestTrue(TEXT("source.selected_die resolves a die index"), SourceSelectedDieResult.Targets.IsValidIndex(0) && SourceSelectedDieResult.Targets[0].DiceHandIndexes.IsValidIndex(0));
	if (SourceSelectedDieResult.Targets.IsValidIndex(0) && SourceSelectedDieResult.Targets[0].DiceHandIndexes.IsValidIndex(0))
	{
		TestEqual(TEXT("source.selected_die resolves source selected index"), SourceSelectedDieResult.Targets[0].DiceHandIndexes[0], 1);
	}

	UGambitItemEffectDefinition* TargetSelectedDieEffect = MakeEffectDefinition(
		GetTransientPackage(),
		EGambitEffectHook::ConsumableUse,
		EGambitItemEffectType::ModifyDieValue);
	TargetSelectedDieEffect->TargetRuleId = GambitEffectTargetRules::TargetSelectedDie;

	const FGambitEffectTargetResolveResult TargetSelectedDieResult = GambitEffectTargetResolver::ResolveEffectTargets(TargetSelectedDieEffect, Context);
	TestTrue(TEXT("target.selected_die resolves successfully"), TargetSelectedDieResult.bSuccess);
	TestEqual(TEXT("target.selected_die resolves target side"), TargetSelectedDieResult.Targets[0].TargetSide, EGambitEffectTarget::Target);
	TestTrue(TEXT("target.selected_die resolves a die index"), TargetSelectedDieResult.Targets.IsValidIndex(0) && TargetSelectedDieResult.Targets[0].DiceHandIndexes.IsValidIndex(0));
	if (TargetSelectedDieResult.Targets.IsValidIndex(0) && TargetSelectedDieResult.Targets[0].DiceHandIndexes.IsValidIndex(0))
	{
		TestEqual(TEXT("target.selected_die resolves target selected index"), TargetSelectedDieResult.Targets[0].DiceHandIndexes[0], 0);
	}

	UGambitItemEffectDefinition* FirstRerolledEffect = MakeEffectDefinition(
		GetTransientPackage(),
		EGambitEffectHook::PostReroll,
		EGambitItemEffectType::ModifyDieValue);
	FirstRerolledEffect->TargetRuleId = GambitEffectTargetRules::FirstRerolledDieThisRound;

	const FGambitEffectTargetResolveResult FirstRerolledResult = GambitEffectTargetResolver::ResolveEffectTargets(FirstRerolledEffect, Context);
	TestTrue(TEXT("first_rerolled_die_this_round resolves successfully"), FirstRerolledResult.bSuccess);
	TestTrue(TEXT("first_rerolled_die_this_round resolves a die index"), FirstRerolledResult.Targets.IsValidIndex(0) && FirstRerolledResult.Targets[0].DiceHandIndexes.IsValidIndex(0));
	if (FirstRerolledResult.Targets.IsValidIndex(0) && FirstRerolledResult.Targets[0].DiceHandIndexes.IsValidIndex(0))
	{
		TestEqual(TEXT("first_rerolled_die_this_round resolves first rerolled index"), FirstRerolledResult.Targets[0].DiceHandIndexes[0], 1);
	}

	UGambitItemEffectDefinition* OpponentEffect = MakeEffectDefinition(
		GetTransientPackage(),
		EGambitEffectHook::ConsumableUse,
		EGambitItemEffectType::StealGold);
	OpponentEffect->Target = EGambitEffectTarget::Target;
	OpponentEffect->TargetRuleId = GambitEffectTargetRules::TargetOpponent;

	FGambitEffectExecutionContext MissingOpponentContext = Context;
	MissingOpponentContext.TargetPlayer = nullptr;
	const FGambitEffectTargetResolveResult MissingOpponentResult = GambitEffectTargetResolver::ResolveEffectTargets(OpponentEffect, MissingOpponentContext);
	TestFalse(TEXT("target.opponent fails without valid target player"), MissingOpponentResult.bSuccess);
	TestFalse(TEXT("target.opponent failure reason is readable"), MissingOpponentResult.FailureReason.IsEmpty());

	FGambitEffectExecutionContext SelfOpponentContext = Context;
	SelfOpponentContext.TargetPlayer = SourcePlayer;
	const FGambitEffectTargetResolveResult SelfOpponentResult = GambitEffectTargetResolver::ResolveEffectTargets(OpponentEffect, SelfOpponentContext);
	TestFalse(TEXT("target.opponent refuses SourcePlayer as target"), SelfOpponentResult.bSuccess);
	TestTrue(TEXT("target.opponent self failure reason mentions SourcePlayer"), SelfOpponentResult.FailureReason.Contains(TEXT("SourcePlayer")));

	const FGambitEffectTargetResolveResult OpponentResult = GambitEffectTargetResolver::ResolveEffectTargets(OpponentEffect, Context);
	TestTrue(TEXT("target.opponent succeeds with different target player"), OpponentResult.bSuccess);
	TestEqual(TEXT("target.opponent resolves target side"), OpponentResult.Targets[0].TargetSide, EGambitEffectTarget::Target);
	TestTrue(TEXT("target.opponent resolves target player"), OpponentResult.Targets[0].Player.Get() == TargetPlayer);

	UGambitItemEffectDefinition* UnknownRuleEffect = MakeEffectDefinition(
		GetTransientPackage(),
		EGambitEffectHook::ConsumableUse,
		EGambitItemEffectType::ModifyDieValue);
	UnknownRuleEffect->TargetRuleId = TEXT("unknown.target_rule");

	const FGambitEffectTargetResolveResult UnknownRuleResult = GambitEffectTargetResolver::ResolveEffectTargets(UnknownRuleEffect, Context);
	TestFalse(TEXT("unknown TargetRuleId fails cleanly"), UnknownRuleResult.bSuccess);
	TestTrue(TEXT("unknown TargetRuleId failure reason mentions unknown"), UnknownRuleResult.FailureReason.Contains(TEXT("Unknown")));

	TestWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitEffectTargetResolverV2RulesTest,
	"GrandpaGambit.Effects.TargetResolver.V2Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitEffectTargetResolverV2RulesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("test world is created"), TestWorld))
	{
		return false;
	}

	const TArray<AGambitPlayerState*> Players = SpawnTestPlayers(TestWorld, 4);
	if (!TestEqual(TEXT("four test players are created"), Players.Num(), 4))
	{
		TestWorld->DestroyWorld(false);
		return false;
	}

	for (int32 PlayerIndex = 0; PlayerIndex < Players.Num(); ++PlayerIndex)
	{
		if (!TestNotNull(FString::Printf(TEXT("player %d is created"), PlayerIndex), Players[PlayerIndex]))
		{
			TestWorld->DestroyWorld(false);
			return false;
		}
	}

	UGambitItemEffectDefinition* AllOpponentsEffect = MakeEffectDefinition(
		GetTransientPackage(),
		EGambitEffectHook::ConsumableUse,
		EGambitItemEffectType::AddGold);
	AllOpponentsEffect->Target = EGambitEffectTarget::Target;
	AllOpponentsEffect->TargetRuleId = GambitEffectTargetRules::AllOpponents;

	for (int32 PlayerCount = 2; PlayerCount <= 4; ++PlayerCount)
	{
		TArray<AGambitPlayerState*> MatchPlayers;
		for (int32 PlayerIndex = 0; PlayerIndex < PlayerCount; ++PlayerIndex)
		{
			MatchPlayers.Add(Players[PlayerIndex]);
		}

		FGambitEffectExecutionContext Context;
		Context.SourcePlayer = MatchPlayers[0];
		Context.TargetPlayer = MatchPlayers.Num() > 1 ? MatchPlayers[1] : nullptr;
		AddMatchPlayers(Context, MatchPlayers);

		const FGambitEffectTargetResolveResult Result = GambitEffectTargetResolver::ResolveEffectTargets(AllOpponentsEffect, Context);
		TestTrue(FString::Printf(TEXT("all_opponents resolves for %d players"), PlayerCount), Result.bSuccess);
		TestEqual(FString::Printf(TEXT("all_opponents returns every opponent for %d players"), PlayerCount), Result.Targets.Num(), PlayerCount - 1);
		for (const FGambitResolvedEffectTarget& Target : Result.Targets)
		{
			TestTrue(TEXT("all_opponents never returns source"), Target.Player.Get() != Context.SourcePlayer.Get());
		}
		for (int32 PlayerIndex = 1; PlayerIndex < PlayerCount; ++PlayerIndex)
		{
			TestTrue(
				FString::Printf(TEXT("all_opponents includes player %d"), PlayerIndex),
				Result.Targets.ContainsByPredicate([ExpectedPlayer = Players[PlayerIndex]](const FGambitResolvedEffectTarget& Target)
				{
					return Target.Player.Get() == ExpectedPlayer;
				}));
		}
	}

	Players[1]->AddVictoryPoints(5);
	Players[2]->AddVictoryPoints(5);
	Players[3]->AddVictoryPoints(3);
	ApplyTestRoundScore(Players[1], 20);
	ApplyTestRoundScore(Players[2], 40);
	ApplyTestRoundScore(Players[3], 80);

	FGambitEffectExecutionContext PlayerRuleContext;
	PlayerRuleContext.SourcePlayer = Players[0];
	PlayerRuleContext.TargetPlayer = Players[1];
	AddMatchPlayers(PlayerRuleContext, Players);

	UGambitItemEffectDefinition* LeadingPlayerEffect = MakeEffectDefinition(
		GetTransientPackage(),
		EGambitEffectHook::ConsumableUse,
		EGambitItemEffectType::AddGold);
	LeadingPlayerEffect->Target = EGambitEffectTarget::Target;
	LeadingPlayerEffect->TargetRuleId = GambitEffectTargetRules::LeadingPlayer;

	const FGambitEffectTargetResolveResult LeadingResult = GambitEffectTargetResolver::ResolveEffectTargets(LeadingPlayerEffect, PlayerRuleContext);
	TestTrue(TEXT("leading_player resolves successfully"), LeadingResult.bSuccess);
	TestTrue(TEXT("leading_player uses VP then current score tie-break"), LeadingResult.Targets.IsValidIndex(0) && LeadingResult.Targets[0].Player.Get() == Players[2]);

	Players[1]->AddGold(12);
	Players[2]->AddGold(30);
	Players[3]->AddGold(20);

	UGambitItemEffectDefinition* RichestPlayerEffect = MakeEffectDefinition(
		GetTransientPackage(),
		EGambitEffectHook::ConsumableUse,
		EGambitItemEffectType::AddGold);
	RichestPlayerEffect->Target = EGambitEffectTarget::Target;
	RichestPlayerEffect->TargetRuleId = GambitEffectTargetRules::RichestPlayer;

	const FGambitEffectTargetResolveResult RichestResult = GambitEffectTargetResolver::ResolveEffectTargets(RichestPlayerEffect, PlayerRuleContext);
	TestTrue(TEXT("richest_player resolves successfully"), RichestResult.bSuccess);
	TestTrue(TEXT("richest_player selects the player with most gold"), RichestResult.Targets.IsValidIndex(0) && RichestResult.Targets[0].Player.Get() == Players[2]);

	FGambitEffectExecutionContext DiceContext;
	DiceContext.SourcePlayer = Players[0];
	DiceContext.TargetPlayer = Players[1];
	DiceContext.SourceDice = {
		MakeTestDie(4, 1, true, true, 0),
		MakeTestDie(8, 1, true, true, 1),
		MakeTestDie(2, 1, true, true, 2),
	};

	UGambitItemEffectDefinition* SourceBestDieEffect = MakeEffectDefinition(
		GetTransientPackage(),
		EGambitEffectHook::ConsumableUse,
		EGambitItemEffectType::ModifyDieValue);
	SourceBestDieEffect->TargetRuleId = GambitEffectTargetRules::SourceBestDie;

	const FGambitEffectTargetResolveResult BestDieResult = GambitEffectTargetResolver::ResolveEffectTargets(SourceBestDieEffect, DiceContext);
	TestTrue(TEXT("source.best_die resolves successfully"), BestDieResult.bSuccess);
	TestTrue(TEXT("source.best_die resolves an index"), BestDieResult.Targets.IsValidIndex(0) && BestDieResult.Targets[0].DiceHandIndexes.IsValidIndex(0));
	if (BestDieResult.Targets.IsValidIndex(0) && BestDieResult.Targets[0].DiceHandIndexes.IsValidIndex(0))
	{
		TestEqual(TEXT("source.best_die selects highest value die"), BestDieResult.Targets[0].DiceHandIndexes[0], 1);
	}

	UGambitItemEffectDefinition* SourceLowestDieEffect = MakeEffectDefinition(
		GetTransientPackage(),
		EGambitEffectHook::ConsumableUse,
		EGambitItemEffectType::ModifyDieValue);
	SourceLowestDieEffect->TargetRuleId = GambitEffectTargetRules::SourceLowestDie;

	const FGambitEffectTargetResolveResult LowestDieResult = GambitEffectTargetResolver::ResolveEffectTargets(SourceLowestDieEffect, DiceContext);
	TestTrue(TEXT("source.lowest_die resolves successfully"), LowestDieResult.bSuccess);
	TestTrue(TEXT("source.lowest_die resolves an index"), LowestDieResult.Targets.IsValidIndex(0) && LowestDieResult.Targets[0].DiceHandIndexes.IsValidIndex(0));
	if (LowestDieResult.Targets.IsValidIndex(0) && LowestDieResult.Targets[0].DiceHandIndexes.IsValidIndex(0))
	{
		TestEqual(TEXT("source.lowest_die selects lowest value die"), LowestDieResult.Targets[0].DiceHandIndexes[0], 2);
	}

	FGambitEffectExecutionContext EmptyDiceContext = DiceContext;
	EmptyDiceContext.SourceDice.Reset();
	const FGambitEffectTargetResolveResult EmptyDiceResult = GambitEffectTargetResolver::ResolveEffectTargets(SourceBestDieEffect, EmptyDiceContext);
	TestFalse(TEXT("die target rule fails without valid dice"), EmptyDiceResult.bSuccess);
	TestTrue(TEXT("die target rule failure reason is readable"), EmptyDiceResult.FailureReason.Contains(TEXT("requires dice")));

	TestWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitEffectTargetResolverNonDiceExecutionTest,
	"GrandpaGambit.Effects.TargetResolver.NonDiceExecution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitEffectTargetResolverNonDiceExecutionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("test world is created"), TestWorld))
	{
		return false;
	}

	AGambitPlayerState* SourcePlayer = TestWorld->SpawnActor<AGambitPlayerState>();
	AGambitPlayerState* TargetPlayer = TestWorld->SpawnActor<AGambitPlayerState>();
	const bool bPlayersCreated = TestNotNull(TEXT("source player is created"), SourcePlayer)
		&& TestNotNull(TEXT("target player is created"), TargetPlayer);
	if (!bPlayersCreated)
	{
		TestWorld->DestroyWorld(false);
		return false;
	}

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitEconomyComponent* SourceEconomy = NewObject<UGambitEconomyComponent>();
	UGambitEconomyComponent* TargetEconomy = NewObject<UGambitEconomyComponent>();
	SourceEconomy->InitializeForMatch();
	TargetEconomy->InitializeForMatch();

	FGambitEffectExecutionContext Context;
	Context.Hook = EGambitEffectHook::ConsumableUse;
	Context.SourcePlayer = SourcePlayer;
	Context.TargetPlayer = TargetPlayer;
	Context.SourceEconomyComponent = SourceEconomy;
	Context.TargetEconomyComponent = TargetEconomy;

	UGambitItemEffectDefinition* AddGoldSource = MakeEffectDefinition(
		GetTransientPackage(),
		EGambitEffectHook::ConsumableUse,
		EGambitItemEffectType::AddGold);
	AddGoldSource->Target = EGambitEffectTarget::Source;
	AddGoldSource->Amount = 3.0f;
	TestTrue(TEXT("AddGold Source executes"), Resolver->ExecuteEffectDefinition(AddGoldSource, Context));
	TestEqual(TEXT("AddGold Source changes source economy"), SourceEconomy->GetCurrentGold(), 3);
	TestEqual(TEXT("AddGold Source does not change target economy"), TargetEconomy->GetCurrentGold(), 0);

	UGambitItemEffectDefinition* AddGoldTarget = MakeEffectDefinition(
		GetTransientPackage(),
		EGambitEffectHook::ConsumableUse,
		EGambitItemEffectType::AddGold);
	AddGoldTarget->Target = EGambitEffectTarget::Target;
	AddGoldTarget->Amount = 4.0f;
	TestTrue(TEXT("AddGold Target executes"), Resolver->ExecuteEffectDefinition(AddGoldTarget, Context));
	TestEqual(TEXT("AddGold Target changes target economy"), TargetEconomy->GetCurrentGold(), 4);

	TargetEconomy->AddGold(6);
	UGambitItemEffectDefinition* SpendGoldTarget = MakeEffectDefinition(
		GetTransientPackage(),
		EGambitEffectHook::ConsumableUse,
		EGambitItemEffectType::SpendGold);
	SpendGoldTarget->Target = EGambitEffectTarget::Target;
	SpendGoldTarget->Amount = 5.0f;
	TestTrue(TEXT("SpendGold Target executes"), Resolver->ExecuteEffectDefinition(SpendGoldTarget, Context));
	TestEqual(TEXT("SpendGold Target spends target economy"), TargetEconomy->GetCurrentGold(), 5);

	UGambitItemEffectDefinition* OpponentAddGold = MakeEffectDefinition(
		GetTransientPackage(),
		EGambitEffectHook::ConsumableUse,
		EGambitItemEffectType::AddGold);
	OpponentAddGold->Target = EGambitEffectTarget::Target;
	OpponentAddGold->TargetRuleId = GambitEffectTargetRules::TargetOpponent;
	OpponentAddGold->Amount = 7.0f;

	FGambitEffectExecutionContext MissingOpponentContext = Context;
	MissingOpponentContext.TargetPlayer = nullptr;
	const int32 TargetGoldBeforeMissingOpponent = TargetEconomy->GetCurrentGold();
	TestFalse(TEXT("target.opponent AddGold fails without TargetPlayer"), Resolver->ExecuteEffectDefinition(OpponentAddGold, MissingOpponentContext));
	TestEqual(TEXT("target.opponent failure does not change target economy"), TargetEconomy->GetCurrentGold(), TargetGoldBeforeMissingOpponent);

	TestTrue(TEXT("target.opponent AddGold executes with different TargetPlayer"), Resolver->ExecuteEffectDefinition(OpponentAddGold, Context));
	TestEqual(TEXT("target.opponent AddGold changes target economy"), TargetEconomy->GetCurrentGold(), TargetGoldBeforeMissingOpponent + 7);

	UGambitItemEffectDefinition* SourceAndTargetGold = MakeEffectDefinition(
		GetTransientPackage(),
		EGambitEffectHook::ConsumableUse,
		EGambitItemEffectType::AddGold);
	SourceAndTargetGold->Target = EGambitEffectTarget::SourceAndTarget;
	SourceAndTargetGold->Amount = 2.0f;
	const int32 SourceGoldBeforeSourceAndTarget = SourceEconomy->GetCurrentGold();
	const int32 TargetGoldBeforeSourceAndTarget = TargetEconomy->GetCurrentGold();
	TestTrue(TEXT("SourceAndTarget AddGold executes"), Resolver->ExecuteEffectDefinition(SourceAndTargetGold, Context));
	TestEqual(TEXT("SourceAndTarget AddGold changes source economy"), SourceEconomy->GetCurrentGold(), SourceGoldBeforeSourceAndTarget + 2);
	TestEqual(TEXT("SourceAndTarget AddGold changes target economy"), TargetEconomy->GetCurrentGold(), TargetGoldBeforeSourceAndTarget + 2);

	TestWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitEffectTargetResolverConditionScalarReadsTest,
	"GrandpaGambit.Effects.TargetResolver.ConditionScalarReads",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitEffectTargetResolverConditionScalarReadsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UObject* TestOuter = GetTransientPackage();
	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();

	{
		UGambitEconomyComponent* SourceEconomy = NewObject<UGambitEconomyComponent>();
		SourceEconomy->InitializeForMatch();
		SourceEconomy->AddGold(6);

		UGambitItemEffectDefinition* SourceConditionEffect = MakeEffectDefinition(
			TestOuter,
			EGambitEffectHook::ConsumableUse,
			EGambitItemEffectType::AddGold);
		SourceConditionEffect->Target = EGambitEffectTarget::Source;
		SourceConditionEffect->Amount = 2.0f;

		FGambitEffectConditionDefinition SourceGoldCondition;
		SourceGoldCondition.ConditionType = EGambitEffectConditionType::GoldThreshold;
		SourceGoldCondition.ConditionTarget = EGambitEffectTarget::Source;
		SourceGoldCondition.Comparison = EGambitEffectComparison::GreaterOrEqual;
		SourceGoldCondition.Value = 5;
		SourceConditionEffect->Conditions.Add(SourceGoldCondition);

		FGambitEffectExecutionContext Context;
		Context.Hook = EGambitEffectHook::ConsumableUse;
		Context.SourceEconomyComponent = SourceEconomy;
		TestTrue(TEXT("source condition resolves through context target"), Resolver->ExecuteEffectDefinition(SourceConditionEffect, Context));
		TestEqual(TEXT("source condition gates source effect"), SourceEconomy->GetCurrentGold(), 8);
	}

	{
		UGambitEconomyComponent* SourceEconomy = NewObject<UGambitEconomyComponent>();
		SourceEconomy->InitializeForMatch();

		UGambitItemEffectDefinition* TargetConditionEffect = MakeEffectDefinition(
			TestOuter,
			EGambitEffectHook::ConsumableUse,
			EGambitItemEffectType::AddGold);
		TargetConditionEffect->Target = EGambitEffectTarget::Source;
		TargetConditionEffect->Amount = 3.0f;

		FGambitEffectConditionDefinition TargetDieCondition;
		TargetDieCondition.ConditionType = EGambitEffectConditionType::DieValueEquals;
		TargetDieCondition.ConditionTarget = EGambitEffectTarget::Target;
		TargetDieCondition.Value = 6;
		TargetConditionEffect->Conditions.Add(TargetDieCondition);

		FGambitEffectExecutionContext Context;
		Context.Hook = EGambitEffectHook::ConsumableUse;
		Context.SourceEconomyComponent = SourceEconomy;
		Context.TargetDice = { MakeTestDie(6, 1, true, true, 0) };
		TestTrue(TEXT("target condition resolves target dice through context target"), Resolver->ExecuteEffectDefinition(TargetConditionEffect, Context));
		TestEqual(TEXT("target condition gates source effect"), SourceEconomy->GetCurrentGold(), 3);
	}

	{
		UGambitEconomyComponent* SourceEconomy = NewObject<UGambitEconomyComponent>();
		SourceEconomy->InitializeForMatch();
		SourceEconomy->AddGold(3);

		UGambitItemEffectDefinition* SourceGoldScalarEffect = MakeEffectDefinition(
			TestOuter,
			EGambitEffectHook::ConsumableUse,
			EGambitItemEffectType::AddGold);
		SourceGoldScalarEffect->Target = EGambitEffectTarget::Source;
		SourceGoldScalarEffect->ScalarParameters.Add(TEXT("AmountPerGold"), 2.0f);

		FGambitEffectExecutionContext Context;
		Context.Hook = EGambitEffectHook::ConsumableUse;
		Context.SourceEconomyComponent = SourceEconomy;
		TestTrue(TEXT("source economy scalar executes"), Resolver->ExecuteEffectDefinition(SourceGoldScalarEffect, Context));
		TestEqual(TEXT("source economy scalar reads source gold"), SourceEconomy->GetCurrentGold(), 9);
	}

	{
		UGambitEconomyComponent* TargetEconomy = NewObject<UGambitEconomyComponent>();
		TargetEconomy->InitializeForMatch();
		TargetEconomy->AddGold(4);

		UGambitItemEffectDefinition* TargetGoldScalarEffect = MakeEffectDefinition(
			TestOuter,
			EGambitEffectHook::ConsumableUse,
			EGambitItemEffectType::AddGold);
		TargetGoldScalarEffect->Target = EGambitEffectTarget::Target;
		TargetGoldScalarEffect->ScalarParameters.Add(TEXT("AmountPerGold"), 2.0f);

		FGambitEffectExecutionContext Context;
		Context.Hook = EGambitEffectHook::ConsumableUse;
		Context.TargetEconomyComponent = TargetEconomy;
		TestTrue(TEXT("target economy scalar executes"), Resolver->ExecuteEffectDefinition(TargetGoldScalarEffect, Context));
		TestEqual(TEXT("target economy scalar reads target gold"), TargetEconomy->GetCurrentGold(), 12);
	}

	{
		UGambitDiceDefinition* SourceDieA = MakeDiceDefinition(TestOuter, TEXT("dice.test.source_scalar_a"), TArray<int32>({ 1, 2, 3 }));
		UGambitDiceDefinition* SourceDieB = MakeDiceDefinition(TestOuter, TEXT("dice.test.source_scalar_b"), TArray<int32>({ 4, 5, 6 }));
		UGambitInventoryComponent* SourceInventory = NewObject<UGambitInventoryComponent>();
		SourceInventory->AddOwnedDie(SourceDieA);
		SourceInventory->AddOwnedDie(SourceDieB);

		UGambitEconomyComponent* SourceEconomy = NewObject<UGambitEconomyComponent>();
		SourceEconomy->InitializeForMatch();

		UGambitItemEffectDefinition* SourceInventoryScalarEffect = MakeEffectDefinition(
			TestOuter,
			EGambitEffectHook::ConsumableUse,
			EGambitItemEffectType::AddGold);
		SourceInventoryScalarEffect->Target = EGambitEffectTarget::Source;
		SourceInventoryScalarEffect->ScalarParameters.Add(TEXT("AmountPerOwnedDie"), 2.0f);

		FGambitEffectExecutionContext Context;
		Context.Hook = EGambitEffectHook::ConsumableUse;
		Context.SourceEconomyComponent = SourceEconomy;
		Context.SourceInventoryComponent = SourceInventory;
		TestTrue(TEXT("source inventory scalar executes"), Resolver->ExecuteEffectDefinition(SourceInventoryScalarEffect, Context));
		TestEqual(TEXT("source inventory scalar reads owned dice count"), SourceEconomy->GetCurrentGold(), 4);
	}

	{
		UGambitEconomyComponent* TargetEconomy = NewObject<UGambitEconomyComponent>();
		TargetEconomy->InitializeForMatch();

		UGambitItemEffectDefinition* TargetDiceScalarEffect = MakeEffectDefinition(
			TestOuter,
			EGambitEffectHook::ConsumableUse,
			EGambitItemEffectType::AddGold);
		TargetDiceScalarEffect->Target = EGambitEffectTarget::Target;
		TargetDiceScalarEffect->ScalarParameters.Add(TEXT("AmountPerOwnedDie"), 3.0f);

		FGambitEffectExecutionContext Context;
		Context.Hook = EGambitEffectHook::ConsumableUse;
		Context.TargetEconomyComponent = TargetEconomy;
		Context.TargetDice = {
			MakeTestDie(1, 1, true, true, 0),
			MakeTestDie(2, 1, true, true, 1),
			MakeTestDie(3, 1, true, true, 2)
		};
		TestTrue(TEXT("target dice scalar executes"), Resolver->ExecuteEffectDefinition(TargetDiceScalarEffect, Context));
		TestEqual(TEXT("target dice scalar falls back to target dice snapshot"), TargetEconomy->GetCurrentGold(), 9);
	}

	{
		UGambitEconomyComponent* TargetEconomy = NewObject<UGambitEconomyComponent>();
		TargetEconomy->InitializeForMatch();

		UGambitItemEffectDefinition* OpponentEffect = MakeEffectDefinition(
			TestOuter,
			EGambitEffectHook::ConsumableUse,
			EGambitItemEffectType::AddGold);
		OpponentEffect->Target = EGambitEffectTarget::Target;
		OpponentEffect->TargetRuleId = GambitEffectTargetRules::TargetOpponent;
		OpponentEffect->Amount = 5.0f;

		FGambitEffectExecutionContext Context;
		Context.Hook = EGambitEffectHook::ConsumableUse;
		Context.TargetEconomyComponent = TargetEconomy;
		TestFalse(TEXT("target.opponent fails cleanly without TargetPlayer"), Resolver->ExecuteEffectDefinition(OpponentEffect, Context));
		TestEqual(TEXT("target.opponent failure leaves target economy unchanged"), TargetEconomy->GetCurrentGold(), 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitEffectTargetRuleValidationTest,
	"GrandpaGambit.Items.TargetRuleValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitEffectTargetRuleValidationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitItemEffectDefinition* EmptyRuleEffect = MakeEffectDefinition(GetTransientPackage(), EGambitEffectHook::ScoreModifier, EGambitItemEffectType::AddGold);
	EmptyRuleEffect->Amount = 1.0f;
	TArray<FGambitDataValidationIssue> EmptyRuleIssues;
	GambitDataValidation::ValidateEffectDefinition(EmptyRuleEffect, TEXT("Test"), 0, EmptyRuleIssues);
	TestFalse(
		TEXT("empty TargetRuleId does not produce a target rule validation error by default"),
		HasValidationIssue(EmptyRuleIssues, EGambitDataValidationSeverity::Error, TEXT("TargetRuleId")));

	UGambitItemEffectDefinition* UnknownRuleEffect = MakeEffectDefinition(GetTransientPackage(), EGambitEffectHook::ScoreModifier, EGambitItemEffectType::AddGold);
	UnknownRuleEffect->Amount = 1.0f;
	UnknownRuleEffect->TargetRuleId = TEXT("unknown.target_rule");
	TArray<FGambitDataValidationIssue> UnknownRuleIssues;
	GambitDataValidation::ValidateEffectDefinition(UnknownRuleEffect, TEXT("Test"), 0, UnknownRuleIssues);
	TestTrue(
		TEXT("unknown TargetRuleId produces validation error"),
		HasValidationIssue(UnknownRuleIssues, EGambitDataValidationSeverity::Error, TEXT("unknown TargetRuleId")));

	UGambitConsumableDefinition* OpponentConsumable = NewObject<UGambitConsumableDefinition>();
	OpponentConsumable->ItemId = TEXT("consumable.test.target_rule_opponent");
	OpponentConsumable->bCanTargetOpponent = true;
	UGambitItemEffectDefinition* OpponentEffect = MakeEffectDefinition(OpponentConsumable, EGambitEffectHook::ConsumableUse, EGambitItemEffectType::StealGold);
	OpponentEffect->Target = EGambitEffectTarget::Target;
	OpponentEffect->TargetRuleId = GambitEffectTargetRules::TargetOpponent;
	OpponentEffect->Amount = 1.0f;
	OpponentConsumable->EffectDefinitions.Add(OpponentEffect);

	TArray<FGambitDataValidationIssue> OpponentIssues;
	GambitDataValidation::ValidateItemDefinition(OpponentConsumable, OpponentIssues);
	TestFalse(
		TEXT("target.opponent satisfies opponent target rule validation"),
		HasValidationIssue(OpponentIssues, EGambitDataValidationSeverity::Error, TEXT("opponent target rule")));
	TestFalse(
		TEXT("target.opponent does not produce a TargetRuleId validation error"),
		HasValidationIssue(OpponentIssues, EGambitDataValidationSeverity::Error, TEXT("TargetRuleId")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitModuleFlatScoreEffectTest,
	"GrandpaGambit.Effects.ModuleFlatScore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitModuleFlatScoreEffectTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitModuleDefinition* ModuleDefinition = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(ModuleDefinition, EGambitEffectHook::ScoreModifier, EGambitItemEffectType::AddScoreFlat);
	EffectDefinition->Amount = 10.0f;
	ModuleDefinition->EffectDefinitions.Add(EffectDefinition);

	FGambitEffectExecutionContext Context;
	Context.Hook = EGambitEffectHook::ScoreModifier;

	Resolver->ExecuteItemEffects(ModuleDefinition, Context);

	TestEqual(TEXT("module effect adds +10 score"), Context.ScoreModifierDelta.AdditiveBonus, 10.0f);
	TestTrue(TEXT("module effect records a debug score line"), Context.DebugScoreLines.Num() > 0);
	TestTrue(TEXT("module effect records a debug effect event"), Context.DebugEffectEvents.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitModuleConditionalMultiplierEffectTest,
	"GrandpaGambit.Effects.ModuleMultiplierWithSix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitModuleConditionalMultiplierEffectTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitModuleDefinition* ModuleDefinition = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(ModuleDefinition, EGambitEffectHook::ScoreModifier, EGambitItemEffectType::MultiplyScore);
	EffectDefinition->Multiplier = 1.5f;

	FGambitEffectConditionDefinition Condition;
	Condition.ConditionType = EGambitEffectConditionType::DieValueEquals;
	Condition.Value = 6;
	EffectDefinition->Conditions.Add(Condition);
	ModuleDefinition->EffectDefinitions.Add(EffectDefinition);

	FGambitEffectExecutionContext Context;
	Context.Hook = EGambitEffectHook::ScoreModifier;
	Context.SourceDice.Add(MakeTestDie(6));

	Resolver->ExecuteItemEffects(ModuleDefinition, Context);

	TestEqual(TEXT("module multiplies score by 1.5 when a six is present"), Context.ScoreModifierDelta.Multiplier, 1.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitConsumableModifyDieEffectTest,
	"GrandpaGambit.Effects.ConsumableModifyDie",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitConsumableModifyDieEffectTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitConsumableDefinition* ConsumableDefinition = NewObject<UGambitConsumableDefinition>();
	UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(ConsumableDefinition, EGambitEffectHook::ConsumableUse, EGambitItemEffectType::ModifyDieValue);
	EffectDefinition->Amount = 2.0f;
	EffectDefinition->DieIndex = 0;
	ConsumableDefinition->EffectDefinitions.Add(EffectDefinition);

	FGambitEffectExecutionContext Context;
	Context.Hook = EGambitEffectHook::ConsumableUse;
	Context.SourceDice.Add(MakeTestDie(3));

	Resolver->ExecuteItemEffects(ConsumableDefinition, Context);

	TestEqual(TEXT("consumable adds +2 to die value"), Context.SourceDice[0].EffectiveValue, 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitConsumableOpponentTargetEffectTest,
	"GrandpaGambit.Effects.ConsumableOpponentTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitConsumableOpponentTargetEffectTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitConsumableDefinition* ConsumableDefinition = NewObject<UGambitConsumableDefinition>();
	ConsumableDefinition->bCanTargetOpponent = true;
	UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(ConsumableDefinition, EGambitEffectHook::ConsumableUse, EGambitItemEffectType::StealGold);
	EffectDefinition->Amount = 4.0f;
	EffectDefinition->bNegativeEffect = true;
	ConsumableDefinition->EffectDefinitions.Add(EffectDefinition);

	UGambitEconomyComponent* SourceEconomy = NewObject<UGambitEconomyComponent>();
	UGambitEconomyComponent* TargetEconomy = NewObject<UGambitEconomyComponent>();
	SourceEconomy->InitializeForMatch();
	TargetEconomy->InitializeForMatch();
	const int32 SourceGoldBefore = SourceEconomy->GetCurrentGold();
	TargetEconomy->AddGold(10);
	const int32 TargetGoldBefore = TargetEconomy->GetCurrentGold();

	FGambitEffectExecutionContext Context;
	Context.Hook = EGambitEffectHook::ConsumableUse;
	Context.SourceEconomyComponent = SourceEconomy;
	Context.TargetEconomyComponent = TargetEconomy;

	Resolver->ExecuteItemEffects(ConsumableDefinition, Context);

	TestEqual(TEXT("target loses 4 gold"), TargetEconomy->GetCurrentGold(), TargetGoldBefore - 4);
	TestEqual(TEXT("source gains 4 gold"), SourceEconomy->GetCurrentGold(), SourceGoldBefore + 4);
	TestEqual(TEXT("steal gold records target loss and source gain"), Context.DebugGoldLines.Num(), 2);
	TestTrue(TEXT("steal gold records a debug effect event"), Context.DebugEffectEvents.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitNegativeEffectProtectionCategoriesTest,
	"GrandpaGambit.Effects.NegativeProtection.Categories",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitNegativeEffectProtectionCategoriesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UObject* TestOuter = GetTransientPackage();
	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();

	{
		FGambitGoldStealProtectionTestContext TestContext = MakeGoldStealProtectionTestContext();
		UGambitItemEffectDefinition* GlobalDefense = MakePreventNegativeEffectDefinition(
			TestOuter,
			EGambitEffectHook::ConsumableUse,
			TArray<EGambitNegativeEffectCategory>());
		UGambitItemEffectDefinition* GoldSteal = MakeStealGoldEffectDefinition(TestOuter, true);

		TestTrue(TEXT("legacy global defense is applied"), Resolver->ExecuteEffectDefinition(GlobalDefense, TestContext.EffectContext));
		TestFalse(TEXT("legacy global defense blocks gold steal"), Resolver->ExecuteEffectDefinition(GoldSteal, TestContext.EffectContext));
		TestEqual(TEXT("global defense leaves target gold unchanged"), TestContext.TargetEconomy->GetCurrentGold(), TestContext.TargetGoldBefore);
		TestEqual(TEXT("global defense leaves source gold unchanged"), TestContext.SourceEconomy->GetCurrentGold(), TestContext.SourceGoldBefore);
		TestEqual(TEXT("global defense records one prevented event"), CountPreventedDebugEvents(TestContext.EffectContext), 1);
	}

	{
		FGambitGoldStealProtectionTestContext TestContext = MakeGoldStealProtectionTestContext();
		UGambitItemEffectDefinition* GoldStealDefense = MakePreventNegativeEffectDefinition(
			TestOuter,
			EGambitEffectHook::ConsumableUse,
			TArray<EGambitNegativeEffectCategory>({ EGambitNegativeEffectCategory::GoldSteal }));
		UGambitItemEffectDefinition* GoldSteal = MakeStealGoldEffectDefinition(TestOuter, true);

		TestTrue(TEXT("filtered GoldSteal defense is applied"), Resolver->ExecuteEffectDefinition(GoldStealDefense, TestContext.EffectContext));
		TestFalse(TEXT("filtered GoldSteal defense blocks gold steal"), Resolver->ExecuteEffectDefinition(GoldSteal, TestContext.EffectContext));
		TestEqual(TEXT("filtered defense leaves target gold unchanged"), TestContext.TargetEconomy->GetCurrentGold(), TestContext.TargetGoldBefore);
		TestEqual(TEXT("filtered defense records one prevented event"), CountPreventedDebugEvents(TestContext.EffectContext), 1);
	}

	{
		FGambitEffectExecutionContext Context;
		Context.Hook = EGambitEffectHook::RoundEnd;
		Context.SourceDice.Add(MakeTestDie(6));
		Context.SourceDieHandIndex = 0;
		Context.RandomStream.Initialize(123);

		UGambitItemEffectDefinition* GoldStealDefense = MakePreventNegativeEffectDefinition(
			TestOuter,
			EGambitEffectHook::RoundEnd,
			TArray<EGambitNegativeEffectCategory>({ EGambitNegativeEffectCategory::GoldSteal }));
		UGambitItemEffectDefinition* DestroyDie = MakeEffectDefinition(TestOuter, EGambitEffectHook::RoundEnd, EGambitItemEffectType::DestroyOrRemoveDiceChance);
		DestroyDie->EffectId = TEXT("effect.test.destroy_die");
		DestroyDie->Amount = 100.0f;
		DestroyDie->bNegativeEffect = true;
		DestroyDie->NegativeEffectCategories.Add(EGambitNegativeEffectCategory::DieDestroyOrRemove);

		TestTrue(TEXT("filtered GoldSteal defense is applied before destroy"), Resolver->ExecuteEffectDefinition(GoldStealDefense, Context));
		const int32 PreventedEventsBeforeDestroy = CountPreventedDebugEvents(Context);
		TestTrue(TEXT("GoldSteal defense does not block die destroy"), Resolver->ExecuteEffectDefinition(DestroyDie, Context));
		TestEqual(TEXT("die destroy removes source die"), Context.SourceDice.Num(), 0);
		TestEqual(TEXT("die destroy did not add a prevented event"), CountPreventedDebugEvents(Context), PreventedEventsBeforeDestroy);
	}

	{
		FGambitGoldStealProtectionTestContext TestContext = MakeGoldStealProtectionTestContext();
		UGambitItemEffectDefinition* GenericDefense = MakePreventNegativeEffectDefinition(
			TestOuter,
			EGambitEffectHook::ConsumableUse,
			TArray<EGambitNegativeEffectCategory>({ EGambitNegativeEffectCategory::Generic }));
		UGambitItemEffectDefinition* UncategorizedGoldSteal = MakeStealGoldEffectDefinition(TestOuter, false);

		TestTrue(TEXT("Generic defense is applied"), Resolver->ExecuteEffectDefinition(GenericDefense, TestContext.EffectContext));
		TestFalse(TEXT("Generic fallback blocks uncategorized negative effect"), Resolver->ExecuteEffectDefinition(UncategorizedGoldSteal, TestContext.EffectContext));
		TestEqual(TEXT("fallback prevention leaves target gold unchanged"), TestContext.TargetEconomy->GetCurrentGold(), TestContext.TargetGoldBefore);
		TestEqual(TEXT("fallback prevention records one prevented event"), CountPreventedDebugEvents(TestContext.EffectContext), 1);
	}

	{
		FGambitGoldStealProtectionTestContext TestContext = MakeGoldStealProtectionTestContext();
		UGambitItemEffectDefinition* GoldSteal = MakeStealGoldEffectDefinition(TestOuter, true);

		TestTrue(TEXT("gold steal triggers without defense"), Resolver->ExecuteEffectDefinition(GoldSteal, TestContext.EffectContext));
		TestEqual(TEXT("no defense lets target lose gold"), TestContext.TargetEconomy->GetCurrentGold(), TestContext.TargetGoldBefore - 4);
		TestEqual(TEXT("no defense lets source gain gold"), TestContext.SourceEconomy->GetCurrentGold(), TestContext.SourceGoldBefore + 4);
		TestEqual(TEXT("no defense records no prevented event"), CountPreventedDebugEvents(TestContext.EffectContext), 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitNegativeEffectProtectionValidationTest,
	"GrandpaGambit.Effects.NegativeProtection.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitNegativeEffectProtectionValidationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitItemEffectDefinition* UncategorizedNegativeEffect = MakeStealGoldEffectDefinition(GetTransientPackage(), false);
	TArray<FGambitDataValidationIssue> UncategorizedIssues;
	GambitDataValidation::ValidateEffectDefinition(UncategorizedNegativeEffect, TEXT("Test"), 0, UncategorizedIssues);
	TestTrue(
		TEXT("negative effect without explicit category warns about Generic fallback"),
		HasValidationIssue(UncategorizedIssues, EGambitDataValidationSeverity::Warning, TEXT("Generic fallback")));

	UGambitItemEffectDefinition* NoneFilteredDefense = MakePreventNegativeEffectDefinition(
		GetTransientPackage(),
		EGambitEffectHook::ConsumableUse,
		TArray<EGambitNegativeEffectCategory>({ EGambitNegativeEffectCategory::None }));
	TArray<FGambitDataValidationIssue> NoneFilterIssues;
	GambitDataValidation::ValidateEffectDefinition(NoneFilteredDefense, TEXT("Test"), 0, NoneFilterIssues);
	TestTrue(
		TEXT("defense filter using None warns"),
		HasValidationIssue(NoneFilterIssues, EGambitDataValidationSeverity::Warning, TEXT("category None")));
	TestTrue(
		TEXT("defense with no concrete typed category warns"),
		HasValidationIssue(NoneFilterIssues, EGambitDataValidationSeverity::Warning, TEXT("no concrete category")));

	UGambitItemEffectDefinition* NonDefenseWithFilter = MakeEffectDefinition(GetTransientPackage(), EGambitEffectHook::ConsumableUse, EGambitItemEffectType::AddGold);
	NonDefenseWithFilter->Amount = 1.0f;
	NonDefenseWithFilter->PreventedNegativeEffectCategories.Add(EGambitNegativeEffectCategory::GoldSteal);
	TArray<FGambitDataValidationIssue> NonDefenseIssues;
	GambitDataValidation::ValidateEffectDefinition(NonDefenseWithFilter, TEXT("Test"), 0, NonDefenseIssues);
	TestTrue(
		TEXT("non-defense with prevention filter warns"),
		HasValidationIssue(NonDefenseIssues, EGambitDataValidationSeverity::Warning, TEXT("not a PreventNegativeEffect defense")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitConsumableUsablePhasesTest,
	"GrandpaGambit.Consumables.UsablePhases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitConsumableUsablePhasesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitConsumableDefinition* DefaultConsumable = NewObject<UGambitConsumableDefinition>();
	TestTrue(TEXT("consumables default to Action phase usage"), DefaultConsumable->CanBeUsedDuringPhase(EGambitRoundPhase::Action));
	TestFalse(TEXT("consumables explicitly reject None phase usage"), DefaultConsumable->CanBeUsedDuringPhase(EGambitRoundPhase::None));
	TestFalse(TEXT("default consumables cannot be used during Reward"), DefaultConsumable->CanBeUsedDuringPhase(EGambitRoundPhase::Reward));

	UGambitConsumableDefinition* RewardConsumable = NewObject<UGambitConsumableDefinition>();
	RewardConsumable->UsablePhases.Reset();
	RewardConsumable->UsablePhases.Add(EGambitRoundPhase::Reward);
	TestFalse(TEXT("configured consumable cannot be used during Action when Action is not listed"), RewardConsumable->CanBeUsedDuringPhase(EGambitRoundPhase::Action));
	TestTrue(TEXT("configured consumable can be used during Reward"), RewardConsumable->CanBeUsedDuringPhase(EGambitRoundPhase::Reward));

	UGambitConsumableDefinition* ConsumableWithNoneListed = NewObject<UGambitConsumableDefinition>();
	ConsumableWithNoneListed->UsablePhases.Reset();
	ConsumableWithNoneListed->UsablePhases.Add(EGambitRoundPhase::None);
	ConsumableWithNoneListed->UsablePhases.Add(EGambitRoundPhase::Action);
	TestFalse(TEXT("listed None phase is still never usable"), ConsumableWithNoneListed->CanBeUsedDuringPhase(EGambitRoundPhase::None));

	UGambitConsumableDefinition* InvalidConsumable = NewObject<UGambitConsumableDefinition>();
	InvalidConsumable->ItemId = TEXT("consumable.test.invalid_phases");
	UGambitItemEffectDefinition* InvalidPhaseEffect = MakeEffectDefinition(InvalidConsumable, EGambitEffectHook::ConsumableUse, EGambitItemEffectType::AddGold);
	InvalidPhaseEffect->Amount = 1.0f;
	InvalidConsumable->EffectDefinitions.Add(InvalidPhaseEffect);
	InvalidConsumable->UsablePhases.Reset();

	TArray<FGambitDataValidationIssue> Issues;
	GambitDataValidation::ValidateItemDefinition(InvalidConsumable, Issues);
	TestTrue(
		TEXT("data validation rejects consumables with no usable phases"),
		HasValidationIssue(Issues, EGambitDataValidationSeverity::Error, TEXT("no usable phases")));

	UGambitConsumableDefinition* InvalidHookConsumable = NewObject<UGambitConsumableDefinition>();
	InvalidHookConsumable->ItemId = TEXT("consumable.test.invalid_hook");
	UGambitItemEffectDefinition* InvalidHookEffect = MakeEffectDefinition(InvalidHookConsumable, EGambitEffectHook::Reward, EGambitItemEffectType::AddGold);
	InvalidHookEffect->Amount = 1.0f;
	InvalidHookConsumable->EffectDefinitions.Add(InvalidHookEffect);

	TArray<FGambitDataValidationIssue> HookIssues;
	GambitDataValidation::ValidateItemDefinition(InvalidHookConsumable, HookIssues);
	TestTrue(
		TEXT("data validation rejects non-ConsumableUse hooks on consumables"),
		HasValidationIssue(HookIssues, EGambitDataValidationSeverity::Error, TEXT("must execute with ConsumableUse")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitItemEffectPayloadValidationTest,
	"GrandpaGambit.Items.EffectPayloadValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitItemEffectPayloadValidationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();

	UGambitModuleDefinition* EmptyModule = NewObject<UGambitModuleDefinition>();
	EmptyModule->ItemId = TEXT("module.test.empty_effect_payload");
	TArray<FGambitDataValidationIssue> EmptyModuleIssues;
	GambitDataValidation::ValidateItemDefinition(EmptyModule, EmptyModuleIssues);
	TestTrue(
		TEXT("module without EffectDefinitions emits an effect payload error"),
		HasValidationIssue(EmptyModuleIssues, EGambitDataValidationSeverity::Error, TEXT("module with no effect payload")));
	TestTrue(TEXT("module without EffectDefinitions is blocking"), GambitDataValidation::HasBlockingIssues(EmptyModuleIssues));

	UGambitModuleDefinition* ModuleDefinition = NewObject<UGambitModuleDefinition>();
	ModuleDefinition->ItemId = TEXT("module.test.effect_definitions");
	UGambitItemEffectDefinition* ModuleEffect = MakeEffectDefinition(ModuleDefinition, EGambitEffectHook::ScoreModifier, EGambitItemEffectType::AddScoreFlat);
	ModuleEffect->Amount = 2.0f;
	ModuleDefinition->EffectDefinitions.Add(ModuleEffect);
	TArray<FGambitDataValidationIssue> ModuleIssues;
	GambitDataValidation::ValidateItemDefinition(ModuleDefinition, ModuleIssues);
	TestFalse(TEXT("module with EffectDefinitions has no blocking validation issues"), GambitDataValidation::HasBlockingIssues(ModuleIssues));

	FGambitEffectExecutionContext ModuleContext;
	ModuleContext.Hook = EGambitEffectHook::ScoreModifier;
	Resolver->ExecuteItemEffects(ModuleDefinition, ModuleContext);
	TestEqual(TEXT("module applies EffectDefinitions additive score"), ModuleContext.ScoreModifierDelta.AdditiveBonus, 2.0f);

	UGambitInventoryComponent* InventoryComponent = NewObject<UGambitInventoryComponent>();
	TestTrue(TEXT("inventory accepts EffectDefinitions-backed module"), InventoryComponent->AddModule(ModuleDefinition));

	UGambitConsumableDefinition* EmptyConsumable = NewObject<UGambitConsumableDefinition>();
	EmptyConsumable->ItemId = TEXT("consumable.test.empty_effect_payload");
	TArray<FGambitDataValidationIssue> EmptyConsumableIssues;
	GambitDataValidation::ValidateItemDefinition(EmptyConsumable, EmptyConsumableIssues);
	TestTrue(
		TEXT("consumable without EffectDefinitions emits an effect payload error"),
		HasValidationIssue(EmptyConsumableIssues, EGambitDataValidationSeverity::Error, TEXT("consumable with no effect payload")));
	TestTrue(TEXT("consumable without EffectDefinitions is blocking"), GambitDataValidation::HasBlockingIssues(EmptyConsumableIssues));

	UGambitConsumableDefinition* MigratedFlatConsumable = NewObject<UGambitConsumableDefinition>();
	MigratedFlatConsumable->ItemId = TEXT("consumable.test.flat_effect");
	UGambitItemEffectDefinition* MigratedFlatEffect = MakeEffectDefinition(MigratedFlatConsumable, EGambitEffectHook::ConsumableUse, EGambitItemEffectType::AddScoreFlat);
	MigratedFlatEffect->Amount = 4.0f;
	MigratedFlatConsumable->EffectDefinitions.Add(MigratedFlatEffect);
	TArray<FGambitDataValidationIssue> MigratedFlatIssues;
	GambitDataValidation::ValidateItemDefinition(MigratedFlatConsumable, MigratedFlatIssues);
	TestFalse(TEXT("flat score consumable with EffectDefinitions has no blocking validation issues"), GambitDataValidation::HasBlockingIssues(MigratedFlatIssues));

	FGambitEffectExecutionContext MigratedFlatConsumableContext;
	MigratedFlatConsumableContext.Hook = EGambitEffectHook::ConsumableUse;
	Resolver->ExecuteItemEffects(MigratedFlatConsumable, MigratedFlatConsumableContext);
	TestEqual(TEXT("migrated consumable flat score uses temporary modifier channel"), MigratedFlatConsumableContext.TemporaryScoreModifierDelta.AdditiveBonus, 4.0f);
	TestEqual(TEXT("migrated consumable flat score does not use persistent score delta"), MigratedFlatConsumableContext.ScoreModifierDelta.AdditiveBonus, 0.0f);

	UGambitConsumableDefinition* MigratedMultiplierConsumable = NewObject<UGambitConsumableDefinition>();
	MigratedMultiplierConsumable->ItemId = TEXT("consumable.test.multiplier_effect");
	UGambitItemEffectDefinition* MigratedMultiplierEffect = MakeEffectDefinition(MigratedMultiplierConsumable, EGambitEffectHook::ConsumableUse, EGambitItemEffectType::MultiplyScore);
	MigratedMultiplierEffect->Multiplier = 1.5f;
	MigratedMultiplierConsumable->EffectDefinitions.Add(MigratedMultiplierEffect);
	FGambitEffectExecutionContext MigratedMultiplierConsumableContext;
	MigratedMultiplierConsumableContext.Hook = EGambitEffectHook::ConsumableUse;
	Resolver->ExecuteItemEffects(MigratedMultiplierConsumable, MigratedMultiplierConsumableContext);
	TestEqual(TEXT("migrated consumable multiplier uses temporary modifier channel"), MigratedMultiplierConsumableContext.TemporaryScoreModifierDelta.Multiplier, 1.5f);
	TestEqual(TEXT("migrated consumable multiplier does not use persistent score delta"), MigratedMultiplierConsumableContext.ScoreModifierDelta.Multiplier, 1.0f);

	UGambitConsumableDefinition* InvalidHookConsumable = NewObject<UGambitConsumableDefinition>();
	InvalidHookConsumable->ItemId = TEXT("consumable.test.invalid_hook_effect_payload");
	UGambitItemEffectDefinition* InvalidHookEffect = MakeEffectDefinition(InvalidHookConsumable, EGambitEffectHook::Reward, EGambitItemEffectType::AddGold);
	InvalidHookEffect->Amount = 1.0f;
	InvalidHookConsumable->EffectDefinitions.Add(InvalidHookEffect);
	TArray<FGambitDataValidationIssue> InvalidHookIssues;
	GambitDataValidation::ValidateItemDefinition(InvalidHookConsumable, InvalidHookIssues);
	TestTrue(
		TEXT("consumable effect payload must use ConsumableUse hook"),
		HasValidationIssue(InvalidHookIssues, EGambitDataValidationSeverity::Error, TEXT("must execute with ConsumableUse")));
	TestTrue(TEXT("invalid consumable hook is blocking"), GambitDataValidation::HasBlockingIssues(InvalidHookIssues));

	UGambitConsumableDefinition* EconomyConsumable = NewObject<UGambitConsumableDefinition>();
	EconomyConsumable->ItemId = TEXT("consumable.test.gold_effect");
	UGambitItemEffectDefinition* ConsumableEffect = MakeEffectDefinition(EconomyConsumable, EGambitEffectHook::ConsumableUse, EGambitItemEffectType::AddGold);
	ConsumableEffect->Amount = 1.0f;
	EconomyConsumable->EffectDefinitions.Add(ConsumableEffect);
	UGambitEconomyComponent* EconomyComponent = NewObject<UGambitEconomyComponent>();
	EconomyComponent->InitializeForMatch();
	const int32 GoldBeforeConsumable = EconomyComponent->GetCurrentGold();
	FGambitEffectExecutionContext ConsumableContext;
	ConsumableContext.Hook = EGambitEffectHook::ConsumableUse;
	ConsumableContext.SourceEconomyComponent = EconomyComponent;
	Resolver->ExecuteItemEffects(EconomyConsumable, ConsumableContext);
	TestEqual(TEXT("consumable executes EffectDefinitions"), EconomyComponent->GetCurrentGold() - GoldBeforeConsumable, 1);

	UGambitInventoryComponent* ConsumableInventoryComponent = NewObject<UGambitInventoryComponent>();
	TestTrue(TEXT("inventory accepts EffectDefinitions-backed consumable"), ConsumableInventoryComponent->AddConsumable(EconomyConsumable));
	UGambitConsumableDefinition* ConsumedDefinition = nullptr;
	TestTrue(TEXT("inventory consumes EffectDefinitions-backed consumable"), ConsumableInventoryComponent->ConsumeConsumableDefinitionAtSlot(0, ConsumedDefinition));
	TestTrue(TEXT("inventory returns consumed consumable definition"), ConsumedDefinition == EconomyConsumable);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitCouponMakesOfferFreeTest,
	"GrandpaGambit.Shop.CouponMakesOfferFree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitCouponMakesOfferFreeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitShopComponent* ShopComponent = NewObject<UGambitShopComponent>();
	UGambitModuleDefinition* Coupon = NewObject<UGambitModuleDefinition>();
	UGambitItemDefinition* OfferItem = MakeShopItem(GetTransientPackage(), TEXT("item.test.free"), 12);
	OfferItem->Tags.Add(TEXT("shop.coupon.free"));

	UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(Coupon, EGambitEffectHook::PrePriceResolve, EGambitItemEffectType::MakeShopOfferFree);
	FGambitEffectConditionDefinition Condition;
	Condition.ConditionType = EGambitEffectConditionType::ShopItemTag;
	Condition.Tag = TEXT("shop.coupon.free");
	EffectDefinition->Conditions.Add(Condition);
	Coupon->EffectDefinitions.Add(EffectDefinition);

	FGambitEffectExecutionContext Context;
	Context.Hook = EGambitEffectHook::PrePriceResolve;
	Context.ShopPurchase = MakePriceContext(OfferItem, 12);

	Resolver->ExecuteItemEffects(Coupon, Context);
	ShopComponent->ResolvePurchasePrice(Context.ShopPurchase);

	TestEqual(TEXT("coupon makes the matching offer free"), Context.ShopPurchase.ResolvedPrice, 0);
	TestTrue(TEXT("coupon records a free reason"), !Context.ShopPurchase.FreeReason.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitBargainFirstPurchaseDiscountTest,
	"GrandpaGambit.Shop.BargainDiscountsFirstPurchase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitBargainFirstPurchaseDiscountTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitShopComponent* ShopComponent = NewObject<UGambitShopComponent>();
	UGambitModuleDefinition* Bargain = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(Bargain, EGambitEffectHook::PrePriceResolve, EGambitItemEffectType::AddShopDiscountPercent);
	EffectDefinition->Amount = 50.0f;

	FGambitEffectConditionDefinition FirstPurchaseCondition;
	FirstPurchaseCondition.ConditionType = EGambitEffectConditionType::ShopPurchaseCount;
	FirstPurchaseCondition.Comparison = EGambitEffectComparison::Equal;
	FirstPurchaseCondition.Value = 0;
	EffectDefinition->Conditions.Add(FirstPurchaseCondition);
	Bargain->EffectDefinitions.Add(EffectDefinition);

	UGambitItemDefinition* OfferItem = MakeShopItem(GetTransientPackage(), TEXT("item.test.bargain"), 10);

	FGambitEffectExecutionContext FirstContext;
	FirstContext.Hook = EGambitEffectHook::PrePriceResolve;
	FirstContext.ShopPurchase = MakePriceContext(OfferItem, 10);
	FirstContext.ShopPurchase.PurchasesMadeBefore = 0;
	Resolver->ExecuteItemEffects(Bargain, FirstContext);
	ShopComponent->ResolvePurchasePrice(FirstContext.ShopPurchase);

	FGambitEffectExecutionContext SecondContext;
	SecondContext.Hook = EGambitEffectHook::PrePriceResolve;
	SecondContext.ShopPurchase = MakePriceContext(OfferItem, 10);
	SecondContext.ShopPurchase.PurchasesMadeBefore = 1;
	Resolver->ExecuteItemEffects(Bargain, SecondContext);
	ShopComponent->ResolvePurchasePrice(SecondContext.ShopPurchase);

	TestEqual(TEXT("bargain halves the first purchase"), FirstContext.ShopPurchase.ResolvedPrice, 5);
	TestEqual(TEXT("bargain does not affect later purchases"), SecondContext.ShopPurchase.ResolvedPrice, 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitCashbackReturnsThirtyPercentTest,
	"GrandpaGambit.Shop.CashbackReturnsThirtyPercent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitCashbackReturnsThirtyPercentTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitShopComponent* ShopComponent = NewObject<UGambitShopComponent>();
	UGambitModuleDefinition* Cashback = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(Cashback, EGambitEffectHook::PostPurchase, EGambitItemEffectType::AddShopCashbackPercent);
	EffectDefinition->Amount = 30.0f;
	Cashback->EffectDefinitions.Add(EffectDefinition);

	UGambitEconomyComponent* EconomyComponent = NewObject<UGambitEconomyComponent>();
	EconomyComponent->InitializeForMatch();
	EconomyComponent->AddGold(20);

	FGambitEffectExecutionContext Context;
	Context.Hook = EGambitEffectHook::PostPurchase;
	Context.SourceEconomyComponent = EconomyComponent;
	Context.ShopPurchase = MakePriceContext(MakeShopItem(GetTransientPackage(), TEXT("item.test.cashback"), 10), 10);
	Context.ShopPurchase.ResolvedPrice = 10;
	Context.ShopPurchase.bPurchaseSucceeded = true;

	EconomyComponent->SpendGold(10);
	Resolver->ExecuteItemEffects(Cashback, Context);
	ShopComponent->ApplyPostPurchaseAdjustments(Context.ShopPurchase, EconomyComponent);

	TestEqual(TEXT("cashback returns 30 percent of a 10 gold purchase"), Context.ShopPurchase.CashbackGold, 3);
	TestEqual(TEXT("gold after spend and cashback is 13"), EconomyComponent->GetCurrentGold(), 13);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitDebtAllowsNegativeTwentyGoldTest,
	"GrandpaGambit.Economy.DebtAllowsNegativeTwenty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitDebtAllowsNegativeTwentyGoldTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEconomyComponent* EconomyComponent = NewObject<UGambitEconomyComponent>();
	EconomyComponent->InitializeForMatch();
	EconomyComponent->ModifyDebtLimit(20);

	TestTrue(TEXT("debt allows spending down to -20"), EconomyComponent->SpendGold(20));
	TestEqual(TEXT("gold reaches -20"), EconomyComponent->GetCurrentGold(), -20);
	TestFalse(TEXT("debt blocks spending below -20"), EconomyComponent->SpendGold(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitB4GoldCapAndInterestModifiersTest,
	"GrandpaGambit.B4.Economy.GoldCapAndInterestModifiers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitB4GoldCapAndInterestModifiersTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEconomyComponent* CappedEconomy = NewObject<UGambitEconomyComponent>();
	CappedEconomy->InitializeForMatch();
	CappedEconomy->AddGold(9999);
	TestEqual(TEXT("gold is clamped to the effective cap"), CappedEconomy->GetCurrentGold(), CappedEconomy->GetEffectiveMaxGold());

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitModuleDefinition* Interests = NewObject<UGambitModuleDefinition>();

	UGambitItemEffectDefinition* IntervalEffect = MakeEffectDefinition(Interests, EGambitEffectHook::RoundStart, EGambitItemEffectType::ModifyInterestInterval);
	IntervalEffect->EffectId = FName(TEXT("effect.test.interests.interval"));
	IntervalEffect->Amount = -5.0f;
	Interests->EffectDefinitions.Add(IntervalEffect);

	UGambitItemEffectDefinition* BonusEffect = MakeEffectDefinition(Interests, EGambitEffectHook::RoundStart, EGambitItemEffectType::ModifyInterestBonus);
	BonusEffect->EffectId = FName(TEXT("effect.test.interests.bonus"));
	BonusEffect->Amount = 1.0f;
	Interests->EffectDefinitions.Add(BonusEffect);

	UGambitEconomyComponent* InterestEconomy = NewObject<UGambitEconomyComponent>();
	InterestEconomy->InitializeForMatch();
	InterestEconomy->AddGold(20);

	FGambitEffectExecutionContext Context;
	Context.Hook = EGambitEffectHook::RoundStart;
	Context.SourceEconomyComponent = InterestEconomy;
	Resolver->ExecuteItemEffects(Interests, Context);

	const int32 InterestGranted = InterestEconomy->ApplyRoundEconomy(0);
	TestEqual(TEXT("interest interval modifier changes interval to 5"), InterestEconomy->GetEffectiveInterestInterval(), 5);
	TestEqual(TEXT("interest bonus applies after interval calculation"), InterestGranted, 5);
	TestEqual(TEXT("interest gold is applied to current gold"), InterestEconomy->GetCurrentGold(), 25);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitB4RecurringIncomeTest,
	"GrandpaGambit.B4.Economy.RecurringIncome",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitB4RecurringIncomeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitConsumableDefinition* Investment = NewObject<UGambitConsumableDefinition>();

	UGambitItemEffectDefinition* RecurringEffect = MakeEffectDefinition(Investment, EGambitEffectHook::ConsumableUse, EGambitItemEffectType::AddRecurringGoldIncome);
	RecurringEffect->EffectId = FName(TEXT("effect.test.investment.recurring"));
	RecurringEffect->Amount = 3.0f;
	RecurringEffect->DurationRounds = 2;
	Investment->EffectDefinitions.Add(RecurringEffect);

	UGambitItemEffectDefinition* SpendEffect = MakeEffectDefinition(Investment, EGambitEffectHook::ConsumableUse, EGambitItemEffectType::SpendGold);
	SpendEffect->Amount = 10.0f;
	Investment->EffectDefinitions.Add(SpendEffect);

	UGambitEconomyComponent* EconomyComponent = NewObject<UGambitEconomyComponent>();
	EconomyComponent->InitializeForMatch();
	EconomyComponent->AddGold(10);

	FGambitEffectExecutionContext Context;
	Context.Hook = EGambitEffectHook::ConsumableUse;
	Context.SourceEconomyComponent = EconomyComponent;
	Resolver->ExecuteItemEffects(Investment, Context);

	TestEqual(TEXT("investment spends its upfront cost"), EconomyComponent->GetCurrentGold(), 0);
	EconomyComponent->ApplyRoundEconomy(0);
	TestEqual(TEXT("recurring income applies on first reward"), EconomyComponent->GetCurrentGold(), 3);
	EconomyComponent->ApplyRoundEconomy(0);
	TestEqual(TEXT("recurring income applies for its configured duration"), EconomyComponent->GetCurrentGold(), 6);
	EconomyComponent->ApplyRoundEconomy(0);
	TestEqual(TEXT("recurring income expires after duration"), EconomyComponent->GetCurrentGold(), 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitB4SkipShopGoldTest,
	"GrandpaGambit.B4.Shop.SkipShopGold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitB4SkipShopGoldTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitModuleDefinition* Cheapskate = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(Cheapskate, EGambitEffectHook::ShopSkipped, EGambitItemEffectType::AddGold);
	EffectDefinition->Amount = 8.0f;
	Cheapskate->EffectDefinitions.Add(EffectDefinition);

	UGambitEconomyComponent* EconomyComponent = NewObject<UGambitEconomyComponent>();
	EconomyComponent->InitializeForMatch();

	FGambitEffectExecutionContext Context;
	Context.Hook = EGambitEffectHook::ShopSkipped;
	Context.SourceEconomyComponent = EconomyComponent;
	Resolver->ExecuteItemEffects(Cheapskate, Context);

	TestEqual(TEXT("skip shop reward adds gold"), EconomyComponent->GetCurrentGold(), 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitB4RerollEconomyAndScoreTest,
	"GrandpaGambit.B4.Reroll.EconomyAndScore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitB4RerollEconomyAndScoreTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();

	UGambitModuleDefinition* FreeReroll = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* FreeRerollEffect = MakeEffectDefinition(FreeReroll, EGambitEffectHook::RoundStart, EGambitItemEffectType::ModifyRerollLimit);
	FreeRerollEffect->Amount = 1.0f;
	FreeReroll->EffectDefinitions.Add(FreeRerollEffect);

	FGambitEffectExecutionContext LimitContext;
	LimitContext.Hook = EGambitEffectHook::RoundStart;
	LimitContext.RerollLimit = 2;
	Resolver->ExecuteItemEffects(FreeReroll, LimitContext);
	TestEqual(TEXT("free reroll modifies reroll limit"), LimitContext.RerollLimitDelta, 1);

	UGambitModuleDefinition* ProfitableReroll = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* ScoreEffect = MakeEffectDefinition(ProfitableReroll, EGambitEffectHook::PostReroll, EGambitItemEffectType::AddTemporaryScoreModifier);
	ScoreEffect->ScoreModifier.AdditiveBonus = 10.0f;
	ProfitableReroll->EffectDefinitions.Add(ScoreEffect);

	FGambitEffectExecutionContext ScoreContext;
	ScoreContext.Hook = EGambitEffectHook::PostReroll;
	ScoreContext.RerollsUsed = 1;
	ScoreContext.RerollLimit = 3;
	Resolver->ExecuteItemEffects(ProfitableReroll, ScoreContext);
	TestEqual(TEXT("profitable reroll adds temporary score per reroll"), ScoreContext.TemporaryScoreModifierDelta.AdditiveBonus, 10.0f);

	UGambitModuleDefinition* DangerousReroll = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* RiskEffect = MakeEffectDefinition(DangerousReroll, EGambitEffectHook::PostReroll, EGambitItemEffectType::AddTemporaryScoreModifier);
	RiskEffect->ScoreModifier.AdditiveBonus = -5.0f;
	RiskEffect->ScoreModifier.Multiplier = 1.2f;
	DangerousReroll->EffectDefinitions.Add(RiskEffect);

	FGambitEffectExecutionContext RiskContext;
	RiskContext.Hook = EGambitEffectHook::PostReroll;
	RiskContext.RerollsUsed = 1;
	RiskContext.RerollLimit = 3;
	Resolver->ExecuteItemEffects(DangerousReroll, RiskContext);
	TestEqual(TEXT("dangerous reroll applies additive risk"), RiskContext.TemporaryScoreModifierDelta.AdditiveBonus, -5.0f);
	TestEqual(TEXT("dangerous reroll applies multiplier reward"), RiskContext.TemporaryScoreModifierDelta.Multiplier, 1.2f);

	UGambitModuleDefinition* GoldPerReroll = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* GoldPerRerollEffect = MakeEffectDefinition(GoldPerReroll, EGambitEffectHook::PostReroll, EGambitItemEffectType::AddGold);
	GoldPerRerollEffect->ScalarParameters.Add(TEXT("AmountPerRerollUsed"), 2.0f);
	GoldPerReroll->EffectDefinitions.Add(GoldPerRerollEffect);

	UGambitEconomyComponent* GoldPerRerollEconomy = NewObject<UGambitEconomyComponent>();
	GoldPerRerollEconomy->InitializeForMatch();

	FGambitEffectExecutionContext GoldPerRerollContext;
	GoldPerRerollContext.Hook = EGambitEffectHook::PostReroll;
	GoldPerRerollContext.RerollsUsed = 2;
	GoldPerRerollContext.RerollLimit = 3;
	GoldPerRerollContext.SourceEconomyComponent = GoldPerRerollEconomy;
	Resolver->ExecuteItemEffects(GoldPerReroll, GoldPerRerollContext);
	TestEqual(TEXT("gold per reroll used scalar applies"), GoldPerRerollEconomy->GetCurrentGold(), 4);

	UGambitModuleDefinition* GreedRoll = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* GreedEffect = MakeEffectDefinition(GreedRoll, EGambitEffectHook::Reward, EGambitItemEffectType::AddGold);
	GreedEffect->ScalarParameters.Add(TEXT("AmountPerUnusedReroll"), 3.0f);
	FGambitEffectConditionDefinition UnusedCondition;
	UnusedCondition.ConditionType = EGambitEffectConditionType::RerollsRemaining;
	UnusedCondition.Comparison = EGambitEffectComparison::Greater;
	UnusedCondition.Value = 0;
	GreedEffect->Conditions.Add(UnusedCondition);
	GreedRoll->EffectDefinitions.Add(GreedEffect);

	UGambitEconomyComponent* GreedEconomy = NewObject<UGambitEconomyComponent>();
	GreedEconomy->InitializeForMatch();

	FGambitEffectExecutionContext GreedContext;
	GreedContext.Hook = EGambitEffectHook::Reward;
	GreedContext.RerollsUsed = 1;
	GreedContext.RerollLimit = 3;
	GreedContext.SourceEconomyComponent = GreedEconomy;
	Resolver->ExecuteItemEffects(GreedRoll, GreedContext);
	TestEqual(TEXT("greed roll grants gold per unused reroll"), GreedEconomy->GetCurrentGold(), 6);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitSharedPoolReservationRemovesOfferForOthersTest,
	"GrandpaGambit.Shop.SharedPoolReservationRemovesOfferForOthers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitSharedPoolReservationRemovesOfferForOthersTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitItemDefinition* SharedItem = MakeShopItem(GetTransientPackage(), TEXT("item.test.shared"), 5);
	SharedItem->bUsesSharedPool = true;
	SharedItem->SharedPoolMaxStock = 1;

	UGambitSharedPoolComponent* SharedPool = NewObject<UGambitSharedPoolComponent>();
	FGambitSharedStockEntry StockEntry;
	StockEntry.ItemDefinition = SharedItem;
	StockEntry.MaxStock = 1;
	SharedPool->InitializeSharedStock({ StockEntry });

	UGambitShopLootTable* LootTable = NewObject<UGambitShopLootTable>();
	FGambitShopWeightedEntry WeightedEntry;
	WeightedEntry.ItemDefinition = SharedItem;
	WeightedEntry.Weight = 1.0f;
	LootTable->WeightedItems.Add(WeightedEntry);

	UGambitShopOfferGenerator* Generator = NewObject<UGambitShopOfferGenerator>();
	FRandomStream RandomStream(99);
	TArray<FGambitShopOffer> FirstPlayerOffers;
	TArray<FGambitShopOffer> SecondPlayerOffers;

	Generator->GenerateOffers(LootTable, 1, RandomStream, SharedPool, GetTransientPackage(), FirstPlayerOffers);
	Generator->GenerateOffers(LootTable, 1, RandomStream, SharedPool, GetTransientPackage(), SecondPlayerOffers);

	TestEqual(TEXT("first player gets the shared item offer"), FirstPlayerOffers.Num(), 1);
	TestEqual(TEXT("second player cannot see reserved shared item"), SecondPlayerOffers.Num(), 0);
	TestEqual(TEXT("shared pool available stock is reserved out"), SharedPool->GetRemainingStock(SharedItem), 0);
	TestEqual(TEXT("shared pool reserved stock is one"), SharedPool->GetReservedStock(SharedItem), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitDeterministicChanceEffectTest,
	"GrandpaGambit.Effects.DeterministicChance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitDeterministicChanceEffectTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitItemDefinition* ItemDefinition = NewObject<UGambitItemDefinition>();
	UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(ItemDefinition, EGambitEffectHook::ScoreModifier, EGambitItemEffectType::AddScoreFlat);
	EffectDefinition->Amount = 7.0f;

	FGambitEffectConditionDefinition ChanceCondition;
	ChanceCondition.ConditionType = EGambitEffectConditionType::ChancePercentage;
	ChanceCondition.ChancePercent = 50.0f;
	EffectDefinition->Conditions.Add(ChanceCondition);
	ItemDefinition->EffectDefinitions.Add(EffectDefinition);

	FGambitEffectExecutionContext FirstContext;
	FirstContext.Hook = EGambitEffectHook::ScoreModifier;
	FirstContext.RandomStream.Initialize(4242);

	FGambitEffectExecutionContext SecondContext;
	SecondContext.Hook = EGambitEffectHook::ScoreModifier;
	SecondContext.RandomStream.Initialize(4242);

	const int32 FirstTriggered = Resolver->ExecuteItemEffects(ItemDefinition, FirstContext);
	const int32 SecondTriggered = Resolver->ExecuteItemEffects(ItemDefinition, SecondContext);

	TestEqual(TEXT("same seed produces same trigger count"), FirstTriggered, SecondTriggered);
	TestEqual(TEXT("same seed produces same score delta"), FirstContext.ScoreModifierDelta.AdditiveBonus, SecondContext.ScoreModifierDelta.AdditiveBonus);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitDiceDefinitionFacesTest,
	"GrandpaGambit.Dice.DefinitionFaces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitDiceDefinitionFacesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UObject* TestOuter = GetTransientPackage();
	UGambitDiceDefinition* StandardDie = MakeDiceDefinition(TestOuter, TEXT("dice.standard"), TArray<int32>({ 1, 2, 3, 4, 5, 6 }));
	UGambitDiceDefinition* HeavyDie = MakeDiceDefinition(TestOuter, TEXT("dice.heavy"), TArray<int32>({ 2, 3, 3, 4, 4, 5 }));
	UGambitDiceDefinition* RiskyDie = MakeDiceDefinition(TestOuter, TEXT("dice.risky"), TArray<int32>({ 0, 0, 3, 6, 6, 9 }));
	UGambitDiceDefinition* CursedDie = MakeDiceDefinition(TestOuter, TEXT("dice.cursed"), TArray<int32>({ -3, -2, 0, 6, 9, 12 }));
	UGambitDiceDefinition* FallbackDie = MakeDiceDefinition(TestOuter, TEXT("dice.fallback"), TArray<int32>());
	HeavyDie->FaceWeights = TArray<float>({ 1.0f, 2.0f, 2.0f, 3.0f, 3.0f, 1.0f });

	TestTrue(TEXT("standard die has explicit D6 faces"), StandardDie->GetResolvedFaces() == TArray<int32>({ 1, 2, 3, 4, 5, 6 }));
	TestTrue(TEXT("heavy die has weighted duplicate faces"), HeavyDie->GetResolvedFaces() == TArray<int32>({ 2, 3, 3, 4, 4, 5 }));
	TestEqual(TEXT("heavy die reads optional per-face weight"), HeavyDie->GetFaceWeight(3), 3.0f);
	TestTrue(TEXT("risky die keeps zero and nine"), RiskyDie->GetResolvedFaces() == TArray<int32>({ 0, 0, 3, 6, 6, 9 }));
	TestTrue(TEXT("cursed die keeps negative zero and high values"), CursedDie->GetResolvedFaces() == TArray<int32>({ -3, -2, 0, 6, 9, 12 }));
	TestTrue(TEXT("empty faces fall back to D6"), FallbackDie->GetResolvedFaces() == TArray<int32>({ 1, 2, 3, 4, 5, 6 }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitDiceComponentExplicitFaceRollTest,
	"GrandpaGambit.Dice.ComponentExplicitFaceRoll",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitDiceComponentExplicitFaceRollTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitDiceDefinition* RiskyDie = MakeDiceDefinition(GetTransientPackage(), TEXT("dice.risky"), TArray<int32>({ 0, 0, 3, 6, 6, 9 }));
	UGambitDiceComponent* DiceComponent = NewObject<UGambitDiceComponent>();
	TArray<TObjectPtr<UGambitDiceDefinition>> OwnedDice;
	OwnedDice.Add(RiskyDie);
	DiceComponent->InitializeDicePool(OwnedDice);

	FRandomStream RandomStream(2026);
	DiceComponent->RollAll(RandomStream);

	const TArray<FGambitDieRuntimeState>& DiceStates = DiceComponent->GetDiceStatesRef();
	TestTrue(TEXT("dice component created at least one runtime die"), DiceStates.Num() >= 1);
	TestTrue(TEXT("rolled value comes from explicit risky faces"), RiskyDie->GetResolvedFaces().Contains(DiceStates[0].RawValue));

	DiceComponent->SetDieValue(0, 12);
	TestEqual(TEXT("controlled mutation does not clamp to authored faces"), DiceComponent->GetDiceStatesRef()[0].EffectiveValue, 12);

	for (int32 DieIndex = 0; DieIndex < DiceComponent->GetDiceStatesRef().Num(); ++DieIndex)
	{
		DiceComponent->SetDieCanBeRerolled(DieIndex, false);
	}
	const int32 ValueBeforeBlockedReroll = DiceComponent->GetDiceStatesRef()[0].EffectiveValue;
	const bool bRerolled = DiceComponent->RollUnlocked(RandomStream);
	TestFalse(TEXT("non-rerollable die blocks unlocked reroll"), bRerolled);
	TestEqual(TEXT("blocked reroll leaves value untouched"), DiceComponent->GetDiceStatesRef()[0].EffectiveValue, ValueBeforeBlockedReroll);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitDiceCombinationContributionRulesTest,
	"GrandpaGambit.Dice.CombinationContributionRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitDiceCombinationContributionRulesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitDiceCombinationEvaluator* Evaluator = NewObject<UGambitDiceCombinationEvaluator>();

	const FGambitDiceCombinationResult DoubleDieResult = Evaluator->EvaluateDice(TArray<FGambitDieRuntimeState>({
		MakeTestDie(6, 2, true, true)
	}));
	TestEqual(TEXT("double die counts as a pair for combinations"), DoubleDieResult.Combination, EGambitCombinationType::Pair);
	TestEqual(TEXT("double die contributes its score value once to sum"), DoubleDieResult.DiceSum, 6);

	const FGambitDiceCombinationResult TripleDieResult = Evaluator->EvaluateDice(TArray<FGambitDieRuntimeState>({
		MakeTestDie(4, 3, true, true)
	}));
	TestEqual(TEXT("triple die counts as three of a kind"), TripleDieResult.Combination, EGambitCombinationType::ThreeOfAKind);
	TestEqual(TEXT("triple die contributes its score value once to sum"), TripleDieResult.DiceSum, 4);

	const FGambitDiceCombinationResult GhostResult = Evaluator->EvaluateDice(TArray<FGambitDieRuntimeState>({
		MakeTestDie(6, 1, false, true),
		MakeTestDie(3, 1, true, true)
	}));
	TestEqual(TEXT("ghost die is ignored by score sum"), GhostResult.DiceSum, 3);
	TestTrue(TEXT("ghost die still participates in combinations when allowed"), GhostResult.MatchedValues == TArray<int32>({ 3, 6 }));

	const FGambitDiceCombinationResult CursedResult = Evaluator->EvaluateDice(TArray<FGambitDieRuntimeState>({
		MakeTestDie(-3),
		MakeTestDie(0),
		MakeTestDie(9),
		MakeTestDie(12)
	}));
	TestEqual(TEXT("combination sum keeps negative zero and high values"), CursedResult.DiceSum, 18);
	TestTrue(TEXT("combination matched values keep authored values sorted"), CursedResult.MatchedValues == TArray<int32>({ -3, 0, 9, 12 }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitScoringDefaultCombinationScoresTest,
	"GrandpaGambit.Scoring.DefaultCombinationScores",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitScoringDefaultCombinationScoresTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const UGambitGameBalanceSettings* Settings = UGambitGameBalanceSettings::Get();
	TestEqual(TEXT("default high dice base score"), Settings->GetBaseScoreForCombination(EGambitCombinationType::HighDice), 5);
	TestEqual(TEXT("default pair base score"), Settings->GetBaseScoreForCombination(EGambitCombinationType::Pair), 12);
	TestEqual(TEXT("default two pair base score"), Settings->GetBaseScoreForCombination(EGambitCombinationType::TwoPair), 18);
	TestEqual(TEXT("default three of a kind base score"), Settings->GetBaseScoreForCombination(EGambitCombinationType::ThreeOfAKind), 25);
	TestEqual(TEXT("default small straight base score"), Settings->GetBaseScoreForCombination(EGambitCombinationType::StraightSmall), 30);
	TestEqual(TEXT("default full house base score"), Settings->GetBaseScoreForCombination(EGambitCombinationType::FullHouse), 40);
	TestEqual(TEXT("default four of a kind base score"), Settings->GetBaseScoreForCombination(EGambitCombinationType::FourOfAKind), 45);
	TestEqual(TEXT("default large straight base score"), Settings->GetBaseScoreForCombination(EGambitCombinationType::StraightLarge), 55);
	TestEqual(TEXT("default five of a kind base score"), Settings->GetBaseScoreForCombination(EGambitCombinationType::FiveOfAKind), 70);
	TestEqual(TEXT("none combination stays scoreless"), Settings->GetBaseScoreForCombination(EGambitCombinationType::None), 0);

	UGambitDiceCombinationEvaluator* Evaluator = NewObject<UGambitDiceCombinationEvaluator>();
	auto TestEvaluatedBaseScore = [this, Evaluator](
		const TCHAR* Label,
		const TArray<FGambitDieRuntimeState>& Dice,
		const int32 ExpectedBaseScore)
	{
		const FGambitDiceCombinationResult Result = Evaluator->EvaluateDice(Dice);
		TestEqual(Label, Result.BaseCombinationScore, ExpectedBaseScore);
	};

	TestEvaluatedBaseScore(TEXT("evaluator keeps high dice default base score"), {
		MakeTestDie(1, 1, true, true, 0),
		MakeTestDie(3, 1, true, true, 1),
		MakeTestDie(5, 1, true, true, 2),
		MakeTestDie(6, 1, true, true, 3)
	}, 5);
	TestEvaluatedBaseScore(TEXT("evaluator keeps pair default base score"), {
		MakeTestDie(2, 1, true, true, 0),
		MakeTestDie(2, 1, true, true, 1),
		MakeTestDie(4, 1, true, true, 2),
		MakeTestDie(6, 1, true, true, 3)
	}, 12);
	TestEvaluatedBaseScore(TEXT("evaluator keeps two pair default base score"), {
		MakeTestDie(2, 1, true, true, 0),
		MakeTestDie(2, 1, true, true, 1),
		MakeTestDie(5, 1, true, true, 2),
		MakeTestDie(5, 1, true, true, 3)
	}, 18);
	TestEvaluatedBaseScore(TEXT("evaluator keeps three of a kind default base score"), {
		MakeTestDie(4, 1, true, true, 0),
		MakeTestDie(4, 1, true, true, 1),
		MakeTestDie(4, 1, true, true, 2),
		MakeTestDie(2, 1, true, true, 3),
		MakeTestDie(5, 1, true, true, 4)
	}, 25);
	TestEvaluatedBaseScore(TEXT("evaluator keeps small straight default base score"), {
		MakeTestDie(1, 1, true, true, 0),
		MakeTestDie(2, 1, true, true, 1),
		MakeTestDie(3, 1, true, true, 2),
		MakeTestDie(4, 1, true, true, 3),
		MakeTestDie(6, 1, true, true, 4)
	}, 30);
	TestEvaluatedBaseScore(TEXT("evaluator keeps full house default base score"), {
		MakeTestDie(3, 1, true, true, 0),
		MakeTestDie(3, 1, true, true, 1),
		MakeTestDie(3, 1, true, true, 2),
		MakeTestDie(5, 1, true, true, 3),
		MakeTestDie(5, 1, true, true, 4)
	}, 40);
	TestEvaluatedBaseScore(TEXT("evaluator keeps four of a kind default base score"), {
		MakeTestDie(6, 1, true, true, 0),
		MakeTestDie(6, 1, true, true, 1),
		MakeTestDie(6, 1, true, true, 2),
		MakeTestDie(6, 1, true, true, 3),
		MakeTestDie(2, 1, true, true, 4)
	}, 45);
	TestEvaluatedBaseScore(TEXT("evaluator keeps large straight default base score"), {
		MakeTestDie(1, 1, true, true, 0),
		MakeTestDie(2, 1, true, true, 1),
		MakeTestDie(3, 1, true, true, 2),
		MakeTestDie(4, 1, true, true, 3)
	}, 55);
	TestEvaluatedBaseScore(TEXT("evaluator keeps five of a kind default base score"), {
		MakeTestDie(6, 1, true, true, 0),
		MakeTestDie(6, 1, true, true, 1),
		MakeTestDie(6, 1, true, true, 2),
		MakeTestDie(6, 1, true, true, 3),
		MakeTestDie(6, 1, true, true, 4)
	}, 70);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitScoringCombinationScoreFallbackTest,
	"GrandpaGambit.Scoring.CombinationScoreFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitScoringCombinationScoreFallbackTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitGameBalanceSettings* Settings = NewObject<UGambitGameBalanceSettings>();
	Settings->CombinationBaseScores.Reset();
	TestEqual(TEXT("empty combination score table falls back to default pair score"), Settings->GetBaseScoreForCombination(EGambitCombinationType::Pair), 12);

	Settings->CombinationBaseScores.Add({ EGambitCombinationType::Pair, 99 });
	TestEqual(TEXT("configured combination score overrides default"), Settings->GetBaseScoreForCombination(EGambitCombinationType::Pair), 99);
	TestEqual(TEXT("partial combination score table falls back for missing full house"), Settings->GetBaseScoreForCombination(EGambitCombinationType::FullHouse), 40);

	Settings->CombinationBaseScores.Add({ EGambitCombinationType::TwoPair, -10 });
	TestEqual(TEXT("negative configured combination score falls back to default"), Settings->GetBaseScoreForCombination(EGambitCombinationType::TwoPair), 18);

	TArray<FString> Warnings;
	Settings->ValidateCombinationBaseScores(Warnings);
	TestTrue(TEXT("partial score table reports warnings"), Warnings.Num() > 0);
	TestTrue(TEXT("negative score entry is reported"), Warnings.ContainsByPredicate([](const FString& Warning)
	{
		return Warning.Contains(TEXT("negative score"));
	}));
	TestTrue(TEXT("missing score entry is reported"), Warnings.ContainsByPredicate([](const FString& Warning)
	{
		return Warning.Contains(TEXT("missing"));
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitScoringScoreModifierMathTest,
	"GrandpaGambit.Scoring.ScoreModifierMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitScoringScoreModifierMathTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FGambitScoreModifierContext A;
	A.AdditiveBonus = 10.0f;
	A.DiceContributionMultiplierBonus = 2.0f;
	A.Multiplier = 1.5f;
	A.ScoreCap = 100.0f;
	A.DiminishingThreshold = 200.0f;
	A.DiminishingFactor = 0.8f;

	FGambitScoreModifierContext B;
	B.AdditiveBonus = -3.0f;
	B.DiceContributionMultiplierBonus = 5.0f;
	B.Multiplier = 2.0f;
	B.ScoreCap = 80.0f;
	B.DiminishingThreshold = 150.0f;
	B.DiminishingFactor = 0.6f;

	const FGambitScoreModifierContext Merged = FGambitScoreModifierMath::Merge(A, B);
	TestEqual(TEXT("score modifier additive bonuses add"), Merged.AdditiveBonus, 7.0f);
	TestEqual(TEXT("score modifier dice contribution bonuses add"), Merged.DiceContributionMultiplierBonus, 7.0f);
	TestEqual(TEXT("score modifier multipliers multiply"), Merged.Multiplier, 3.0f);
	TestEqual(TEXT("score modifier uses lower positive cap"), Merged.ScoreCap, 80.0f);
	TestEqual(TEXT("score modifier uses lower positive diminishing threshold"), Merged.DiminishingThreshold, 150.0f);
	TestEqual(TEXT("score modifier uses lower positive diminishing factor"), Merged.DiminishingFactor, 0.6f);

	FGambitScoreModifierContext InvalidMultiplier;
	InvalidMultiplier.Multiplier = -2.0f;
	InvalidMultiplier.DiminishingFactor = -1.0f;
	const FGambitScoreModifierContext NormalizedMerge = FGambitScoreModifierMath::Merge(InvalidMultiplier, FGambitScoreModifierMath::MakeNeutral());
	TestEqual(TEXT("non-positive merged multiplier normalizes to neutral"), NormalizedMerge.Multiplier, 1.0f);
	TestEqual(TEXT("non-positive merged diminishing factor normalizes to neutral"), NormalizedMerge.DiminishingFactor, 1.0f);

	UGambitScoreCalculator* ScoreCalculator = NewObject<UGambitScoreCalculator>();
	const FGambitScoreModifierContext CalculatorMerged = ScoreCalculator->MergeModifiers(A, B);
	const FGambitScoreModifierContext ResolverMerged = UGambitEffectResolver::MergeScoreModifiers(A, B);
	TestEqual(TEXT("score calculator merge delegates to score modifier math"), CalculatorMerged.ScoreCap, Merged.ScoreCap);
	TestEqual(TEXT("effect resolver merge wrapper delegates to score modifier math"), ResolverMerged.Multiplier, Merged.Multiplier);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitGhostDieTriggersValueEffectsTest,
	"GrandpaGambit.Dice.GhostDieTriggersValueEffects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitGhostDieTriggersValueEffectsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitDiceDefinition* GhostDie = MakeDiceDefinition(GetTransientPackage(), TEXT("dice.ghost"), TArray<int32>({ 6 }), 1, false, true);
	UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(GhostDie, EGambitEffectHook::ScoreModifier, EGambitItemEffectType::AddScoreFlat);
	EffectDefinition->Amount = 5.0f;

	FGambitEffectConditionDefinition ValueCondition;
	ValueCondition.ConditionType = EGambitEffectConditionType::DieValueEquals;
	ValueCondition.Value = 6;
	EffectDefinition->Conditions.Add(ValueCondition);
	GhostDie->EffectDefinitions.Add(EffectDefinition);

	FGambitEffectExecutionContext Context;
	Context.Hook = EGambitEffectHook::ScoreModifier;
	Context.SourceDiceDefinition = GhostDie;
	Context.SourceDice.Add(MakeTestDie(6, 1, false, true));

	Resolver->ExecuteDiceEffects(GhostDie, Context);

	TestEqual(TEXT("ghost die value condition still triggers dice effect"), Context.ScoreModifierDelta.AdditiveBonus, 5.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitAmountPerMatchingOneTest,
	"GrandpaGambit.Effects.AmountPerMatchingOne",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitAmountPerMatchingOneTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitModuleDefinition* ModuleDefinition = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(ModuleDefinition, EGambitEffectHook::ScoreModifier, EGambitItemEffectType::AddScoreFlat);
	EffectDefinition->ScalarParameters.Add(TEXT("AmountPerMatchingDie"), 15.0f);

	FGambitEffectConditionDefinition Condition;
	Condition.ConditionType = EGambitEffectConditionType::DieValueEquals;
	Condition.Value = 1;
	EffectDefinition->Conditions.Add(Condition);
	ModuleDefinition->EffectDefinitions.Add(EffectDefinition);

	FGambitEffectExecutionContext Context;
	Context.Hook = EGambitEffectHook::ScoreModifier;
	Context.SourceDice = {
		MakeTestDie(1, 1, true, true, 0),
		MakeTestDie(1, 1, true, true, 1),
		MakeTestDie(2, 1, true, true, 2)
	};

	Resolver->ExecuteItemEffects(ModuleDefinition, Context);

	TestEqual(TEXT("+15 is applied for each die showing 1"), Context.ScoreModifierDelta.AdditiveBonus, 30.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitAmountPerEvenDieTest,
	"GrandpaGambit.Effects.AmountPerEvenDie",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitAmountPerEvenDieTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitModuleDefinition* ModuleDefinition = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(ModuleDefinition, EGambitEffectHook::ScoreModifier, EGambitItemEffectType::AddScoreFlat);
	EffectDefinition->ScalarParameters.Add(TEXT("AmountPerMatchingDie"), 8.0f);

	FGambitEffectConditionDefinition Condition;
	Condition.ConditionType = EGambitEffectConditionType::EvenDiceCount;
	Condition.Comparison = EGambitEffectComparison::GreaterOrEqual;
	Condition.Count = 1;
	EffectDefinition->Conditions.Add(Condition);
	ModuleDefinition->EffectDefinitions.Add(EffectDefinition);

	FGambitEffectExecutionContext Context;
	Context.Hook = EGambitEffectHook::ScoreModifier;
	Context.SourceDice = {
		MakeTestDie(2, 1, true, true, 0),
		MakeTestDie(3, 1, true, true, 1),
		MakeTestDie(4, 1, true, true, 2),
		MakeTestDie(6, 1, true, true, 3)
	};

	Resolver->ExecuteItemEffects(ModuleDefinition, Context);

	TestEqual(TEXT("+8 is applied for each even die"), Context.ScoreModifierDelta.AdditiveBonus, 24.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitAllEvenMultiplierTest,
	"GrandpaGambit.Effects.AllEvenMultiplier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitAllEvenMultiplierTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitModuleDefinition* ModuleDefinition = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(ModuleDefinition, EGambitEffectHook::ScoreModifier, EGambitItemEffectType::MultiplyScore);
	EffectDefinition->Multiplier = 4.0f;

	FGambitEffectConditionDefinition Condition;
	Condition.ConditionType = EGambitEffectConditionType::AllDiceEven;
	EffectDefinition->Conditions.Add(Condition);
	ModuleDefinition->EffectDefinitions.Add(EffectDefinition);

	FGambitEffectExecutionContext Context;
	Context.Hook = EGambitEffectHook::ScoreModifier;
	Context.SourceDice = {
		MakeTestDie(2, 1, true, true, 0),
		MakeTestDie(4, 1, true, true, 1),
		MakeTestDie(6, 1, true, true, 2),
		MakeTestDie(8, 1, true, true, 3)
	};

	Resolver->ExecuteItemEffects(ModuleDefinition, Context);

	TestEqual(TEXT("all-even hand applies x4"), Context.ScoreModifierDelta.Multiplier, 4.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitAllDiceBetweenFlatBonusTest,
	"GrandpaGambit.Effects.AllDiceBetweenFlatBonus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitAllDiceBetweenFlatBonusTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitModuleDefinition* ModuleDefinition = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(ModuleDefinition, EGambitEffectHook::ScoreModifier, EGambitItemEffectType::AddScoreFlat);
	EffectDefinition->Amount = 50.0f;

	FGambitEffectConditionDefinition Condition;
	Condition.ConditionType = EGambitEffectConditionType::AllDiceValueBetween;
	Condition.Value = 2;
	Condition.Count = 5;
	EffectDefinition->Conditions.Add(Condition);
	ModuleDefinition->EffectDefinitions.Add(EffectDefinition);

	FGambitEffectExecutionContext Context;
	Context.Hook = EGambitEffectHook::ScoreModifier;
	Context.SourceDice = {
		MakeTestDie(2, 1, true, true, 0),
		MakeTestDie(3, 1, true, true, 1),
		MakeTestDie(5, 1, true, true, 2)
	};

	Resolver->ExecuteItemEffects(ModuleDefinition, Context);

	TestEqual(TEXT("+50 is applied when all dice are between 2 and 5"), Context.ScoreModifierDelta.AdditiveBonus, 50.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitRawScoreMultiplierTest,
	"GrandpaGambit.Effects.RawScoreMultiplier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitRawScoreMultiplierTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitModuleDefinition* ModuleDefinition = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(ModuleDefinition, EGambitEffectHook::ScoreModifier, EGambitItemEffectType::MultiplyScore);
	EffectDefinition->Multiplier = 1.5f;

	FGambitEffectConditionDefinition Condition;
	Condition.ConditionType = EGambitEffectConditionType::RawScore;
	Condition.Comparison = EGambitEffectComparison::Greater;
	Condition.Value = 30;
	EffectDefinition->Conditions.Add(Condition);
	ModuleDefinition->EffectDefinitions.Add(EffectDefinition);

	FGambitEffectExecutionContext Context;
	Context.Hook = EGambitEffectHook::ScoreModifier;
	Context.CurrentCombinationResult.RawScore = 31;

	Resolver->ExecuteItemEffects(ModuleDefinition, Context);

	TestEqual(TEXT("raw score above 30 applies x1.5"), Context.ScoreModifierDelta.Multiplier, 1.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitNoComboFlatBonusTest,
	"GrandpaGambit.Effects.NoComboFlatBonus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitNoComboFlatBonusTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitModuleDefinition* ModuleDefinition = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(ModuleDefinition, EGambitEffectHook::ScoreModifier, EGambitItemEffectType::AddScoreFlat);
	EffectDefinition->Amount = 40.0f;

	FGambitEffectConditionDefinition Condition;
	Condition.ConditionType = EGambitEffectConditionType::NoComboOrHighDice;
	EffectDefinition->Conditions.Add(Condition);
	ModuleDefinition->EffectDefinitions.Add(EffectDefinition);

	FGambitEffectExecutionContext Context;
	Context.Hook = EGambitEffectHook::ScoreModifier;
	Context.CurrentCombinationResult.Combination = EGambitCombinationType::HighDice;

	Resolver->ExecuteItemEffects(ModuleDefinition, Context);

	TestEqual(TEXT("+40 is applied when no real combo is present"), Context.ScoreModifierDelta.AdditiveBonus, 40.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitRerollsRemainingZeroTest,
	"GrandpaGambit.Effects.RerollsRemainingZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitRerollsRemainingZeroTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitModuleDefinition* ModuleDefinition = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(ModuleDefinition, EGambitEffectHook::ScoreModifier, EGambitItemEffectType::AddScoreFlat);
	EffectDefinition->Amount = 11.0f;

	FGambitEffectConditionDefinition Condition;
	Condition.ConditionType = EGambitEffectConditionType::RerollsRemaining;
	Condition.Comparison = EGambitEffectComparison::Equal;
	Condition.Value = 0;
	EffectDefinition->Conditions.Add(Condition);
	ModuleDefinition->EffectDefinitions.Add(EffectDefinition);

	FGambitEffectExecutionContext Context;
	Context.Hook = EGambitEffectHook::ScoreModifier;
	Context.RerollsUsed = 2;
	Context.RerollLimit = 2;

	Resolver->ExecuteItemEffects(ModuleDefinition, Context);

	TestEqual(TEXT("rerolls remaining == 0 condition applies the effect"), Context.ScoreModifierDelta.AdditiveBonus, 11.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitPerDieEvenContributionMultiplierTest,
	"GrandpaGambit.Effects.PerDieEvenContributionMultiplier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitPerDieEvenContributionMultiplierTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitScoreCalculator* ScoreCalculator = NewObject<UGambitScoreCalculator>();
	UGambitModuleDefinition* ModuleDefinition = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(ModuleDefinition, EGambitEffectHook::ScoreModifier, EGambitItemEffectType::MultiplyDiceContribution);
	EffectDefinition->Multiplier = 1.5f;

	FGambitEffectConditionDefinition Condition;
	Condition.ConditionType = EGambitEffectConditionType::EvenDiceCount;
	Condition.Comparison = EGambitEffectComparison::GreaterOrEqual;
	Condition.Count = 1;
	EffectDefinition->Conditions.Add(Condition);
	ModuleDefinition->EffectDefinitions.Add(EffectDefinition);

	FGambitEffectExecutionContext Context;
	Context.Hook = EGambitEffectHook::ScoreModifier;
	Context.SourceDice = {
		MakeTestDie(2, 1, true, true, 0),
		MakeTestDie(3, 1, true, true, 1),
		MakeTestDie(4, 1, true, true, 2)
	};

	Resolver->ExecuteItemEffects(ModuleDefinition, Context);

	FGambitDiceCombinationResult CombinationResult;
	CombinationResult.DiceSum = 9;
	CombinationResult.RawScore = 9;
	const FGambitScoreBreakdown Breakdown = ScoreCalculator->CalculateScore(CombinationResult, Context.ScoreModifierDelta);

	TestEqual(TEXT("x1.5 applies only to even die contributions 2 and 4"), Context.ScoreModifierDelta.DiceContributionMultiplierBonus, 3.0f);
	TestEqual(TEXT("adjusted dice sum includes the per-die contribution bonus"), Breakdown.AdjustedDiceSum, 12.0f);
	TestEqual(TEXT("final score is not globally multiplied"), Breakdown.FinalScore, 12);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitMultiplierDeltaPerSixTest,
	"GrandpaGambit.Effects.MultiplierDeltaPerSix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitMultiplierDeltaPerSixTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitModuleDefinition* ModuleDefinition = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(ModuleDefinition, EGambitEffectHook::ScoreModifier, EGambitItemEffectType::MultiplyScore);
	EffectDefinition->Multiplier = 1.0f;
	EffectDefinition->ScalarParameters.Add(TEXT("MultiplierDeltaPerMatchingDie"), 0.5f);

	FGambitEffectConditionDefinition Condition;
	Condition.ConditionType = EGambitEffectConditionType::DieValueEquals;
	Condition.Value = 6;
	EffectDefinition->Conditions.Add(Condition);
	ModuleDefinition->EffectDefinitions.Add(EffectDefinition);

	FGambitEffectExecutionContext Context;
	Context.Hook = EGambitEffectHook::ScoreModifier;
	Context.SourceDice = {
		MakeTestDie(6, 1, true, true, 0),
		MakeTestDie(6, 1, true, true, 1),
		MakeTestDie(5, 1, true, true, 2)
	};

	Resolver->ExecuteItemEffects(ModuleDefinition, Context);

	TestEqual(TEXT("two sixes make final multiplier 1 + 0.5 + 0.5"), Context.ScoreModifierDelta.Multiplier, 2.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitAllDiceUsedByCombinationConditionTest,
	"GrandpaGambit.Effects.AllDiceUsedByCombination",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitAllDiceUsedByCombinationConditionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitDiceCombinationEvaluator* Evaluator = NewObject<UGambitDiceCombinationEvaluator>();
	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitModuleDefinition* ModuleDefinition = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(ModuleDefinition, EGambitEffectHook::ScoreModifier, EGambitItemEffectType::MultiplyScore);
	EffectDefinition->Multiplier = 2.0f;

	FGambitEffectConditionDefinition Condition;
	Condition.ConditionType = EGambitEffectConditionType::AllDiceUsedBySelectedCombination;
	EffectDefinition->Conditions.Add(Condition);
	ModuleDefinition->EffectDefinitions.Add(EffectDefinition);

	FGambitEffectExecutionContext FullTableContext;
	FullTableContext.Hook = EGambitEffectHook::ScoreModifier;
	FullTableContext.SourceDice = {
		MakeTestDie(2, 1, true, true, 0),
		MakeTestDie(2, 1, true, true, 1),
		MakeTestDie(3, 1, true, true, 2),
		MakeTestDie(3, 1, true, true, 3)
	};
	FullTableContext.CurrentCombinationResult = Evaluator->EvaluateDice(FullTableContext.SourceDice);

	FGambitEffectExecutionContext PartialContext;
	PartialContext.Hook = EGambitEffectHook::ScoreModifier;
	PartialContext.SourceDice = {
		MakeTestDie(2, 1, true, true, 0),
		MakeTestDie(2, 1, true, true, 1),
		MakeTestDie(3, 1, true, true, 2),
		MakeTestDie(4, 1, true, true, 3)
	};
	PartialContext.CurrentCombinationResult = Evaluator->EvaluateDice(PartialContext.SourceDice);

	Resolver->ExecuteItemEffects(ModuleDefinition, FullTableContext);
	Resolver->ExecuteItemEffects(ModuleDefinition, PartialContext);

	TestTrue(TEXT("two-pair hand uses every die in selected combo"), FullTableContext.CurrentCombinationResult.bAllDiceUsedBySelectedCombo);
	TestEqual(TEXT("full table applies x2"), FullTableContext.ScoreModifierDelta.Multiplier, 2.0f);
	TestFalse(TEXT("single-pair hand leaves unused dice"), PartialContext.CurrentCombinationResult.bAllDiceUsedBySelectedCombo);
	TestEqual(TEXT("partial combo does not apply multiplier"), PartialContext.ScoreModifierDelta.Multiplier, 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitDiceValuesPalindromeConditionTest,
	"GrandpaGambit.Effects.DiceValuesPalindrome",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitDiceValuesPalindromeConditionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitModuleDefinition* ModuleDefinition = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(ModuleDefinition, EGambitEffectHook::ScoreModifier, EGambitItemEffectType::MultiplyScore);
	EffectDefinition->Multiplier = 3.0f;

	FGambitEffectConditionDefinition Condition;
	Condition.ConditionType = EGambitEffectConditionType::DiceValuesPalindrome;
	EffectDefinition->Conditions.Add(Condition);
	ModuleDefinition->EffectDefinitions.Add(EffectDefinition);

	FGambitEffectExecutionContext Context;
	Context.Hook = EGambitEffectHook::ScoreModifier;
	Context.SourceDice = {
		MakeTestDie(2, 1, true, true, 0),
		MakeTestDie(5, 1, true, true, 1),
		MakeTestDie(5, 1, true, true, 2),
		MakeTestDie(2, 1, true, true, 3)
	};

	Resolver->ExecuteItemEffects(ModuleDefinition, Context);

	TestEqual(TEXT("palindrome hand applies x3"), Context.ScoreModifierDelta.Multiplier, 3.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitInventoryCountScalarsTest,
	"GrandpaGambit.Effects.InventoryCountScalars",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitInventoryCountScalarsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UObject* TestOuter = GetTransientPackage();
	UGambitDiceDefinition* CommonA = MakeDiceDefinition(TestOuter, TEXT("dice.common_a"), TArray<int32>({ 1, 2, 3 }));
	UGambitDiceDefinition* CommonB = MakeDiceDefinition(TestOuter, TEXT("dice.common_b"), TArray<int32>({ 4, 5, 6 }));
	UGambitDiceDefinition* RareDie = MakeDiceDefinition(TestOuter, TEXT("dice.rare"), TArray<int32>({ 6, 7, 8 }));
	RareDie->Rarity = EGambitItemRarity::Rare;

	UGambitInventoryComponent* InventoryComponent = NewObject<UGambitInventoryComponent>();
	InventoryComponent->AddOwnedDie(CommonA);
	InventoryComponent->AddOwnedDie(CommonA);
	InventoryComponent->AddOwnedDie(CommonB);
	InventoryComponent->AddOwnedDie(RareDie);

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitModuleDefinition* ModuleDefinition = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(ModuleDefinition, EGambitEffectHook::ScoreModifier, EGambitItemEffectType::AddScoreFlat);
	EffectDefinition->ScalarParameters.Add(TEXT("AmountPerDistinctOwnedDiceType"), 10.0f);
	EffectDefinition->ScalarParameters.Add(TEXT("AmountPerMostRepeatedOwnedDiceType"), 15.0f);
	EffectDefinition->ScalarParameters.Add(TEXT("AmountPerOwnedDiceRarityCount"), 20.0f);

	FGambitEffectConditionDefinition RareCountCondition;
	RareCountCondition.ConditionType = EGambitEffectConditionType::OwnedDiceRarityCount;
	RareCountCondition.Rarity = EGambitItemRarity::Rare;
	RareCountCondition.Comparison = EGambitEffectComparison::GreaterOrEqual;
	RareCountCondition.Count = 1;
	EffectDefinition->Conditions.Add(RareCountCondition);
	ModuleDefinition->EffectDefinitions.Add(EffectDefinition);

	FGambitEffectExecutionContext Context;
	Context.Hook = EGambitEffectHook::ScoreModifier;
	Context.SourceInventoryComponent = InventoryComponent;

	Resolver->ExecuteItemEffects(ModuleDefinition, Context);

	TestEqual(TEXT("inventory scalars count distinct dice, repeated dice and rare dice"), Context.ScoreModifierDelta.AdditiveBonus, 80.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitB2SmokeScoringRepresentativeHandsTest,
	"GrandpaGambit.B2.SmokeScoringRepresentativeHands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitB2SmokeScoringRepresentativeHandsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitDiceCombinationEvaluator* Evaluator = NewObject<UGambitDiceCombinationEvaluator>();
	UGambitScoreCalculator* ScoreCalculator = NewObject<UGambitScoreCalculator>();

	auto MakeModuleEffect = [](const EGambitItemEffectType EffectType, const float Multiplier = 1.0f)
	{
		UGambitModuleDefinition* ModuleDefinition = NewObject<UGambitModuleDefinition>();
		UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(ModuleDefinition, EGambitEffectHook::ScoreModifier, EffectType);
		EffectDefinition->Multiplier = Multiplier;
		ModuleDefinition->EffectDefinitions.Add(EffectDefinition);
		return TPair<UGambitModuleDefinition*, UGambitItemEffectDefinition*>(ModuleDefinition, EffectDefinition);
	};

	{
		TPair<UGambitModuleDefinition*, UGambitItemEffectDefinition*> EvenKing = MakeModuleEffect(EGambitItemEffectType::MultiplyDiceContribution, 1.5f);
		FGambitEffectConditionDefinition Condition;
		Condition.ConditionType = EGambitEffectConditionType::EvenDiceCount;
		Condition.Count = 1;
		EvenKing.Value->Conditions.Add(Condition);

		FGambitEffectExecutionContext Context;
		Context.Hook = EGambitEffectHook::ScoreModifier;
		Context.SourceDice = {
			MakeTestDie(2, 1, true, true, 0),
			MakeTestDie(4, 1, true, true, 1),
			MakeTestDie(5, 1, true, true, 2),
			MakeTestDie(6, 1, true, true, 3)
		};
		Resolver->ExecuteItemEffects(EvenKing.Key, Context);

		FGambitDiceCombinationResult CombinationResult;
		CombinationResult.DiceSum = 17;
		CombinationResult.RawScore = 17;
		const FGambitScoreBreakdown Breakdown = ScoreCalculator->CalculateScore(CombinationResult, Context.ScoreModifierDelta);

		TestEqual(TEXT("B2 smoke: several even dice add only per-die contribution bonus"), Context.ScoreModifierDelta.DiceContributionMultiplierBonus, 6.0f);
		TestEqual(TEXT("B2 smoke: even contribution score remains additive to dice sum"), Breakdown.FinalScore, 23);
	}

	{
		TPair<UGambitModuleDefinition*, UGambitItemEffectDefinition*> SacredFive = MakeModuleEffect(EGambitItemEffectType::MultiplyDiceContribution, 2.0f);
		FGambitEffectConditionDefinition Condition;
		Condition.ConditionType = EGambitEffectConditionType::DieValueEquals;
		Condition.Value = 5;
		SacredFive.Value->Conditions.Add(Condition);

		FGambitEffectExecutionContext Context;
		Context.Hook = EGambitEffectHook::ScoreModifier;
		Context.SourceDice = {
			MakeTestDie(5, 1, true, true, 0),
			MakeTestDie(5, 1, true, true, 1),
			MakeTestDie(2, 1, true, true, 2),
			MakeTestDie(3, 1, true, true, 3)
		};
		Resolver->ExecuteItemEffects(SacredFive.Key, Context);

		TestEqual(TEXT("B2 smoke: several 5s receive per-die x2 contribution"), Context.ScoreModifierDelta.DiceContributionMultiplierBonus, 10.0f);
	}

	{
		UObject* TestOuter = GetTransientPackage();
		UGambitDiceDefinition* CommonDie = MakeDiceDefinition(TestOuter, TEXT("dice.smoke.common"), TArray<int32>({ 1, 2, 3 }));
		CommonDie->Rarity = EGambitItemRarity::Common;
		UGambitDiceDefinition* UncommonDie = MakeDiceDefinition(TestOuter, TEXT("dice.smoke.uncommon"), TArray<int32>({ 2, 3, 4 }));
		UncommonDie->Rarity = EGambitItemRarity::Uncommon;
		UGambitDiceDefinition* RareDie = MakeDiceDefinition(TestOuter, TEXT("dice.smoke.rare"), TArray<int32>({ 4, 5, 6 }));
		RareDie->Rarity = EGambitItemRarity::Rare;
		UGambitDiceDefinition* EpicDie = MakeDiceDefinition(TestOuter, TEXT("dice.smoke.epic"), TArray<int32>({ 6, 7, 8 }));
		EpicDie->Rarity = EGambitItemRarity::Epic;

		UGambitInventoryComponent* InventoryComponent = NewObject<UGambitInventoryComponent>();
		InventoryComponent->AddOwnedDie(CommonDie);
		InventoryComponent->AddOwnedDie(UncommonDie);
		InventoryComponent->AddOwnedDie(RareDie);
		InventoryComponent->AddOwnedDie(EpicDie);

		TPair<UGambitModuleDefinition*, UGambitItemEffectDefinition*> PerfectBalance = MakeModuleEffect(EGambitItemEffectType::MultiplyScore, 3.0f);
		for (const EGambitItemRarity Rarity : { EGambitItemRarity::Common, EGambitItemRarity::Uncommon, EGambitItemRarity::Rare, EGambitItemRarity::Epic })
		{
			FGambitEffectConditionDefinition Condition;
			Condition.ConditionType = EGambitEffectConditionType::OwnedDiceRarityCount;
			Condition.Rarity = Rarity;
			Condition.Count = 1;
			PerfectBalance.Value->Conditions.Add(Condition);
		}

		FGambitEffectExecutionContext Context;
		Context.Hook = EGambitEffectHook::ScoreModifier;
		Context.SourceInventoryComponent = InventoryComponent;
		Resolver->ExecuteItemEffects(PerfectBalance.Key, Context);

		TestEqual(TEXT("B2 smoke: different dice rarities satisfy the rarity set"), Context.ScoreModifierDelta.Multiplier, 3.0f);
		TestEqual(TEXT("B2 smoke: rare dice counter is exact"), InventoryComponent->CountOwnedDiceByRarity(EGambitItemRarity::Rare), 1);
	}

	{
		TPair<UGambitModuleDefinition*, UGambitItemEffectDefinition*> Symmetry = MakeModuleEffect(EGambitItemEffectType::MultiplyScore, 3.0f);
		FGambitEffectConditionDefinition Condition;
		Condition.ConditionType = EGambitEffectConditionType::DiceValuesPalindrome;
		Symmetry.Value->Conditions.Add(Condition);

		FGambitEffectExecutionContext Context;
		Context.Hook = EGambitEffectHook::ScoreModifier;
		Context.SourceDice = {
			MakeTestDie(2, 1, true, true, 0),
			MakeTestDie(5, 1, true, true, 1),
			MakeTestDie(5, 1, true, true, 2),
			MakeTestDie(2, 1, true, true, 3)
		};
		Resolver->ExecuteItemEffects(Symmetry.Key, Context);

		TestEqual(TEXT("B2 smoke: symmetric hand applies x3"), Context.ScoreModifierDelta.Multiplier, 3.0f);
	}

	{
		TPair<UGambitModuleDefinition*, UGambitItemEffectDefinition*> FullTable = MakeModuleEffect(EGambitItemEffectType::MultiplyScore, 2.0f);
		FGambitEffectConditionDefinition Condition;
		Condition.ConditionType = EGambitEffectConditionType::AllDiceUsedBySelectedCombination;
		FullTable.Value->Conditions.Add(Condition);

		FGambitEffectExecutionContext Context;
		Context.Hook = EGambitEffectHook::ScoreModifier;
		Context.SourceDice = {
			MakeTestDie(2, 1, true, true, 0),
			MakeTestDie(2, 1, true, true, 1),
			MakeTestDie(3, 1, true, true, 2),
			MakeTestDie(3, 1, true, true, 3)
		};
		Context.CurrentCombinationResult = Evaluator->EvaluateDice(Context.SourceDice);
		Resolver->ExecuteItemEffects(FullTable.Key, Context);

		TestTrue(TEXT("B2 smoke: two pair uses all dice"), Context.CurrentCombinationResult.bAllDiceUsedBySelectedCombo);
		TestEqual(TEXT("B2 smoke: full table applies x2"), Context.ScoreModifierDelta.Multiplier, 2.0f);
	}

	{
		TPair<UGambitModuleDefinition*, UGambitItemEffectDefinition*> LowBuild = MakeModuleEffect(EGambitItemEffectType::MultiplyDiceContribution, 2.0f);
		FGambitEffectConditionDefinition MinCondition;
		MinCondition.ConditionType = EGambitEffectConditionType::DieValueGreater;
		MinCondition.Value = 0;
		LowBuild.Value->Conditions.Add(MinCondition);

		FGambitEffectConditionDefinition MaxCondition;
		MaxCondition.ConditionType = EGambitEffectConditionType::DieValueLower;
		MaxCondition.Value = 4;
		LowBuild.Value->Conditions.Add(MaxCondition);

		FGambitEffectExecutionContext Context;
		Context.Hook = EGambitEffectHook::ScoreModifier;
		Context.SourceDice = {
			MakeTestDie(1, 1, true, true, 0),
			MakeTestDie(2, 1, true, true, 1),
			MakeTestDie(3, 1, true, true, 2),
			MakeTestDie(5, 1, true, true, 3)
		};
		Resolver->ExecuteItemEffects(LowBuild.Key, Context);

		TestEqual(TEXT("B2 smoke: build-low 1-3 dice get per-die x2 contribution"), Context.ScoreModifierDelta.DiceContributionMultiplierBonus, 6.0f);
	}

	{
		TPair<UGambitModuleDefinition*, UGambitItemEffectDefinition*> BigDiceKing = MakeModuleEffect(EGambitItemEffectType::MultiplyDiceContribution, 3.0f);
		FGambitEffectConditionDefinition Condition;
		Condition.ConditionType = EGambitEffectConditionType::DieValueGreater;
		Condition.Value = 6;
		BigDiceKing.Value->Conditions.Add(Condition);

		FGambitEffectExecutionContext Context;
		Context.Hook = EGambitEffectHook::ScoreModifier;
		Context.SourceDice = {
			MakeTestDie(7, 1, true, true, 0),
			MakeTestDie(8, 1, true, true, 1),
			MakeTestDie(2, 1, true, true, 2),
			MakeTestDie(4, 1, true, true, 3)
		};
		Resolver->ExecuteItemEffects(BigDiceKing.Key, Context);

		TestEqual(TEXT("B2 smoke: build-high >6 dice get per-die x3 contribution"), Context.ScoreModifierDelta.DiceContributionMultiplierBonus, 30.0f);
	}

	{
		TPair<UGambitModuleDefinition*, UGambitItemEffectDefinition*> SixDomination = MakeModuleEffect(EGambitItemEffectType::MultiplyScore, 1.0f);
		SixDomination.Value->ScalarParameters.Add(TEXT("MultiplierDeltaPerMatchingDie"), 0.5f);
		FGambitEffectConditionDefinition Condition;
		Condition.ConditionType = EGambitEffectConditionType::DieValueEquals;
		Condition.Value = 6;
		SixDomination.Value->Conditions.Add(Condition);

		FGambitEffectExecutionContext Context;
		Context.Hook = EGambitEffectHook::ScoreModifier;
		Context.SourceDice = {
			MakeTestDie(6, 1, true, true, 0),
			MakeTestDie(6, 1, true, true, 1),
			MakeTestDie(2, 1, true, true, 2),
			MakeTestDie(4, 1, true, true, 3)
		};
		Resolver->ExecuteItemEffects(SixDomination.Key, Context);

		TestEqual(TEXT("B2 smoke: six domination uses additive final multiplier delta"), Context.ScoreModifierDelta.Multiplier, 2.0f);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitB3RedDieSourceValueTest,
	"GrandpaGambit.B3.Dice.RedSourceValue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitB3RedDieSourceValueTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitDiceDefinition* RedDie = MakeDiceDefinition(GetTransientPackage(), TEXT("dice.red"), TArray<int32>({ 1, 2, 3, 4, 5, 6 }));
	UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(RedDie, EGambitEffectHook::ScoreModifier, EGambitItemEffectType::AddScoreFlat);
	EffectDefinition->Amount = 20.0f;

	FGambitEffectConditionDefinition Condition;
	Condition.ConditionType = EGambitEffectConditionType::SourceDieValue;
	Condition.Comparison = EGambitEffectComparison::GreaterOrEqual;
	Condition.Value = 5;
	EffectDefinition->Conditions.Add(Condition);
	RedDie->EffectDefinitions.Add(EffectDefinition);

	FGambitEffectExecutionContext LowContext;
	LowContext.Hook = EGambitEffectHook::ScoreModifier;
	LowContext.SourceDieHandIndex = 0;
	LowContext.SourceDice = { MakeTestDie(4, 1, true, true, 0), MakeTestDie(6, 1, true, true, 1) };
	Resolver->ExecuteDiceEffects(RedDie, LowContext);
	TestEqual(TEXT("red die ignores other dice >= 5"), LowContext.ScoreModifierDelta.AdditiveBonus, 0.0f);

	FGambitEffectExecutionContext HighContext;
	HighContext.Hook = EGambitEffectHook::ScoreModifier;
	HighContext.SourceDieHandIndex = 0;
	HighContext.SourceDice = { MakeTestDie(5, 1, true, true, 0), MakeTestDie(1, 1, true, true, 1) };
	Resolver->ExecuteDiceEffects(RedDie, HighContext);
	TestEqual(TEXT("red die adds +20 when source die is >= 5"), HighContext.ScoreModifierDelta.AdditiveBonus, 20.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitB3GoldenDieLockedRewardTest,
	"GrandpaGambit.B3.Dice.GoldenLockedReward",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitB3GoldenDieLockedRewardTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitDiceDefinition* GoldenDie = MakeDiceDefinition(GetTransientPackage(), TEXT("dice.golden"), TArray<int32>({ 1, 2, 3, 4, 5, 6 }));
	UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(GoldenDie, EGambitEffectHook::Reward, EGambitItemEffectType::AddGold);
	EffectDefinition->ScalarParameters.Add(TEXT("AmountFromSourceDieValue"), 1.0f);

	FGambitEffectConditionDefinition Condition;
	Condition.ConditionType = EGambitEffectConditionType::SourceDieLocked;
	EffectDefinition->Conditions.Add(Condition);
	GoldenDie->EffectDefinitions.Add(EffectDefinition);

	UGambitEconomyComponent* UnlockedEconomy = NewObject<UGambitEconomyComponent>();
	UnlockedEconomy->InitializeForMatch();

	FGambitEffectExecutionContext UnlockedContext;
	UnlockedContext.Hook = EGambitEffectHook::Reward;
	UnlockedContext.SourceDieHandIndex = 0;
	UnlockedContext.SourceDice = { MakeTestDie(4, 1, true, true, 0) };
	UnlockedContext.SourceEconomyComponent = UnlockedEconomy;
	Resolver->ExecuteDiceEffects(GoldenDie, UnlockedContext);
	TestEqual(TEXT("golden die gives no gold when source die is not locked"), UnlockedEconomy->GetCurrentGold(), 0);

	UGambitEconomyComponent* LockedEconomy = NewObject<UGambitEconomyComponent>();
	LockedEconomy->InitializeForMatch();
	FGambitDieRuntimeState LockedDie = MakeTestDie(4, 1, true, true, 0);
	LockedDie.bLocked = true;

	FGambitEffectExecutionContext LockedContext;
	LockedContext.Hook = EGambitEffectHook::Reward;
	LockedContext.SourceDieHandIndex = 0;
	LockedContext.SourceDice = { LockedDie };
	LockedContext.SourceEconomyComponent = LockedEconomy;
	Resolver->ExecuteDiceEffects(GoldenDie, LockedContext);
	TestEqual(TEXT("golden die gives gold equal to locked source die value"), LockedEconomy->GetCurrentGold(), 4);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitB3GreenDieRerollLimitTest,
	"GrandpaGambit.B3.Dice.GreenRerollLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitB3GreenDieRerollLimitTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitDiceDefinition* GreenDie = MakeDiceDefinition(GetTransientPackage(), TEXT("dice.green"), TArray<int32>({ 1, 2, 3, 4, 5, 6 }));
	UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(GreenDie, EGambitEffectHook::PostRoll, EGambitItemEffectType::AddReroll);
	EffectDefinition->Amount = 1.0f;

	FGambitEffectConditionDefinition Condition;
	Condition.ConditionType = EGambitEffectConditionType::SourceDieValue;
	Condition.Comparison = EGambitEffectComparison::LowerOrEqual;
	Condition.Value = 3;
	EffectDefinition->Conditions.Add(Condition);
	GreenDie->EffectDefinitions.Add(EffectDefinition);

	FGambitEffectExecutionContext Context;
	Context.Hook = EGambitEffectHook::PostRoll;
	Context.SourceDieHandIndex = 0;
	Context.SourceDice = { MakeTestDie(3, 1, true, true, 0) };
	Resolver->ExecuteDiceEffects(GreenDie, Context);

	TestEqual(TEXT("green die adds one reroll when source die <= 3"), Context.RerollLimitDelta, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitB3PolishTransformResetsTest,
	"GrandpaGambit.B3.Consumables.PolishTransformResets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitB3PolishTransformResetsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UObject* TestOuter = GetTransientPackage();
	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitDiceDefinition* StandardDie = MakeDiceDefinition(TestOuter, TEXT("dice.standard"), TArray<int32>({ 1, 2, 3, 4, 5, 6 }));
	UGambitDiceDefinition* SilverDie = MakeDiceDefinition(TestOuter, TEXT("dice.silver"), TArray<int32>({ 1, 2, 3, 4, 5, 6 }));
	UGambitDiceComponent* DiceComponent = NewObject<UGambitDiceComponent>();
	TArray<TObjectPtr<UGambitDiceDefinition>> OwnedDice;
	OwnedDice.Add(StandardDie);
	DiceComponent->InitializeDicePool(OwnedDice);
	DiceComponent->SetDieValue(0, 4);

	UGambitConsumableDefinition* Polish = NewObject<UGambitConsumableDefinition>();
	UGambitItemEffectDefinition* EffectDefinition = MakeEffectDefinition(Polish, EGambitEffectHook::ConsumableUse, EGambitItemEffectType::TransformDiceForRound);
	EffectDefinition->TargetRuleId = GambitEffectTargetRules::SelectedDie;
	EffectDefinition->TransformDiceDefinition = SilverDie;
	Polish->EffectDefinitions.Add(EffectDefinition);

	FGambitEffectExecutionContext Context;
	Context.Hook = EGambitEffectHook::ConsumableUse;
	Context.SourceDiceComponent = DiceComponent;
	Context.SourceDice = DiceComponent->GetDiceStates();
	Context.SourceDieHandIndex = 0;
	Resolver->ExecuteItemEffects(Polish, Context);

	TestEqual(TEXT("polish transforms selected die to silver"), DiceComponent->GetDiceStatesRef()[0].DiceDefinition.Get(), SilverDie);
	TestEqual(TEXT("polish preserves current value"), DiceComponent->GetDiceStatesRef()[0].EffectiveValue, 4);

	DiceComponent->InitializeDicePool(OwnedDice);
	TestEqual(TEXT("next round runtime reset restores owned standard die"), DiceComponent->GetDiceStatesRef()[0].DiceDefinition.Get(), StandardDie);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitB3DirectConsumableDieMutationTest,
	"GrandpaGambit.B3.Consumables.DirectDieMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitB3DirectConsumableDieMutationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitDiceDefinition* StandardDie = MakeDiceDefinition(GetTransientPackage(), TEXT("dice.standard"), TArray<int32>({ 1, 2, 3, 4, 5, 6 }));
	UGambitDiceComponent* DiceComponent = NewObject<UGambitDiceComponent>();
	TArray<TObjectPtr<UGambitDiceDefinition>> OwnedDice;
	OwnedDice.Add(StandardDie);
	OwnedDice.Add(StandardDie);
	DiceComponent->InitializeDicePool(OwnedDice);
	DiceComponent->SetDieValue(0, 5);

	UGambitConsumableDefinition* Injection = NewObject<UGambitConsumableDefinition>();
	UGambitItemEffectDefinition* InjectionEffect = MakeEffectDefinition(Injection, EGambitEffectHook::ConsumableUse, EGambitItemEffectType::ModifyDieValue);
	InjectionEffect->TargetRuleId = GambitEffectTargetRules::SelectedDie;
	InjectionEffect->Amount = 2.0f;
	Injection->EffectDefinitions.Add(InjectionEffect);

	FGambitEffectExecutionContext InjectionContext;
	InjectionContext.Hook = EGambitEffectHook::ConsumableUse;
	InjectionContext.SourceDiceComponent = DiceComponent;
	InjectionContext.SourceDice = DiceComponent->GetDiceStates();
	InjectionContext.SourceDieHandIndex = 0;
	Resolver->ExecuteItemEffects(Injection, InjectionContext);
	TestEqual(TEXT("injection can raise a die above 6"), DiceComponent->GetDiceStatesRef()[0].EffectiveValue, 7);

	UGambitConsumableDefinition* Lock = NewObject<UGambitConsumableDefinition>();
	UGambitItemEffectDefinition* LockEffect = MakeEffectDefinition(Lock, EGambitEffectHook::ConsumableUse, EGambitItemEffectType::LockDie);
	LockEffect->TargetRuleId = GambitEffectTargetRules::SelectedDie;
	Lock->EffectDefinitions.Add(LockEffect);

	FGambitEffectExecutionContext LockContext;
	LockContext.Hook = EGambitEffectHook::ConsumableUse;
	LockContext.SourceDiceComponent = DiceComponent;
	LockContext.SourceDice = DiceComponent->GetDiceStates();
	LockContext.SourceDieHandIndex = 1;
	Resolver->ExecuteItemEffects(Lock, LockContext);
	TestTrue(TEXT("lock consumable locks the selected die"), DiceComponent->GetDiceStatesRef()[1].bLocked);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitB3BribeOpponentTargetTest,
	"GrandpaGambit.B3.Consumables.BribeOpponentTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitB3BribeOpponentTargetTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitConsumableDefinition* Bribe = NewObject<UGambitConsumableDefinition>();
	Bribe->bCanTargetOpponent = true;

	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("test world is created"), TestWorld))
	{
		return false;
	}

	AGambitPlayerState* SourcePlayer = TestWorld->SpawnActor<AGambitPlayerState>();
	AGambitPlayerState* TargetPlayer = TestWorld->SpawnActor<AGambitPlayerState>();
	const bool bPlayersCreated = TestNotNull(TEXT("source player is created"), SourcePlayer)
		&& TestNotNull(TEXT("target player is created"), TargetPlayer);
	if (!bPlayersCreated)
	{
		TestWorld->DestroyWorld(false);
		return false;
	}

	FGambitEffectConditionDefinition HasGold;
	HasGold.ConditionType = EGambitEffectConditionType::GoldThreshold;
	HasGold.Comparison = EGambitEffectComparison::GreaterOrEqual;
	HasGold.Value = 5;

	UGambitItemEffectDefinition* TargetPenalty = MakeEffectDefinition(Bribe, EGambitEffectHook::ConsumableUse, EGambitItemEffectType::AddTemporaryScoreModifier);
	TargetPenalty->Target = EGambitEffectTarget::Target;
	TargetPenalty->TargetRuleId = GambitEffectTargetRules::TargetOpponent;
	TargetPenalty->Multiplier = 0.9f;
	TargetPenalty->Conditions.Add(HasGold);
	Bribe->EffectDefinitions.Add(TargetPenalty);

	UGambitItemEffectDefinition* SourceCost = MakeEffectDefinition(Bribe, EGambitEffectHook::ConsumableUse, EGambitItemEffectType::SpendGold);
	SourceCost->Amount = 5.0f;
	SourceCost->Conditions.Add(HasGold);
	Bribe->EffectDefinitions.Add(SourceCost);

	UGambitEconomyComponent* SourceEconomy = NewObject<UGambitEconomyComponent>();
	UGambitEconomyComponent* TargetEconomy = NewObject<UGambitEconomyComponent>();
	SourceEconomy->InitializeForMatch();
	TargetEconomy->InitializeForMatch();
	SourceEconomy->AddGold(10);
	const int32 SourceGoldBefore = SourceEconomy->GetCurrentGold();

	FGambitEffectExecutionContext Context;
	Context.Hook = EGambitEffectHook::ConsumableUse;
	Context.SourcePlayer = SourcePlayer;
	Context.TargetPlayer = TargetPlayer;
	Context.SourceEconomyComponent = SourceEconomy;
	Context.TargetEconomyComponent = TargetEconomy;
	Resolver->ExecuteItemEffects(Bribe, Context);

	TestEqual(TEXT("bribe spends 5 source gold"), SourceEconomy->GetCurrentGold(), SourceGoldBefore - 5);
	TestEqual(TEXT("bribe applies target x0.9 temporary multiplier"), Context.TargetTemporaryScoreModifierDelta.Multiplier, 0.9f);
	TestWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitB3GoldenRerollIncreaseRewardTest,
	"GrandpaGambit.B3.Consumables.GoldenRerollIncreaseReward",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitB3GoldenRerollIncreaseRewardTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const auto MakeGoldenReroll = []()
	{
		UGambitConsumableDefinition* GoldenReroll = NewObject<UGambitConsumableDefinition>();
		UGambitItemEffectDefinition* RerollEffect = MakeEffectDefinition(GoldenReroll, EGambitEffectHook::ConsumableUse, EGambitItemEffectType::RerollDie);
		RerollEffect->TargetRuleId = GambitEffectTargetRules::SelectedDie;
		GoldenReroll->EffectDefinitions.Add(RerollEffect);

		UGambitItemEffectDefinition* GoldEffect = MakeEffectDefinition(GoldenReroll, EGambitEffectHook::ConsumableUse, EGambitItemEffectType::AddGold);
		GoldEffect->Amount = 3.0f;
		FGambitEffectConditionDefinition Increased;
		Increased.ConditionType = EGambitEffectConditionType::LastAffectedDieValueIncreased;
		GoldEffect->Conditions.Add(Increased);
		GoldenReroll->EffectDefinitions.Add(GoldEffect);
		return GoldenReroll;
	};

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitDiceDefinition* AlwaysSix = MakeDiceDefinition(GetTransientPackage(), TEXT("dice.test.six"), TArray<int32>({ 6 }));
	UGambitDiceComponent* DiceComponent = NewObject<UGambitDiceComponent>();
	TArray<TObjectPtr<UGambitDiceDefinition>> OwnedDice;
	OwnedDice.Add(AlwaysSix);
	DiceComponent->InitializeDicePool(OwnedDice);
	DiceComponent->SetDieValue(0, 3);

	UGambitEconomyComponent* Economy = NewObject<UGambitEconomyComponent>();
	Economy->InitializeForMatch();
	const int32 GoldBeforeIncrease = Economy->GetCurrentGold();

	FGambitEffectExecutionContext IncreaseContext;
	IncreaseContext.Hook = EGambitEffectHook::ConsumableUse;
	IncreaseContext.SourceDiceComponent = DiceComponent;
	IncreaseContext.SourceDice = DiceComponent->GetDiceStates();
	IncreaseContext.SourceDieHandIndex = 0;
	IncreaseContext.SourceEconomyComponent = Economy;
	IncreaseContext.RandomStream.Initialize(1);
	Resolver->ExecuteItemEffects(MakeGoldenReroll(), IncreaseContext);

	TestEqual(TEXT("golden reroll rerolls selected die upward"), DiceComponent->GetDiceStatesRef()[0].EffectiveValue, 6);
	TestEqual(TEXT("golden reroll grants +3 gold only when value increases"), Economy->GetCurrentGold(), GoldBeforeIncrease + 3);

	UGambitDiceDefinition* AlwaysOne = MakeDiceDefinition(GetTransientPackage(), TEXT("dice.test.one"), TArray<int32>({ 1 }));
	TArray<TObjectPtr<UGambitDiceDefinition>> LowOwnedDice;
	LowOwnedDice.Add(AlwaysOne);
	DiceComponent->InitializeDicePool(LowOwnedDice);
	DiceComponent->SetDieValue(0, 6);
	const int32 GoldBeforeDecrease = Economy->GetCurrentGold();

	FGambitEffectExecutionContext DecreaseContext;
	DecreaseContext.Hook = EGambitEffectHook::ConsumableUse;
	DecreaseContext.SourceDiceComponent = DiceComponent;
	DecreaseContext.SourceDice = DiceComponent->GetDiceStates();
	DecreaseContext.SourceDieHandIndex = 0;
	DecreaseContext.SourceEconomyComponent = Economy;
	DecreaseContext.RandomStream.Initialize(1);
	Resolver->ExecuteItemEffects(MakeGoldenReroll(), DecreaseContext);

	TestEqual(TEXT("golden reroll can reroll selected die downward"), DiceComponent->GetDiceStatesRef()[0].EffectiveValue, 1);
	TestEqual(TEXT("golden reroll gives no gold when value does not increase"), Economy->GetCurrentGold(), GoldBeforeDecrease);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitB3ConsumableSingleUseRemovesSlotTest,
	"GrandpaGambit.B3.Consumables.SingleUseRemovesSlot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitB3ConsumableSingleUseRemovesSlotTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitInventoryComponent* InventoryComponent = NewObject<UGambitInventoryComponent>();
	UGambitConsumableDefinition* Consumable = NewObject<UGambitConsumableDefinition>();
	TestTrue(TEXT("consumable can be added to an inventory slot"), InventoryComponent->AddConsumable(Consumable));
	TestEqual(TEXT("consumable occupies one slot before use"), InventoryComponent->GetConsumableSlotsRef().Num(), 1);

	UGambitConsumableDefinition* ConsumedDefinition = nullptr;
	TestTrue(TEXT("first use consumes the item"), InventoryComponent->ConsumeConsumableDefinitionAtSlot(0, ConsumedDefinition));
	TestEqual(TEXT("used consumable is removed from inventory"), InventoryComponent->GetConsumableSlotsRef().Num(), 0);
	TestFalse(TEXT("second use is blocked because the slot is gone"), InventoryComponent->ConsumeConsumableDefinitionAtSlot(0, ConsumedDefinition));

	InventoryComponent->ResetRoundState();
	TestEqual(TEXT("round reset does not restore consumed consumables"), InventoryComponent->GetConsumableSlotsRef().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitB5SpecialDiceDefinitionsTest,
	"GrandpaGambit.B5.Dice.SpecialDefinitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitB5SpecialDiceDefinitionsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UObject* TestOuter = GetTransientPackage();
	UGambitDiceCombinationEvaluator* Evaluator = NewObject<UGambitDiceCombinationEvaluator>();
	UGambitScoreCalculator* ScoreCalculator = NewObject<UGambitScoreCalculator>();

	UGambitDiceDefinition* RoyalDie = MakeDiceDefinition(TestOuter, TEXT("dice.royal"), TArray<int32>({ 3, 4, 5, 6, 7, 8 }));
	TestTrue(TEXT("royal die exposes 7 and 8 faces"), RoyalDie->GetResolvedFaces().Contains(7) && RoyalDie->GetResolvedFaces().Contains(8));
	const FGambitDiceCombinationResult RoyalResult = Evaluator->EvaluateDice(TArray<FGambitDieRuntimeState>({
		MakeTestDie(8, 1, true, true, 0),
		MakeTestDie(8, 1, true, true, 1),
		MakeTestDie(7, 1, true, true, 2),
		MakeTestDie(6, 1, true, true, 3)
	}));
	TestEqual(TEXT("royal values above 6 participate in combinations"), RoyalResult.Combination, EGambitCombinationType::Pair);

	const FGambitDiceCombinationResult CursedResult = Evaluator->EvaluateDice(TArray<FGambitDieRuntimeState>({
		MakeTestDie(-3, 1, true, false, 0),
		MakeTestDie(-2, 1, true, false, 1),
		MakeTestDie(0, 1, true, false, 2),
		MakeTestDie(0, 1, true, false, 3)
	}));
	TestEqual(TEXT("cursed dice allow negative and zero score contributions"), CursedResult.DiceSum, -5);
	const FGambitScoreBreakdown CursedBreakdown = ScoreCalculator->CalculateScore(CursedResult, FGambitScoreModifierContext());
	TestEqual(TEXT("negative raw score clamps final score to zero"), CursedBreakdown.FinalScore, 0);

	FGambitDieRuntimeState GhostSix = MakeTestDie(6, 1, false, true, 0);
	FGambitDieRuntimeState NormalSix = MakeTestDie(6, 1, true, true, 1);
	const FGambitDiceCombinationResult GhostResult = Evaluator->EvaluateDice(TArray<FGambitDieRuntimeState>({ GhostSix, NormalSix }));
	TestEqual(TEXT("ghost die does not count in score sum"), GhostResult.DiceSum, 6);
	TestEqual(TEXT("ghost die still helps create a pair"), GhostResult.Combination, EGambitCombinationType::Pair);

	const FGambitDiceCombinationResult DoubleResult = Evaluator->EvaluateDice(TArray<FGambitDieRuntimeState>({
		MakeTestDie(4, 2, true, true, 0)
	}));
	TestEqual(TEXT("double die counts as a pair"), DoubleResult.Combination, EGambitCombinationType::Pair);

	FGambitDieRuntimeState TripleNoScore = MakeTestDie(5, 3, true, true, 0);
	TripleNoScore.ScoreContributionValue = 0;
	const FGambitDiceCombinationResult TripleResult = Evaluator->EvaluateDice(TArray<FGambitDieRuntimeState>({ TripleNoScore }));
	TestEqual(TEXT("triple die counts as a three of a kind"), TripleResult.Combination, EGambitCombinationType::ThreeOfAKind);
	TestEqual(TEXT("score contribution override to zero is respected"), TripleResult.DiceSum, 0);

	UGambitDiceDefinition* GodDie = MakeDiceDefinition(TestOuter, TEXT("dice.god"), TArray<int32>({ 6, 6, 6, 6, 6, 6 }));
	GodDie->bCanBeRerolled = false;
	UGambitDiceComponent* DiceComponent = NewObject<UGambitDiceComponent>();
	TArray<TObjectPtr<UGambitDiceDefinition>> GodDice;
	GodDice.Add(GodDie);
	GodDice.Add(GodDie);
	GodDice.Add(GodDie);
	GodDice.Add(GodDie);
	DiceComponent->InitializeDicePool(GodDice);
	FRandomStream RandomStream(5);
	TestFalse(TEXT("god dice cannot be rerolled during unlocked reroll"), DiceComponent->RollUnlocked(RandomStream));

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitDiceDefinition* JackpotDie = MakeDiceDefinition(TestOuter, TEXT("dice.jackpot"), TArray<int32>({ 1, 1, 1, 6, 6, 6 }));
	UGambitItemEffectDefinition* JackpotEffect = MakeEffectDefinition(JackpotDie, EGambitEffectHook::ScoreModifier, EGambitItemEffectType::MultiplyScore);
	JackpotEffect->Multiplier = 10.0f;
	FGambitEffectConditionDefinition AllOneOrSixCondition;
	AllOneOrSixCondition.ConditionType = EGambitEffectConditionType::AllDiceValueInSet;
	AllOneOrSixCondition.Value = 1;
	AllOneOrSixCondition.Count = 6;
	JackpotEffect->Conditions.Add(AllOneOrSixCondition);
	JackpotDie->EffectDefinitions.Add(JackpotEffect);

	FGambitEffectExecutionContext JackpotContext;
	JackpotContext.Hook = EGambitEffectHook::ScoreModifier;
	JackpotContext.SourceDice = {
		MakeTestDie(1, 1, true, true, 0),
		MakeTestDie(6, 1, true, true, 1),
		MakeTestDie(6, 1, true, true, 2),
		MakeTestDie(1, 1, true, true, 3)
	};
	Resolver->ExecuteDiceEffects(JackpotDie, JackpotContext);
	TestEqual(TEXT("jackpot applies x10 only when every die is 1 or 6"), JackpotContext.ScoreModifierDelta.Multiplier, 10.0f);

	FGambitEffectExecutionContext FailedJackpotContext;
	FailedJackpotContext.Hook = EGambitEffectHook::ScoreModifier;
	FailedJackpotContext.SourceDice = {
		MakeTestDie(1, 1, true, true, 0),
		MakeTestDie(2, 1, true, true, 1),
		MakeTestDie(6, 1, true, true, 2),
		MakeTestDie(1, 1, true, true, 3)
	};
	Resolver->ExecuteDiceEffects(JackpotDie, FailedJackpotContext);
	TestEqual(TEXT("jackpot ignores hands containing values outside 1 or 6"), FailedJackpotContext.ScoreModifierDelta.Multiplier, 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitB5DestroyedDiceDoesNotRollbackTest,
	"GrandpaGambit.B5.Dice.DestroyedDiceDoesNotRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitB5DestroyedDiceDoesNotRollbackTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UObject* TestOuter = GetTransientPackage();
	UGambitDiceDefinition* CrackedDie = MakeDiceDefinition(TestOuter, TEXT("dice.cracked"), TArray<int32>({ 1, 2, 3, 4, 5, 6 }));
	UGambitDiceDefinition* StableDie = MakeDiceDefinition(TestOuter, TEXT("dice.stable"), TArray<int32>({ 1, 2, 3, 4, 5, 6 }));
	UGambitInventoryComponent* InventoryComponent = NewObject<UGambitInventoryComponent>();
	InventoryComponent->AddOwnedDie(CrackedDie);
	InventoryComponent->AddOwnedDie(StableDie);

	UGambitDiceComponent* DiceComponent = NewObject<UGambitDiceComponent>();
	DiceComponent->InitializeDicePool(InventoryComponent->GetOwnedDiceDefinitions());

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitDiceDefinition* SourceDefinition = MakeDiceDefinition(TestOuter, TEXT("dice.destroyer"), TArray<int32>({ 6 }));
	UGambitItemEffectDefinition* DestroyEffect = MakeEffectDefinition(SourceDefinition, EGambitEffectHook::RoundEnd, EGambitItemEffectType::DestroyOrRemoveDiceChance);
	DestroyEffect->Amount = 100.0f;
	SourceDefinition->EffectDefinitions.Add(DestroyEffect);

	FGambitEffectExecutionContext Context;
	Context.Hook = EGambitEffectHook::RoundEnd;
	Context.SourceDiceComponent = DiceComponent;
	Context.SourceInventoryComponent = InventoryComponent;
	Context.SourceDice = DiceComponent->GetDiceStates();
	Context.SourceDieHandIndex = 0;
	Context.RandomStream.Initialize(123);
	Resolver->ExecuteDiceEffects(SourceDefinition, Context);

	TestEqual(TEXT("destroy effect removes the owned die from inventory"), InventoryComponent->GetOwnedDiceCount(), 1);
	TestEqual(TEXT("remaining owned die is the stable die"), InventoryComponent->GetOwnedDiceDefinitions()[0].Get(), StableDie);

	DiceComponent->InitializeDicePool(InventoryComponent->GetOwnedDiceDefinitions());
	const bool bCrackedReturned = DiceComponent->GetDiceStatesRef().ContainsByPredicate([CrackedDie](const FGambitDieRuntimeState& DieState)
	{
		return DieState.DiceDefinition == CrackedDie;
	});
	TestFalse(TEXT("destroyed die is not restored by next round dice initialization"), bCrackedReturned);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitB5JackpotCapsAndBreakdownTest,
	"GrandpaGambit.B5.BigNumbers.JackpotCapsAndBreakdown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitB5JackpotCapsAndBreakdownTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitScoreCalculator* ScoreCalculator = NewObject<UGambitScoreCalculator>();

	UGambitModuleDefinition* CrazyMultiplier = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* CrazyEffect = MakeEffectDefinition(CrazyMultiplier, EGambitEffectHook::ScoreModifier, EGambitItemEffectType::MultiplyScore);
	CrazyEffect->ScalarParameters.Add(TEXT("MultiplierPerMatchingDie"), 1.5f);
	FGambitEffectConditionDefinition SixCondition;
	SixCondition.ConditionType = EGambitEffectConditionType::DieValueEquals;
	SixCondition.Value = 6;
	CrazyEffect->Conditions.Add(SixCondition);
	CrazyMultiplier->EffectDefinitions.Add(CrazyEffect);

	FGambitEffectExecutionContext CrazyContext;
	CrazyContext.Hook = EGambitEffectHook::ScoreModifier;
	CrazyContext.SourceDice = {
		MakeTestDie(6, 1, true, true, 0),
		MakeTestDie(6, 1, true, true, 1),
		MakeTestDie(2, 1, true, true, 2),
		MakeTestDie(3, 1, true, true, 3)
	};
	Resolver->ExecuteItemEffects(CrazyMultiplier, CrazyContext);
	TestEqual(TEXT("per-matching-die multiplier compounds once per matching die"), CrazyContext.ScoreModifierDelta.Multiplier, 2.25f);

	FGambitDiceCombinationResult BigCombination;
	BigCombination.Combination = EGambitCombinationType::FiveOfAKind;
	BigCombination.BaseCombinationScore = 100;
	BigCombination.DiceSum = 120;
	BigCombination.RawScore = 220;

	FGambitScoreModifierContext UltimateJackpotModifier;
	UltimateJackpotModifier.Multiplier = 50.0f;
	UltimateJackpotModifier.ScoreCap = 5000.0f;
	UltimateJackpotModifier.DiminishingFactor = 1.0f;

	const FGambitScoreBreakdown Breakdown = ScoreCalculator->CalculateScore(BigCombination, UltimateJackpotModifier);
	TestEqual(TEXT("extreme multiplier remains visible in breakdown"), Breakdown.Multiplier, 50.0f);
	TestTrue(TEXT("score before cap records the jackpot explosion"), Breakdown.ScoreBeforeCap > Breakdown.ScoreAfterCap);
	TestEqual(TEXT("jackpot score is capped"), Breakdown.FinalScore, 5000);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitB5SharedPoolRareLegendaryStockTest,
	"GrandpaGambit.B5.Shop.SharedPoolRareLegendaryStock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitB5SharedPoolRareLegendaryStockTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitItemDefinition* RareDiceItem = MakeShopItem(GetTransientPackage(), TEXT("item.dice.royal"), 12);
	RareDiceItem->Rarity = EGambitItemRarity::Rare;
	RareDiceItem->bUsesSharedPool = true;
	RareDiceItem->SharedPoolMaxStock = 2;
	RareDiceItem->SharedPoolPurchaseLimit = 2;

	UGambitItemDefinition* LegendaryDiceItem = MakeShopItem(GetTransientPackage(), TEXT("item.dice.jackpot"), 20);
	LegendaryDiceItem->Rarity = EGambitItemRarity::Legendary;
	LegendaryDiceItem->bUsesSharedPool = true;
	LegendaryDiceItem->SharedPoolMaxStock = 1;
	LegendaryDiceItem->SharedPoolPurchaseLimit = 1;

	FGambitSharedStockEntry RareStock;
	RareStock.ItemDefinition = RareDiceItem;
	RareStock.MaxStock = 2;
	RareStock.GlobalPurchaseLimit = 2;

	FGambitSharedStockEntry LegendaryStock;
	LegendaryStock.ItemDefinition = LegendaryDiceItem;
	LegendaryStock.MaxStock = 1;
	LegendaryStock.GlobalPurchaseLimit = 1;

	UGambitSharedPoolComponent* SharedPool = NewObject<UGambitSharedPoolComponent>();
	SharedPool->InitializeSharedStock({ RareStock, LegendaryStock });

	TestTrue(TEXT("rare B5 item is initially available"), SharedPool->IsItemAvailable(RareDiceItem));
	TestTrue(TEXT("legendary B5 item is initially available"), SharedPool->IsItemAvailable(LegendaryDiceItem));

	TestTrue(TEXT("first rare purchase consumes shared stock"), SharedPool->ConsumeItemStock(RareDiceItem));
	TestTrue(TEXT("second rare purchase consumes shared stock"), SharedPool->ConsumeItemStock(RareDiceItem));
	TestFalse(TEXT("third rare purchase is blocked by stock/limit"), SharedPool->ConsumeItemStock(RareDiceItem));

	FGuid ReservationId;
	FGambitSharedPoolAvailabilityResult Availability;
	TestTrue(TEXT("legendary B5 item can be reserved for shop purchase"), SharedPool->TryReserveItemStock(LegendaryDiceItem, GetTransientPackage(), ReservationId, Availability));
	TestEqual(TEXT("legendary reservation removes visible stock"), SharedPool->GetRemainingStock(LegendaryDiceItem), 0);
	TestTrue(TEXT("legendary B5 reservation commits purchase"), SharedPool->CommitReservedItemStock(LegendaryDiceItem, ReservationId));
	TestFalse(TEXT("legendary B5 stock is gone after purchase"), SharedPool->IsItemAvailable(LegendaryDiceItem));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitB5ShopPurchaseRareLegendaryOffersTest,
	"GrandpaGambit.B5.Shop.PurchaseRareLegendarySharedPoolOffers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitB5ShopPurchaseRareLegendaryOffersTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UObject* TestOuter = GetTransientPackage();
	UGambitDiceDefinition* RoyalDie = MakeDiceDefinition(TestOuter, TEXT("dice.royal"), TArray<int32>({ 3, 4, 5, 6, 7, 8 }));
	UGambitDiceDefinition* JackpotDie = MakeDiceDefinition(TestOuter, TEXT("dice.jackpot"), TArray<int32>({ 1, 1, 1, 6, 6, 6 }));

	UGambitDiceItemDefinition* RoyalItem = NewObject<UGambitDiceItemDefinition>(TestOuter);
	RoyalItem->ItemId = TEXT("item.dice.royal");
	RoyalItem->Cost = 11;
	RoyalItem->Rarity = EGambitItemRarity::Rare;
	RoyalItem->bUsesSharedPool = true;
	RoyalItem->SharedPoolMaxStock = 2;
	RoyalItem->SharedPoolPurchaseLimit = 2;
	RoyalItem->GrantedDiceDefinition = RoyalDie;

	UGambitDiceItemDefinition* JackpotItem = NewObject<UGambitDiceItemDefinition>(TestOuter);
	JackpotItem->ItemId = TEXT("item.dice.jackpot");
	JackpotItem->Cost = 22;
	JackpotItem->Rarity = EGambitItemRarity::Legendary;
	JackpotItem->bUsesSharedPool = true;
	JackpotItem->SharedPoolMaxStock = 1;
	JackpotItem->SharedPoolPurchaseLimit = 1;
	JackpotItem->GrantedDiceDefinition = JackpotDie;

	FGambitSharedStockEntry RoyalStock;
	RoyalStock.ItemDefinition = RoyalItem;
	RoyalStock.MaxStock = 2;
	RoyalStock.GlobalPurchaseLimit = 2;

	FGambitSharedStockEntry JackpotStock;
	JackpotStock.ItemDefinition = JackpotItem;
	JackpotStock.MaxStock = 1;
	JackpotStock.GlobalPurchaseLimit = 1;

	UGambitSharedPoolComponent* SharedPool = NewObject<UGambitSharedPoolComponent>(TestOuter);
	SharedPool->InitializeSharedStock({ RoyalStock, JackpotStock });

	FGambitShopOffer RoyalOffer;
	RoyalOffer.OfferId = 1;
	RoyalOffer.ItemDefinition = RoyalItem;
	RoyalOffer.BasePrice = RoyalItem->Cost;
	RoyalOffer.Price = RoyalItem->Cost;
	RoyalOffer.bUsesSharedPool = true;

	FGambitShopOffer JackpotOffer;
	JackpotOffer.OfferId = 2;
	JackpotOffer.ItemDefinition = JackpotItem;
	JackpotOffer.BasePrice = JackpotItem->Cost;
	JackpotOffer.Price = JackpotItem->Cost;
	JackpotOffer.bUsesSharedPool = true;

	UGambitShopComponent* ShopComponent = NewObject<UGambitShopComponent>(TestOuter);
	ShopComponent->SetCurrentOffers({ RoyalOffer, JackpotOffer });

	UGambitInventoryComponent* InventoryComponent = NewObject<UGambitInventoryComponent>(TestOuter);
	UGambitEconomyComponent* EconomyComponent = NewObject<UGambitEconomyComponent>(TestOuter);
	EconomyComponent->AddGold(50);

	TestTrue(TEXT("rare B5 shared-pool shop offer purchases successfully"), ShopComponent->PurchaseOffer(1, EconomyComponent, InventoryComponent, SharedPool));
	TestEqual(TEXT("rare B5 purchase grants royal die"), InventoryComponent->GetOwnedDiceDefinitions()[0].Get(), RoyalDie);
	TestEqual(TEXT("rare B5 purchase decrements shared stock"), SharedPool->GetRemainingStock(RoyalItem), 1);

	TestTrue(TEXT("legendary B5 shared-pool shop offer purchases successfully"), ShopComponent->PurchaseOffer(2, EconomyComponent, InventoryComponent, SharedPool));
	TestEqual(TEXT("legendary B5 purchase grants jackpot die"), InventoryComponent->GetOwnedDiceDefinitions()[1].Get(), JackpotDie);
	TestEqual(TEXT("legendary B5 purchase consumes final shared stock"), SharedPool->GetRemainingStock(JackpotItem), 0);
	TestFalse(TEXT("legendary B5 shared stock blocks another purchase"), SharedPool->IsItemAvailable(JackpotItem));
	TestEqual(TEXT("B5 rare and legendary purchases spend expected gold"), EconomyComponent->GetCurrentGold(), 17);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitB4CompletionAverageGoldAndRankTest,
	"GrandpaGambit.B4.Completion.AverageGoldAndRank",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitB4CompletionAverageGoldAndRankTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();

	UGambitDiceDefinition* BankDie = MakeDiceDefinition(GetTransientPackage(), TEXT("dice.bank"), TArray<int32>({ 1, 2, 3, 4, 5, 6 }));
	UGambitItemEffectDefinition* BankEffect = MakeEffectDefinition(BankDie, EGambitEffectHook::Reward, EGambitItemEffectType::AddGold);
	BankEffect->Amount = 2.0f;
	FGambitEffectConditionDefinition BelowAverage;
	BelowAverage.ConditionType = EGambitEffectConditionType::SourceDieComparedToAverage;
	BelowAverage.Comparison = EGambitEffectComparison::Lower;
	BankEffect->Conditions.Add(BelowAverage);
	BankDie->EffectDefinitions.Add(BankEffect);

	UGambitEconomyComponent* Economy = NewObject<UGambitEconomyComponent>();
	Economy->InitializeForMatch();

	FGambitEffectExecutionContext BankContext;
	BankContext.Hook = EGambitEffectHook::Reward;
	BankContext.SourceEconomyComponent = Economy;
	BankContext.SourceDice = {
		MakeTestDie(2, 1, true, true, 0),
		MakeTestDie(4, 1, true, true, 1),
		MakeTestDie(6, 1, true, true, 2),
	};
	BankContext.SourceDieHandIndex = 0;
	Resolver->ExecuteDiceEffects(BankDie, BankContext);
	TestEqual(TEXT("dice.bank grants gold when source die is below roll average"), Economy->GetCurrentGold(), 2);

	BankContext.SourceDieHandIndex = 2;
	Resolver->ExecuteDiceEffects(BankDie, BankContext);
	TestEqual(TEXT("dice.bank does not grant gold above average"), Economy->GetCurrentGold(), 2);

	UGambitModuleDefinition* PrivateBank = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* PrivateBankEffect = MakeEffectDefinition(PrivateBank, EGambitEffectHook::ScoreModifier, EGambitItemEffectType::AddScoreFlat);
	PrivateBankEffect->ScalarParameters.Add(TEXT("AmountPerGold"), 5.0f);
	PrivateBank->EffectDefinitions.Add(PrivateBankEffect);
	Economy->AddGold(5);

	FGambitEffectExecutionContext GoldScoreContext;
	GoldScoreContext.Hook = EGambitEffectHook::ScoreModifier;
	GoldScoreContext.SourceEconomyComponent = Economy;
	Resolver->ExecuteItemEffects(PrivateBank, GoldScoreContext);
	TestEqual(TEXT("score scalar uses current gold"), GoldScoreContext.ScoreModifierDelta.AdditiveBonus, 35.0f);

	UGambitModuleDefinition* Capitalism = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* CapitalismEffect = MakeEffectDefinition(Capitalism, EGambitEffectHook::ScoreModifier, EGambitItemEffectType::MultiplyScore);
	CapitalismEffect->ScalarParameters.Add(TEXT("MultiplierPerGoldTier"), 1.1f);
	CapitalismEffect->ScalarParameters.Add(TEXT("GoldTierSize"), 10.0f);
	Capitalism->EffectDefinitions.Add(CapitalismEffect);
	Economy->AddGold(18);

	FGambitEffectExecutionContext GoldTierContext;
	GoldTierContext.Hook = EGambitEffectHook::ScoreModifier;
	GoldTierContext.SourceEconomyComponent = Economy;
	Resolver->ExecuteItemEffects(Capitalism, GoldTierContext);
	TestTrue(TEXT("gold tier multiplier applies once per full tier"), FMath::IsNearlyEqual(GoldTierContext.ScoreModifierDelta.Multiplier, 1.21f, 0.001f));

	UGambitEconomyComponent* LowEconomy = NewObject<UGambitEconomyComponent>();
	UGambitEconomyComponent* MidEconomy = NewObject<UGambitEconomyComponent>();
	UGambitEconomyComponent* HighEconomy = NewObject<UGambitEconomyComponent>();
	LowEconomy->InitializeForMatch();
	MidEconomy->InitializeForMatch();
	HighEconomy->InitializeForMatch();
	LowEconomy->AddGold(3);
	MidEconomy->AddGold(7);
	HighEconomy->AddGold(12);

	UGambitModuleDefinition* Outsider = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* OutsiderEffect = MakeEffectDefinition(Outsider, EGambitEffectHook::ScoreModifier, EGambitItemEffectType::MultiplyScore);
	OutsiderEffect->Multiplier = 1.5f;
	FGambitEffectConditionDefinition LowestGold;
	LowestGold.ConditionType = EGambitEffectConditionType::LowestGold;
	OutsiderEffect->Conditions.Add(LowestGold);
	Outsider->EffectDefinitions.Add(OutsiderEffect);

	FGambitEffectExecutionContext LowestContext;
	LowestContext.Hook = EGambitEffectHook::ScoreModifier;
	LowestContext.SourceEconomyComponent = LowEconomy;
	LowestContext.MatchEconomyComponents = { LowEconomy, MidEconomy, HighEconomy };
	Resolver->ExecuteItemEffects(Outsider, LowestContext);
	TestEqual(TEXT("lowest gold condition enables outsider bonus"), LowestContext.ScoreModifierDelta.Multiplier, 1.5f);

	FGambitEffectExecutionContext NotLowestContext;
	NotLowestContext.Hook = EGambitEffectHook::ScoreModifier;
	NotLowestContext.SourceEconomyComponent = HighEconomy;
	NotLowestContext.MatchEconomyComponents = { LowEconomy, MidEconomy, HighEconomy };
	Resolver->ExecuteItemEffects(Outsider, NotLowestContext);
	TestEqual(TEXT("lowest gold condition blocks richer players"), NotLowestContext.ScoreModifierDelta.Multiplier, 1.0f);

	UGambitModuleDefinition* MinimumWage = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* MinimumWageEffect = MakeEffectDefinition(MinimumWage, EGambitEffectHook::Ranking, EGambitItemEffectType::AddGold);
	MinimumWageEffect->Amount = 8.0f;
	FGambitEffectConditionDefinition LastRank;
	LastRank.ConditionType = EGambitEffectConditionType::LastRank;
	MinimumWageEffect->Conditions.Add(LastRank);
	MinimumWage->EffectDefinitions.Add(MinimumWageEffect);

	for (const int32 PlayerCount : TArray<int32>({ 2, 3, 4 }))
	{
		UGambitEconomyComponent* WageEconomy = NewObject<UGambitEconomyComponent>();
		WageEconomy->InitializeForMatch();
		FGambitEffectExecutionContext WageContext;
		WageContext.Hook = EGambitEffectHook::Ranking;
		WageContext.SourceEconomyComponent = WageEconomy;
		WageContext.SourceRank = PlayerCount;
		WageContext.MatchPlayerCount = PlayerCount;
		Resolver->ExecuteItemEffects(MinimumWage, WageContext);
		TestEqual(FString::Printf(TEXT("last rank condition grants minimum wage for %d players"), PlayerCount), WageEconomy->GetCurrentGold(), 8);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitB4CompletionShopAndRerollTest,
	"GrandpaGambit.B4.Completion.ShopAndReroll",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitB4CompletionShopAndRerollTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();

	UGambitModuleDefinition* Coupon = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* CouponEffect = MakeEffectDefinition(Coupon, EGambitEffectHook::PostShopGenerate, EGambitItemEffectType::MakeShopOfferFree);
	Coupon->EffectDefinitions.Add(CouponEffect);

	UGambitItemDefinition* OfferItemA = MakeShopItem(GetTransientPackage(), TEXT("item.test.a"), 5);
	UGambitItemDefinition* OfferItemB = MakeShopItem(GetTransientPackage(), TEXT("item.test.b"), 7);
	UGambitItemDefinition* OfferItemC = MakeShopItem(GetTransientPackage(), TEXT("item.test.c"), 9);
	TArray<FGambitShopOffer> Offers;
	for (int32 OfferIndex = 0; OfferIndex < 3; ++OfferIndex)
	{
		FGambitShopOffer Offer;
		Offer.OfferId = OfferIndex;
		Offer.ItemDefinition = OfferIndex == 0 ? OfferItemA : (OfferIndex == 1 ? OfferItemB : OfferItemC);
		Offer.BasePrice = Offer.ItemDefinition->Cost;
		Offer.Price = Offer.BasePrice;
		Offers.Add(Offer);
	}

	FGambitEffectExecutionContext CouponContext;
	CouponContext.Hook = EGambitEffectHook::PostShopGenerate;
	CouponContext.GeneratedShopOffers = Offers;
	CouponContext.RandomStream.Initialize(123);
	Resolver->ExecuteItemEffects(Coupon, CouponContext);
	int32 FreeOfferCount = 0;
	for (const FGambitShopOffer& Offer : CouponContext.GeneratedShopOffers)
	{
		if (Offer.bFree && Offer.Price == 0)
		{
			FreeOfferCount++;
		}
	}
	TestEqual(TEXT("coupon marks exactly one generated shop offer free"), FreeOfferCount, 1);

	UGambitModuleDefinition* RerollDeltaModule = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* IncreasedEffect = MakeEffectDefinition(RerollDeltaModule, EGambitEffectHook::PostReroll, EGambitItemEffectType::AddTemporaryScoreModifier);
	IncreasedEffect->ScalarParameters.Add(TEXT("AmountPerRerolledDieValueIncreased"), 25.0f);
	RerollDeltaModule->EffectDefinitions.Add(IncreasedEffect);
	UGambitItemEffectDefinition* DecreasedEffect = MakeEffectDefinition(RerollDeltaModule, EGambitEffectHook::PostReroll, EGambitItemEffectType::AddTemporaryScoreModifier);
	DecreasedEffect->ScalarParameters.Add(TEXT("AmountPerRerolledDieValueDecreased"), 15.0f);
	RerollDeltaModule->EffectDefinitions.Add(DecreasedEffect);

	FGambitEffectExecutionContext DeltaContext;
	DeltaContext.Hook = EGambitEffectHook::PostReroll;
	FGambitRerollDieDelta IncreasedDelta;
	IncreasedDelta.ValueBefore = 2;
	IncreasedDelta.ValueAfter = 5;
	FGambitRerollDieDelta DecreasedDelta;
	DecreasedDelta.ValueBefore = 6;
	DecreasedDelta.ValueAfter = 1;
	FGambitRerollDieDelta SameDelta;
	SameDelta.ValueBefore = 4;
	SameDelta.ValueAfter = 4;
	DeltaContext.RerollDeltas = { IncreasedDelta, DecreasedDelta, SameDelta };
	Resolver->ExecuteItemEffects(RerollDeltaModule, DeltaContext);
	TestEqual(TEXT("reroll delta scalars count increased and decreased dice"), DeltaContext.TemporaryScoreModifierDelta.AdditiveBonus, 40.0f);

	UGambitModuleDefinition* OneMore = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* OneMoreEffect = MakeEffectDefinition(OneMore, EGambitEffectHook::PostReroll, EGambitItemEffectType::AddReroll);
	OneMoreEffect->Amount = 1.0f;
	FGambitEffectConditionDefinition NoChange;
	NoChange.ConditionType = EGambitEffectConditionType::RerollNoValueChanged;
	OneMoreEffect->Conditions.Add(NoChange);
	OneMore->EffectDefinitions.Add(OneMoreEffect);

	FGambitEffectExecutionContext NoChangeContext;
	NoChangeContext.Hook = EGambitEffectHook::PostReroll;
	NoChangeContext.RerollDeltas = { SameDelta };
	Resolver->ExecuteItemEffects(OneMore, NoChangeContext);
	TestEqual(TEXT("one more grants reroll only when no rerolled value changed"), NoChangeContext.RerollLimitDelta, 1);

	UGambitModuleDefinition* Determination = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* DeterminationEffect = MakeEffectDefinition(Determination, EGambitEffectHook::ScoreModifier, EGambitItemEffectType::MultiplyScore);
	DeterminationEffect->Multiplier = 2.0f;
	FGambitEffectConditionDefinition Twice;
	Twice.ConditionType = EGambitEffectConditionType::AnyDieRerolledAtLeastCount;
	Twice.Comparison = EGambitEffectComparison::GreaterOrEqual;
	Twice.Count = 2;
	DeterminationEffect->Conditions.Add(Twice);
	Determination->EffectDefinitions.Add(DeterminationEffect);

	FGambitEffectExecutionContext DeterminationContext;
	DeterminationContext.Hook = EGambitEffectHook::ScoreModifier;
	DeterminationContext.MaxRerollCountForAnyDieThisRound = 2;
	Resolver->ExecuteItemEffects(Determination, DeterminationContext);
	TestEqual(TEXT("same die rerolled twice condition enables determination"), DeterminationContext.ScoreModifierDelta.Multiplier, 2.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGambitB4CompletionSharedPoolAndGlobalPurchasesTest,
	"GrandpaGambit.B4.Completion.SharedPoolAndGlobalPurchases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGambitB4CompletionSharedPoolAndGlobalPurchasesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UGambitEffectResolver* Resolver = NewObject<UGambitEffectResolver>();
	UGambitDiceDefinition* GoldenDie = MakeDiceDefinition(GetTransientPackage(), TEXT("dice.golden"), TArray<int32>({ 1, 2, 3, 4, 5, 6 }));
	UGambitDiceItemDefinition* GoldenItem = NewObject<UGambitDiceItemDefinition>(GetTransientPackage());
	GoldenItem->ItemId = TEXT("item.dice.golden");
	GoldenItem->ItemType = EGambitItemType::Dice;
	GoldenItem->Rarity = EGambitItemRarity::Uncommon;
	GoldenItem->Cost = 8;
	GoldenItem->bUsesSharedPool = true;
	GoldenItem->SharedPoolMaxStock = 1;
	GoldenItem->SharedPoolPurchaseLimit = 4;
	GoldenItem->GrantedDiceDefinition = GoldenDie;

	FGambitSharedStockEntry GoldenStock;
	GoldenStock.ItemDefinition = GoldenItem;
	GoldenStock.MaxStock = 1;
	GoldenStock.GlobalPurchaseLimit = 4;

	UGambitSharedPoolComponent* SharedPool = NewObject<UGambitSharedPoolComponent>();
	SharedPool->InitializeSharedStock({ GoldenStock });

	UGambitModuleDefinition* GoldenPool = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* GoldenPoolEffect = MakeEffectDefinition(GoldenPool, EGambitEffectHook::PostPurchase, EGambitItemEffectType::AddSharedPoolStock);
	GoldenPoolEffect->Amount = 3.0f;
	GoldenPoolEffect->ReferencedItemDefinition = GoldenItem;
	GoldenPool->EffectDefinitions.Add(GoldenPoolEffect);

	FGambitEffectExecutionContext GoldenPoolContext;
	GoldenPoolContext.Hook = EGambitEffectHook::PostPurchase;
	GoldenPoolContext.SharedPoolComponent = SharedPool;
	Resolver->ExecuteItemEffects(GoldenPool, GoldenPoolContext);
	TestEqual(TEXT("golden pool adds dynamic shared stock"), SharedPool->GetRemainingStock(GoldenItem), 4);

	SharedPool->RecordGlobalPurchase(GoldenItem);
	SharedPool->RecordGlobalPurchase(GoldenItem);

	UGambitModuleDefinition* Shortage = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* ShortageEffect = MakeEffectDefinition(Shortage, EGambitEffectHook::PrePriceResolve, EGambitItemEffectType::AddShopFlatPriceDelta);
	ShortageEffect->ScalarParameters.Add(TEXT("AmountPerGlobalPurchaseForItemTypeAndRarity"), 1.0f);
	FGambitEffectConditionDefinition RareOffer;
	RareOffer.ConditionType = EGambitEffectConditionType::ShopItemRarity;
	RareOffer.Comparison = EGambitEffectComparison::GreaterOrEqual;
	RareOffer.Rarity = EGambitItemRarity::Uncommon;
	ShortageEffect->Conditions.Add(RareOffer);
	Shortage->EffectDefinitions.Add(ShortageEffect);

	FGambitEffectExecutionContext ShortageContext;
	ShortageContext.Hook = EGambitEffectHook::PrePriceResolve;
	ShortageContext.ShopPurchase.ItemDefinition = GoldenItem;
	ShortageContext.ShopPurchase.GlobalPurchasesForItemTypeAndRarity = SharedPool->GetGlobalPurchaseCountForTypeAndRarity(EGambitItemType::Dice, EGambitItemRarity::Uncommon);
	Resolver->ExecuteItemEffects(Shortage, ShortageContext);
	TestEqual(TEXT("shortage uses global rarity purchase count for price delta"), ShortageContext.ShopPurchase.FlatPriceDelta, 2);

	UGambitModuleDefinition* Stockout = NewObject<UGambitModuleDefinition>();
	UGambitItemEffectDefinition* StockoutEffect = MakeEffectDefinition(Stockout, EGambitEffectHook::PostPurchase, EGambitItemEffectType::SetSharedPoolItemUnavailable);
	FGambitEffectConditionDefinition ThreeDiceBought;
	ThreeDiceBought.ConditionType = EGambitEffectConditionType::ShopGlobalPurchaseTypeCount;
	ThreeDiceBought.Comparison = EGambitEffectComparison::GreaterOrEqual;
	ThreeDiceBought.Count = 3;
	StockoutEffect->Conditions.Add(ThreeDiceBought);
	Stockout->EffectDefinitions.Add(StockoutEffect);

	SharedPool->RecordGlobalPurchase(GoldenItem);
	FGambitEffectExecutionContext StockoutContext;
	StockoutContext.Hook = EGambitEffectHook::PostPurchase;
	StockoutContext.SharedPoolComponent = SharedPool;
	StockoutContext.ShopPurchase.ItemDefinition = GoldenItem;
	StockoutContext.ShopPurchase.GlobalPurchasesForItemType = SharedPool->GetGlobalPurchaseCountForType(EGambitItemType::Dice);
	Resolver->ExecuteItemEffects(Stockout, StockoutContext);
	TestFalse(TEXT("stockout makes shared item unavailable after type threshold"), SharedPool->IsItemAvailable(GoldenItem));

	return true;
}

#endif
