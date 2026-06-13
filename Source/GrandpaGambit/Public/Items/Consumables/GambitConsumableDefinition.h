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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable")
	FGambitScoreModifierContext ActionScoreModifier;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable")
	bool bCanTargetOpponent = false;
};
