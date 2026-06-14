#include "Items/Effects/GambitEffectTargetRules.h"

namespace GambitEffectTargetRules
{
	const FName SelectedDie(TEXT("selected_die"));
	const FName SourceSelectedDie(TEXT("source.selected_die"));
	const FName TargetSelectedDie(TEXT("target.selected_die"));
	const FName FirstRerolledDie(TEXT("first_rerolled_die"));
	const FName FirstRerolledDieThisRound(TEXT("first_rerolled_die_this_round"));
	const FName TargetOpponent(TEXT("target.opponent"));

	bool IsKnownRule(const FName TargetRuleId)
	{
		return IsSelectedDieRule(TargetRuleId)
			|| IsFirstRerolledDieRule(TargetRuleId)
			|| IsOpponentRule(TargetRuleId);
	}

	bool IsSelectedDieRule(const FName TargetRuleId)
	{
		return TargetRuleId == SelectedDie
			|| TargetRuleId == SourceSelectedDie
			|| TargetRuleId == TargetSelectedDie;
	}

	bool IsFirstRerolledDieRule(const FName TargetRuleId)
	{
		return TargetRuleId == FirstRerolledDie
			|| TargetRuleId == FirstRerolledDieThisRound;
	}

	bool IsOpponentRule(const FName TargetRuleId)
	{
		return TargetRuleId == TargetOpponent;
	}

	bool RequiresSelectedDie(const FName TargetRuleId)
	{
		return IsSelectedDieRule(TargetRuleId);
	}

	FString DescribeRule(const FName TargetRuleId)
	{
		if (TargetRuleId.IsNone())
		{
			return TEXT("No target rule");
		}

		if (TargetRuleId == SelectedDie)
		{
			return TEXT("Selected die");
		}

		if (TargetRuleId == SourceSelectedDie)
		{
			return TEXT("Source selected die");
		}

		if (TargetRuleId == TargetSelectedDie)
		{
			return TEXT("Target selected die");
		}

		if (TargetRuleId == FirstRerolledDie)
		{
			return TEXT("First rerolled die");
		}

		if (TargetRuleId == FirstRerolledDieThisRound)
		{
			return TEXT("First rerolled die this round");
		}

		if (TargetRuleId == TargetOpponent)
		{
			return TEXT("Target opponent");
		}

		return FString::Printf(TEXT("Unknown target rule '%s'"), *TargetRuleId.ToString());
	}
}
