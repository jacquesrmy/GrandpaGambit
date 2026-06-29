#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "GambitShopComponent.generated.h"

class UGambitEconomyComponent;
class UGambitInventoryComponent;
class UGambitItemDefinition;
class UGambitSharedPoolComponent;
class UGambitShopLootTable;
class UGambitShopOfferGenerator;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGambitShopOffersUpdated);

UCLASS(ClassGroup = (Gambit), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class GRANDPAGAMBIT_API UGambitShopComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGambitShopComponent();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Shop")
	void InitializeForMatch();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Shop")
	void RefreshOffers(FRandomStream& RandomStream, UGambitSharedPoolComponent* SharedPoolComponent);

	void RefreshOffersWithCount(FRandomStream& RandomStream, UGambitSharedPoolComponent* SharedPoolComponent, int32 OfferCountOverride);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Shop")
	bool PurchaseOffer(
		int32 OfferId,
		UGambitEconomyComponent* EconomyComponent,
		UGambitInventoryComponent* InventoryComponent,
		UGambitSharedPoolComponent* SharedPoolComponent);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Shop")
	FGambitShopPurchaseContext BuildPurchaseContext(
		int32 OfferId,
		const UGambitEconomyComponent* EconomyComponent,
		const UGambitSharedPoolComponent* SharedPoolComponent) const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Shop")
	FGambitShopPurchaseContext BuildPurchasePreviewContext(
		int32 OfferId,
		const UGambitEconomyComponent* EconomyComponent,
		const UGambitInventoryComponent* InventoryComponent,
		const UGambitSharedPoolComponent* SharedPoolComponent) const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Shop")
	void ResolvePurchasePrice(UPARAM(ref) FGambitShopPurchaseContext& PurchaseContext) const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Shop")
	bool PurchaseOfferWithContext(
		UPARAM(ref) FGambitShopPurchaseContext& PurchaseContext,
		UGambitEconomyComponent* EconomyComponent,
		UGambitInventoryComponent* InventoryComponent,
		UGambitSharedPoolComponent* SharedPoolComponent);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Shop")
	void ApplyPostPurchaseAdjustments(UPARAM(ref) FGambitShopPurchaseContext& PurchaseContext, UGambitEconomyComponent* EconomyComponent) const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Shop")
	void SkipShop(UGambitSharedPoolComponent* SharedPoolComponent);

	UFUNCTION(BlueprintPure, Category = "Gambit|Shop")
	TArray<FGambitShopOffer> GetCurrentOffers() const { return CurrentOffers; }

	const TArray<FGambitShopOffer>& GetCurrentOffersRef() const { return CurrentOffers; }

	UFUNCTION(BlueprintCallable, Category = "Gambit|Shop")
	void SetCurrentOffers(const TArray<FGambitShopOffer>& NewOffers);

	UFUNCTION(BlueprintPure, Category = "Gambit|Shop")
	int32 GetPurchasesMadeThisShop() const { return PurchasesMadeThisShop; }

	UFUNCTION(BlueprintPure, Category = "Gambit|Shop")
	int32 GetConfiguredOfferCount() const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Shop")
	void ClearOffers();

	UPROPERTY(BlueprintAssignable, Category = "Gambit|Shop")
	FOnGambitShopOffersUpdated OnShopOffersUpdated;

private:
	FGambitShopOffer* FindOfferMutable(int32 OfferId);
	const FGambitShopOffer* FindOffer(int32 OfferId) const;
	void ReleaseOfferReservations(UGambitSharedPoolComponent* SharedPoolComponent);
	bool GrantPurchasedItemToInventory(
		UGambitItemDefinition* ItemDefinition,
		UGambitInventoryComponent* InventoryComponent,
		FName SourcePurchaseId) const;
	bool ValidateInventoryCapacity(UGambitItemDefinition* ItemDefinition, const UGambitInventoryComponent* InventoryComponent, FString& OutFailureReason) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gambit|Shop", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGambitShopLootTable> ShopLootTable = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UGambitShopOfferGenerator> OfferGenerator = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Shop", meta = (AllowPrivateAccess = "true"))
	TArray<FGambitShopOffer> CurrentOffers;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Shop", meta = (AllowPrivateAccess = "true"))
	int32 PurchasesMadeThisShop = 0;

	UPROPERTY(Transient)
	TWeakObjectPtr<UGambitSharedPoolComponent> LastSharedPoolComponent;
};
