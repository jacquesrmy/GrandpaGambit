#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/Types/GambitGameplayTypes.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif
#include "GambitSharedPoolDefinition.generated.h"

class UGambitItemCatalogDataAsset;

UCLASS(BlueprintType)
class GRANDPAGAMBIT_API UGambitSharedPoolDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shared Pool")
	FName SharedPoolId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shared Pool")
	TObjectPtr<UGambitItemCatalogDataAsset> ItemCatalog = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shared Pool")
	TArray<FGambitSharedStockEntry> StockEntries;
};
