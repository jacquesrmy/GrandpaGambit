#include "Shop/Components/GambitShopComponent.h"

#include "Core/Logging/GambitLog.h"
#include "Core/Settings/GambitGameBalanceSettings.h"
#include "Core/Settings/GambitStaticDataSettings.h"
#include "Items/Consumables/GambitConsumableDefinition.h"
#include "Items/Data/GambitItemDefinition.h"
#include "Items/DiceItems/GambitDiceItemDefinition.h"
#include "Items/Modules/GambitModuleDefinition.h"
#include "Managers/SharedPool/GambitSharedPoolComponent.h"
#include "Players/Components/GambitEconomyComponent.h"
#include "Players/Components/GambitInventoryComponent.h"
#include "Shop/Data/GambitShopLootTable.h"
#include "Shop/Models/GambitShopOfferGenerator.h"

namespace
{
	FString FormatShopItemName(const UGambitItemDefinition* ItemDefinition)
	{
		if (!ItemDefinition)
		{
			return TEXT("None");
		}

		if (!ItemDefinition->DisplayName.IsEmpty())
		{
			return ItemDefinition->DisplayName.ToString();
		}

		return ItemDefinition->GetResolvedItemId().ToString();
	}

	int32 CalculateResolvedPrice(const FGambitShopPurchaseContext& PurchaseContext)
	{
		if (PurchaseContext.bFree)
		{
			return 0;
		}

		const float DiscountMultiplier = 1.0f - FMath::Clamp(PurchaseContext.DiscountPercent, 0.0f, 100.0f) / 100.0f;
		const float SurchargeMultiplier = 1.0f + FMath::Max(0.0f, PurchaseContext.SurchargePercent) / 100.0f;
		const float ModifiedPrice = static_cast<float>(FMath::Max(0, PurchaseContext.PriceBeforeModifiers)) * DiscountMultiplier * SurchargeMultiplier;
		return FMath::Max(0, FMath::RoundToInt(ModifiedPrice) + PurchaseContext.FlatPriceDelta);
	}
}

UGambitShopComponent::UGambitShopComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGambitShopComponent::InitializeForMatch()
{
	OfferGenerator = NewObject<UGambitShopOfferGenerator>(this);
	PurchasesMadeThisShop = 0;
	ClearOffers();
}

void UGambitShopComponent::RefreshOffers(FRandomStream& RandomStream, UGambitSharedPoolComponent* SharedPoolComponent)
{
	RefreshOffersWithCount(RandomStream, SharedPoolComponent, INDEX_NONE);
}

void UGambitShopComponent::RefreshOffersWithCount(
	FRandomStream& RandomStream,
	UGambitSharedPoolComponent* SharedPoolComponent,
	const int32 OfferCountOverride)
{
	LastSharedPoolComponent = SharedPoolComponent;
	ReleaseOfferReservations(SharedPoolComponent);
	PurchasesMadeThisShop = 0;

	if (!OfferGenerator)
	{
		OfferGenerator = NewObject<UGambitShopOfferGenerator>(this);
	}

	const UGambitShopLootTable* EffectiveLootTable = ShopLootTable;
	if (!EffectiveLootTable)
	{
		EffectiveLootTable = UGambitStaticDataSettings::Get()->DefaultShopLootTable.LoadSynchronous();
	}

	int32 OfferCount = UGambitGameBalanceSettings::Get()->ShopOfferCount;
	if (EffectiveLootTable && EffectiveLootTable->OfferCountOverride > 0)
	{
		OfferCount = EffectiveLootTable->OfferCountOverride;
	}
	if (OfferCountOverride >= 0)
	{
		OfferCount = OfferCountOverride;
	}

	OfferGenerator->GenerateOffers(EffectiveLootTable, OfferCount, RandomStream, SharedPoolComponent, this, CurrentOffers);
	OnShopOffersUpdated.Broadcast();
}

bool UGambitShopComponent::PurchaseOffer(
	const int32 OfferId,
	UGambitEconomyComponent* EconomyComponent,
	UGambitInventoryComponent* InventoryComponent,
	UGambitSharedPoolComponent* SharedPoolComponent)
{
	FGambitShopPurchaseContext PurchaseContext = BuildPurchaseContext(OfferId, EconomyComponent, SharedPoolComponent);
	ResolvePurchasePrice(PurchaseContext);
	return PurchaseOfferWithContext(PurchaseContext, EconomyComponent, InventoryComponent, SharedPoolComponent);
}

FGambitShopPurchaseContext UGambitShopComponent::BuildPurchaseContext(
	const int32 OfferId,
	const UGambitEconomyComponent* EconomyComponent,
	const UGambitSharedPoolComponent* SharedPoolComponent) const
{
	FGambitShopPurchaseContext PurchaseContext;
	PurchaseContext.OfferId = OfferId;
	PurchaseContext.PurchasesMadeBefore = PurchasesMadeThisShop;

	const FGambitShopOffer* Offer = FindOffer(OfferId);
	if (!Offer)
	{
		PurchaseContext.FailureReason = FString::Printf(TEXT("Offer %d was not found"), OfferId);
		return PurchaseContext;
	}

	PurchaseContext.ItemDefinition = Offer->ItemDefinition;
	PurchaseContext.BasePrice = Offer->BasePrice > 0 ? Offer->BasePrice : Offer->Price;
	PurchaseContext.PriceBeforeModifiers = PurchaseContext.BasePrice;
	PurchaseContext.ResolvedPrice = PurchaseContext.BasePrice;
	PurchaseContext.bUsesSharedPool = Offer->bUsesSharedPool;
	PurchaseContext.bHasSharedPoolReservation = Offer->bHasSharedPoolReservation;
	PurchaseContext.SharedPoolReservationId = Offer->SharedPoolReservationId;
	PurchaseContext.bFree = Offer->bFree;
	PurchaseContext.FreeReason = Offer->FreeReason;

	if (!PurchaseContext.ItemDefinition)
	{
		PurchaseContext.FailureReason = TEXT("Offer has no item definition");
		return PurchaseContext;
	}

	if (SharedPoolComponent)
	{
		PurchaseContext.GlobalPurchasesForItemType = SharedPoolComponent->GetGlobalPurchaseCountForType(PurchaseContext.ItemDefinition->ItemType);
		PurchaseContext.GlobalPurchasesForItemRarity = SharedPoolComponent->GetGlobalPurchaseCountForRarity(PurchaseContext.ItemDefinition->Rarity);
		PurchaseContext.GlobalPurchasesForItemTypeAndRarity = SharedPoolComponent->GetGlobalPurchaseCountForTypeAndRarity(PurchaseContext.ItemDefinition->ItemType, PurchaseContext.ItemDefinition->Rarity);
		if (PurchaseContext.ItemDefinition->bUsesSharedPool)
		{
			PurchaseContext.SharedPoolAvailability = SharedPoolComponent->QueryItemAvailability(PurchaseContext.ItemDefinition);
		}
	}

	if (EconomyComponent && !EconomyComponent->CanSpendGold(PurchaseContext.ResolvedPrice))
	{
		PurchaseContext.FailureReason = TEXT("Not enough gold");
	}

	return PurchaseContext;
}

void UGambitShopComponent::ResolvePurchasePrice(FGambitShopPurchaseContext& PurchaseContext) const
{
	const int32 OldPrice = PurchaseContext.ResolvedPrice;
	PurchaseContext.ResolvedPrice = CalculateResolvedPrice(PurchaseContext);
	PurchaseContext.DebugLines.Add(FString::Printf(
		TEXT("Price Base=%d Before=%d OldResolved=%d Final=%d Discount=%.2f Surcharge=%.2f FlatDelta=%d Free=%s Reason=%s"),
		PurchaseContext.BasePrice,
		PurchaseContext.PriceBeforeModifiers,
		OldPrice,
		PurchaseContext.ResolvedPrice,
		PurchaseContext.DiscountPercent,
		PurchaseContext.SurchargePercent,
		PurchaseContext.FlatPriceDelta,
		PurchaseContext.bFree ? TEXT("Yes") : TEXT("No"),
		*PurchaseContext.FreeReason));

	UE_LOG(
		LogGambit,
		Log,
		TEXT("Shop: PriceResolve OfferId=%d Item=%s Base=%d Before=%d After=%d Discount=%.2f Surcharge=%.2f FlatDelta=%d Free=%s Reason=%s"),
		PurchaseContext.OfferId,
		*FormatShopItemName(PurchaseContext.ItemDefinition),
		PurchaseContext.BasePrice,
		PurchaseContext.PriceBeforeModifiers,
		PurchaseContext.ResolvedPrice,
		PurchaseContext.DiscountPercent,
		PurchaseContext.SurchargePercent,
		PurchaseContext.FlatPriceDelta,
		PurchaseContext.bFree ? TEXT("Yes") : TEXT("No"),
		*PurchaseContext.FreeReason);
}

bool UGambitShopComponent::PurchaseOfferWithContext(
	FGambitShopPurchaseContext& PurchaseContext,
	UGambitEconomyComponent* EconomyComponent,
	UGambitInventoryComponent* InventoryComponent,
	UGambitSharedPoolComponent* SharedPoolComponent)
{
	LastSharedPoolComponent = SharedPoolComponent;
	const int32 OfferIndex = CurrentOffers.IndexOfByPredicate([&PurchaseContext](const FGambitShopOffer& Offer)
	{
		return Offer.OfferId == PurchaseContext.OfferId;
	});
	if (!CurrentOffers.IsValidIndex(OfferIndex))
	{
		PurchaseContext.FailureReason = FString::Printf(TEXT("Offer %d was not found"), PurchaseContext.OfferId);
		UE_LOG(LogGambit, Log, TEXT("Shop: Purchase failed OfferId=%d Reason=%s"), PurchaseContext.OfferId, *PurchaseContext.FailureReason);
		return false;
	}

	FGambitShopOffer& Offer = CurrentOffers[OfferIndex];
	UGambitItemDefinition* ItemDefinition = Offer.ItemDefinition;
	if (!ItemDefinition || !EconomyComponent || !InventoryComponent)
	{
		PurchaseContext.FailureReason = TEXT("Missing item, economy, or inventory component");
		UE_LOG(LogGambit, Log, TEXT("Shop: Purchase failed OfferId=%d Reason=%s"), PurchaseContext.OfferId, *PurchaseContext.FailureReason);
		return false;
	}

	PurchaseContext.ItemDefinition = ItemDefinition;
	PurchaseContext.bUsesSharedPool = Offer.bUsesSharedPool;
	PurchaseContext.bHasSharedPoolReservation = Offer.bHasSharedPoolReservation;
	PurchaseContext.SharedPoolReservationId = Offer.SharedPoolReservationId;
	PurchaseContext.bFree = Offer.bFree;
	PurchaseContext.FreeReason = Offer.FreeReason;

	if (PurchaseContext.bBlockedByEffect)
	{
		if (PurchaseContext.FailureReason.IsEmpty())
		{
			PurchaseContext.FailureReason = TEXT("Blocked by effect");
		}
		UE_LOG(LogGambit, Log, TEXT("Shop: Purchase blocked OfferId=%d Item=%s Reason=%s"), PurchaseContext.OfferId, *FormatShopItemName(ItemDefinition), *PurchaseContext.FailureReason);
		return false;
	}

	FString InventoryFailureReason;
	if (!ValidateInventoryCapacity(ItemDefinition, InventoryComponent, InventoryFailureReason))
	{
		PurchaseContext.FailureReason = InventoryFailureReason;
		UE_LOG(LogGambit, Log, TEXT("Shop: Purchase failed OfferId=%d Item=%s Reason=%s"), PurchaseContext.OfferId, *FormatShopItemName(ItemDefinition), *PurchaseContext.FailureReason);
		return false;
	}

	if (!EconomyComponent->CanSpendGold(PurchaseContext.ResolvedPrice))
	{
		PurchaseContext.FailureReason = FString::Printf(
			TEXT("Not enough gold: Gold=%d Price=%d MinGold=%d"),
			EconomyComponent->GetCurrentGold(),
			PurchaseContext.ResolvedPrice,
			EconomyComponent->GetEffectiveMinimumGold());
		UE_LOG(LogGambit, Log, TEXT("Shop: Purchase failed OfferId=%d Item=%s Reason=%s"), PurchaseContext.OfferId, *FormatShopItemName(ItemDefinition), *PurchaseContext.FailureReason);
		return false;
	}

	if (Offer.bUsesSharedPool)
	{
		if (!SharedPoolComponent)
		{
			PurchaseContext.FailureReason = TEXT("Missing shared pool component");
			UE_LOG(LogGambit, Log, TEXT("Shop: Purchase failed OfferId=%d Item=%s Reason=%s"), PurchaseContext.OfferId, *FormatShopItemName(ItemDefinition), *PurchaseContext.FailureReason);
			return false;
		}

		PurchaseContext.SharedPoolAvailability = SharedPoolComponent->QueryItemAvailability(ItemDefinition);
		if (!Offer.bHasSharedPoolReservation && !PurchaseContext.SharedPoolAvailability.bAvailable)
		{
			PurchaseContext.FailureReason = PurchaseContext.SharedPoolAvailability.Reason;
			UE_LOG(LogGambit, Log, TEXT("Shop: Purchase failed OfferId=%d Item=%s Reason=%s"), PurchaseContext.OfferId, *FormatShopItemName(ItemDefinition), *PurchaseContext.FailureReason);
			return false;
		}
	}

	if (!EconomyComponent->SpendGold(PurchaseContext.ResolvedPrice))
	{
		PurchaseContext.FailureReason = TEXT("SpendGold rejected purchase");
		UE_LOG(LogGambit, Log, TEXT("Shop: Purchase failed OfferId=%d Item=%s Reason=%s"), PurchaseContext.OfferId, *FormatShopItemName(ItemDefinition), *PurchaseContext.FailureReason);
		return false;
	}

	bool bStockCommitted = true;
	if (Offer.bUsesSharedPool && SharedPoolComponent)
	{
		bStockCommitted = Offer.bHasSharedPoolReservation
			? SharedPoolComponent->CommitReservedItemStock(ItemDefinition, Offer.SharedPoolReservationId)
			: SharedPoolComponent->ConsumeItemStock(ItemDefinition);
	}

	if (!bStockCommitted)
	{
		if (Offer.bUsesSharedPool && Offer.bHasSharedPoolReservation && SharedPoolComponent)
		{
			SharedPoolComponent->ReleaseReservedItemStock(ItemDefinition, Offer.SharedPoolReservationId);
		}
		EconomyComponent->AddGold(PurchaseContext.ResolvedPrice);
		PurchaseContext.FailureReason = TEXT("Shared pool stock commit failed");
		UE_LOG(LogGambit, Log, TEXT("Shop: Purchase failed OfferId=%d Item=%s Reason=%s"), PurchaseContext.OfferId, *FormatShopItemName(ItemDefinition), *PurchaseContext.FailureReason);
		return false;
	}

	const FName SourcePurchaseId = FName(*FString::Printf(TEXT("shop.offer.%d"), PurchaseContext.OfferId));
	if (!GrantPurchasedItemToInventory(ItemDefinition, InventoryComponent, SourcePurchaseId))
	{
		if (Offer.bUsesSharedPool && SharedPoolComponent)
		{
			SharedPoolComponent->ReleaseItemStock(ItemDefinition);
		}
		EconomyComponent->AddGold(PurchaseContext.ResolvedPrice);
		PurchaseContext.FailureReason = TEXT("Inventory rejected purchased item");
		UE_LOG(LogGambit, Log, TEXT("Shop: Purchase failed OfferId=%d Item=%s Reason=%s"), PurchaseContext.OfferId, *FormatShopItemName(ItemDefinition), *PurchaseContext.FailureReason);
		return false;
	}

	if (SharedPoolComponent)
	{
		SharedPoolComponent->RecordGlobalPurchase(ItemDefinition);
		PurchaseContext.GlobalPurchasesForItemType = SharedPoolComponent->GetGlobalPurchaseCountForType(ItemDefinition->ItemType);
		PurchaseContext.GlobalPurchasesForItemRarity = SharedPoolComponent->GetGlobalPurchaseCountForRarity(ItemDefinition->Rarity);
		PurchaseContext.GlobalPurchasesForItemTypeAndRarity = SharedPoolComponent->GetGlobalPurchaseCountForTypeAndRarity(ItemDefinition->ItemType, ItemDefinition->Rarity);
	}

	PurchaseContext.bCanPurchase = true;
	PurchaseContext.bPurchaseSucceeded = true;
	PurchasesMadeThisShop++;
	CurrentOffers.RemoveAtSwap(OfferIndex);

	UE_LOG(
		LogGambit,
		Log,
		TEXT("Shop: Purchase success OfferId=%d Item=%s Price=%d GoldAfterSpend=%d PurchasesMade=%d"),
		PurchaseContext.OfferId,
		*FormatShopItemName(ItemDefinition),
		PurchaseContext.ResolvedPrice,
		EconomyComponent->GetCurrentGold(),
		PurchasesMadeThisShop);

	OnShopOffersUpdated.Broadcast();
	return true;
}

void UGambitShopComponent::ApplyPostPurchaseAdjustments(
	FGambitShopPurchaseContext& PurchaseContext,
	UGambitEconomyComponent* EconomyComponent) const
{
	if (!PurchaseContext.bPurchaseSucceeded || !EconomyComponent)
	{
		return;
	}

	const float SafeCashbackPercent = FMath::Max(0.0f, PurchaseContext.CashbackPercent);
	PurchaseContext.CashbackGold = FMath::RoundToInt(static_cast<float>(FMath::Max(0, PurchaseContext.ResolvedPrice)) * SafeCashbackPercent / 100.0f);
	const int32 TotalGoldDelta = PurchaseContext.CashbackGold + PurchaseContext.GoldDeltaOnPurchase;
	if (TotalGoldDelta != 0)
	{
		EconomyComponent->AddGold(TotalGoldDelta);
	}

	UE_LOG(
		LogGambit,
		Log,
		TEXT("Shop: PostPurchase OfferId=%d Item=%s CashbackPercent=%.2f CashbackGold=%d GoldDelta=%d GoldFinal=%d"),
		PurchaseContext.OfferId,
		*FormatShopItemName(PurchaseContext.ItemDefinition),
		PurchaseContext.CashbackPercent,
		PurchaseContext.CashbackGold,
		PurchaseContext.GoldDeltaOnPurchase,
		EconomyComponent->GetCurrentGold());
}

void UGambitShopComponent::SkipShop(UGambitSharedPoolComponent* SharedPoolComponent)
{
	LastSharedPoolComponent = SharedPoolComponent;
	ReleaseOfferReservations(SharedPoolComponent);
	CurrentOffers.Reset();
	OnShopOffersUpdated.Broadcast();
}

void UGambitShopComponent::SetCurrentOffers(const TArray<FGambitShopOffer>& NewOffers)
{
	CurrentOffers = NewOffers;
	OnShopOffersUpdated.Broadcast();
}

int32 UGambitShopComponent::GetConfiguredOfferCount() const
{
	const UGambitShopLootTable* EffectiveLootTable = ShopLootTable;
	if (!EffectiveLootTable)
	{
		EffectiveLootTable = UGambitStaticDataSettings::Get()->DefaultShopLootTable.LoadSynchronous();
	}

	if (EffectiveLootTable && EffectiveLootTable->OfferCountOverride > 0)
	{
		return EffectiveLootTable->OfferCountOverride;
	}

	return UGambitGameBalanceSettings::Get()->ShopOfferCount;
}

void UGambitShopComponent::ClearOffers()
{
	ReleaseOfferReservations(LastSharedPoolComponent.Get());
	CurrentOffers.Reset();
	OnShopOffersUpdated.Broadcast();
}

FGambitShopOffer* UGambitShopComponent::FindOfferMutable(const int32 OfferId)
{
	return CurrentOffers.FindByPredicate([OfferId](const FGambitShopOffer& Offer) { return Offer.OfferId == OfferId; });
}

const FGambitShopOffer* UGambitShopComponent::FindOffer(const int32 OfferId) const
{
	return CurrentOffers.FindByPredicate([OfferId](const FGambitShopOffer& Offer) { return Offer.OfferId == OfferId; });
}

void UGambitShopComponent::ReleaseOfferReservations(UGambitSharedPoolComponent* SharedPoolComponent)
{
	if (!SharedPoolComponent)
	{
		return;
	}

	for (FGambitShopOffer& Offer : CurrentOffers)
	{
		if (Offer.ItemDefinition && Offer.bUsesSharedPool && Offer.bHasSharedPoolReservation)
		{
			SharedPoolComponent->ReleaseReservedItemStock(Offer.ItemDefinition, Offer.SharedPoolReservationId);
			Offer.bHasSharedPoolReservation = false;
			Offer.SharedPoolReservationId.Invalidate();
		}
	}
}

bool UGambitShopComponent::GrantPurchasedItemToInventory(
	UGambitItemDefinition* ItemDefinition,
	UGambitInventoryComponent* InventoryComponent,
	const FName SourcePurchaseId) const
{
	if (!ItemDefinition || !InventoryComponent)
	{
		return false;
	}

	return InventoryComponent->AddItemDefinitionWithSource(ItemDefinition, SourcePurchaseId);
}

bool UGambitShopComponent::ValidateInventoryCapacity(
	UGambitItemDefinition* ItemDefinition,
	const UGambitInventoryComponent* InventoryComponent,
	FString& OutFailureReason) const
{
	OutFailureReason.Reset();
	if (!ItemDefinition || !InventoryComponent)
	{
		OutFailureReason = TEXT("Missing item or inventory component");
		return false;
	}

	if (Cast<UGambitModuleDefinition>(ItemDefinition) && !InventoryComponent->HasAvailableModuleSlot())
	{
		OutFailureReason = TEXT("No available module slot");
		return false;
	}

	if (Cast<UGambitConsumableDefinition>(ItemDefinition) && !InventoryComponent->HasAvailableConsumableSlot())
	{
		OutFailureReason = TEXT("No available consumable slot");
		return false;
	}

	if (const UGambitDiceItemDefinition* DiceItemDefinition = Cast<UGambitDiceItemDefinition>(ItemDefinition))
	{
		if (!DiceItemDefinition->GrantedDiceDefinition)
		{
			OutFailureReason = TEXT("Dice item has no granted dice definition");
			return false;
		}
	}

	return true;
}
