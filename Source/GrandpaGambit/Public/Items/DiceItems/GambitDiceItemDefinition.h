#pragma once

#include "CoreMinimal.h"
#include "Items/Data/GambitItemDefinition.h"
#include "GambitDiceItemDefinition.generated.h"

class UGambitDiceDefinition;

UCLASS(BlueprintType)
class GRANDPAGAMBIT_API UGambitDiceItemDefinition : public UGambitItemDefinition
{
	GENERATED_BODY()

public:
	UGambitDiceItemDefinition();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dice Item")
	TObjectPtr<UGambitDiceDefinition> GrantedDiceDefinition = nullptr;
};
