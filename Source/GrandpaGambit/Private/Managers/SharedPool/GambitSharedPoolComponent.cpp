#include "Managers/SharedPool/GambitSharedPoolComponent.h"

#include "Core/Logging/GambitLog.h"
#include "Core/Settings/GambitStaticDataSettings.h"
#include "Data/Assets/GambitItemCatalogDataAsset.h"
#include "Data/Assets/GambitSharedPoolDefinition.h"
#include "Items/Data/GambitItemDefinition.h"

namespace
{
	UGambitItemDefinition* ResolveSharedPoolItemFromCatalog(const FName ItemId, const UGambitItemCatalogDataAsset* Catalog)
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

	UGambitItemDefinition* ResolveStockEntryDefinition(
		const FGambitSharedStockEntry& Entry,
		const UGambitItemCatalogDataAsset* Catalog)
	{
		if (Entry.ItemDefinition)
		{
			return Entry.ItemDefinition;
		}

		return ResolveSharedPoolItemFromCatalog(Entry.ItemId, Catalog);
	}

	FName ResolveStockEntryId(const FGambitSharedStockEntry& Entry, const UGambitItemDefinition* Definition)
	{
		if (!Entry.ItemId.IsNone())
		{
			return Entry.ItemId;
		}

		return Definition ? Definition->GetResolvedItemId() : NAME_None;
	}
}

UGambitSharedPoolComponent::UGambitSharedPoolComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGambitSharedPoolComponent::InitializeSharedStock(const TArray<FGambitSharedStockEntry>& InitialStockEntries)
{
	RemainingStockByItemId.Reset();
	MaxStockByItemId.Reset();
	ReservedStockByItemId.Reset();
	PurchasedCountByItemId.Reset();
	PurchaseLimitByItemId.Reset();
	ReservationItemById.Reset();
	ReservationOwnerById.Reset();
	GlobalPurchaseCountByType.Reset();
	GlobalPurchaseCountByRarity.Reset();
	GlobalPurchaseCountByTypeAndRarity.Reset();

	const UGambitSharedPoolDefinition* EffectiveDefinition = SharedPoolDefinition;
	if (!EffectiveDefinition)
	{
		EffectiveDefinition = UGambitStaticDataSettings::Get()->DefaultSharedPoolDefinition.LoadSynchronous();
	}

	const UGambitItemCatalogDataAsset* EffectiveCatalog = EffectiveDefinition ? EffectiveDefinition->ItemCatalog : nullptr;
	if (!EffectiveCatalog)
	{
		EffectiveCatalog = UGambitStaticDataSettings::Get()->DefaultItemCatalog.LoadSynchronous();
	}

	const TArray<FGambitSharedStockEntry>* Entries = &DefaultSharedStock;
	if (InitialStockEntries.Num() > 0)
	{
		Entries = &InitialStockEntries;
	}
	else if (EffectiveDefinition && EffectiveDefinition->StockEntries.Num() > 0)
	{
		Entries = &EffectiveDefinition->StockEntries;
	}

	if (!Entries)
	{
		OnSharedPoolChanged.Broadcast();
		return;
	}

	for (const FGambitSharedStockEntry& Entry : *Entries)
	{
		UGambitItemDefinition* Definition = ResolveStockEntryDefinition(Entry, EffectiveCatalog);
		const FName ItemId = ResolveStockEntryId(Entry, Definition);
		const int32 MaxStock = Entry.MaxStock > 0 ? Entry.MaxStock : (Definition ? Definition->SharedPoolMaxStock : 0);
		if (ItemId.IsNone() || MaxStock <= 0)
		{
			continue;
		}

		RemainingStockByItemId.Add(ItemId, MaxStock);
		MaxStockByItemId.Add(ItemId, MaxStock);
		ReservedStockByItemId.Add(ItemId, 0);
		PurchasedCountByItemId.Add(ItemId, 0);
		PurchaseLimitByItemId.Add(ItemId, Entry.GlobalPurchaseLimit > 0 ? Entry.GlobalPurchaseLimit : ResolveDefinitionPurchaseLimit(Definition, MaxStock));

		UE_LOG(
			LogGambit,
			Log,
			TEXT("SharedPool: Init ItemId=%s Stock=%d PurchaseLimit=%d"),
			*ItemId.ToString(),
			MaxStock,
			PurchaseLimitByItemId[ItemId]);
	}

	OnSharedPoolChanged.Broadcast();
}

bool UGambitSharedPoolComponent::IsItemAvailable(const UGambitItemDefinition* ItemDefinition) const
{
	return QueryItemAvailability(ItemDefinition).bAvailable;
}

FGambitSharedPoolAvailabilityResult UGambitSharedPoolComponent::QueryItemAvailability(const UGambitItemDefinition* ItemDefinition) const
{
	return BuildAvailabilityResult(ItemDefinition, ResolveItemId(ItemDefinition));
}

bool UGambitSharedPoolComponent::ConsumeItemStock(const UGambitItemDefinition* ItemDefinition)
{
	if (!ItemDefinition)
	{
		return false;
	}

	if (!ItemDefinition->bUsesSharedPool)
	{
		return true;
	}

	EnsureItemTracked(ItemDefinition);
	const FName ItemId = ItemDefinition->GetResolvedItemId();
	int32* Remaining = RemainingStockByItemId.Find(ItemId);
	if (!Remaining || *Remaining <= 0)
	{
		const FGambitSharedPoolAvailabilityResult Availability = QueryItemAvailability(ItemDefinition);
		UE_LOG(
			LogGambit,
			Log,
			TEXT("SharedPool: Consume failed ItemId=%s Reason=%s Available=%d Reserved=%d Purchased=%d Limit=%d"),
			*ItemId.ToString(),
			*Availability.Reason,
			Availability.AvailableStock,
			Availability.ReservedStock,
			Availability.PurchasedCount,
			Availability.PurchaseLimit);
		return false;
	}

	int32& PurchasedCount = PurchasedCountByItemId.FindOrAdd(ItemId);
	const int32 PurchaseLimit = PurchaseLimitByItemId.FindRef(ItemId);
	if (PurchaseLimit > 0 && PurchasedCount >= PurchaseLimit)
	{
		UE_LOG(LogGambit, Log, TEXT("SharedPool: Consume failed ItemId=%s Reason=PurchaseLimitReached Purchased=%d Limit=%d"), *ItemId.ToString(), PurchasedCount, PurchaseLimit);
		return false;
	}

	(*Remaining)--;
	PurchasedCount++;
	UE_LOG(LogGambit, Log, TEXT("SharedPool: Consume ItemId=%s Remaining=%d Purchased=%d Limit=%d"), *ItemId.ToString(), *Remaining, PurchasedCount, PurchaseLimit);
	OnSharedPoolChanged.Broadcast();
	return true;
}

bool UGambitSharedPoolComponent::TryReserveItemStock(
	const UGambitItemDefinition* ItemDefinition,
	UObject* ReservationOwner,
	FGuid& OutReservationId,
	FGambitSharedPoolAvailabilityResult& OutAvailability)
{
	OutReservationId.Invalidate();
	OutAvailability = QueryItemAvailability(ItemDefinition);

	if (!ItemDefinition)
	{
		return false;
	}

	if (!ItemDefinition->bUsesSharedPool)
	{
		OutReservationId = FGuid::NewGuid();
		OutAvailability.bAvailable = true;
		OutAvailability.State = EGambitSharedPoolAvailabilityState::NotSharedPoolItem;
		OutAvailability.Reason = TEXT("Item does not use shared pool");
		return true;
	}

	EnsureItemTracked(ItemDefinition);
	const FName ItemId = ItemDefinition->GetResolvedItemId();
	OutAvailability = QueryItemAvailability(ItemDefinition);
	if (!OutAvailability.bAvailable)
	{
		UE_LOG(
			LogGambit,
			Log,
			TEXT("SharedPool: Reserve failed ItemId=%s Reason=%s Available=%d Reserved=%d Purchased=%d Limit=%d"),
			*ItemId.ToString(),
			*OutAvailability.Reason,
			OutAvailability.AvailableStock,
			OutAvailability.ReservedStock,
			OutAvailability.PurchasedCount,
			OutAvailability.PurchaseLimit);
		return false;
	}

	int32& Remaining = RemainingStockByItemId.FindOrAdd(ItemId);
	int32& Reserved = ReservedStockByItemId.FindOrAdd(ItemId);
	Remaining--;
	Reserved++;
	OutReservationId = FGuid::NewGuid();
	ReservationItemById.Add(OutReservationId, ItemId);
	ReservationOwnerById.Add(OutReservationId, ReservationOwner);
	OutAvailability = QueryItemAvailability(ItemDefinition);

	UE_LOG(
		LogGambit,
		Log,
		TEXT("SharedPool: Reserve ItemId=%s Reservation=%s Available=%d Reserved=%d Purchased=%d"),
		*ItemId.ToString(),
		*OutReservationId.ToString(),
		OutAvailability.AvailableStock,
		OutAvailability.ReservedStock,
		OutAvailability.PurchasedCount);
	OnSharedPoolChanged.Broadcast();
	return true;
}

bool UGambitSharedPoolComponent::CommitReservedItemStock(const UGambitItemDefinition* ItemDefinition, const FGuid ReservationId)
{
	if (!ItemDefinition)
	{
		return false;
	}

	if (!ItemDefinition->bUsesSharedPool)
	{
		return true;
	}

	const FName ItemId = ItemDefinition->GetResolvedItemId();
	const FName* ReservedItemId = ReservationItemById.Find(ReservationId);
	if (!ReservedItemId || *ReservedItemId != ItemId)
	{
		UE_LOG(LogGambit, Log, TEXT("SharedPool: CommitReservation failed ItemId=%s Reservation=%s"), *ItemId.ToString(), *ReservationId.ToString());
		return false;
	}

	int32& Reserved = ReservedStockByItemId.FindOrAdd(ItemId);
	if (Reserved <= 0)
	{
		return false;
	}

	int32& PurchasedCount = PurchasedCountByItemId.FindOrAdd(ItemId);
	const int32 PurchaseLimit = PurchaseLimitByItemId.FindRef(ItemId);
	if (PurchaseLimit > 0 && PurchasedCount >= PurchaseLimit)
	{
		UE_LOG(LogGambit, Log, TEXT("SharedPool: CommitReservation failed ItemId=%s Reason=PurchaseLimitReached Purchased=%d Limit=%d"), *ItemId.ToString(), PurchasedCount, PurchaseLimit);
		return false;
	}

	Reserved--;
	PurchasedCount++;
	ReservationItemById.Remove(ReservationId);
	ReservationOwnerById.Remove(ReservationId);

	UE_LOG(
		LogGambit,
		Log,
		TEXT("SharedPool: CommitReservation ItemId=%s Reservation=%s Available=%d Reserved=%d Purchased=%d Limit=%d"),
		*ItemId.ToString(),
		*ReservationId.ToString(),
		RemainingStockByItemId.FindRef(ItemId),
		Reserved,
		PurchasedCount,
		PurchaseLimit);
	OnSharedPoolChanged.Broadcast();
	return true;
}

void UGambitSharedPoolComponent::ReleaseReservedItemStock(const UGambitItemDefinition* ItemDefinition, const FGuid ReservationId)
{
	if (!ItemDefinition || !ItemDefinition->bUsesSharedPool)
	{
		return;
	}

	const FName ItemId = ItemDefinition->GetResolvedItemId();
	const FName* ReservedItemId = ReservationItemById.Find(ReservationId);
	if (!ReservedItemId || *ReservedItemId != ItemId)
	{
		return;
	}

	int32& Remaining = RemainingStockByItemId.FindOrAdd(ItemId);
	int32& Reserved = ReservedStockByItemId.FindOrAdd(ItemId);
	if (Reserved > 0)
	{
		Reserved--;
		Remaining = FMath::Min(Remaining + 1, MaxStockByItemId.FindRef(ItemId));
	}

	ReservationItemById.Remove(ReservationId);
	ReservationOwnerById.Remove(ReservationId);

	UE_LOG(
		LogGambit,
		Log,
		TEXT("SharedPool: ReleaseReservation ItemId=%s Reservation=%s Available=%d Reserved=%d"),
		*ItemId.ToString(),
		*ReservationId.ToString(),
		Remaining,
		Reserved);
	OnSharedPoolChanged.Broadcast();
}

void UGambitSharedPoolComponent::ReleaseItemStock(const UGambitItemDefinition* ItemDefinition)
{
	if (!ItemDefinition || !ItemDefinition->bUsesSharedPool)
	{
		return;
	}

	EnsureItemTracked(ItemDefinition);
	const FName ItemId = ItemDefinition->GetResolvedItemId();
	int32* Remaining = RemainingStockByItemId.Find(ItemId);
	const int32* MaxStock = MaxStockByItemId.Find(ItemId);
	if (!Remaining || !MaxStock)
	{
		return;
	}

	*Remaining = FMath::Clamp(*Remaining + 1, 0, *MaxStock);
	int32& PurchasedCount = PurchasedCountByItemId.FindOrAdd(ItemId);
	PurchasedCount = FMath::Max(0, PurchasedCount - 1);
	UE_LOG(LogGambit, Log, TEXT("SharedPool: ReleaseConsumed ItemId=%s Remaining=%d Purchased=%d"), *ItemId.ToString(), *Remaining, PurchasedCount);
	OnSharedPoolChanged.Broadcast();
}

void UGambitSharedPoolComponent::AddDynamicStock(const UGambitItemDefinition* ItemDefinition, const int32 StockDelta)
{
	if (!ItemDefinition || !ItemDefinition->bUsesSharedPool || StockDelta == 0)
	{
		return;
	}

	EnsureItemTracked(ItemDefinition);
	const FName ItemId = ItemDefinition->GetResolvedItemId();
	int32& Remaining = RemainingStockByItemId.FindOrAdd(ItemId);
	int32& MaxStock = MaxStockByItemId.FindOrAdd(ItemId);
	MaxStock = FMath::Max(0, MaxStock + StockDelta);
	Remaining = FMath::Clamp(Remaining + StockDelta, 0, MaxStock);
	UE_LOG(LogGambit, Log, TEXT("SharedPool: DynamicStock ItemId=%s Delta=%d Remaining=%d Max=%d"), *ItemId.ToString(), StockDelta, Remaining, MaxStock);
	OnSharedPoolChanged.Broadcast();
}

void UGambitSharedPoolComponent::SetPurchaseLimit(const UGambitItemDefinition* ItemDefinition, const int32 PurchaseLimit)
{
	if (!ItemDefinition || !ItemDefinition->bUsesSharedPool)
	{
		return;
	}

	EnsureItemTracked(ItemDefinition);
	const FName ItemId = ItemDefinition->GetResolvedItemId();
	PurchaseLimitByItemId.Add(ItemId, FMath::Max(0, PurchaseLimit));
	UE_LOG(LogGambit, Log, TEXT("SharedPool: SetPurchaseLimit ItemId=%s Limit=%d"), *ItemId.ToString(), PurchaseLimitByItemId[ItemId]);
	OnSharedPoolChanged.Broadcast();
}

void UGambitSharedPoolComponent::SetItemUnavailable(const UGambitItemDefinition* ItemDefinition)
{
	if (!ItemDefinition || !ItemDefinition->bUsesSharedPool)
	{
		return;
	}

	EnsureItemTracked(ItemDefinition);
	const FName ItemId = ItemDefinition->GetResolvedItemId();
	RemainingStockByItemId.Add(ItemId, 0);
	PurchaseLimitByItemId.Add(ItemId, PurchasedCountByItemId.FindRef(ItemId));
	UE_LOG(LogGambit, Log, TEXT("SharedPool: SetUnavailable ItemId=%s Purchased=%d"), *ItemId.ToString(), PurchasedCountByItemId.FindRef(ItemId));
	OnSharedPoolChanged.Broadcast();
}

void UGambitSharedPoolComponent::RecordGlobalPurchase(const UGambitItemDefinition* ItemDefinition)
{
	if (!ItemDefinition)
	{
		return;
	}

	int32& TypeCount = GlobalPurchaseCountByType.FindOrAdd(ItemDefinition->ItemType);
	TypeCount++;

	int32& RarityCount = GlobalPurchaseCountByRarity.FindOrAdd(ItemDefinition->Rarity);
	RarityCount++;

	const FName TypeAndRarityKey = FName(*FString::Printf(TEXT("%d.%d"), static_cast<int32>(ItemDefinition->ItemType), static_cast<int32>(ItemDefinition->Rarity)));
	int32& TypeAndRarityCount = GlobalPurchaseCountByTypeAndRarity.FindOrAdd(TypeAndRarityKey);
	TypeAndRarityCount++;

	UE_LOG(
		LogGambit,
		Log,
		TEXT("SharedPool: RecordGlobalPurchase ItemId=%s Type=%d TypeCount=%d Rarity=%d RarityCount=%d TypeRarityCount=%d"),
		*ItemDefinition->GetResolvedItemId().ToString(),
		static_cast<int32>(ItemDefinition->ItemType),
		TypeCount,
		static_cast<int32>(ItemDefinition->Rarity),
		RarityCount,
		TypeAndRarityCount);
	OnSharedPoolChanged.Broadcast();
}

int32 UGambitSharedPoolComponent::GetRemainingStock(const UGambitItemDefinition* ItemDefinition) const
{
	if (!ItemDefinition)
	{
		return 0;
	}

	if (!ItemDefinition->bUsesSharedPool)
	{
		return MAX_int32;
	}

	const int32* Found = RemainingStockByItemId.Find(ItemDefinition->GetResolvedItemId());
	if (Found)
	{
		return *Found;
	}

	return ItemDefinition->SharedPoolMaxStock;
}

int32 UGambitSharedPoolComponent::GetReservedStock(const UGambitItemDefinition* ItemDefinition) const
{
	if (!ItemDefinition || !ItemDefinition->bUsesSharedPool)
	{
		return 0;
	}

	return ReservedStockByItemId.FindRef(ItemDefinition->GetResolvedItemId());
}

bool UGambitSharedPoolComponent::IsItemTracked(const UGambitItemDefinition* ItemDefinition) const
{
	if (!ItemDefinition)
	{
		return false;
	}

	return IsItemIdTracked(ItemDefinition->GetResolvedItemId());
}

int32 UGambitSharedPoolComponent::GetGlobalPurchaseCountForType(const EGambitItemType ItemType) const
{
	return GlobalPurchaseCountByType.FindRef(ItemType);
}

int32 UGambitSharedPoolComponent::GetGlobalPurchaseCountForRarity(const EGambitItemRarity Rarity) const
{
	return GlobalPurchaseCountByRarity.FindRef(Rarity);
}

int32 UGambitSharedPoolComponent::GetGlobalPurchaseCountForTypeAndRarity(
	const EGambitItemType ItemType,
	const EGambitItemRarity Rarity) const
{
	const FName TypeAndRarityKey = FName(*FString::Printf(TEXT("%d.%d"), static_cast<int32>(ItemType), static_cast<int32>(Rarity)));
	return GlobalPurchaseCountByTypeAndRarity.FindRef(TypeAndRarityKey);
}

bool UGambitSharedPoolComponent::IsItemIdTracked(const FName ItemId) const
{
	return !ItemId.IsNone() && RemainingStockByItemId.Contains(ItemId);
}

void UGambitSharedPoolComponent::EnsureItemTracked(const UGambitItemDefinition* ItemDefinition)
{
	if (!ItemDefinition || !ItemDefinition->bUsesSharedPool)
	{
		return;
	}

	const FName ItemId = ItemDefinition->GetResolvedItemId();
	if (!RemainingStockByItemId.Contains(ItemId))
	{
		const int32 MaxStock = ResolveDefinitionStock(ItemDefinition);
		RemainingStockByItemId.Add(ItemId, MaxStock);
		MaxStockByItemId.Add(ItemId, MaxStock);
		ReservedStockByItemId.Add(ItemId, 0);
		PurchasedCountByItemId.Add(ItemId, 0);
		PurchaseLimitByItemId.Add(ItemId, ResolveDefinitionPurchaseLimit(ItemDefinition, MaxStock));
		UE_LOG(LogGambit, Log, TEXT("SharedPool: TrackRuntime ItemId=%s Stock=%d PurchaseLimit=%d"), *ItemId.ToString(), MaxStock, PurchaseLimitByItemId[ItemId]);
	}
}

FName UGambitSharedPoolComponent::ResolveItemId(const UGambitItemDefinition* ItemDefinition) const
{
	return ItemDefinition ? ItemDefinition->GetResolvedItemId() : NAME_None;
}

int32 UGambitSharedPoolComponent::ResolveDefinitionStock(const UGambitItemDefinition* ItemDefinition) const
{
	return ItemDefinition ? FMath::Max(0, ItemDefinition->SharedPoolMaxStock) : 0;
}

int32 UGambitSharedPoolComponent::ResolveDefinitionPurchaseLimit(
	const UGambitItemDefinition* ItemDefinition,
	const int32 MaxStock) const
{
	if (!ItemDefinition)
	{
		return 0;
	}

	return ItemDefinition->SharedPoolPurchaseLimit > 0 ? ItemDefinition->SharedPoolPurchaseLimit : MaxStock;
}

FGambitSharedPoolAvailabilityResult UGambitSharedPoolComponent::BuildAvailabilityResult(
	const UGambitItemDefinition* ItemDefinition,
	const FName ItemId,
	const bool bAllowUnreservedReservation) const
{
	FGambitSharedPoolAvailabilityResult Result;
	Result.ItemId = ItemId;

	if (!ItemDefinition)
	{
		Result.State = EGambitSharedPoolAvailabilityState::MissingItem;
		Result.Reason = TEXT("Missing item definition");
		return Result;
	}

	if (!ItemDefinition->bUsesSharedPool)
	{
		Result.bAvailable = true;
		Result.State = EGambitSharedPoolAvailabilityState::NotSharedPoolItem;
		Result.Reason = TEXT("Item does not use shared pool");
		Result.AvailableStock = MAX_int32;
		return Result;
	}

	const bool bTracked = RemainingStockByItemId.Contains(ItemId);
	Result.MaxStock = bTracked ? MaxStockByItemId.FindRef(ItemId) : ResolveDefinitionStock(ItemDefinition);
	Result.AvailableStock = bTracked ? RemainingStockByItemId.FindRef(ItemId) : Result.MaxStock;
	Result.ReservedStock = bTracked ? ReservedStockByItemId.FindRef(ItemId) : 0;
	Result.PurchasedCount = bTracked ? PurchasedCountByItemId.FindRef(ItemId) : 0;
	Result.PurchaseLimit = bTracked ? PurchaseLimitByItemId.FindRef(ItemId) : ResolveDefinitionPurchaseLimit(ItemDefinition, Result.MaxStock);

	if (ItemId.IsNone())
	{
		Result.State = EGambitSharedPoolAvailabilityState::UntrackedItem;
		Result.Reason = TEXT("Missing item id");
		return Result;
	}

	if (!bTracked && Result.MaxStock <= 0)
	{
		Result.State = EGambitSharedPoolAvailabilityState::UntrackedItem;
		Result.Reason = TEXT("Shared item is not tracked and has no definition stock");
		return Result;
	}

	if (Result.PurchaseLimit > 0 && Result.PurchasedCount >= Result.PurchaseLimit)
	{
		Result.State = EGambitSharedPoolAvailabilityState::PurchaseLimitReached;
		Result.Reason = TEXT("Global purchase limit reached");
		return Result;
	}

	if (Result.AvailableStock <= 0)
	{
		Result.State = Result.ReservedStock > 0 || bAllowUnreservedReservation
			? EGambitSharedPoolAvailabilityState::ReservedOut
			: EGambitSharedPoolAvailabilityState::NoStockRemaining;
		Result.Reason = Result.ReservedStock > 0 ? TEXT("All stock is reserved") : TEXT("No stock remaining");
		return Result;
	}

	Result.bAvailable = true;
	Result.State = EGambitSharedPoolAvailabilityState::Available;
	Result.Reason = TEXT("Available");
	return Result;
}
