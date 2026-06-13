#include "Items/Effects/GambitItemEffect.h"

bool UGambitItemEffect::CanExecute_Implementation(const FGambitEffectExecutionContext& Context) const
{
	return Context.Hook == Hook;
}

bool UGambitItemEffect::ExecuteEffect_Implementation(FGambitEffectExecutionContext& Context) const
{
	if (Context.Hook == EGambitEffectHook::ScoreModifier)
	{
		ApplyToScoreModifier(Context.ScoreModifierDelta);
		return true;
	}

	if (Context.Hook == EGambitEffectHook::PostScoreCalculation)
	{
		ApplyToScoreBreakdown(Context.CurrentScoreBreakdown);
		return true;
	}

	return false;
}

void UGambitItemEffect::ApplyToScoreModifier_Implementation(FGambitScoreModifierContext& InOutModifier) const
{
}

void UGambitItemEffect::ApplyToScoreBreakdown_Implementation(FGambitScoreBreakdown& InOutBreakdown) const
{
}
