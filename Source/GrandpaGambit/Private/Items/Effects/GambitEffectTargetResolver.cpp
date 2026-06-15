#include "Items/Effects/GambitEffectTargetResolver.h"

#include "Data/Assets/GambitItemEffectDefinition.h"
#include "Items/Effects/GambitEffectTargetRules.h"
#include "Players/Components/GambitDiceComponent.h"
#include "Players/Components/GambitEconomyComponent.h"
#include "Players/Components/GambitInventoryComponent.h"
#include "Players/States/GambitPlayerState.h"
#include "Shop/Components/GambitShopComponent.h"

namespace
{
	FGambitEffectTargetResolveResult MakeFailure(const FName TargetRuleId, const FString& FailureReason)
	{
		FGambitEffectTargetResolveResult Result;
		Result.TargetRuleId = TargetRuleId;
		Result.FailureReason = FailureReason;
		return Result;
	}

	EGambitEffectTarget NormalizeTargetSide(const EGambitEffectTarget TargetSide)
	{
		return TargetSide == EGambitEffectTarget::Target
			? EGambitEffectTarget::Target
			: EGambitEffectTarget::Source;
	}

	EGambitEffectTarget ResolveTargetSideForRule(const FName TargetRuleId, const EGambitEffectTarget RequestedTarget)
	{
		if (TargetRuleId == GambitEffectTargetRules::SourceSelectedDie)
		{
			return EGambitEffectTarget::Source;
		}

		if (TargetRuleId == GambitEffectTargetRules::TargetSelectedDie
			|| GambitEffectTargetRules::IsOpponentRule(TargetRuleId))
		{
			return EGambitEffectTarget::Target;
		}

		return NormalizeTargetSide(RequestedTarget);
	}

	AGambitPlayerState* ResolvePlayer(const FGambitEffectExecutionContext& Context, const EGambitEffectTarget TargetSide)
	{
		return TargetSide == EGambitEffectTarget::Target ? Context.TargetPlayer.Get() : Context.SourcePlayer.Get();
	}

	UGambitDiceComponent* ResolveDiceComponent(const FGambitEffectExecutionContext& Context, const EGambitEffectTarget TargetSide)
	{
		if (UGambitDiceComponent* DiceComponent = TargetSide == EGambitEffectTarget::Target
			? Context.TargetDiceComponent.Get()
			: Context.SourceDiceComponent.Get())
		{
			return DiceComponent;
		}

		if (AGambitPlayerState* Player = ResolvePlayer(Context, TargetSide))
		{
			return Player->GetDiceComponent();
		}

		return nullptr;
	}

	UGambitEconomyComponent* ResolveEconomyComponent(const FGambitEffectExecutionContext& Context, const EGambitEffectTarget TargetSide)
	{
		if (UGambitEconomyComponent* EconomyComponent = TargetSide == EGambitEffectTarget::Target
			? Context.TargetEconomyComponent.Get()
			: Context.SourceEconomyComponent.Get())
		{
			return EconomyComponent;
		}

		if (AGambitPlayerState* Player = ResolvePlayer(Context, TargetSide))
		{
			return Player->GetEconomyComponent();
		}

		return nullptr;
	}

	UGambitInventoryComponent* ResolveInventoryComponent(const FGambitEffectExecutionContext& Context, const EGambitEffectTarget TargetSide)
	{
		if (UGambitInventoryComponent* InventoryComponent = TargetSide == EGambitEffectTarget::Target
			? Context.TargetInventoryComponent.Get()
			: Context.SourceInventoryComponent.Get())
		{
			return InventoryComponent;
		}

		if (AGambitPlayerState* Player = ResolvePlayer(Context, TargetSide))
		{
			return Player->GetInventoryComponent();
		}

		return nullptr;
	}

	UGambitShopComponent* ResolveShopComponent(const FGambitEffectExecutionContext& Context, const EGambitEffectTarget TargetSide)
	{
		if (UGambitShopComponent* ShopComponent = TargetSide == EGambitEffectTarget::Target
			? Context.TargetShopComponent.Get()
			: Context.SourceShopComponent.Get())
		{
			return ShopComponent;
		}

		if (AGambitPlayerState* Player = ResolvePlayer(Context, TargetSide))
		{
			return Player->GetShopComponent();
		}

		return nullptr;
	}

	const TArray<FGambitDieRuntimeState>& ResolveDiceSnapshot(
		const FGambitEffectExecutionContext& Context,
		const EGambitEffectTarget TargetSide)
	{
		return TargetSide == EGambitEffectTarget::Target ? Context.TargetDice : Context.SourceDice;
	}

	float ResolveScalar(const UGambitItemEffectDefinition* EffectDefinition, const FName ParameterName, const float Fallback)
	{
		if (!EffectDefinition)
		{
			return Fallback;
		}

		if (const float* Value = EffectDefinition->ScalarParameters.Find(ParameterName))
		{
			return *Value;
		}

		return Fallback;
	}

	int32 ResolveIntScalar(const UGambitItemEffectDefinition* EffectDefinition, const FName ParameterName, const int32 Fallback)
	{
		return FMath::RoundToInt(ResolveScalar(EffectDefinition, ParameterName, static_cast<float>(Fallback)));
	}

	bool HasExplicitDieIndex(const UGambitItemEffectDefinition* EffectDefinition)
	{
		if (!EffectDefinition)
		{
			return false;
		}

		return EffectDefinition->ScalarParameters.Contains(TEXT("DieIndex"))
			|| EffectDefinition->DieIndex != INDEX_NONE;
	}

	bool RequiresDiceIndexes(const UGambitItemEffectDefinition* EffectDefinition)
	{
		return EffectDefinition
			&& (EffectDefinition->bAffectAllDice
				|| HasExplicitDieIndex(EffectDefinition)
				|| GambitEffectTargetRules::IsSelectedDieRule(EffectDefinition->TargetRuleId)
				|| GambitEffectTargetRules::IsFirstRerolledDieRule(EffectDefinition->TargetRuleId));
	}

	bool BuildAffectedDieIndexes(
		const UGambitItemEffectDefinition* EffectDefinition,
		const FGambitEffectExecutionContext& Context,
		const EGambitEffectTarget TargetSide,
		const TArray<FGambitDieRuntimeState>& DiceStates,
		TArray<int32>& OutIndexes,
		FString& OutFailureReason)
	{
		if (DiceStates.Num() == 0)
		{
			if (RequiresDiceIndexes(EffectDefinition))
			{
				OutFailureReason = TEXT("No dice states available for target rule.");
				return false;
			}

			return true;
		}

		if (EffectDefinition && EffectDefinition->bAffectAllDice)
		{
			OutIndexes.Reserve(DiceStates.Num());
			for (int32 Index = 0; Index < DiceStates.Num(); ++Index)
			{
				OutIndexes.Add(Index);
			}
			return true;
		}

		if (EffectDefinition && GambitEffectTargetRules::IsSelectedDieRule(EffectDefinition->TargetRuleId))
		{
			const int32 SelectedDieIndex = GambitEffectTargetResolver::ResolveSelectedDieHandIndex(Context, TargetSide, EffectDefinition->TargetRuleId);
			if (!DiceStates.IsValidIndex(SelectedDieIndex))
			{
				OutFailureReason = FString::Printf(TEXT("Selected die index %d is invalid."), SelectedDieIndex);
				return false;
			}

			OutIndexes.Add(SelectedDieIndex);
			return true;
		}

		if (EffectDefinition && GambitEffectTargetRules::IsFirstRerolledDieRule(EffectDefinition->TargetRuleId))
		{
			const int32 FirstRerolledDieIndex = Context.FirstRerolledDieHandIndexThisRound;
			if (!DiceStates.IsValidIndex(FirstRerolledDieIndex))
			{
				OutFailureReason = FString::Printf(TEXT("First rerolled die index %d is invalid."), FirstRerolledDieIndex);
				return false;
			}

			OutIndexes.Add(FirstRerolledDieIndex);
			return true;
		}

		const int32 RequestedIndex = ResolveIntScalar(EffectDefinition, TEXT("DieIndex"), EffectDefinition ? EffectDefinition->DieIndex : INDEX_NONE);
		if (DiceStates.IsValidIndex(RequestedIndex))
		{
			OutIndexes.Add(RequestedIndex);
			return true;
		}

		if (TargetSide == EGambitEffectTarget::Source && DiceStates.IsValidIndex(Context.SourceDieHandIndex))
		{
			OutIndexes.Add(Context.SourceDieHandIndex);
			return true;
		}

		OutIndexes.Add(0);
		return true;
	}

	bool ShouldResolveDiceIndexes(const UGambitItemEffectDefinition* EffectDefinition, const TArray<FGambitDieRuntimeState>& DiceStates)
	{
		return DiceStates.Num() > 0 || RequiresDiceIndexes(EffectDefinition);
	}

	FGambitEffectTargetResolveResult ResolveSingleTarget(
		const UGambitItemEffectDefinition* EffectDefinition,
		const FGambitEffectExecutionContext& Context,
		const EGambitEffectTarget RequestedTarget)
	{
		if (!EffectDefinition)
		{
			return MakeFailure(NAME_None, TEXT("EffectDefinition is null."));
		}

		const FName TargetRuleId = EffectDefinition->TargetRuleId;
		if (!TargetRuleId.IsNone() && !GambitEffectTargetRules::IsKnownRule(TargetRuleId))
		{
			return MakeFailure(
				TargetRuleId,
				FString::Printf(TEXT("Unknown TargetRuleId '%s'."), *TargetRuleId.ToString()));
		}

		const EGambitEffectTarget TargetSide = ResolveTargetSideForRule(TargetRuleId, RequestedTarget);
		if (GambitEffectTargetRules::IsOpponentRule(TargetRuleId))
		{
			AGambitPlayerState* SourcePlayer = Context.SourcePlayer.Get();
			AGambitPlayerState* TargetPlayer = Context.TargetPlayer.Get();
			if (!TargetPlayer || TargetPlayer == SourcePlayer)
			{
				return MakeFailure(
					TargetRuleId,
					TEXT("target.opponent requires a valid TargetPlayer different from SourcePlayer."));
			}
		}

		FGambitResolvedEffectTarget ResolvedTarget;
		ResolvedTarget.TargetSide = TargetSide;
		ResolvedTarget.Player = ResolvePlayer(Context, TargetSide);
		ResolvedTarget.DiceComponent = ResolveDiceComponent(Context, TargetSide);
		ResolvedTarget.EconomyComponent = ResolveEconomyComponent(Context, TargetSide);
		ResolvedTarget.InventoryComponent = ResolveInventoryComponent(Context, TargetSide);
		ResolvedTarget.ShopComponent = ResolveShopComponent(Context, TargetSide);
		ResolvedTarget.TargetRuleId = TargetRuleId;

		const TArray<FGambitDieRuntimeState>& DiceStates = ResolveDiceSnapshot(Context, TargetSide);
		if (ShouldResolveDiceIndexes(EffectDefinition, DiceStates))
		{
			FString FailureReason;
			if (!BuildAffectedDieIndexes(EffectDefinition, Context, TargetSide, DiceStates, ResolvedTarget.DiceHandIndexes, FailureReason))
			{
				return MakeFailure(TargetRuleId, FailureReason);
			}
		}

		FGambitEffectTargetResolveResult Result;
		Result.bSuccess = true;
		Result.TargetRuleId = TargetRuleId;
		Result.Targets.Add(ResolvedTarget);
		return Result;
	}

	void AppendResolvedTargets(
		FGambitEffectTargetResolveResult& AggregateResult,
		const FGambitEffectTargetResolveResult& SingleResult)
	{
		if (!SingleResult.bSuccess)
		{
			if (AggregateResult.FailureReason.IsEmpty())
			{
				AggregateResult.FailureReason = SingleResult.FailureReason;
			}
			return;
		}

		AggregateResult.Targets.Append(SingleResult.Targets);
		AggregateResult.bSuccess = AggregateResult.Targets.Num() > 0;
	}
}

namespace GambitEffectTargetResolver
{
	FGambitEffectTargetResolveResult ResolveEffectTargets(
		const UGambitItemEffectDefinition* EffectDefinition,
		const FGambitEffectExecutionContext& Context)
	{
		if (!EffectDefinition)
		{
			return MakeFailure(NAME_None, TEXT("EffectDefinition is null."));
		}

		FGambitEffectTargetResolveResult Result;
		Result.TargetRuleId = EffectDefinition->TargetRuleId;

		if (EffectDefinition->Target == EGambitEffectTarget::SourceAndTarget)
		{
			AppendResolvedTargets(Result, ResolveSingleTarget(EffectDefinition, Context, EGambitEffectTarget::Source));
			if (Context.TargetPlayer && Context.TargetPlayer != Context.SourcePlayer)
			{
				AppendResolvedTargets(Result, ResolveSingleTarget(EffectDefinition, Context, EGambitEffectTarget::Target));
			}

			if (!Result.bSuccess && Result.FailureReason.IsEmpty())
			{
				Result.FailureReason = TEXT("SourceAndTarget resolved no valid target.");
			}

			return Result;
		}

		return ResolveSingleTarget(EffectDefinition, Context, EffectDefinition->Target);
	}

	FGambitEffectTargetResolveResult ResolveEffectTarget(
		const UGambitItemEffectDefinition* EffectDefinition,
		const FGambitEffectExecutionContext& Context,
		const EGambitEffectTarget RequestedTarget)
	{
		return ResolveSingleTarget(EffectDefinition, Context, RequestedTarget);
	}

	int32 ResolveSelectedDieHandIndex(
		const FGambitEffectExecutionContext& Context,
		const EGambitEffectTarget RequestedTarget,
		const FName TargetRuleId)
	{
		if (TargetRuleId == GambitEffectTargetRules::SourceSelectedDie)
		{
			return Context.SourceDieHandIndex;
		}

		if (TargetRuleId == GambitEffectTargetRules::TargetSelectedDie)
		{
			return Context.TargetDieHandIndex;
		}

		return NormalizeTargetSide(RequestedTarget) == EGambitEffectTarget::Target
			? Context.TargetDieHandIndex
			: Context.SourceDieHandIndex;
	}
}
