#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "GambitPlayerLoadoutDefinition.generated.h"

UCLASS(BlueprintType)
class GRANDPAGAMBIT_API UGambitPlayerLoadoutDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout")
	FName LoadoutId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout")
	FGambitPlayerLoadout Loadout;
};
