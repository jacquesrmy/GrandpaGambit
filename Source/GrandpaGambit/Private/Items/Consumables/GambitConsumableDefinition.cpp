#include "Items/Consumables/GambitConsumableDefinition.h"

UGambitConsumableDefinition::UGambitConsumableDefinition()
{
	ItemType = EGambitItemType::Consumable;
	ItemTypeId = TEXT("item.consumable");
}
