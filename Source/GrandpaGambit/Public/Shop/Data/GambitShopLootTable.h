#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/Types/GambitGameplayTypes.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif
#include "GambitShopLootTable.generated.h"

class UGambitItemCatalogDataAsset;

UCLASS(BlueprintType)
class GRANDPAGAMBIT_API UGambitShopLootTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop")
	FName LootTableId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop")
	TObjectPtr<UGambitItemCatalogDataAsset> ItemCatalog = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = "0"))
	int32 OfferCountOverride = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop")
	TArray<FGambitShopWeightedEntry> WeightedItems;
};
