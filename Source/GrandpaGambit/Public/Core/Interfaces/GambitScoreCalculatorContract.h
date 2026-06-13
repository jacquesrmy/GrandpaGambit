#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "GambitScoreCalculatorContract.generated.h"

UCLASS(Abstract, BlueprintType, EditInlineNew)
class GRANDPAGAMBIT_API UGambitScoreCalculatorContract : public UObject
{
	GENERATED_BODY()

public:
	virtual FGambitScoreBreakdown CalculateScore(
		const FGambitDiceCombinationResult& CombinationResult,
		const FGambitScoreModifierContext& ModifierContext) const PURE_VIRTUAL(
			UGambitScoreCalculatorContract::CalculateScore,
			return FGambitScoreBreakdown(););

	virtual FGambitScoreModifierContext MergeModifiers(
		const FGambitScoreModifierContext& PersistentModifier,
		const FGambitScoreModifierContext& RoundModifier) const PURE_VIRTUAL(
			UGambitScoreCalculatorContract::MergeModifiers,
			return FGambitScoreModifierContext(););
};
