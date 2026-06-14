#include "Players/Components/GambitInventoryComponent.h"

#include "Core/Settings/GambitGameBalanceSettings.h"
#include "Core/Settings/GambitStaticDataSettings.h"
#include "Data/Assets/GambitPlayerLoadoutDefinition.h"
#include "Dice/Data/GambitDiceDefinition.h"
#include "Items/Consumables/GambitConsumableDefinition.h"
#include "Items/Data/GambitItemDefinition.h"
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
				OwnedDiceDefinitions.Add(DiceDefinition);
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

			ActiveModules.Add(ModuleDefinition);
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

			FGambitConsumableRuntimeSlot NewSlot;
			NewSlot.Definition = ConsumableDefinition;
			ConsumableSlots.Add(NewSlot);
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
	if (!DieDefinition)
	{
		return false;
	}

	OwnedDiceDefinitions.Add(DieDefinition);
	OnInventoryChanged.Broadcast();
	return true;
}

bool UGambitInventoryComponent::AddModule(UGambitModuleDefinition* ModuleDefinition)
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
	OnInventoryChanged.Broadcast();
	return true;
}

bool UGambitInventoryComponent::AddConsumable(UGambitConsumableDefinition* ConsumableDefinition)
{
	if (!ConsumableDefinition)
	{
		return false;
	}

	if (!HasAvailableConsumableSlot())
	{
		return false;
	}

	FGambitConsumableRuntimeSlot NewSlot;
	NewSlot.Definition = ConsumableDefinition;
	ConsumableSlots.Add(NewSlot);
	OnInventoryChanged.Broadcast();
	return true;
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
	ConsumableSlots.RemoveAt(SlotIndex);
	OnInventoryChanged.Broadcast();
	return true;
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
