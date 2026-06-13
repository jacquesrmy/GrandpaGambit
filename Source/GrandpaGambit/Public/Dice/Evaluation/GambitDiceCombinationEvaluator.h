#pragma once

#include "CoreMinimal.h"
#include "Core/Interfaces/GambitDiceEvaluatorContract.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "GambitDiceCombinationEvaluator.generated.h"

UCLASS(BlueprintType)
class GRANDPAGAMBIT_API UGambitDiceCombinationEvaluator : public UGambitDiceEvaluatorContract
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Gambit|Scoring")
	virtual FGambitDiceCombinationResult EvaluateDice(const TArray<FGambitDieRuntimeState>& DiceStates) const override;
};
