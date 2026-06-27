#include "Players/Components/GambitInventoryComponent.h"

#include "Core/Settings/GambitGameBalanceSettings.h"
#include "Core/Settings/GambitStaticDataSettings.h"
#include "Data/Assets/GambitPlayerLoadoutDefinition.h"
#include "Dice/Data/GambitDiceDefinition.h"
#include "Items/Consumables/GambitConsumableDefinition.h"
#include "Items/Data/GambitItemDefinition.h"
#include "Items/DiceItems/GambitDiceItemDefinition.h"
#include "Items/Modules/GambitModuleDefinition.h"

namespace
{
	bool InventoryRarityMatches(const EGambitItemRarity ActualRarity, const EGambitItemRarity ExpectedRarity, const bool bAtLeastRarity)
	{
		if (bAtLeastRarity)
		{
			return static_cast<int32>(ActualRarity) >= static_cast<int32>(ExpectedRarity);
		}

		return ActualRarity == ExpectedRarity;
	}

}

UGambitInventoryComponent::UGambitInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGambitInventoryComponent::InitializeForMatch()
{
	OwnedItemInstances.Reset();
	OwnedDiceDefinitions.Reset();
	ActiveModules.Reset();
	ConsumableSlots.Reset();

	const UGambitPlayerLoadoutDefinition* EffectiveLoadout = DefaultLoadoutDefinition;
	if (!EffectiveLoadout)
	{
		EffectiveLoadout = UGambitStaticDataSettings::Get()->DefaultPlayerLoadout.LoadSynchronous();
	}

	if (EffectiveLoadout)
	{
		for (UGambitDiceDefinition* DiceDefinition : EffectiveLoadout->Loadout.StartingDice)
		{
			if (DiceDefinition)
			{
				AddOwnedDieDefinition(
					DiceDefinition,
					nullptr,
					DiceDefinition->GetResolvedDiceId(),
					NAME_None,
					NAME_None);
			}
		}

		for (UGambitModuleDefinition* ModuleDefinition : EffectiveLoadout->Loadout.StartingModules)
		{
			if (!ModuleDefinition || ActiveModules.Contains(ModuleDefinition))
			{
				continue;
			}

			if (ActiveModules.Num() >= GetModuleSlotCapacity())
			{
				break;
			}

			AddModuleDefinition(ModuleDefinition, NAME_None, NAME_None);
		}

		for (UGambitConsumableDefinition* ConsumableDefinition : EffectiveLoadout->Loadout.StartingConsumables)
		{
			if (!ConsumableDefinition)
			{
				continue;
			}

			if (ConsumableSlots.Num() >= GetConsumableSlotCapacity())
			{
				break;
			}

			AddConsumableDefinition(ConsumableDefinition, NAME_None, NAME_None);
		}
	}

	ResetRoundState();
}

void UGambitInventoryComponent::ResetRoundState()
{
	OnInventoryChanged.Broadcast();
}

bool UGambitInventoryComponent::AddOwnedDie(UGambitDiceDefinition* DieDefinition)
{
	if (!AddOwnedDieDefinition(
		DieDefinition,
		nullptr,
		DieDefinition ? DieDefinition->GetResolvedDiceId() : NAME_None,
		NAME_None,
		NAME_None))
	{
		return false;
	}

	OnInventoryChanged.Broadcast();
	return true;
}

bool UGambitInventoryComponent::AddModule(UGambitModuleDefinition* ModuleDefinition)
{
	if (!AddModuleDefinition(ModuleDefinition, NAME_None, NAME_None))
	{
		return false;
	}

	OnInventoryChanged.Broadcast();
	return true;
}

bool UGambitInventoryComponent::AddConsumable(UGambitConsumableDefinition* ConsumableDefinition)
{
	if (!AddConsumableDefinition(ConsumableDefinition, NAME_None, NAME_None))
	{
		return false;
	}

	OnInventoryChanged.Broadcast();
	return true;
}

bool UGambitInventoryComponent::AddItemDefinition(UGambitItemDefinition* ItemDefinition)
{
	return AddItemDefinitionWithSource(ItemDefinition, NAME_None, NAME_None);
}

bool UGambitInventoryComponent::AddItemDefinitionWithSource(
	UGambitItemDefinition* ItemDefinition,
	const FName SourcePurchaseId,
	const FName SourceEffectId)
{
	if (!ItemDefinition)
	{
		return false;
	}

	bool bAdded = false;
	if (UGambitModuleDefinition* ModuleDefinition = Cast<UGambitModuleDefinition>(ItemDefinition))
	{
		bAdded = AddModuleDefinition(ModuleDefinition, SourcePurchaseId, SourceEffectId);
	}
	else if (UGambitConsumableDefinition* ConsumableDefinition = Cast<UGambitConsumableDefinition>(ItemDefinition))
	{
		bAdded = AddConsumableDefinition(ConsumableDefinition, SourcePurchaseId, SourceEffectId);
	}
	else if (UGambitDiceItemDefinition* DiceItemDefinition = Cast<UGambitDiceItemDefinition>(ItemDefinition))
	{
		bAdded = AddDiceItemDefinition(DiceItemDefinition, SourcePurchaseId, SourceEffectId);
	}

	if (bAdded)
	{
		OnInventoryChanged.Broadcast();
	}
	return bAdded;
}

bool UGambitInventoryComponent::RemoveOwnedDieAtIndex(
	const int32 DieIndex,
	UGambitDiceDefinition*& OutRemovedDieDefinition)
{
	OutRemovedDieDefinition = nullptr;
	if (!OwnedDiceDefinitions.IsValidIndex(DieIndex))
	{
		return false;
	}

	OutRemovedDieDefinition = OwnedDiceDefinitions[DieIndex];
	OwnedDiceDefinitions.RemoveAt(DieIndex);
	RemoveDiceInventoryInstanceAtIndex(DieIndex);
	OnInventoryChanged.Broadcast();
	return true;
}

bool UGambitInventoryComponent::RemoveModule(UGambitModuleDefinition* ModuleDefinition)
{
	if (!ModuleDefinition)
	{
		return false;
	}

	const int32 RemovedCount = ActiveModules.Remove(ModuleDefinition);
	if (RemovedCount <= 0)
	{
		return false;
	}

	RemoveFirstInventoryInstanceByDefinition(ModuleDefinition);
	OnInventoryChanged.Broadcast();
	return true;
}

bool UGambitInventoryComponent::RemoveConsumableAtSlot(
	const int32 SlotIndex,
	UGambitConsumableDefinition*& OutRemovedConsumableDefinition)
{
	OutRemovedConsumableDefinition = nullptr;
	if (!ConsumableSlots.IsValidIndex(SlotIndex))
	{
		return false;
	}

	OutRemovedConsumableDefinition = ConsumableSlots[SlotIndex].Definition;
	RemoveInventoryInstanceById(ConsumableSlots[SlotIndex].InventoryInstanceId);
	ConsumableSlots.RemoveAt(SlotIndex);
	OnInventoryChanged.Broadcast();
	return OutRemovedConsumableDefinition != nullptr;
}

bool UGambitInventoryComponent::RemoveItemDefinition(UGambitItemDefinition* ItemDefinition)
{
	if (!ItemDefinition)
	{
		return false;
	}

	if (UGambitModuleDefinition* ModuleDefinition = Cast<UGambitModuleDefinition>(ItemDefinition))
	{
		return RemoveModule(ModuleDefinition);
	}

	if (UGambitConsumableDefinition* ConsumableDefinition = Cast<UGambitConsumableDefinition>(ItemDefinition))
	{
		const int32 ConsumableIndex = ConsumableSlots.IndexOfByPredicate([ConsumableDefinition](const FGambitConsumableRuntimeSlot& Slot)
		{
			return Slot.Definition == ConsumableDefinition;
		});

		UGambitConsumableDefinition* RemovedDefinition = nullptr;
		return RemoveConsumableAtSlot(ConsumableIndex, RemovedDefinition);
	}

	return false;
}

bool UGambitInventoryComponent::ConsumeConsumableDefinitionAtSlot(
	const int32 SlotIndex,
	UGambitConsumableDefinition*& OutDefinition)
{
	OutDefinition = nullptr;

	if (!ConsumableSlots.IsValidIndex(SlotIndex))
	{
		return false;
	}

	FGambitConsumableRuntimeSlot& Slot = ConsumableSlots[SlotIndex];
	if (!Slot.IsValid())
	{
		return false;
	}

	OutDefinition = Slot.Definition;
	RemoveInventoryInstanceById(Slot.InventoryInstanceId);
	ConsumableSlots.RemoveAt(SlotIndex);
	OnInventoryChanged.Broadcast();
	return true;
}

FGambitInventoryItemInstance UGambitInventoryComponent::BuildItemInstance(
	UGambitItemDefinition* ItemDefinition,
	UGambitDiceDefinition* DiceDefinition,
	const EGambitItemType ItemType,
	FName SourceStableId,
	const bool bEquipped,
	const bool bActive,
	const FName SourcePurchaseId,
	const FName SourceEffectId) const
{
	if (SourceStableId.IsNone())
	{
		if (ItemDefinition)
		{
			SourceStableId = ItemDefinition->GetResolvedItemId();
		}
		else if (DiceDefinition)
		{
			SourceStableId = DiceDefinition->GetResolvedDiceId();
		}
	}

	FGambitInventoryItemInstance Instance;
	Instance.InstanceId = FGuid::NewGuid();
	Instance.ItemDefinition = ItemDefinition;
	Instance.DiceDefinition = DiceDefinition;
	Instance.ItemType = ItemType;
	Instance.SourceStableId = SourceStableId;
	Instance.StackCount = 1;
	Instance.bEquipped = bEquipped;
	Instance.bActive = bActive;
	Instance.SourcePurchaseId = SourcePurchaseId;
	Instance.SourceEffectId = SourceEffectId;
	return Instance;
}

bool UGambitInventoryComponent::AddDiceItemDefinition(
	UGambitDiceItemDefinition* DiceItemDefinition,
	const FName SourcePurchaseId,
	const FName SourceEffectId)
{
	if (!DiceItemDefinition || !DiceItemDefinition->GrantedDiceDefinition)
	{
		return false;
	}

	return AddOwnedDieDefinition(
		DiceItemDefinition->GrantedDiceDefinition,
		DiceItemDefinition,
		DiceItemDefinition->GetResolvedItemId(),
		SourcePurchaseId,
		SourceEffectId);
}

bool UGambitInventoryComponent::AddOwnedDieDefinition(
	UGambitDiceDefinition* DieDefinition,
	UGambitItemDefinition* SourceItemDefinition,
	const FName SourceStableId,
	const FName SourcePurchaseId,
	const FName SourceEffectId)
{
	if (!DieDefinition)
	{
		return false;
	}

	OwnedDiceDefinitions.Add(DieDefinition);
	OwnedItemInstances.Add(BuildItemInstance(
		SourceItemDefinition,
		DieDefinition,
		EGambitItemType::Dice,
		SourceStableId,
		true,
		true,
		SourcePurchaseId,
		SourceEffectId));
	return true;
}

bool UGambitInventoryComponent::AddModuleDefinition(
	UGambitModuleDefinition* ModuleDefinition,
	const FName SourcePurchaseId,
	const FName SourceEffectId)
{
	if (!ModuleDefinition || ActiveModules.Contains(ModuleDefinition))
	{
		return false;
	}

	if (!HasAvailableModuleSlot())
	{
		return false;
	}

	ActiveModules.Add(ModuleDefinition);
	OwnedItemInstances.Add(BuildItemInstance(
		ModuleDefinition,
		nullptr,
		EGambitItemType::Module,
		ModuleDefinition->GetResolvedItemId(),
		true,
		true,
		SourcePurchaseId,
		SourceEffectId));
	return true;
}

bool UGambitInventoryComponent::AddConsumableDefinition(
	UGambitConsumableDefinition* ConsumableDefinition,
	const FName SourcePurchaseId,
	const FName SourceEffectId)
{
	if (!ConsumableDefinition)
	{
		return false;
	}

	if (!HasAvailableConsumableSlot())
	{
		return false;
	}

	FGambitInventoryItemInstance Instance = BuildItemInstance(
		ConsumableDefinition,
		nullptr,
		EGambitItemType::Consumable,
		ConsumableDefinition->GetResolvedItemId(),
		true,
		false,
		SourcePurchaseId,
		SourceEffectId);

	FGambitConsumableRuntimeSlot NewSlot;
	NewSlot.InventoryInstanceId = Instance.InstanceId;
	NewSlot.Definition = ConsumableDefinition;

	OwnedItemInstances.Add(Instance);
	ConsumableSlots.Add(NewSlot);
	return true;
}

bool UGambitInventoryComponent::RemoveInventoryInstanceById(const FGuid& InstanceId)
{
	if (!InstanceId.IsValid())
	{
		return false;
	}

	const int32 InstanceIndex = OwnedItemInstances.IndexOfByPredicate([&InstanceId](const FGambitInventoryItemInstance& Instance)
	{
		return Instance.InstanceId == InstanceId;
	});
	if (!OwnedItemInstances.IsValidIndex(InstanceIndex))
	{
		return false;
	}

	OwnedItemInstances.RemoveAt(InstanceIndex);
	return true;
}

bool UGambitInventoryComponent::RemoveFirstInventoryInstanceByDefinition(const UGambitItemDefinition* ItemDefinition)
{
	if (!ItemDefinition)
	{
		return false;
	}

	const int32 InstanceIndex = OwnedItemInstances.IndexOfByPredicate([ItemDefinition](const FGambitInventoryItemInstance& Instance)
	{
		return Instance.ItemDefinition == ItemDefinition;
	});
	if (!OwnedItemInstances.IsValidIndex(InstanceIndex))
	{
		return false;
	}

	OwnedItemInstances.RemoveAt(InstanceIndex);
	return true;
}

bool UGambitInventoryComponent::RemoveDiceInventoryInstanceAtIndex(const int32 DieIndex)
{
	if (DieIndex < 0)
	{
		return false;
	}

	int32 DiceInstanceIndex = 0;
	for (int32 InstanceIndex = 0; InstanceIndex < OwnedItemInstances.Num(); ++InstanceIndex)
	{
		if (OwnedItemInstances[InstanceIndex].ItemType != EGambitItemType::Dice)
		{
			continue;
		}

		if (DiceInstanceIndex == DieIndex)
		{
			OwnedItemInstances.RemoveAt(InstanceIndex);
			return true;
		}
		DiceInstanceIndex++;
	}

	return false;
}

const FGambitInventoryItemInstance* UGambitInventoryComponent::FindItemInstanceById(const FGuid& InstanceId) const
{
	if (!InstanceId.IsValid())
	{
		return nullptr;
	}

	return OwnedItemInstances.FindByPredicate([&InstanceId](const FGambitInventoryItemInstance& Instance)
	{
		return Instance.InstanceId == InstanceId;
	});
}

const FGambitInventoryItemInstance* UGambitInventoryComponent::FindFirstItemInstanceByDefinition(
	const UGambitItemDefinition* ItemDefinition) const
{
	if (!ItemDefinition)
	{
		return nullptr;
	}

	return OwnedItemInstances.FindByPredicate([ItemDefinition](const FGambitInventoryItemInstance& Instance)
	{
		return Instance.ItemDefinition == ItemDefinition;
	});
}

TArray<FGambitInventoryItemInstance> UGambitInventoryComponent::GetActiveModuleInstances() const
{
	TArray<FGambitInventoryItemInstance> ModuleInstances;
	for (const FGambitInventoryItemInstance& Instance : OwnedItemInstances)
	{
		if (Instance.ItemType == EGambitItemType::Module && Instance.bActive)
		{
			ModuleInstances.Add(Instance);
		}
	}

	return ModuleInstances;
}

TArray<FGambitInventoryItemInstance> UGambitInventoryComponent::GetConsumableInstances() const
{
	TArray<FGambitInventoryItemInstance> ConsumableInstances;
	for (const FGambitInventoryItemInstance& Instance : OwnedItemInstances)
	{
		if (Instance.ItemType == EGambitItemType::Consumable)
		{
			ConsumableInstances.Add(Instance);
		}
	}

	return ConsumableInstances;
}

bool UGambitInventoryComponent::HasItemDefinition(UGambitItemDefinition* ItemDefinition) const
{
	return FindFirstItemInstanceByDefinition(ItemDefinition) != nullptr;
}

UGambitItemDefinition* UGambitInventoryComponent::GetItemDefinitionFromInstance(
	const FGambitInventoryItemInstance& ItemInstance) const
{
	return ItemInstance.ItemDefinition.Get();
}

int32 UGambitInventoryComponent::GetDistinctOwnedDiceTypeCount() const
{
	TSet<FName> DiceIds;
	for (const UGambitDiceDefinition* DiceDefinition : OwnedDiceDefinitions)
	{
		if (!DiceDefinition)
		{
			continue;
		}

		DiceIds.Add(DiceDefinition->GetResolvedDiceId());
	}

	return DiceIds.Num();
}

int32 UGambitInventoryComponent::GetMostRepeatedOwnedDiceTypeCount() const
{
	TMap<FName, int32> CountsByDiceId;
	for (const UGambitDiceDefinition* DiceDefinition : OwnedDiceDefinitions)
	{
		if (!DiceDefinition)
		{
			continue;
		}

		int32& Count = CountsByDiceId.FindOrAdd(DiceDefinition->GetResolvedDiceId());
		Count++;
	}

	int32 HighestCount = 0;
	for (const TPair<FName, int32>& Pair : CountsByDiceId)
	{
		HighestCount = FMath::Max(HighestCount, Pair.Value);
	}

	return HighestCount;
}

int32 UGambitInventoryComponent::CountOwnedDiceByRarity(
	const EGambitItemRarity Rarity,
	const bool bAtLeastRarity) const
{
	int32 Count = 0;
	for (const UGambitDiceDefinition* DiceDefinition : OwnedDiceDefinitions)
	{
		if (DiceDefinition && InventoryRarityMatches(DiceDefinition->Rarity, Rarity, bAtLeastRarity))
		{
			Count++;
		}
	}

	return Count;
}

int32 UGambitInventoryComponent::CountActiveModulesByRarity(
	const EGambitItemRarity Rarity,
	const bool bAtLeastRarity) const
{
	int32 Count = 0;
	for (const UGambitModuleDefinition* ModuleDefinition : ActiveModules)
	{
		if (ModuleDefinition && InventoryRarityMatches(ModuleDefinition->Rarity, Rarity, bAtLeastRarity))
		{
			Count++;
		}
	}

	return Count;
}

int32 UGambitInventoryComponent::CountOwnedItemsByRarity(
	const EGambitItemRarity Rarity,
	const bool bAtLeastRarity) const
{
	int32 Count = CountOwnedDiceByRarity(Rarity, bAtLeastRarity);
	Count += CountActiveModulesByRarity(Rarity, bAtLeastRarity);

	for (const FGambitConsumableRuntimeSlot& Slot : ConsumableSlots)
	{
		if (Slot.Definition && InventoryRarityMatches(Slot.Definition->Rarity, Rarity, bAtLeastRarity))
		{
			Count++;
		}
	}

	return Count;
}

int32 UGambitInventoryComponent::GetModuleSlotCapacity() const
{
	return FMath::Max(1, UGambitGameBalanceSettings::Get()->ModuleSlotCount);
}

int32 UGambitInventoryComponent::GetConsumableSlotCapacity() const
{
	return FMath::Max(1, UGambitGameBalanceSettings::Get()->ConsumableSlotCount);
}

bool UGambitInventoryComponent::HasAvailableModuleSlot() const
{
	return ActiveModules.Num() < GetModuleSlotCapacity();
}

bool UGambitInventoryComponent::HasAvailableConsumableSlot() const
{
	return ConsumableSlots.Num() < GetConsumableSlotCapacity();
}

FGambitPlayerSlotState UGambitInventoryComponent::BuildSlotState() const
{
	FGambitPlayerSlotState SlotState;
	SlotState.OwnedDiceCount = OwnedDiceDefinitions.Num();
	SlotState.ModuleSlotsUsed = ActiveModules.Num();
	SlotState.ModuleSlotsCapacity = GetModuleSlotCapacity();
	SlotState.ConsumableSlotsUsed = ConsumableSlots.Num();
	SlotState.ConsumableSlotsCapacity = GetConsumableSlotCapacity();
	return SlotState;
}
