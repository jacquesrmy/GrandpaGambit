#pragma once

#include "CoreMinimal.h"

namespace GambitEffectTargetRules
{
	extern GRANDPAGAMBIT_API const FName SelectedDie;
	extern GRANDPAGAMBIT_API const FName SourceSelectedDie;
	extern GRANDPAGAMBIT_API const FName TargetSelectedDie;
	extern GRANDPAGAMBIT_API const FName FirstRerolledDie;
	extern GRANDPAGAMBIT_API const FName FirstRerolledDieThisRound;
	extern GRANDPAGAMBIT_API const FName TargetOpponent;

	GRANDPAGAMBIT_API bool IsKnownRule(FName TargetRuleId);
	GRANDPAGAMBIT_API bool IsSelectedDieRule(FName TargetRuleId);
	GRANDPAGAMBIT_API bool IsFirstRerolledDieRule(FName TargetRuleId);
	GRANDPAGAMBIT_API bool IsOpponentRule(FName TargetRuleId);
	GRANDPAGAMBIT_API bool RequiresSelectedDie(FName TargetRuleId);
	GRANDPAGAMBIT_API FString DescribeRule(FName TargetRuleId);
}
