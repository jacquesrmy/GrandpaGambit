#include "Shop/Models/GambitShopOfferGenerator.h"

#include "Core/Logging/GambitLog.h"
#include "Data/Assets/GambitItemCatalogDataAsset.h"
#include "Items/Data/GambitItemDefinition.h"
#include "Managers/SharedPool/GambitSharedPoolComponent.h"
#include "Shop/Data/GambitShopLootTable.h"

namespace
{
	UGambitItemDefinition* ResolveItemFromCatalog(const FName ItemId, const UGambitItemCatalogDataAsset* Catalog)
	{
		if (ItemId.IsNone() || !Catalog)
		{
			return nullptr;
		}

		const auto FindInEntries = [ItemId](const TArray<FGambitItemCatalogEntry>& Entries) -> UGambitItemDefinition*
		{
			for (const FGambitItemCatalogEntry& Entry : Entries)
			{
				if (!Entry.ItemDefinition)
				{
					continue;
				}

				const FName EntryId = !Entry.ItemId.IsNone() ? Entry.ItemId : Entry.ItemDefinition->GetStableItemId();
				if (EntryId == ItemId)
				{
					return Entry.ItemDefinition;
				}
			}

			return nullptr;
		};

		if (UGambitItemDefinition* Module = FindInEntries(Catalog->ModuleEntries))
		{
			return Module;
		}

		if (UGambitItemDefinition* Consumable = FindInEntries(Catalog->ConsumableEntries))
		{
			return Consumable;
		}

		return FindInEntries(Catalog->DiceItemEntries);
	}

	UGambitItemDefinition* ResolveEntryDefinition(
		const FGambitShopWeightedEntry& Entry,
		const UGambitItemCatalogDataAsset* Catalog)
	{
		if (Entry.ItemDefinition)
		{
			return Entry.ItemDefinition;
		}

		return ResolveItemFromCatalog(Entry.ItemId, Catalog);
	}

	FName ResolveEntryId(const FGambitShopWeightedEntry& Entry, const UGambitItemDefinition* Definition)
	{
		if (!Entry.ItemId.IsNone())
		{
			return Entry.ItemId;
		}

		return Definition ? Definition->GetResolvedItemId() : NAME_None;
	}

	int32 ResolveOfferPrice(const FGambitShopWeightedEntry& Entry, const UGambitItemDefinition* Definition)
	{
		if (Entry.PriceOverride >= 0)
		{
			return Entry.PriceOverride;
		}

		return Definition ? Definition->Cost : 0;
	}
}

void UGambitShopOfferGenerator::GenerateOffers(
	const UGambitShopLootTable* LootTable,
	const int32 OfferCount,
	FRandomStream& RandomStream,
	UGambitSharedPoolComponent* SharedPoolComponent,
	UObject* ReservationOwner,
	TArray<FGambitShopOffer>& OutOffers) const
{
	OutOffers.Reset();
	if (!LootTable || OfferCount <= 0)
	{
		return;
	}

	const UGambitItemCatalogDataAsset* ItemCatalog = LootTable->ItemCatalog;
	TSet<FName> PickedIds;
	int32 NextOfferId = 0;
	int32 SafetyCounter = 0;
	const int32 MaxIterations = 100;

	while (OutOffers.Num() < OfferCount && SafetyCounter < MaxIterations)
	{
		SafetyCounter++;

		float TotalWeight = 0.0f;
		for (const FGambitShopWeightedEntry& Entry : LootTable->WeightedItems)
		{
			const UGambitItemDefinition* Definition = ResolveEntryDefinition(Entry, ItemCatalog);
			if (!Definition || Entry.Weight <= 0.0f)
			{
				continue;
			}

			const FName ItemId = ResolveEntryId(Entry, Definition);
			if (ItemId.IsNone())
			{
				continue;
			}

			if (PickedIds.Contains(ItemId))
			{
				continue;
			}

			if (SharedPoolComponent && Definition->bUsesSharedPool && !SharedPoolComponent->IsItemAvailable(Definition))
			{
				const FGambitSharedPoolAvailabilityResult Availability = SharedPoolComponent->QueryItemAvailability(Definition);
				UE_LOG(
					LogGambit,
					Verbose,
					TEXT("ShopOfferGenerator: skip ItemId=%s Reason=%s Available=%d Reserved=%d Purchased=%d Limit=%d"),
					*ItemId.ToString(),
					*Availability.Reason,
					Availability.AvailableStock,
					Availability.ReservedStock,
					Availability.PurchasedCount,
					Availability.PurchaseLimit);
				continue;
			}

			TotalWeight += Entry.Weight;
		}

		if (TotalWeight <= KINDA_SMALL_NUMBER)
		{
			break;
		}

		float WeightPick = RandomStream.FRandRange(0.0f, TotalWeight);
		const FGambitShopWeightedEntry* ChosenEntry = nullptr;

		for (const FGambitShopWeightedEntry& Entry : LootTable->WeightedItems)
		{
			const UGambitItemDefinition* Definition = ResolveEntryDefinition(Entry, ItemCatalog);
			if (!Definition || Entry.Weight <= 0.0f)
			{
				continue;
			}

			const FName ItemId = ResolveEntryId(Entry, Definition);
			if (ItemId.IsNone())
			{
				continue;
			}

			if (PickedIds.Contains(ItemId))
			{
				continue;
			}

			if (SharedPoolComponent && Definition->bUsesSharedPool && !SharedPoolComponent->IsItemAvailable(Definition))
			{
				continue;
			}

			WeightPick -= Entry.Weight;
			if (WeightPick <= 0.0f)
			{
				ChosenEntry = &Entry;
				break;
			}
		}

		if (!ChosenEntry)
		{
			continue;
		}

		UGambitItemDefinition* ChosenDefinition = ResolveEntryDefinition(*ChosenEntry, ItemCatalog);
		if (!ChosenDefinition)
		{
			continue;
		}

		const FName ChosenItemId = ResolveEntryId(*ChosenEntry, ChosenDefinition);
		if (ChosenItemId.IsNone())
		{
			continue;
		}

		FGambitShopOffer Offer;
		Offer.OfferId = NextOfferId++;
		Offer.ItemDefinition = ChosenDefinition;
		Offer.BasePrice = ResolveOfferPrice(*ChosenEntry, ChosenDefinition);
		Offer.Price = Offer.BasePrice;
		Offer.bUsesSharedPool = ChosenDefinition->bUsesSharedPool;

		if (Offer.bUsesSharedPool && SharedPoolComponent)
		{
			FGambitSharedPoolAvailabilityResult Availability;
			if (!SharedPoolComponent->TryReserveItemStock(ChosenDefinition, ReservationOwner, Offer.SharedPoolReservationId, Availability))
			{
				UE_LOG(
					LogGambit,
					Log,
					TEXT("ShopOfferGenerator: reservation failed ItemId=%s Reason=%s"),
					*ChosenItemId.ToString(),
					*Availability.Reason);
				continue;
			}

			Offer.bHasSharedPoolReservation = true;
		}

		OutOffers.Add(Offer);
		PickedIds.Add(ChosenItemId);
	}
}
