#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "GambitDiceEvaluatorContract.generated.h"

UCLASS(Abstract, BlueprintType, EditInlineNew)
class GRANDPAGAMBIT_API UGambitDiceEvaluatorContract : public UObject
{
	GENERATED_BODY()

public:
	virtual FGambitDiceCombinationResult EvaluateDice(const TArray<FGambitDieRuntimeState>& DiceStates) const PURE_VIRTUAL(
		UGambitDiceEvaluatorContract::EvaluateDice,
		return FGambitDiceCombinationResult(););
};
