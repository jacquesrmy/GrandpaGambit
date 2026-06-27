#pragma once

#include "CoreMinimal.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "GambitInventoryItemInstance.generated.h"

class UGambitDiceDefinition;
class UGambitItemDefinition;

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitInventoryItemInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Inventory")
	FGuid InstanceId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Inventory")
	TObjectPtr<UGambitItemDefinition> ItemDefinition = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Inventory")
	TObjectPtr<UGambitDiceDefinition> DiceDefinition = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Inventory")
	EGambitItemType ItemType = EGambitItemType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Inventory")
	FName SourceStableId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Inventory")
	int32 StackCount = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Inventory")
	bool bEquipped = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Inventory")
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Inventory")
	int32 AcquisitionRound = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Inventory")
	FName SourcePurchaseId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Inventory")
	FName SourceEffectId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Inventory")
	TArray<FName> RuntimeTags;

	bool IsValid() const
	{
		return InstanceId.IsValid() && (ItemDefinition != nullptr || DiceDefinition != nullptr);
	}
};
