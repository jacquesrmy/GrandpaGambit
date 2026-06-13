#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "GambitDiceCatalogDataAsset.generated.h"

UCLASS(BlueprintType)
class GRANDPAGAMBIT_API UGambitDiceCatalogDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Catalog")
	FName CatalogId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Catalog")
	TArray<FGambitDiceCatalogEntry> DiceEntries;
};
