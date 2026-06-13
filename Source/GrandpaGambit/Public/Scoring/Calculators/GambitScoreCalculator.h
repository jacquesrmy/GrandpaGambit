#pragma once

#include "CoreMinimal.h"
#include "Core/Interfaces/GambitScoreCalculatorContract.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "GambitScoreCalculator.generated.h"

UCLASS(BlueprintType)
class GRANDPAGAMBIT_API UGambitScoreCalculator : public UGambitScoreCalculatorContract
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Gambit|Scoring")
	virtual FGambitScoreBreakdown CalculateScore(
		const FGambitDiceCombinationResult& CombinationResult,
		const FGambitScoreModifierContext& ModifierContext) const override;

	UFUNCTION(BlueprintPure, Category = "Gambit|Scoring")
	virtual FGambitScoreModifierContext MergeModifiers(
		const FGambitScoreModifierContext& PersistentModifier,
		const FGambitScoreModifierContext& RoundModifier) const override;
};
