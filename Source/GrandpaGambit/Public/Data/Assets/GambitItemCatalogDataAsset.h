#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "GambitItemCatalogDataAsset.generated.h"

UCLASS(BlueprintType)
class GRANDPAGAMBIT_API UGambitItemCatalogDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Catalog")
	FName CatalogId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Catalog|Modules")
	TArray<FGambitItemCatalogEntry> ModuleEntries;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Catalog|Consumables")
	TArray<FGambitItemCatalogEntry> ConsumableEntries;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Catalog|Dice Items")
	TArray<FGambitItemCatalogEntry> DiceItemEntries;
};
