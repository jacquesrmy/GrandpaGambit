#pragma once

#include "CoreMinimal.h"

namespace GambitEffectTargetRules
{
	enum class ERuleCategory : uint8
	{
		None,
		PlayerTarget,
		DieTarget,
		MultiTarget
	};

	struct GRANDPAGAMBIT_API FRuleMetadata
	{
		FName Id;
		FString Description;
		ERuleCategory Category = ERuleCategory::None;
		bool bCanReturnMultipleTargets = false;
		bool bRequiresExplicitTargetPlayer = false;
		bool bRequiresDice = false;
	};

	extern GRANDPAGAMBIT_API const FName SelectedDie;
	extern GRANDPAGAMBIT_API const FName SourceSelectedDie;
	extern GRANDPAGAMBIT_API const FName TargetSelectedDie;
	extern GRANDPAGAMBIT_API const FName FirstRerolledDie;
	extern GRANDPAGAMBIT_API const FName FirstRerolledDieThisRound;
	extern GRANDPAGAMBIT_API const FName TargetOpponent;
	extern GRANDPAGAMBIT_API const FName AllOpponents;
	extern GRANDPAGAMBIT_API const FName LeadingPlayer;
	extern GRANDPAGAMBIT_API const FName RichestPlayer;
	extern GRANDPAGAMBIT_API const FName PoorestPlayer;
	extern GRANDPAGAMBIT_API const FName LowestScorePlayer;
	extern GRANDPAGAMBIT_API const FName LeftOpponent;
	extern GRANDPAGAMBIT_API const FName RightOpponent;
	extern GRANDPAGAMBIT_API const FName OppositePlayer;
	extern GRANDPAGAMBIT_API const FName RandomOpponent;
	extern GRANDPAGAMBIT_API const FName SourceRandomDie;
	extern GRANDPAGAMBIT_API const FName SourceBestDie;
	extern GRANDPAGAMBIT_API const FName SourceLowestDie;
	extern GRANDPAGAMBIT_API const FName SourceAllDice;
	extern GRANDPAGAMBIT_API const FName TargetRandomDie;
	extern GRANDPAGAMBIT_API const FName TargetBestDie;
	extern GRANDPAGAMBIT_API const FName TargetLowestDie;
	extern GRANDPAGAMBIT_API const FName TargetAllDice;

	GRANDPAGAMBIT_API TArray<FName> GetKnownRuleIds();
	GRANDPAGAMBIT_API TArray<FName> GetAuthorableRuleIds();
	GRANDPAGAMBIT_API TArray<FRuleMetadata> GetKnownRuleMetadata();
	GRANDPAGAMBIT_API bool GetRuleMetadata(FName TargetRuleId, FRuleMetadata& OutMetadata);
	GRANDPAGAMBIT_API bool IsKnownRule(FName TargetRuleId);
	GRANDPAGAMBIT_API bool IsSelectedDieRule(FName TargetRuleId);
	GRANDPAGAMBIT_API bool IsFirstRerolledDieRule(FName TargetRuleId);
	GRANDPAGAMBIT_API bool IsOpponentRule(FName TargetRuleId);
	GRANDPAGAMBIT_API bool IsPlayerRule(FName TargetRuleId);
	GRANDPAGAMBIT_API bool IsDieRule(FName TargetRuleId);
	GRANDPAGAMBIT_API bool IsMultiTargetRule(FName TargetRuleId);
	GRANDPAGAMBIT_API bool RequiresExplicitTargetPlayer(FName TargetRuleId);
	GRANDPAGAMBIT_API bool RequiresDice(FName TargetRuleId);
	GRANDPAGAMBIT_API bool RequiresSelectedDie(FName TargetRuleId);
	GRANDPAGAMBIT_API FString DescribeRule(FName TargetRuleId);
}
