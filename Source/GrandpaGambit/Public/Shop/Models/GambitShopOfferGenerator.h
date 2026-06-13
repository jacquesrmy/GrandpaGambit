#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "GambitShopOfferGenerator.generated.h"

class UGambitSharedPoolComponent;
class UGambitShopLootTable;

UCLASS(BlueprintType)
class GRANDPAGAMBIT_API UGambitShopOfferGenerator : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Gambit|Shop")
	void GenerateOffers(
		const UGambitShopLootTable* LootTable,
		int32 OfferCount,
		FRandomStream& RandomStream,
		UGambitSharedPoolComponent* SharedPoolComponent,
		UObject* ReservationOwner,
		TArray<FGambitShopOffer>& OutOffers) const;
};
