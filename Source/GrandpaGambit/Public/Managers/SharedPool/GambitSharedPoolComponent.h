#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "GambitSharedPoolComponent.generated.h"

class UGambitItemDefinition;
class UGambitSharedPoolDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGambitSharedPoolChanged);

UCLASS(ClassGroup = (Gambit), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class GRANDPAGAMBIT_API UGambitSharedPoolComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGambitSharedPoolComponent();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Shared Pool")
	void InitializeSharedStock(const TArray<FGambitSharedStockEntry>& InitialStockEntries);

	UFUNCTION(BlueprintPure, Category = "Gambit|Shared Pool")
	bool IsItemAvailable(const UGambitItemDefinition* ItemDefinition) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Shared Pool")
	FGambitSharedPoolAvailabilityResult QueryItemAvailability(const UGambitItemDefinition* ItemDefinition) const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Shared Pool")
	bool ConsumeItemStock(const UGambitItemDefinition* ItemDefinition);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Shared Pool")
	bool TryReserveItemStock(
		const UGambitItemDefinition* ItemDefinition,
		UObject* ReservationOwner,
		FGuid& OutReservationId,
		FGambitSharedPoolAvailabilityResult& OutAvailability);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Shared Pool")
	bool CommitReservedItemStock(const UGambitItemDefinition* ItemDefinition, FGuid ReservationId);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Shared Pool")
	void ReleaseReservedItemStock(const UGambitItemDefinition* ItemDefinition, FGuid ReservationId);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Shared Pool")
	void ReleaseItemStock(const UGambitItemDefinition* ItemDefinition);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Shared Pool")
	void AddDynamicStock(const UGambitItemDefinition* ItemDefinition, int32 StockDelta);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Shared Pool")
	void SetPurchaseLimit(const UGambitItemDefinition* ItemDefinition, int32 PurchaseLimit);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Shared Pool")
	void SetItemUnavailable(const UGambitItemDefinition* ItemDefinition);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Shared Pool")
	void RecordGlobalPurchase(const UGambitItemDefinition* ItemDefinition);

	UFUNCTION(BlueprintPure, Category = "Gambit|Shared Pool")
	int32 GetRemainingStock(const UGambitItemDefinition* ItemDefinition) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Shared Pool")
	int32 GetReservedStock(const UGambitItemDefinition* ItemDefinition) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Shared Pool")
	bool IsItemTracked(const UGambitItemDefinition* ItemDefinition) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Shared Pool")
	int32 GetGlobalPurchaseCountForType(EGambitItemType ItemType) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Shared Pool")
	int32 GetGlobalPurchaseCountForRarity(EGambitItemRarity Rarity) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Shared Pool")
	int32 GetGlobalPurchaseCountForTypeAndRarity(EGambitItemType ItemType, EGambitItemRarity Rarity) const;

	bool IsItemIdTracked(FName ItemId) const;

	const TMap<FName, int32>& GetRemainingStockByItemIdRef() const { return RemainingStockByItemId; }

	const TMap<FName, int32>& GetMaxStockByItemIdRef() const { return MaxStockByItemId; }

	const TMap<FName, int32>& GetReservedStockByItemIdRef() const { return ReservedStockByItemId; }

	const TMap<FName, int32>& GetPurchasedCountByItemIdRef() const { return PurchasedCountByItemId; }

	UPROPERTY(BlueprintAssignable, Category = "Gambit|Shared Pool")
	FOnGambitSharedPoolChanged OnSharedPoolChanged;

private:
	void EnsureItemTracked(const UGambitItemDefinition* ItemDefinition);
	FName ResolveItemId(const UGambitItemDefinition* ItemDefinition) const;
	int32 ResolveDefinitionStock(const UGambitItemDefinition* ItemDefinition) const;
	int32 ResolveDefinitionPurchaseLimit(const UGambitItemDefinition* ItemDefinition, int32 MaxStock) const;
	FGambitSharedPoolAvailabilityResult BuildAvailabilityResult(
		const UGambitItemDefinition* ItemDefinition,
		FName ItemId,
		bool bAllowUnreservedReservation = false) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gambit|Shared Pool", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGambitSharedPoolDefinition> SharedPoolDefinition = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gambit|Shared Pool", meta = (AllowPrivateAccess = "true"))
	TArray<FGambitSharedStockEntry> DefaultSharedStock;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Shared Pool", meta = (AllowPrivateAccess = "true"))
	TMap<FName, int32> RemainingStockByItemId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Shared Pool", meta = (AllowPrivateAccess = "true"))
	TMap<FName, int32> MaxStockByItemId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Shared Pool", meta = (AllowPrivateAccess = "true"))
	TMap<FName, int32> ReservedStockByItemId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Shared Pool", meta = (AllowPrivateAccess = "true"))
	TMap<FName, int32> PurchasedCountByItemId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Shared Pool", meta = (AllowPrivateAccess = "true"))
	TMap<FName, int32> PurchaseLimitByItemId;

	TMap<FGuid, FName> ReservationItemById;

	TMap<FGuid, TWeakObjectPtr<UObject>> ReservationOwnerById;

	TMap<EGambitItemType, int32> GlobalPurchaseCountByType;

	TMap<EGambitItemRarity, int32> GlobalPurchaseCountByRarity;

	TMap<FName, int32> GlobalPurchaseCountByTypeAndRarity;
};
