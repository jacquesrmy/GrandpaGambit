#include "Game/Flow/GambitRoundEffectPipeline.h"

#include "Dice/Data/GambitDiceDefinition.h"
#include "Items/Effects/GambitEffectResolver.h"
#include "Items/Modules/GambitModuleDefinition.h"
#include "Players/States/GambitPlayerState.h"
#include "Scoring/Calculators/GambitScoreModifierMath.h"
#include "Shop/Components/GambitShopComponent.h"

void UGambitRoundEffectPipeline::SetEffectResolver(UGambitEffectResolver* InEffectResolver)
{
	EffectResolver = InEffectResolver;
}

FGambitEffectExecutionContext UGambitRoundEffectPipeline::MakeEffectContext(const FGambitRoundEffectContextRequest& Request) const
{
	FGambitEffectExecutionContext Context;
	Context.Hook = Request.Hook;
	Context.CurrentPhase = Request.CurrentPhase;
	Context.SourcePlayer = Request.SourcePlayer;
	Context.TargetPlayer = Request.TargetPlayer ? Request.TargetPlayer : Request.SourcePlayer;
	Context.SharedPoolComponent = Request.SharedPoolComponent;
	Context.RandomStream = Request.RandomStream;
	Context.RerollsUsed = Request.RerollsUsed;
	Context.RerollLimit = Request.RerollLimit;
	Context.SourceRank = Request.SourceRank;
	Context.TargetRank = Request.TargetRank;
	Context.MatchPlayerCount = Request.MatchPlayers.Num();
	Context.FirstRerolledDieHandIndexThisRound = Request.FirstRerolledDieHandIndexThisRound;
	Context.MaxRerollCountForAnyDieThisRound = Request.MaxRerollCountForAnyDieThisRound;

	for (AGambitPlayerState* PlayerState : Request.MatchPlayers)
	{
		if (!PlayerState)
		{
			continue;
		}

		Context.MatchPlayerStates.Add(PlayerState);
		if (PlayerState->GetEconomyComponent())
		{
			Context.MatchEconomyComponents.Add(PlayerState->GetEconomyComponent());
		}
	}

	if (AGambitPlayerState* SourcePlayer = Context.SourcePlayer.Get())
	{
		Context.SourceDiceComponent = SourcePlayer->GetDiceComponent();
		Context.SourceEconomyComponent = SourcePlayer->GetEconomyComponent();
		Context.SourceInventoryComponent = SourcePlayer->GetInventoryComponent();
		Context.SourceShopComponent = SourcePlayer->GetShopComponent();
		Context.SourceDice = SourcePlayer->GetDiceStates();
		Context.CurrentScoreBreakdown = SourcePlayer->GetLastScoreBreakdown();
		Context.CurrentScoreModifier = SourcePlayer->GetTemporaryScoreModifier();
		if (Context.SourceShopComponent)
		{
			Context.GeneratedShopOffers = Context.SourceShopComponent->GetCurrentOffers();
			Context.ShopPurchase.PurchasesMadeBefore = Context.SourceShopComponent->GetPurchasesMadeThisShop();
		}
	}

	if (AGambitPlayerState* EffectiveTarget = Context.TargetPlayer.Get())
	{
		Context.TargetDiceComponent = EffectiveTarget->GetDiceComponent();
		Context.TargetEconomyComponent = EffectiveTarget->GetEconomyComponent();
		Context.TargetInventoryComponent = EffectiveTarget->GetInventoryComponent();
		Context.TargetShopComponent = EffectiveTarget->GetShopComponent();
		Context.TargetDice = EffectiveTarget->GetDiceStates();
	}

	return Context;
}

void UGambitRoundEffectPipeline::CommitEffectContext(
	const FGambitEffectExecutionContext& Context,
	const FGambitRoundEffectCommitRequest& CommitRequest) const
{
	if (CommitRequest.MatchRandomStream)
	{
		*CommitRequest.MatchRandomStream = Context.RandomStream;
	}

	if (Context.SourcePlayer)
	{
		Context.SourcePlayer->AppendDebugEffectEvents(Context.DebugEffectEvents);
		Context.SourcePlayer->AppendDebugScoreLines(Context.DebugScoreLines);
		Context.SourcePlayer->AppendDebugGoldLines(Context.DebugGoldLines);
		Context.SourcePlayer->AppendDebugShopLines(Context.DebugShopLines);
	}

	if (Context.TargetPlayer && Context.TargetPlayer != Context.SourcePlayer)
	{
		Context.TargetPlayer->AppendDebugEffectEvents(Context.DebugEffectEvents);
		Context.TargetPlayer->AppendDebugScoreLines(Context.DebugScoreLines);
		Context.TargetPlayer->AppendDebugGoldLines(Context.DebugGoldLines);
		Context.TargetPlayer->AppendDebugShopLines(Context.DebugShopLines);
	}

	if (Context.SourcePlayer && Context.RerollLimitDelta != 0 && CommitRequest.RerollLimitDeltaByPlayer)
	{
		int32& RerollDelta = CommitRequest.RerollLimitDeltaByPlayer->FindOrAdd(Context.SourcePlayer);
		RerollDelta += Context.RerollLimitDelta;
	}

	if (Context.SourcePlayer && !FGambitScoreModifierMath::IsNeutral(Context.TemporaryScoreModifierDelta))
	{
		Context.SourcePlayer->ApplyTemporaryScoreModifier(Context.TemporaryScoreModifierDelta);
	}

	if (Context.TargetPlayer && Context.TargetPlayer != Context.SourcePlayer
		&& !FGambitScoreModifierMath::IsNeutral(Context.TargetTemporaryScoreModifierDelta))
	{
		Context.TargetPlayer->ApplyTemporaryScoreModifier(Context.TargetTemporaryScoreModifierDelta);
	}
}

int32 UGambitRoundEffectPipeline::ExecuteActiveSourceEffects(FGambitEffectExecutionContext& Context) const
{
	int32 TriggeredCount = ExecuteActiveModuleEffects(Context);
	TriggeredCount += ExecuteActiveDiceEffects(Context);
	return TriggeredCount;
}

int32 UGambitRoundEffectPipeline::ExecuteActiveModuleEffects(FGambitEffectExecutionContext& Context) const
{
	int32 TriggeredCount = 0;
	if (!Context.SourcePlayer)
	{
		return TriggeredCount;
	}

	for (const TObjectPtr<UGambitModuleDefinition>& ModuleDefinition : Context.SourcePlayer->GetActiveModulesRef())
	{
		TriggeredCount += ExecuteItemEffects(ModuleDefinition.Get(), Context);
	}

	return TriggeredCount;
}

int32 UGambitRoundEffectPipeline::ExecuteActiveDiceEffects(FGambitEffectExecutionContext& Context) const
{
	int32 TriggeredCount = 0;
	if (!Context.SourcePlayer || !EffectResolver)
	{
		return TriggeredCount;
	}

	const TArray<FGambitDieRuntimeState> DiceSnapshot = Context.SourcePlayer->GetDiceStates();
	for (const FGambitDieRuntimeState& DieState : DiceSnapshot)
	{
		UGambitDiceDefinition* DiceDefinition = DieState.DiceDefinition.Get();
		if (!DiceDefinition)
		{
			continue;
		}

		Context.SourceDiceDefinition = DiceDefinition;
		Context.SourceDieInstanceId = DieState.InstanceId;
		Context.SourceDieHandIndex = DieState.HandIndex;
		TriggeredCount += EffectResolver->ExecuteDiceEffects(DiceDefinition, Context);
	}

	Context.SourceDiceDefinition = nullptr;
	Context.SourceDieInstanceId = INDEX_NONE;
	Context.SourceDieHandIndex = INDEX_NONE;
	return TriggeredCount;
}

int32 UGambitRoundEffectPipeline::ExecuteItemEffects(
	UGambitItemDefinition* ItemDefinition,
	FGambitEffectExecutionContext& Context) const
{
	return EffectResolver ? EffectResolver->ExecuteItemEffects(ItemDefinition, Context) : 0;
}

FGambitRoundScoreModifierEffectResult UGambitRoundEffectPipeline::BuildScoreModifierFromEffects(
	const FGambitRoundScoreModifierEffectRequest& Request) const
{
	FGambitRoundScoreModifierEffectResult Result;
	FGambitScoreModifierContext Modifier = Request.ContextRequest.SourcePlayer
		? Request.ContextRequest.SourcePlayer->GetTemporaryScoreModifier()
		: FGambitScoreModifierContext();
	Modifier = FGambitScoreModifierMath::Normalize(Modifier);

	FGambitEffectExecutionContext Context = MakeEffectContext(Request.ContextRequest);
	Context.CurrentCombinationResult = Request.CombinationResult;
	Context.CurrentScoreModifier = Modifier;
	Result.TriggeredEffectCount = ExecuteActiveSourceEffects(Context);
	CommitEffectContext(Context, Request.CommitRequest);

	Result.ScoreModifier = FGambitScoreModifierMath::Merge(Modifier, Context.ScoreModifierDelta);
	Result.ExecutionContext = Context;
	return Result;
}
