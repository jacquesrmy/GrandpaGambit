#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "GambitInventoryComponent.generated.h"

class UGambitConsumableDefinition;
class UGambitDiceDefinition;
class UGambitItemDefinition;
class UGambitModuleDefinition;
class UGambitPlayerLoadoutDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGambitInventoryChanged);

UCLASS(ClassGroup = (Gambit), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class GRANDPAGAMBIT_API UGambitInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGambitInventoryComponent();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Inventory")
	void InitializeForMatch();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Inventory")
	void ResetRoundState();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Inventory")
	bool AddOwnedDie(UGambitDiceDefinition* DieDefinition);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Inventory")
	bool AddModule(UGambitModuleDefinition* ModuleDefinition);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Inventory")
	bool AddConsumable(UGambitConsumableDefinition* ConsumableDefinition);

	bool RemoveOwnedDieAtIndex(int32 DieIndex, UGambitDiceDefinition*& OutRemovedDieDefinition);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Inventory")
	bool RemoveModule(UGambitModuleDefinition* ModuleDefinition);

	bool RemoveConsumableAtSlot(int32 SlotIndex, UGambitConsumableDefinition*& OutRemovedConsumableDefinition);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Inventory")
	bool RemoveItemDefinition(UGambitItemDefinition* ItemDefinition);

	bool ConsumeConsumableDefinitionAtSlot(int32 SlotIndex, UGambitConsumableDefinition*& OutDefinition);

	UFUNCTION(BlueprintPure, Category = "Gambit|Inventory")
	int32 GetOwnedDiceCount() const { return OwnedDiceDefinitions.Num(); }

	UFUNCTION(BlueprintPure, Category = "Gambit|Inventory")
	int32 GetDistinctOwnedDiceTypeCount() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Inventory")
	int32 GetMostRepeatedOwnedDiceTypeCount() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Inventory")
	int32 CountOwnedDiceByRarity(EGambitItemRarity Rarity, bool bAtLeastRarity = false) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Inventory")
	int32 CountActiveModulesByRarity(EGambitItemRarity Rarity, bool bAtLeastRarity = false) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Inventory")
	int32 CountOwnedItemsByRarity(EGambitItemRarity Rarity, bool bAtLeastRarity = false) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Inventory")
	int32 GetModuleSlotsUsed() const { return ActiveModules.Num(); }

	UFUNCTION(BlueprintPure, Category = "Gambit|Inventory")
	int32 GetModuleSlotCapacity() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Inventory")
	int32 GetConsumableSlotsUsed() const { return ConsumableSlots.Num(); }

	UFUNCTION(BlueprintPure, Category = "Gambit|Inventory")
	int32 GetConsumableSlotCapacity() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Inventory")
	bool HasAvailableModuleSlot() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Inventory")
	bool HasAvailableConsumableSlot() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Inventory")
	FGambitPlayerSlotState BuildSlotState() const;

	const TArray<TObjectPtr<UGambitDiceDefinition>>& GetOwnedDiceDefinitions() const { return OwnedDiceDefinitions; }

	const TArray<TObjectPtr<UGambitModuleDefinition>>& GetActiveModules() const { return ActiveModules; }

	UFUNCTION(BlueprintPure, Category = "Gambit|Inventory")
	TArray<FGambitConsumableRuntimeSlot> GetConsumableSlots() const { return ConsumableSlots; }

	const TArray<FGambitConsumableRuntimeSlot>& GetConsumableSlotsRef() const { return ConsumableSlots; }

	UPROPERTY(BlueprintAssignable, Category = "Gambit|Inventory")
	FOnGambitInventoryChanged OnInventoryChanged;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gambit|Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGambitPlayerLoadoutDefinition> DefaultLoadoutDefinition = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Inventory", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UGambitDiceDefinition>> OwnedDiceDefinitions;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Inventory", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UGambitModuleDefinition>> ActiveModules;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Inventory", meta = (AllowPrivateAccess = "true"))
	TArray<FGambitConsumableRuntimeSlot> ConsumableSlots;
};
