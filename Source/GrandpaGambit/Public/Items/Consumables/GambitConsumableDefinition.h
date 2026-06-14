#pragma once

#include "CoreMinimal.h"
#include "Items/Data/GambitItemDefinition.h"
#include "GambitConsumableDefinition.generated.h"

UCLASS(BlueprintType)
class GRANDPAGAMBIT_API UGambitConsumableDefinition : public UGambitItemDefinition
{
	GENERATED_BODY()

public:
	UGambitConsumableDefinition();

	UFUNCTION(BlueprintPure, Category = "Gambit|Consumable")
	bool CanBeUsedDuringPhase(EGambitRoundPhase Phase) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable", meta = (ToolTip = "Player activation windows only. Effects still execute with the ConsumableUse hook; use delayed-effect state for later resolution."))
	TArray<EGambitRoundPhase> UsablePhases;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable")
	bool bCanTargetOpponent = false;
};
