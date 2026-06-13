#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/Types/GambitGameplayTypes.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif
#include "GambitItemDefinition.generated.h"

class UGambitItemEffect;
class UGambitItemEffectDefinition;

UCLASS(BlueprintType)
class GRANDPAGAMBIT_API UGambitItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UFUNCTION(BlueprintPure, Category = "Gambit|Item")
	FName GetStableItemId() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Item")
	FName GetResolvedItemId() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FName ItemId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	EGambitItemType ItemType = EGambitItemType::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FName ItemTypeId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	EGambitItemRarity Rarity = EGambitItemRarity::Common;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FName RarityId = TEXT("rarity.common");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "0"))
	int32 Cost = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TArray<FName> Tags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	bool bUsesSharedPool = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (EditCondition = "bUsesSharedPool", ClampMin = "0"))
	int32 SharedPoolMaxStock = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (EditCondition = "bUsesSharedPool", ClampMin = "0"))
	int32 SharedPoolPurchaseLimit = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	TArray<TSubclassOf<UGambitItemEffect>> EffectClasses;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	TArray<TObjectPtr<UGambitItemEffectDefinition>> EffectDefinitions;
};
