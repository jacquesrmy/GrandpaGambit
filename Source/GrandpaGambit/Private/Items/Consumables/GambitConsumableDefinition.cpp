#include "Items/Consumables/GambitConsumableDefinition.h"

UGambitConsumableDefinition::UGambitConsumableDefinition()
{
	ItemType = EGambitItemType::Consumable;
	ItemTypeId = TEXT("item.consumable");
	UsablePhases.Add(EGambitRoundPhase::Action);
}

bool UGambitConsumableDefinition::CanBeUsedDuringPhase(const EGambitRoundPhase Phase) const
{
	return Phase != EGambitRoundPhase::None && UsablePhases.Contains(Phase);
}
