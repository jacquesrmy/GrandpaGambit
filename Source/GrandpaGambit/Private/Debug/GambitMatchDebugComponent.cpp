#include "Debug/GambitMatchDebugComponent.h"

#include "Core/Logging/GambitLog.h"
#include "Core/Settings/GambitGameBalanceSettings.h"
#include "Core/Settings/GambitStaticDataSettings.h"
#include "Core/Types/GambitDebugTypes.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "Data/Assets/GambitDiceCatalogDataAsset.h"
#include "Data/Assets/GambitItemCatalogDataAsset.h"
#include "Data/Assets/GambitItemEffectDefinition.h"
#include "Data/Assets/GambitPlayerLoadoutDefinition.h"
#include "Data/Assets/GambitSharedPoolDefinition.h"
#include "Data/Validation/GambitDataValidation.h"
#include "Debug/GambitDebugAutoPlayer.h"
#include "Dice/Data/GambitDiceDefinition.h"
#include "Game/Modes/GambitGameMode.h"
#include "Game/States/GambitGameState.h"
#include "Items/Consumables/GambitConsumableDefinition.h"
#include "Items/Data/GambitItemDefinition.h"
#include "Items/DiceItems/GambitDiceItemDefinition.h"
#include "Items/Effects/GambitItemEffect.h"
#include "Items/Modules/GambitModuleDefinition.h"
#include "Managers/SharedPool/GambitSharedPoolComponent.h"
#include "Players/Components/GambitInventoryComponent.h"
#include "Players/States/GambitPlayerState.h"
#include "Shop/Data/GambitShopLootTable.h"

namespace
{
	FString MatchDebugPhaseToString(const EGambitRoundPhase Phase)
	{
		switch (Phase)
		{
		case EGambitRoundPhase::None: return TEXT("None");
		case EGambitRoundPhase::Roll: return TEXT("Roll");
		case EGambitRoundPhase::SelectionReroll: return TEXT("SelectionReroll");
		case EGambitRoundPhase::Action: return TEXT("Action");
		case EGambitRoundPhase::Resolution: return TEXT("Resolution");
		case EGambitRoundPhase::Reward: return TEXT("Reward");
		case EGambitRoundPhase::Ranking: return TEXT("Ranking");
		case EGambitRoundPhase::Shop: return TEXT("Shop");
		case EGambitRoundPhase::RoundEnd: return TEXT("RoundEnd");
		default: return TEXT("Unknown");
		}
	}

	FString MatchDebugCombinationToString(const EGambitCombinationType Combination)
	{
		switch (Combination)
		{
		case EGambitCombinationType::None: return TEXT("None");
		case EGambitCombinationType::HighDice: return TEXT("HighDice");
		case EGambitCombinationType::Pair: return TEXT("Pair");
		case EGambitCombinationType::TwoPair: return TEXT("TwoPair");
		case EGambitCombinationType::ThreeOfAKind: return TEXT("ThreeOfAKind");
		case EGambitCombinationType::StraightSmall: return TEXT("StraightSmall");
		case EGambitCombinationType::FullHouse: return TEXT("FullHouse");
		case EGambitCombinationType::FourOfAKind: return TEXT("FourOfAKind");
		case EGambitCombinationType::StraightLarge: return TEXT("StraightLarge");
		case EGambitCombinationType::FiveOfAKind: return TEXT("FiveOfAKind");
		default: return TEXT("Unknown");
		}
	}

	FString DiceTypeToString(const EGambitDiceType DiceType)
	{
		switch (DiceType)
		{
		case EGambitDiceType::D6: return TEXT("D6");
		case EGambitDiceType::D8: return TEXT("D8");
		case EGambitDiceType::D10: return TEXT("D10");
		case EGambitDiceType::D12: return TEXT("D12");
		case EGambitDiceType::Wildcard: return TEXT("Wildcard");
		default: return TEXT("Unknown");
		}
	}

	FString MatchDebugItemTypeToString(const EGambitItemType ItemType)
	{
		switch (ItemType)
		{
		case EGambitItemType::None: return TEXT("None");
		case EGambitItemType::Dice: return TEXT("Dice");
		case EGambitItemType::Module: return TEXT("Module");
		case EGambitItemType::Consumable: return TEXT("Consumable");
		default: return TEXT("Unknown");
		}
	}

	FString RarityToString(const EGambitItemRarity Rarity)
	{
		switch (Rarity)
		{
		case EGambitItemRarity::Common: return TEXT("Common");
		case EGambitItemRarity::Uncommon: return TEXT("Uncommon");
		case EGambitItemRarity::Rare: return TEXT("Rare");
		case EGambitItemRarity::Epic: return TEXT("Epic");
		case EGambitItemRarity::Legendary: return TEXT("Legendary");
		default: return TEXT("Unknown");
		}
	}

	FString FormatMatchDebugDiceValues(const TArray<FGambitDieRuntimeState>& DiceStates)
	{
		TArray<FString> Values;
		Values.Reserve(DiceStates.Num());

		for (const FGambitDieRuntimeState& DieState : DiceStates)
		{
			Values.Add(DieState.bLocked
				? FString::Printf(TEXT("%dL"), DieState.EffectiveValue)
				: FString::FromInt(DieState.EffectiveValue));
		}

		return FString::Printf(TEXT("[%s]"), *FString::Join(Values, TEXT(", ")));
	}

	FString FormatIntArray(const TArray<int32>& Values)
	{
		TArray<FString> Parts;
		Parts.Reserve(Values.Num());
		for (const int32 Value : Values)
		{
			Parts.Add(FString::FromInt(Value));
		}

		return FString::Printf(TEXT("[%s]"), *FString::Join(Parts, TEXT(",")));
	}

	FString FormatDiceDefinition(const UGambitDiceDefinition* DiceDefinition)
	{
		if (!DiceDefinition)
		{
			return TEXT("None");
		}

		return FString::Printf(
			TEXT("DiceId=%s Type=%s Faces=%s ComboCount=%d Sum=%s Combos=%s Reroll=%s Lock=%s Path=%s"),
			*DiceDefinition->GetResolvedDiceId().ToString(),
			*DiceTypeToString(DiceDefinition->DiceType),
			*FormatIntArray(DiceDefinition->GetResolvedFaces()),
			DiceDefinition->DefaultComboContributionCount,
			DiceDefinition->bCountsForScoreSum ? TEXT("Yes") : TEXT("No"),
			DiceDefinition->bCountsForCombinations ? TEXT("Yes") : TEXT("No"),
			DiceDefinition->bCanBeRerolled ? TEXT("Yes") : TEXT("No"),
			DiceDefinition->bCanBeLocked ? TEXT("Yes") : TEXT("No"),
			*DiceDefinition->GetPathName());
	}

	FString FormatMatchDebugItemName(const UGambitItemDefinition* ItemDefinition)
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

	FString FormatItemDetails(const UGambitItemDefinition* ItemDefinition)
	{
		if (!ItemDefinition)
		{
			return TEXT("Item=None");
		}

		return FString::Printf(
			TEXT("Item=%s ItemId=%s Type=%s Rarity=%s Cost=%d SharedPool=%s MaxStock=%d Path=%s"),
			*FormatMatchDebugItemName(ItemDefinition),
			*ItemDefinition->GetResolvedItemId().ToString(),
			*MatchDebugItemTypeToString(ItemDefinition->ItemType),
			*RarityToString(ItemDefinition->Rarity),
			ItemDefinition->Cost,
			ItemDefinition->bUsesSharedPool ? TEXT("Yes") : TEXT("No"),
			ItemDefinition->bUsesSharedPool ? ItemDefinition->SharedPoolMaxStock : 0,
			*ItemDefinition->GetPathName());
	}

	FString FormatMatchDebugOffer(const FGambitShopOffer& Offer)
	{
		const UGambitItemDefinition* ItemDefinition = Offer.ItemDefinition;
		const FString ItemId = ItemDefinition ? ItemDefinition->GetResolvedItemId().ToString() : TEXT("None");
		return FString::Printf(
			TEXT("OfferId=%d Item=%s ItemId=%s Type=%s BasePrice=%d Price=%d SharedPool=%s Reserved=%s"),
			Offer.OfferId,
			*FormatMatchDebugItemName(ItemDefinition),
			*ItemId,
			ItemDefinition ? *MatchDebugItemTypeToString(ItemDefinition->ItemType) : TEXT("None"),
			Offer.BasePrice,
			Offer.Price,
			Offer.bUsesSharedPool ? TEXT("Yes") : TEXT("No"),
			Offer.bHasSharedPoolReservation ? TEXT("Yes") : TEXT("No"));
	}

	FString FormatOfferForPlayer(
		const FGambitShopOffer& Offer,
		const AGambitPlayerState* PlayerState,
		const UGambitSharedPoolComponent* SharedPoolComponent)
	{
		const UGambitItemDefinition* ItemDefinition = Offer.ItemDefinition;
		const bool bAffordable = PlayerState && PlayerState->GetCurrentGold() >= Offer.Price;
		const int32 RemainingSharedStock = (ItemDefinition && ItemDefinition->bUsesSharedPool && SharedPoolComponent)
			? SharedPoolComponent->GetRemainingStock(ItemDefinition)
			: INDEX_NONE;
		const int32 ReservedSharedStock = (ItemDefinition && ItemDefinition->bUsesSharedPool && SharedPoolComponent)
			? SharedPoolComponent->GetReservedStock(ItemDefinition)
			: INDEX_NONE;
		const FString RemainingSharedStockString = RemainingSharedStock == INDEX_NONE ? FString(TEXT("N/A")) : FString::FromInt(RemainingSharedStock);
		const FString ReservedSharedStockString = ReservedSharedStock == INDEX_NONE ? FString(TEXT("N/A")) : FString::FromInt(ReservedSharedStock);
		const bool bTracked = (ItemDefinition && ItemDefinition->bUsesSharedPool && SharedPoolComponent)
			? SharedPoolComponent->IsItemTracked(ItemDefinition)
			: true;

		return FString::Printf(
			TEXT("%s Affordable=%s RemainingSharedStock=%s ReservedSharedStock=%s SharedTracked=%s"),
			*FormatMatchDebugOffer(Offer),
			bAffordable ? TEXT("Yes") : TEXT("No"),
			*RemainingSharedStockString,
			*ReservedSharedStockString,
			bTracked ? TEXT("Yes") : TEXT("No"));
	}

	FString BuildMatchDebugPlayerLabel(const AGambitPlayerState* PlayerState, const TArray<AGambitPlayerState*>& Players)
	{
		const int32 PlayerIndex = Players.IndexOfByKey(PlayerState);
		const FString PlayerName = PlayerState ? PlayerState->GetPlayerName() : FString();
		const FString ResolvedName = PlayerName.IsEmpty() ? TEXT("Unnamed") : PlayerName;
		return FString::Printf(TEXT("P%d %s"), PlayerIndex, *ResolvedName);
	}

	UGambitItemDefinition* ResolveMatchDebugItemFromCatalog(const FName ItemId, const UGambitItemCatalogDataAsset* Catalog)
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

	UGambitItemDefinition* ResolveMatchDebugCatalogEntryDefinition(
		const FName ItemId,
		UGambitItemDefinition* ItemDefinition,
		const UGambitItemCatalogDataAsset* Catalog)
	{
		if (ItemDefinition)
		{
			return ItemDefinition;
		}

		return ResolveMatchDebugItemFromCatalog(ItemId, Catalog);
	}

	FName ResolveMatchDebugCatalogEntryId(const FName EntryId, const UGambitItemDefinition* ItemDefinition)
	{
		if (!EntryId.IsNone())
		{
			return EntryId;
		}

		return ItemDefinition ? ItemDefinition->GetResolvedItemId() : NAME_None;
	}

	FString BuildPurchaseFailureReason(
		const AGambitPlayerState* PlayerState,
		const FGambitShopOffer* Offer,
		const int32 OfferId,
		const UGambitSharedPoolComponent* SharedPoolComponent)
	{
		if (!PlayerState)
		{
			return TEXT("joueur invalide");
		}

		if (!Offer)
		{
			return FString::Printf(TEXT("offre introuvable (%d)"), OfferId);
		}

		const UGambitItemDefinition* ItemDefinition = Offer->ItemDefinition;
		if (!ItemDefinition)
		{
			return TEXT("item definition manquante");
		}

		if (PlayerState->GetCurrentGold() < Offer->Price)
		{
			return TEXT("pas assez d'or");
		}

		if (ItemDefinition->bUsesSharedPool)
		{
			if (!SharedPoolComponent || !SharedPoolComponent->IsItemAvailable(ItemDefinition))
			{
				return TEXT("shared pool indisponible");
			}
		}

		const FGambitPlayerSlotState SlotState = PlayerState->GetSlotState();
		if (Cast<UGambitModuleDefinition>(ItemDefinition) && SlotState.ModuleSlotsUsed >= SlotState.ModuleSlotsCapacity)
		{
			return TEXT("slot module plein");
		}

		if (Cast<UGambitConsumableDefinition>(ItemDefinition) && SlotState.ConsumableSlotsUsed >= SlotState.ConsumableSlotsCapacity)
		{
			return TEXT("slot consommable plein");
		}

		if (const UGambitDiceItemDefinition* DiceItemDefinition = Cast<UGambitDiceItemDefinition>(ItemDefinition))
		{
			if (!DiceItemDefinition->GrantedDiceDefinition)
			{
				return TEXT("dice definition manquante");
			}
		}

		return TEXT("aucune raison previsible");
	}

	void LogInventoryStateForPlayer(const AGambitPlayerState* PlayerState, const TArray<AGambitPlayerState*>& Players, const TCHAR* Prefix)
	{
		if (!PlayerState)
		{
			return;
		}

		const FGambitPlayerSlotState SlotState = PlayerState->GetSlotState();
		UE_LOG(
			LogGambit,
			Log,
			TEXT("%s %s Inventory Gold=%d VP=%d Score=%d DiceOwned=%d RuntimeDice=%d Modules=%d/%d Consumables=%d/%d"),
			Prefix,
			*BuildMatchDebugPlayerLabel(PlayerState, Players),
			PlayerState->GetCurrentGold(),
			PlayerState->GetTotalVictoryPoints(),
			PlayerState->GetCurrentRoundScore(),
			PlayerState->GetOwnedDiceDefinitionsRef().Num(),
			PlayerState->GetDiceStatesRef().Num(),
			SlotState.ModuleSlotsUsed,
			SlotState.ModuleSlotsCapacity,
			SlotState.ConsumableSlotsUsed,
			SlotState.ConsumableSlotsCapacity);
	}

	template <typename TEnum>
	FString MatchDebugEnumToDebugString(const TEnum Value)
	{
		if (const UEnum* Enum = StaticEnum<TEnum>())
		{
			return Enum->GetNameStringByValue(static_cast<int64>(Value));
		}

		return TEXT("Unknown");
	}
}

UGambitMatchDebugComponent::UGambitMatchDebugComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGambitMatchDebugComponent::PrintMatchSnapshot() const
{
	const AGambitGameState* GameState = GetGambitGameState();
	if (!GameState)
	{
		UE_LOG(LogGambit, Log, TEXT("DebugMatch: PrintMatchSnapshot failed, missing GambitGameState"));
		return;
	}

	const UGambitGameBalanceSettings* Settings = UGambitGameBalanceSettings::Get();
	const TArray<AGambitPlayerState*> Players = GetAllPlayers();

	UE_LOG(
		LogGambit,
		Log,
		TEXT("DebugMatch: Snapshot Round=%d/%d Phase=%s Players=%d DefaultDice=%d MaxRerolls=%d ShopOffers=%d"),
		GameState->GetCurrentRoundIndex(),
		Settings->RoundCount,
		*MatchDebugPhaseToString(GameState->GetCurrentPhase()),
		Players.Num(),
		Settings->DefaultActiveDiceCount,
		Settings->MaxRerollsPerRound,
		Settings->ShopOfferCount);

	const TArray<FGambitRankingEntry>& Ranking = GameState->GetRoundRankingSnapshotRef();
	if (Ranking.Num() > 0)
	{
		UE_LOG(LogGambit, Log, TEXT("DebugMatch: Current ranking snapshot entries=%d"), Ranking.Num());
		for (const FGambitRankingEntry& Entry : Ranking)
		{
			const AGambitPlayerState* RankedPlayer = Cast<AGambitPlayerState>(Entry.PlayerState);
			UE_LOG(
				LogGambit,
				Log,
				TEXT("DebugMatch: Rank=%d Player=%s RoundScore=%d VPGained=%d TotalVP=%d"),
				Entry.Rank,
				*BuildMatchDebugPlayerLabel(RankedPlayer, Players),
				Entry.RoundScore,
				Entry.VictoryPointsGranted,
				RankedPlayer ? RankedPlayer->GetTotalVictoryPoints() : 0);
		}
	}
}

void UGambitMatchDebugComponent::PrintPlayerSnapshot(AGambitPlayerState* PlayerState) const
{
	if (!PlayerState)
	{
		UE_LOG(LogGambit, Log, TEXT("DebugMatch: PrintPlayerSnapshot failed, PlayerState is null"));
		return;
	}

	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	const FGambitPlayerRuntimeSnapshot Snapshot = PlayerState->BuildRuntimeSnapshot();
	const TArray<FGambitDieRuntimeState>& DiceStates = PlayerState->GetDiceStatesRef();
	const FGambitScoreBreakdown ScoreBreakdown = PlayerState->GetLastScoreBreakdown();

	UE_LOG(
		LogGambit,
		Log,
		TEXT("DebugMatch: %s Gold=%d VP=%d Score=%d DiceCount=%d Modules=%d/%d Consumables=%d/%d Dice=%s"),
		*BuildMatchDebugPlayerLabel(PlayerState, Players),
		Snapshot.CurrentGold,
		Snapshot.TotalVictoryPoints,
		Snapshot.CurrentRoundScore,
		DiceStates.Num(),
		Snapshot.SlotState.ModuleSlotsUsed,
		Snapshot.SlotState.ModuleSlotsCapacity,
		Snapshot.SlotState.ConsumableSlotsUsed,
		Snapshot.SlotState.ConsumableSlotsCapacity,
		*FormatMatchDebugDiceValues(DiceStates));

	UE_LOG(
		LogGambit,
		Log,
		TEXT("DebugMatch: %s ScoreBreakdown Combination=%s DiceSum=%d Base=%d Additive=%.2f Multiplier=%.2f Final=%d"),
		*BuildMatchDebugPlayerLabel(PlayerState, Players),
		*MatchDebugCombinationToString(ScoreBreakdown.Combination),
		ScoreBreakdown.DiceSum,
		ScoreBreakdown.BaseCombinationScore,
		ScoreBreakdown.AdditiveBonus,
		ScoreBreakdown.Multiplier,
		ScoreBreakdown.FinalScore);

	const TArray<FGambitShopOffer>& Offers = PlayerState->GetCurrentShopOffersRef();
	if (Offers.Num() == 0)
	{
		UE_LOG(LogGambit, Log, TEXT("DebugMatch: %s ShopOffers=None"), *BuildMatchDebugPlayerLabel(PlayerState, Players));
		return;
	}

	for (const FGambitShopOffer& Offer : Offers)
	{
		UE_LOG(LogGambit, Log, TEXT("DebugMatch: %s ShopOffer %s"), *BuildMatchDebugPlayerLabel(PlayerState, Players), *FormatMatchDebugOffer(Offer));
	}
}

void UGambitMatchDebugComponent::PrintAllPlayerSnapshots() const
{
	for (AGambitPlayerState* PlayerState : GetAllPlayers())
	{
		PrintPlayerSnapshot(PlayerState);
	}
}

void UGambitMatchDebugComponent::PrintPlayerDebugReport(AGambitPlayerState* PlayerState) const
{
	if (!PlayerState)
	{
		UE_LOG(LogGambit, Log, TEXT("DebugMatch: PrintPlayerDebugReport failed, PlayerState is null"));
		return;
	}

	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	const FGambitDebugPlayerSnapshot Snapshot = PlayerState->BuildDebugPlayerSnapshot(Players.IndexOfByKey(PlayerState));
	UE_LOG(
		LogGambit,
		Log,
		TEXT("DebugReport: %s Gold=%d VP=%d Score=%d Dice=%d Modules=%d Consumables=%d Effects=%d ScoreLines=%d GoldLines=%d ShopLines=%d"),
		*BuildMatchDebugPlayerLabel(PlayerState, Players),
		Snapshot.CurrentGold,
		Snapshot.TotalVictoryPoints,
		Snapshot.CurrentRoundScore,
		Snapshot.Dice.Num(),
		Snapshot.ActiveModules.Num(),
		Snapshot.Consumables.Num(),
		Snapshot.EffectEvents.Num(),
		Snapshot.ScoreLines.Num(),
		Snapshot.GoldLines.Num(),
		Snapshot.ShopLines.Num());

	for (const FGambitDebugDieSnapshot& Die : Snapshot.Dice)
	{
		UE_LOG(
			LogGambit,
			Log,
			TEXT("DebugReport: %s Die Hand=%d Instance=%d Name=%s EffectiveId=%s OriginalId=%s Rarity=%s Value=%d Raw=%d ScoreValue=%d ComboCount=%d Locked=%s FaceOverride=%s RuntimeFaces=%s DefTags=%d RuntimeTags=%d Effects=%d Summary=%s"),
			*BuildMatchDebugPlayerLabel(PlayerState, Players),
			Die.HandIndex,
			Die.InstanceId,
			*Die.DisplayName,
			*Die.EffectiveDiceId.ToString(),
			*Die.OriginalDiceId.ToString(),
			*RarityToString(Die.Rarity),
			Die.EffectiveValue,
			Die.RawValue,
			Die.ScoreContributionValue,
			Die.ComboContributionCount,
			Die.bLocked ? TEXT("Yes") : TEXT("No"),
			Die.bHasRuntimeFaceOverride ? TEXT("Yes") : TEXT("No"),
			*FormatIntArray(Die.RuntimeFaces),
			Die.DefinitionTags.Num(),
			Die.RuntimeTags.Num(),
			Die.EffectIds.Num(),
			*Die.Summary);
	}

	for (const FGambitDebugItemSnapshot& Module : Snapshot.ActiveModules)
	{
		UE_LOG(
			LogGambit,
			Log,
			TEXT("DebugReport: %s Module Name=%s Id=%s Rarity=%s Cost=%d Tags=%d Effects=%d Summary=%s"),
			*BuildMatchDebugPlayerLabel(PlayerState, Players),
			*Module.DisplayName,
			*Module.ItemId.ToString(),
			*RarityToString(Module.Rarity),
			Module.Cost,
			Module.Tags.Num(),
			Module.EffectIds.Num(),
			*Module.Summary);
	}

	for (const FGambitDebugItemSnapshot& Consumable : Snapshot.Consumables)
	{
		UE_LOG(
			LogGambit,
			Log,
			TEXT("DebugReport: %s Consumable Name=%s Id=%s Rarity=%s Cost=%d Tags=%d Effects=%d Summary=%s"),
			*BuildMatchDebugPlayerLabel(PlayerState, Players),
			*Consumable.DisplayName,
			*Consumable.ItemId.ToString(),
			*RarityToString(Consumable.Rarity),
			Consumable.Cost,
			Consumable.Tags.Num(),
			Consumable.EffectIds.Num(),
			*Consumable.Summary);
	}

	for (const FGambitDebugScoreLine& Line : Snapshot.ScoreLines)
	{
		UE_LOG(
			LogGambit,
			Log,
			TEXT("DebugReport: %s ScoreLine #%d Type=%s Source=%s Add=%.2f DiceDelta=%.2f Mult=%.2f Before=%.2f After=%.2f Summary=%s"),
			*BuildMatchDebugPlayerLabel(PlayerState, Players),
			Line.Sequence,
			*MatchDebugEnumToDebugString(Line.LineType),
			*Line.SourceName,
			Line.AdditiveDelta,
			Line.DiceContributionDelta,
			Line.MultiplierValue,
			Line.ScoreBefore,
			Line.ScoreAfter,
			*Line.Summary);
	}

	for (const FGambitDebugGoldLine& Line : Snapshot.GoldLines)
	{
		UE_LOG(
			LogGambit,
			Log,
			TEXT("DebugReport: %s GoldLine #%d Type=%s Source=%s Requested=%+d Actual=%+d Before=%d After=%d Clamped=%s Summary=%s"),
			*BuildMatchDebugPlayerLabel(PlayerState, Players),
			Line.Sequence,
			*MatchDebugEnumToDebugString(Line.LineType),
			*Line.SourceName,
			Line.RequestedDelta,
			Line.ActualDelta,
			Line.GoldBefore,
			Line.GoldAfter,
			Line.bClamped ? TEXT("Yes") : TEXT("No"),
			*Line.Summary);
	}

	for (const FGambitDebugShopLine& Line : Snapshot.ShopLines)
	{
		UE_LOG(
			LogGambit,
			Log,
			TEXT("DebugReport: %s ShopLine #%d Type=%s Offer=%d Item=%s Base=%d Final=%d Discount=%.2f Surcharge=%.2f Cashback=%d Shared=%s Result=%s Failure=%s Summary=%s"),
			*BuildMatchDebugPlayerLabel(PlayerState, Players),
			Line.Sequence,
			*MatchDebugEnumToDebugString(Line.LineType),
			Line.OfferId,
			*Line.ItemName,
			Line.BasePrice,
			Line.ResolvedPrice,
			Line.DiscountPercent,
			Line.SurchargePercent,
			Line.CashbackGold,
			Line.bUsesSharedPool ? TEXT("Yes") : TEXT("No"),
			Line.bPurchaseSucceeded ? TEXT("Success") : (Line.bRefused ? TEXT("Refused") : TEXT("Info")),
			*Line.FailureReason,
			*Line.Summary);
	}

	for (const FGambitDebugEffectEvent& Event : Snapshot.EffectEvents)
	{
		UE_LOG(
			LogGambit,
			Log,
			TEXT("DebugReport: %s EffectEvent #%d Category=%s Hook=%s Source=%s Effect=%s Type=%s Target=%s Triggered=%s Negative=%s Prevented=%s Summary=%s"),
			*BuildMatchDebugPlayerLabel(PlayerState, Players),
			Event.Sequence,
			*MatchDebugEnumToDebugString(Event.Category),
			*Event.HookId.ToString(),
			*Event.SourceName,
			*Event.EffectId.ToString(),
			*Event.EffectTypeName,
			*Event.TargetName,
			Event.bTriggered ? TEXT("Yes") : TEXT("No"),
			Event.bNegative ? TEXT("Yes") : TEXT("No"),
			Event.bPrevented ? TEXT("Yes") : TEXT("No"),
			*Event.Summary);
	}
}

void UGambitMatchDebugComponent::PrintAllPlayerDebugReports() const
{
	for (AGambitPlayerState* PlayerState : GetAllPlayers())
	{
		PrintPlayerDebugReport(PlayerState);
	}
}

void UGambitMatchDebugComponent::DebugReadyAllPlayers()
{
	const AGambitGameState* GameState = GetGambitGameState();
	const EGambitRoundPhase Phase = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	UE_LOG(LogGambit, Log, TEXT("DebugMatch: Marking all players ready in phase %s"), *MatchDebugPhaseToString(Phase));
	SetAllPlayersReady(true);
}

void UGambitMatchDebugComponent::DebugAutoAdvanceCurrentPhase()
{
	const AGambitGameState* GameState = GetGambitGameState();
	if (!GameState)
	{
		UE_LOG(LogGambit, Log, TEXT("DebugMatch: DebugAutoAdvanceCurrentPhase failed, missing GambitGameState"));
		return;
	}

	const EGambitRoundPhase CurrentPhase = GameState->GetCurrentPhase();
	switch (CurrentPhase)
	{
	case EGambitRoundPhase::SelectionReroll:
	case EGambitRoundPhase::Action:
	case EGambitRoundPhase::Shop:
		UE_LOG(LogGambit, Log, TEXT("DebugMatch: Auto-advancing ready-gated phase %s"), *MatchDebugPhaseToString(CurrentPhase));
		DebugReadyAllPlayers();
		break;
	default:
		UE_LOG(LogGambit, Log, TEXT("DebugMatch: Phase %s has no debug ready action"), *MatchDebugPhaseToString(CurrentPhase));
		break;
	}
}

void UGambitMatchDebugComponent::DebugAutoAdvanceUntilShopOrMatchEnd()
{
	constexpr int32 MaxIterations = 20;

	for (int32 Iteration = 0; Iteration < MaxIterations; ++Iteration)
	{
		const AGambitGameState* GameState = GetGambitGameState();
		if (!GameState)
		{
			UE_LOG(LogGambit, Log, TEXT("DebugMatch: AutoAdvanceUntilShop stopped, missing GambitGameState"));
			return;
		}

		const EGambitRoundPhase PhaseBefore = GameState->GetCurrentPhase();
		if (PhaseBefore == EGambitRoundPhase::Shop || PhaseBefore == EGambitRoundPhase::None)
		{
			UE_LOG(
				LogGambit,
				Log,
				TEXT("DebugMatch: AutoAdvanceUntilShop stopped at iteration %d because phase is %s"),
				Iteration,
				*MatchDebugPhaseToString(PhaseBefore));
			return;
		}

		DebugAutoAdvanceCurrentPhase();

		const AGambitGameState* UpdatedGameState = GetGambitGameState();
		const EGambitRoundPhase PhaseAfter = UpdatedGameState ? UpdatedGameState->GetCurrentPhase() : EGambitRoundPhase::None;
		if (PhaseAfter == EGambitRoundPhase::Shop || PhaseAfter == EGambitRoundPhase::None)
		{
			UE_LOG(
				LogGambit,
				Log,
				TEXT("DebugMatch: AutoAdvanceUntilShop stopped at iteration %d because phase reached %s"),
				Iteration,
				*MatchDebugPhaseToString(PhaseAfter));
			return;
		}

		if (PhaseAfter == PhaseBefore)
		{
			UE_LOG(
				LogGambit,
				Log,
				TEXT("DebugMatch: AutoAdvanceUntilShop stopped at iteration %d because phase did not change from %s"),
				Iteration,
				*MatchDebugPhaseToString(PhaseBefore));
			return;
		}
	}

	const AGambitGameState* GameState = GetGambitGameState();
	const FString PhaseString = GameState ? MatchDebugPhaseToString(GameState->GetCurrentPhase()) : FString(TEXT("MissingGameState"));
	UE_LOG(
		LogGambit,
		Log,
		TEXT("DebugMatch: AutoAdvanceUntilShop stopped after safety limit %d, current phase %s"),
		MaxIterations,
		*PhaseString);
}

void UGambitMatchDebugComponent::DebugBuyFirstAffordableOfferForAllPlayers()
{
	const AGambitGameState* GameState = GetGambitGameState();
	if (!GameState || GameState->GetCurrentPhase() != EGambitRoundPhase::Shop)
	{
		const FString PhaseString = GameState ? MatchDebugPhaseToString(GameState->GetCurrentPhase()) : FString(TEXT("MissingGameState"));
		UE_LOG(
			LogGambit,
			Log,
			TEXT("DebugMatch: BuyFirstAffordableOfferAll skipped because phase is %s"),
			*PhaseString);
		return;
	}

	AGambitGameMode* GameMode = GetGambitGameMode();
	if (!GameMode)
	{
		UE_LOG(LogGambit, Log, TEXT("DebugMatch: BuyFirstAffordableOfferAll failed, missing GambitGameMode"));
		return;
	}

	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	UGambitSharedPoolComponent* SharedPoolComponent = GameState->GetSharedPoolComponent();
	for (AGambitPlayerState* PlayerState : Players)
	{
		if (!PlayerState)
		{
			continue;
		}

		const TArray<FGambitShopOffer>& Offers = PlayerState->GetCurrentShopOffersRef();
		const int32 GoldBefore = PlayerState->GetCurrentGold();
		const FGambitShopOffer* ChosenOffer = nullptr;

		for (const FGambitShopOffer& Offer : Offers)
		{
			if (Offer.Price <= GoldBefore)
			{
				ChosenOffer = &Offer;
				break;
			}
		}

		if (!ChosenOffer)
		{
			UE_LOG(
				LogGambit,
				Log,
				TEXT("DebugMatch: %s has no affordable offer at Gold=%d"),
				*BuildMatchDebugPlayerLabel(PlayerState, Players),
				GoldBefore);
			continue;
		}

		const FGambitShopOffer OfferCopy = *ChosenOffer;
		const FString PredictedFailureReason = BuildPurchaseFailureReason(PlayerState, &OfferCopy, OfferCopy.OfferId, SharedPoolComponent);
		const bool bPurchased = GameMode->RequestPurchaseOffer(PlayerState, OfferCopy.OfferId);
		const FString ResultReason = bPurchased ? FString(TEXT("Success")) : PredictedFailureReason;
		UE_LOG(
			LogGambit,
			Log,
			TEXT("DebugMatch: BuyFirstOffer Player=%s GoldBefore=%d OfferId=%d Item=%s Type=%s Cost=%d Result=%s Reason=%s GoldAfter=%d"),
			*BuildMatchDebugPlayerLabel(PlayerState, Players),
			GoldBefore,
			OfferCopy.OfferId,
			*FormatMatchDebugItemName(OfferCopy.ItemDefinition),
			OfferCopy.ItemDefinition ? *MatchDebugItemTypeToString(OfferCopy.ItemDefinition->ItemType) : TEXT("None"),
			OfferCopy.Price,
			bPurchased ? TEXT("Success") : TEXT("Failure"),
			*ResultReason,
			PlayerState->GetCurrentGold());
		LogInventoryStateForPlayer(PlayerState, Players, TEXT("DebugMatch: AfterPurchase"));
	}
}

void UGambitMatchDebugComponent::DebugSkipShopForAllPlayers()
{
	const AGambitGameState* GameState = GetGambitGameState();
	if (!GameState || GameState->GetCurrentPhase() != EGambitRoundPhase::Shop)
	{
		const FString PhaseString = GameState ? MatchDebugPhaseToString(GameState->GetCurrentPhase()) : FString(TEXT("MissingGameState"));
		UE_LOG(
			LogGambit,
			Log,
			TEXT("DebugMatch: SkipShopAll skipped because phase is %s"),
			*PhaseString);
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DebugMatch: Skipping shop for all players"));
	SetAllPlayersReady(true);
}

void UGambitMatchDebugComponent::PrintDebugCommandHelp() const
{
	UE_LOG(LogGambit, Log, TEXT("Gambit debug commands:"));
	UE_LOG(LogGambit, Log, TEXT("  Gambit - print this help"));
	UE_LOG(LogGambit, Log, TEXT("  GambitPrintMatch - print match and player snapshots"));
	UE_LOG(LogGambit, Log, TEXT("  GambitValidateData - validate configured DataAssets and print known dice/modules/consumables/effects"));
	UE_LOG(LogGambit, Log, TEXT("  GambitPrintInventory - print all player inventories"));
	UE_LOG(LogGambit, Log, TEXT("  GambitPrintSharedPool - print tracked shared stock"));
	UE_LOG(LogGambit, Log, TEXT("  GambitReadyAll - mark all players ready"));
	UE_LOG(LogGambit, Log, TEXT("  GambitAutoAdvance - auto-ready the current ready-gated phase"));
	UE_LOG(LogGambit, Log, TEXT("  GambitAutoToShop - advance until Shop or match end"));
	UE_LOG(LogGambit, Log, TEXT("  GambitBuyFirstOfferAll - buy first affordable offer for each player"));
	UE_LOG(LogGambit, Log, TEXT("  GambitSkipShop - mark all players ready in Shop"));
	UE_LOG(LogGambit, Log, TEXT("  GambitGrantGold <Amount> - add gold to all players"));
	UE_LOG(LogGambit, Log, TEXT("  GambitGrantConsumable <PlayerIndex> <ConsumableId> - add one catalog consumable to a player"));
	UE_LOG(LogGambit, Log, TEXT("  GambitPrintShop - print current shop offers"));
	UE_LOG(LogGambit, Log, TEXT("  GambitBuyOffer <PlayerIndex> <OfferId> - buy one offer"));
	UE_LOG(LogGambit, Log, TEXT("  GambitRerollPlayer <PlayerIndex> - reroll one player's unlocked dice"));
	UE_LOG(LogGambit, Log, TEXT("  GambitLockDie <PlayerIndex> <DieIndex> - lock one die"));
	UE_LOG(LogGambit, Log, TEXT("  GambitUseConsumable <PlayerIndex> <SlotIndex> - use one consumable slot"));
	UE_LOG(LogGambit, Log, TEXT("  GambitUseConsumableOnDie <PlayerIndex> <SlotIndex> <DieIndex> - use one consumable on a selected die"));
	UE_LOG(LogGambit, Log, TEXT("  GambitAutoFullMatch - auto-play ready gates and shops until match end"));
	UE_LOG(LogGambit, Log, TEXT("  GambitAIDecideRerolls - debug AI locks and rerolls in SelectionReroll"));
	UE_LOG(LogGambit, Log, TEXT("  GambitAIDecideActions - debug AI uses one consumable per player in Action"));
	UE_LOG(LogGambit, Log, TEXT("  GambitAIDecideShop - debug AI buys one useful shop offer per player"));
	UE_LOG(LogGambit, Log, TEXT("  GambitAIFullRound - debug AI plays until the next round or match end"));
	UE_LOG(LogGambit, Log, TEXT("  GambitAIFullMatch - debug AI plays the full match until match end"));
}

void UGambitMatchDebugComponent::DebugGrantGoldToAllPlayers(const int32 Amount)
{
	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	UE_LOG(LogGambit, Log, TEXT("DebugMatch: GrantGold Amount=%d PlayerCount=%d"), Amount, Players.Num());

	for (AGambitPlayerState* PlayerState : Players)
	{
		if (!PlayerState)
		{
			continue;
		}

		const int32 GoldBefore = PlayerState->GetCurrentGold();
		const int32 GoldAfter = PlayerState->AddGold(Amount);
		UE_LOG(
			LogGambit,
			Log,
			TEXT("DebugMatch: %s Gold %d -> %d"),
			*BuildMatchDebugPlayerLabel(PlayerState, Players),
			GoldBefore,
			GoldAfter);
	}
}

void UGambitMatchDebugComponent::DebugGrantConsumable(const int32 PlayerIndex, const FName ConsumableId)
{
	AGambitPlayerState* PlayerState = GetPlayerByIndex(PlayerIndex);
	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	const UGambitStaticDataSettings* Settings = UGambitStaticDataSettings::Get();
	const UGambitItemCatalogDataAsset* ItemCatalog = Settings ? Settings->DefaultItemCatalog.LoadSynchronous() : nullptr;
	UGambitItemDefinition* ItemDefinition = ResolveMatchDebugItemFromCatalog(ConsumableId, ItemCatalog);
	UGambitConsumableDefinition* ConsumableDefinition = Cast<UGambitConsumableDefinition>(ItemDefinition);
	UGambitInventoryComponent* InventoryComponent = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;

	if (!PlayerState || !InventoryComponent || ConsumableId.IsNone() || !ConsumableDefinition)
	{
		UE_LOG(
			LogGambit,
			Log,
			TEXT("DebugMatch: GrantConsumable failed PlayerIndex=%d ConsumableId=%s PlayerValid=%s CatalogLoaded=%s"),
			PlayerIndex,
			*ConsumableId.ToString(),
			PlayerState ? TEXT("Yes") : TEXT("No"),
			ItemCatalog ? TEXT("Yes") : TEXT("No"));
		return;
	}

	const FGambitPlayerSlotState SlotStateBefore = PlayerState->GetSlotState();
	const bool bAdded = InventoryComponent->AddConsumable(ConsumableDefinition);
	const FGambitPlayerSlotState SlotStateAfter = PlayerState->GetSlotState();
	UE_LOG(
		LogGambit,
		Log,
		TEXT("DebugMatch: GrantConsumable Player=%s Item=%s Result=%s Consumables=%d/%d -> %d/%d"),
		*BuildMatchDebugPlayerLabel(PlayerState, Players),
		*FormatItemDetails(ConsumableDefinition),
		bAdded ? TEXT("Success") : TEXT("Failure"),
		SlotStateBefore.ConsumableSlotsUsed,
		SlotStateBefore.ConsumableSlotsCapacity,
		SlotStateAfter.ConsumableSlotsUsed,
		SlotStateAfter.ConsumableSlotsCapacity);
	LogInventoryStateForPlayer(PlayerState, Players, TEXT("DebugMatch: AfterGrantConsumable"));
}

void UGambitMatchDebugComponent::DebugPrintShop() const
{
	const AGambitGameState* GameState = GetGambitGameState();
	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	const FString PhaseString = GameState ? MatchDebugPhaseToString(GameState->GetCurrentPhase()) : FString(TEXT("MissingGameState"));
	const UGambitSharedPoolComponent* SharedPoolComponent = GameState ? GameState->GetSharedPoolComponent() : nullptr;

	UE_LOG(LogGambit, Log, TEXT("DebugMatch: PrintShop Phase=%s PlayerCount=%d"), *PhaseString, Players.Num());
	for (AGambitPlayerState* PlayerState : Players)
	{
		if (!PlayerState)
		{
			continue;
		}

		const TArray<FGambitShopOffer>& Offers = PlayerState->GetCurrentShopOffersRef();
		UE_LOG(
			LogGambit,
			Log,
			TEXT("DebugMatch: %s Gold=%d ShopOfferCount=%d"),
			*BuildMatchDebugPlayerLabel(PlayerState, Players),
			PlayerState->GetCurrentGold(),
			Offers.Num());

		if (Offers.Num() == 0)
		{
			UE_LOG(LogGambit, Log, TEXT("DebugMatch: %s ShopOffers=None"), *BuildMatchDebugPlayerLabel(PlayerState, Players));
			continue;
		}

		for (const FGambitShopOffer& Offer : Offers)
		{
			UE_LOG(LogGambit, Log, TEXT("DebugMatch: %s ShopOffer %s"), *BuildMatchDebugPlayerLabel(PlayerState, Players), *FormatOfferForPlayer(Offer, PlayerState, SharedPoolComponent));
			if (Offer.ItemDefinition && Offer.ItemDefinition->bUsesSharedPool && SharedPoolComponent && !SharedPoolComponent->IsItemTracked(Offer.ItemDefinition))
			{
				UE_LOG(
					LogGambit,
					Warning,
					TEXT("DebugMatch: Shop warning %s offers shared-pool item %s but it is not tracked"),
					*BuildMatchDebugPlayerLabel(PlayerState, Players),
					*Offer.ItemDefinition->GetResolvedItemId().ToString());
			}
		}
	}
}

void UGambitMatchDebugComponent::DebugBuyOffer(const int32 PlayerIndex, const int32 OfferId)
{
	AGambitGameMode* GameMode = GetGambitGameMode();
	AGambitGameState* GameState = GetGambitGameState();
	AGambitPlayerState* PlayerState = GetPlayerByIndex(PlayerIndex);
	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	UGambitSharedPoolComponent* SharedPoolComponent = GameState ? GameState->GetSharedPoolComponent() : nullptr;

	if (!GameMode || !PlayerState)
	{
		UE_LOG(LogGambit, Log, TEXT("DebugMatch: BuyOffer failed PlayerIndex=%d OfferId=%d"), PlayerIndex, OfferId);
		return;
	}

	FGambitShopOffer OfferSnapshot;
	bool bFoundOffer = false;
	if (const FGambitShopOffer* FoundOffer = PlayerState->GetCurrentShopOffersRef().FindByPredicate([OfferId](const FGambitShopOffer& Offer)
	{
		return Offer.OfferId == OfferId;
	}))
	{
		OfferSnapshot = *FoundOffer;
		bFoundOffer = true;
	}

	const int32 GoldBefore = PlayerState->GetCurrentGold();
	const FString PredictedFailureReason = BuildPurchaseFailureReason(PlayerState, bFoundOffer ? &OfferSnapshot : nullptr, OfferId, SharedPoolComponent);
	const bool bPurchased = GameMode->RequestPurchaseOffer(PlayerState, OfferId);
	const FString ResultReason = bPurchased ? FString(TEXT("Success")) : PredictedFailureReason;
	UE_LOG(
		LogGambit,
		Log,
		TEXT("DebugMatch: BuyOffer Player=%s GoldBefore=%d OfferId=%d Item=%s Type=%s Cost=%d Result=%s Reason=%s GoldAfter=%d"),
		*BuildMatchDebugPlayerLabel(PlayerState, Players),
		GoldBefore,
		OfferId,
		bFoundOffer ? *FormatMatchDebugItemName(OfferSnapshot.ItemDefinition) : TEXT("None"),
		(bFoundOffer && OfferSnapshot.ItemDefinition) ? *MatchDebugItemTypeToString(OfferSnapshot.ItemDefinition->ItemType) : TEXT("None"),
		bFoundOffer ? OfferSnapshot.Price : 0,
		bPurchased ? TEXT("Success") : TEXT("Failure"),
		*ResultReason,
		PlayerState->GetCurrentGold());
	LogInventoryStateForPlayer(PlayerState, Players, TEXT("DebugMatch: AfterPurchase"));
}

void UGambitMatchDebugComponent::DebugRerollPlayer(const int32 PlayerIndex)
{
	AGambitGameMode* GameMode = GetGambitGameMode();
	AGambitPlayerState* PlayerState = GetPlayerByIndex(PlayerIndex);
	const TArray<AGambitPlayerState*> Players = GetAllPlayers();

	if (!GameMode || !PlayerState)
	{
		UE_LOG(LogGambit, Log, TEXT("DebugMatch: RerollPlayer failed PlayerIndex=%d"), PlayerIndex);
		return;
	}

	const bool bRerolled = GameMode->RequestReroll(PlayerState);
	UE_LOG(
		LogGambit,
		Log,
		TEXT("DebugMatch: RerollPlayer Player=%s Result=%s Dice=%s"),
		*BuildMatchDebugPlayerLabel(PlayerState, Players),
		bRerolled ? TEXT("Success") : TEXT("Failure"),
		*FormatMatchDebugDiceValues(PlayerState->GetDiceStatesRef()));
}

void UGambitMatchDebugComponent::DebugLockDie(const int32 PlayerIndex, const int32 DieIndex)
{
	AGambitGameMode* GameMode = GetGambitGameMode();
	AGambitPlayerState* PlayerState = GetPlayerByIndex(PlayerIndex);
	const TArray<AGambitPlayerState*> Players = GetAllPlayers();

	if (!GameMode || !PlayerState)
	{
		UE_LOG(LogGambit, Log, TEXT("DebugMatch: LockDie failed PlayerIndex=%d DieIndex=%d"), PlayerIndex, DieIndex);
		return;
	}

	const bool bLocked = GameMode->RequestSetDieLocked(PlayerState, DieIndex, true);
	UE_LOG(
		LogGambit,
		Log,
		TEXT("DebugMatch: LockDie Player=%s DieIndex=%d Result=%s Dice=%s"),
		*BuildMatchDebugPlayerLabel(PlayerState, Players),
		DieIndex,
		bLocked ? TEXT("Success") : TEXT("Failure"),
		*FormatMatchDebugDiceValues(PlayerState->GetDiceStatesRef()));
}

void UGambitMatchDebugComponent::DebugUseConsumable(const int32 PlayerIndex, const int32 SlotIndex)
{
	AGambitGameMode* GameMode = GetGambitGameMode();
	AGambitPlayerState* PlayerState = GetPlayerByIndex(PlayerIndex);
	const TArray<AGambitPlayerState*> Players = GetAllPlayers();

	if (!GameMode || !PlayerState)
	{
		UE_LOG(LogGambit, Log, TEXT("DebugMatch: UseConsumable failed PlayerIndex=%d SlotIndex=%d"), PlayerIndex, SlotIndex);
		return;
	}

	const bool bUsed = GameMode->RequestUseConsumable(PlayerState, SlotIndex);
	UE_LOG(
		LogGambit,
		Log,
		TEXT("DebugMatch: UseConsumable Player=%s SlotIndex=%d Result=%s"),
		*BuildMatchDebugPlayerLabel(PlayerState, Players),
		SlotIndex,
			bUsed ? TEXT("Success") : TEXT("Failure"));
}

void UGambitMatchDebugComponent::DebugUseConsumableOnDie(const int32 PlayerIndex, const int32 SlotIndex, const int32 DieIndex)
{
	AGambitGameMode* GameMode = GetGambitGameMode();
	AGambitPlayerState* PlayerState = GetPlayerByIndex(PlayerIndex);
	const TArray<AGambitPlayerState*> Players = GetAllPlayers();

	if (!GameMode || !PlayerState)
	{
		UE_LOG(LogGambit, Log, TEXT("DebugMatch: UseConsumableOnDie failed PlayerIndex=%d SlotIndex=%d DieIndex=%d"), PlayerIndex, SlotIndex, DieIndex);
		return;
	}

	const bool bUsed = GameMode->RequestUseConsumableOnTargetSelectedDie(PlayerState, SlotIndex, PlayerState, DieIndex);
	UE_LOG(
		LogGambit,
		Log,
		TEXT("DebugMatch: UseConsumableOnDie Player=%s SlotIndex=%d DieIndex=%d Result=%s Dice=%s"),
		*BuildMatchDebugPlayerLabel(PlayerState, Players),
		SlotIndex,
		DieIndex,
		bUsed ? TEXT("Success") : TEXT("Failure"),
		*FormatMatchDebugDiceValues(PlayerState->GetDiceStatesRef()));
}

void UGambitMatchDebugComponent::DebugAutoFullMatch()
{
	const UGambitGameBalanceSettings* Settings = UGambitGameBalanceSettings::Get();
	const int32 MaxIterations = FMath::Max(50, Settings->RoundCount * 12 + 20);

	UE_LOG(LogGambit, Log, TEXT("DebugMatch: AutoFullMatch started MaxIterations=%d"), MaxIterations);
	for (int32 Iteration = 0; Iteration < MaxIterations; ++Iteration)
	{
		const AGambitGameState* GameState = GetGambitGameState();
		if (!GameState)
		{
			UE_LOG(LogGambit, Log, TEXT("DebugMatch: AutoFullMatch stopped, missing GambitGameState"));
			return;
		}

		const EGambitRoundPhase PhaseBefore = GameState->GetCurrentPhase();
		if (PhaseBefore == EGambitRoundPhase::None)
		{
			UE_LOG(LogGambit, Log, TEXT("DebugMatch: AutoFullMatch stopped at iteration %d, match ended"), Iteration);
			PrintFinalMatchSummary();
			return;
		}

		if (PhaseBefore == EGambitRoundPhase::Shop)
		{
			DebugBuyFirstAffordableOfferForAllPlayers();
			DebugSkipShopForAllPlayers();
		}
		else
		{
			DebugAutoAdvanceCurrentPhase();
		}

		const AGambitGameState* UpdatedGameState = GetGambitGameState();
		const EGambitRoundPhase PhaseAfter = UpdatedGameState ? UpdatedGameState->GetCurrentPhase() : EGambitRoundPhase::None;
		if (PhaseAfter == EGambitRoundPhase::None)
		{
			UE_LOG(LogGambit, Log, TEXT("DebugMatch: AutoFullMatch reached match end at iteration %d"), Iteration);
			PrintFinalMatchSummary();
			return;
		}

		if (PhaseAfter == PhaseBefore)
		{
			UE_LOG(
				LogGambit,
				Log,
				TEXT("DebugMatch: AutoFullMatch stopped at iteration %d because phase did not change from %s"),
				Iteration,
				*MatchDebugPhaseToString(PhaseBefore));
			PrintFinalMatchSummary();
			return;
		}
	}

	const AGambitGameState* GameState = GetGambitGameState();
	const FString PhaseString = GameState ? MatchDebugPhaseToString(GameState->GetCurrentPhase()) : FString(TEXT("MissingGameState"));
	UE_LOG(LogGambit, Log, TEXT("DebugMatch: AutoFullMatch stopped at safety limit, phase=%s"), *PhaseString);
	PrintFinalMatchSummary();
}

void UGambitMatchDebugComponent::DebugAIDecideRerolls()
{
	if (UGambitDebugAutoPlayer* AutoPlayer = GetOrCreateDebugAutoPlayer())
	{
		AutoPlayer->DecideRerolls(GetGambitGameMode());
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DebugAI: AIDecideRerolls failed, missing debug auto player"));
}

void UGambitMatchDebugComponent::DebugAIDecideActions()
{
	if (UGambitDebugAutoPlayer* AutoPlayer = GetOrCreateDebugAutoPlayer())
	{
		AutoPlayer->DecideActions(GetGambitGameMode());
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DebugAI: AIDecideActions failed, missing debug auto player"));
}

void UGambitMatchDebugComponent::DebugAIDecideShop()
{
	if (UGambitDebugAutoPlayer* AutoPlayer = GetOrCreateDebugAutoPlayer())
	{
		AutoPlayer->DecideShop(GetGambitGameMode());
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DebugAI: AIDecideShop failed, missing debug auto player"));
}

void UGambitMatchDebugComponent::DebugAIFullRound()
{
	constexpr int32 MaxIterations = 30;
	const AGambitGameState* InitialGameState = GetGambitGameState();
	if (!InitialGameState)
	{
		UE_LOG(LogGambit, Log, TEXT("DebugAI: AIFullRound stopped, missing GambitGameState"));
		return;
	}

	const int32 StartRound = InitialGameState->GetCurrentRoundIndex();
	UE_LOG(
		LogGambit,
		Log,
		TEXT("DebugAI: AIFullRound started Round=%d Phase=%s MaxIterations=%d"),
		StartRound,
		*MatchDebugPhaseToString(InitialGameState->GetCurrentPhase()),
		MaxIterations);

	for (int32 Iteration = 0; Iteration < MaxIterations; ++Iteration)
	{
		const AGambitGameState* GameState = GetGambitGameState();
		if (!GameState)
		{
			UE_LOG(LogGambit, Log, TEXT("DebugAI: AIFullRound stopped at iteration %d, missing GambitGameState"), Iteration);
			return;
		}

		const EGambitRoundPhase PhaseBefore = GameState->GetCurrentPhase();
		if (PhaseBefore == EGambitRoundPhase::None)
		{
			UE_LOG(LogGambit, Log, TEXT("DebugAI: AIFullRound reached match end at iteration %d"), Iteration);
			PrintFinalMatchSummary();
			return;
		}

		if (GameState->GetCurrentRoundIndex() > StartRound)
		{
			UE_LOG(
				LogGambit,
				Log,
				TEXT("DebugAI: AIFullRound completed at iteration %d NewRound=%d Phase=%s"),
				Iteration,
				GameState->GetCurrentRoundIndex(),
				*MatchDebugPhaseToString(PhaseBefore));
			return;
		}

		switch (PhaseBefore)
		{
		case EGambitRoundPhase::SelectionReroll:
			DebugAIDecideRerolls();
			SetAllPlayersReady(true);
			break;
		case EGambitRoundPhase::Action:
			DebugAIDecideActions();
			SetAllPlayersReady(true);
			break;
		case EGambitRoundPhase::Shop:
			DebugAIDecideShop();
			DebugSkipShopForAllPlayers();
			break;
		default:
			DebugAutoAdvanceCurrentPhase();
			break;
		}

		const AGambitGameState* UpdatedGameState = GetGambitGameState();
		const EGambitRoundPhase PhaseAfter = UpdatedGameState ? UpdatedGameState->GetCurrentPhase() : EGambitRoundPhase::None;
		if (PhaseAfter == EGambitRoundPhase::None)
		{
			UE_LOG(LogGambit, Log, TEXT("DebugAI: AIFullRound reached match end at iteration %d"), Iteration);
			PrintFinalMatchSummary();
			return;
		}

		if (UpdatedGameState && UpdatedGameState->GetCurrentRoundIndex() > StartRound)
		{
			UE_LOG(
				LogGambit,
				Log,
				TEXT("DebugAI: AIFullRound completed at iteration %d NewRound=%d Phase=%s"),
				Iteration,
				UpdatedGameState->GetCurrentRoundIndex(),
				*MatchDebugPhaseToString(PhaseAfter));
			return;
		}

		if (PhaseAfter == PhaseBefore)
		{
			UE_LOG(
				LogGambit,
				Log,
				TEXT("DebugAI: AIFullRound stopped at iteration %d because phase did not change from %s"),
				Iteration,
				*MatchDebugPhaseToString(PhaseBefore));
			return;
		}
	}

	const AGambitGameState* GameState = GetGambitGameState();
	const FString PhaseString = GameState ? MatchDebugPhaseToString(GameState->GetCurrentPhase()) : FString(TEXT("MissingGameState"));
	UE_LOG(LogGambit, Log, TEXT("DebugAI: AIFullRound stopped at safety limit, phase=%s"), *PhaseString);
}

void UGambitMatchDebugComponent::DebugAIFullMatch()
{
	const UGambitGameBalanceSettings* Settings = UGambitGameBalanceSettings::Get();
	const int32 MaxIterations = FMath::Max(100, Settings->RoundCount * 16 + 40);

	UE_LOG(LogGambit, Log, TEXT("DebugAI: AIFullMatch started MaxIterations=%d"), MaxIterations);
	for (int32 Iteration = 0; Iteration < MaxIterations; ++Iteration)
	{
		const AGambitGameState* GameState = GetGambitGameState();
		if (!GameState)
		{
			UE_LOG(LogGambit, Log, TEXT("DebugAI: AIFullMatch stopped, missing GambitGameState"));
			return;
		}

		const EGambitRoundPhase PhaseBefore = GameState->GetCurrentPhase();
		if (PhaseBefore == EGambitRoundPhase::None)
		{
			UE_LOG(LogGambit, Log, TEXT("DebugAI: AIFullMatch stopped at iteration %d, match ended"), Iteration);
			PrintFinalMatchSummary();
			return;
		}

		switch (PhaseBefore)
		{
		case EGambitRoundPhase::SelectionReroll:
			DebugAIDecideRerolls();
			SetAllPlayersReady(true);
			break;
		case EGambitRoundPhase::Action:
			DebugAIDecideActions();
			SetAllPlayersReady(true);
			break;
		case EGambitRoundPhase::Shop:
			DebugAIDecideShop();
			DebugSkipShopForAllPlayers();
			break;
		default:
			DebugAutoAdvanceCurrentPhase();
			break;
		}

		const AGambitGameState* UpdatedGameState = GetGambitGameState();
		const EGambitRoundPhase PhaseAfter = UpdatedGameState ? UpdatedGameState->GetCurrentPhase() : EGambitRoundPhase::None;
		if (PhaseAfter == EGambitRoundPhase::None)
		{
			UE_LOG(LogGambit, Log, TEXT("DebugAI: AIFullMatch reached match end at iteration %d"), Iteration);
			PrintFinalMatchSummary();
			return;
		}

		if (PhaseAfter == PhaseBefore)
		{
			UE_LOG(
				LogGambit,
				Log,
				TEXT("DebugAI: AIFullMatch stopped at iteration %d because phase did not change from %s"),
				Iteration,
				*MatchDebugPhaseToString(PhaseBefore));
			PrintFinalMatchSummary();
			return;
		}
	}

	const AGambitGameState* GameState = GetGambitGameState();
	const FString PhaseString = GameState ? MatchDebugPhaseToString(GameState->GetCurrentPhase()) : FString(TEXT("MissingGameState"));
	UE_LOG(LogGambit, Log, TEXT("DebugAI: AIFullMatch stopped at safety limit, phase=%s"), *PhaseString);
	PrintFinalMatchSummary();
}

void UGambitMatchDebugComponent::DebugValidateData() const
{
	const UGambitStaticDataSettings* Settings = UGambitStaticDataSettings::Get();
	if (!Settings)
	{
		UE_LOG(LogGambit, Warning, TEXT("DebugData: Missing UGambitStaticDataSettings"));
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DebugData: === STATIC DATA VALIDATION START ==="));
	const int32 DefaultActiveDiceCount = FMath::Max(0, UGambitGameBalanceSettings::Get()->DefaultActiveDiceCount);
	const int32 DefaultShopOfferCount = FMath::Max(0, UGambitGameBalanceSettings::Get()->ShopOfferCount);

	const auto LogSoftAssetHeader = [](const TCHAR* Label, const FSoftObjectPath& Path, const UObject* Asset)
	{
		if (Path.IsNull())
		{
			UE_LOG(LogGambit, Warning, TEXT("DebugData: %s is NOT assigned"), Label);
			return;
		}

		if (Asset)
		{
			UE_LOG(LogGambit, Log, TEXT("DebugData: %s assigned Path=%s Loaded=%s"), Label, *Path.ToString(), *Asset->GetPathName());
		}
		else
		{
			UE_LOG(LogGambit, Warning, TEXT("DebugData: %s assigned Path=%s Loaded=FAILED_TO_LOAD"), Label, *Path.ToString());
		}
	};

	TArray<const UGambitDiceDefinition*> KnownDiceDefinitions;
	TArray<const UGambitModuleDefinition*> KnownModuleDefinitions;
	TArray<const UGambitConsumableDefinition*> KnownConsumableDefinitions;
	TArray<const UGambitItemDefinition*> KnownDiceItemDefinitions;
	TSet<const UGambitItemEffectDefinition*> KnownEffectDefinitions;
	TSet<const UClass*> KnownEffectClasses;
	TSet<const UObject*> ValidatedObjects;
	TSet<FString> BlockingObjectLabels;

	const auto AddBlockingObject = [&BlockingObjectLabels](const UObject* Object, const FString& FallbackLabel)
	{
		const FString Label = Object ? Object->GetPathName() : FallbackLabel;
		if (!Label.IsEmpty())
		{
			BlockingObjectLabels.Add(Label);
		}
	};

	const auto LogValidationIssues = [&AddBlockingObject](const UObject* Object, const FString& FallbackLabel, const TArray<FGambitDataValidationIssue>& Issues)
	{
		if (Issues.Num() == 0)
		{
			return;
		}

		for (const FGambitDataValidationIssue& Issue : Issues)
		{
			if (Issue.Severity == EGambitDataValidationSeverity::Error)
			{
				UE_LOG(LogGambit, Error, TEXT("DebugDataValidation: %s"), *Issue.Message);
			}
			else if (Issue.Severity == EGambitDataValidationSeverity::Warning)
			{
				UE_LOG(LogGambit, Warning, TEXT("DebugDataValidation: %s"), *Issue.Message);
			}
		}

		if (GambitDataValidation::HasBlockingIssues(Issues))
		{
			AddBlockingObject(Object, FallbackLabel);
		}
	};

	const auto RegisterEffects = [&KnownEffectDefinitions, &KnownEffectClasses](
		const TArray<TObjectPtr<UGambitItemEffectDefinition>>& EffectDefinitions,
		const TArray<TSubclassOf<UGambitItemEffect>>& EffectClasses)
	{
		for (const TObjectPtr<UGambitItemEffectDefinition>& EffectDefinition : EffectDefinitions)
		{
			if (EffectDefinition)
			{
				KnownEffectDefinitions.Add(EffectDefinition.Get());
			}
		}

		for (const TSubclassOf<UGambitItemEffect>& EffectClass : EffectClasses)
		{
			if (EffectClass)
			{
				KnownEffectClasses.Add(EffectClass.Get());
			}
		}
	};

	const auto ValidateDiceDefinition = [&ValidatedObjects, &LogValidationIssues](const UGambitDiceDefinition* DiceDefinition)
	{
		if (!DiceDefinition || ValidatedObjects.Contains(DiceDefinition))
		{
			return;
		}

		ValidatedObjects.Add(DiceDefinition);
		TArray<FGambitDataValidationIssue> Issues;
		GambitDataValidation::ValidateDiceDefinition(DiceDefinition, Issues);
		LogValidationIssues(DiceDefinition, TEXT("MissingDiceDefinition"), Issues);
	};

	const auto ValidateItemDefinition = [&ValidatedObjects, &LogValidationIssues](const UGambitItemDefinition* ItemDefinition)
	{
		if (!ItemDefinition || ValidatedObjects.Contains(ItemDefinition))
		{
			return;
		}

		ValidatedObjects.Add(ItemDefinition);
		TArray<FGambitDataValidationIssue> Issues;
		GambitDataValidation::ValidateItemDefinition(ItemDefinition, Issues);
		LogValidationIssues(ItemDefinition, TEXT("MissingItemDefinition"), Issues);
	};

	const auto AddKnownDice = [&KnownDiceDefinitions, &RegisterEffects, &ValidateDiceDefinition](const UGambitDiceDefinition* DiceDefinition)
	{
		if (!DiceDefinition)
		{
			return;
		}

		KnownDiceDefinitions.AddUnique(DiceDefinition);
		RegisterEffects(DiceDefinition->EffectDefinitions, DiceDefinition->EffectClasses);
		ValidateDiceDefinition(DiceDefinition);
	};

	const auto AddKnownItem = [
		&KnownModuleDefinitions,
		&KnownConsumableDefinitions,
		&KnownDiceItemDefinitions,
		&RegisterEffects,
		&ValidateItemDefinition,
		&AddKnownDice](const UGambitItemDefinition* ItemDefinition)
	{
		if (!ItemDefinition)
		{
			return;
		}

		RegisterEffects(ItemDefinition->EffectDefinitions, ItemDefinition->EffectClasses);
		ValidateItemDefinition(ItemDefinition);

		if (const UGambitModuleDefinition* ModuleDefinition = Cast<UGambitModuleDefinition>(ItemDefinition))
		{
			KnownModuleDefinitions.AddUnique(ModuleDefinition);
			return;
		}

		if (const UGambitConsumableDefinition* ConsumableDefinition = Cast<UGambitConsumableDefinition>(ItemDefinition))
		{
			KnownConsumableDefinitions.AddUnique(ConsumableDefinition);
			return;
		}

		if (const UGambitDiceItemDefinition* DiceItemDefinition = Cast<UGambitDiceItemDefinition>(ItemDefinition))
		{
			KnownDiceItemDefinitions.AddUnique(DiceItemDefinition);
			AddKnownDice(DiceItemDefinition->GrantedDiceDefinition);
		}
	};

	const UGambitDiceCatalogDataAsset* DiceCatalog = Settings->DefaultDiceCatalog.LoadSynchronous();
	LogSoftAssetHeader(TEXT("DefaultDiceCatalog"), Settings->DefaultDiceCatalog.ToSoftObjectPath(), DiceCatalog);
	if (DiceCatalog)
	{
		int32 UsefulEntries = 0;
		UE_LOG(LogGambit, Log, TEXT("DebugData: DiceCatalog Entries=%d"), DiceCatalog->DiceEntries.Num());
		if (DiceCatalog->DiceEntries.Num() == 0)
		{
			UE_LOG(LogGambit, Warning, TEXT("DebugData: DiceCatalog is empty"));
		}

		for (const FGambitDiceCatalogEntry& Entry : DiceCatalog->DiceEntries)
		{
			const UGambitDiceDefinition* DiceDefinition = Entry.DiceDefinition;
			if (!DiceDefinition)
			{
				UE_LOG(LogGambit, Warning, TEXT("DebugData: DiceCatalog entry missing DiceDefinition EntryId=%s"), *Entry.DiceId.ToString());
				continue;
			}

			AddKnownDice(DiceDefinition);
			UsefulEntries++;
			const FName DiceId = !Entry.DiceId.IsNone() ? Entry.DiceId : DiceDefinition->GetResolvedDiceId();
			UE_LOG(
				LogGambit,
				Log,
				TEXT("DebugData: Dice EntryId=%s Type=%s Faces=%s Weights=%d Asset=%s"),
				*DiceId.ToString(),
				*DiceTypeToString(DiceDefinition->DiceType),
				*FormatIntArray(DiceDefinition->GetResolvedFaces()),
				DiceDefinition->FaceWeights.Num(),
				*DiceDefinition->GetPathName());
			if (DiceDefinition->Faces.Num() == 0)
			{
				UE_LOG(LogGambit, Log, TEXT("DebugData: Dice %s uses fallback faces [1,2,3,4,5,6]"), *DiceId.ToString());
			}
		}
		UE_LOG(LogGambit, Log, TEXT("DebugData: DiceCatalog UsefulEntries=%d/%d"), UsefulEntries, DiceCatalog->DiceEntries.Num());
	}

	const UGambitItemCatalogDataAsset* ItemCatalog = Settings->DefaultItemCatalog.LoadSynchronous();
	LogSoftAssetHeader(TEXT("DefaultItemCatalog"), Settings->DefaultItemCatalog.ToSoftObjectPath(), ItemCatalog);
	if (ItemCatalog)
	{
		const auto LogItemCatalogEntries = [ItemCatalog, &AddKnownItem](const TCHAR* Category, const TArray<FGambitItemCatalogEntry>& Entries, const EGambitItemType ExpectedType)
		{
			int32 UsefulEntries = 0;
			UE_LOG(LogGambit, Log, TEXT("DebugData: ItemCatalog %s Entries=%d"), Category, Entries.Num());
			if (Entries.Num() == 0)
			{
				UE_LOG(LogGambit, Warning, TEXT("DebugData: ItemCatalog %s entries are empty"), Category);
			}

			for (const FGambitItemCatalogEntry& Entry : Entries)
			{
				UGambitItemDefinition* ItemDefinition = ResolveMatchDebugCatalogEntryDefinition(Entry.ItemId, Entry.ItemDefinition, ItemCatalog);
				if (!ItemDefinition)
				{
					UE_LOG(LogGambit, Warning, TEXT("DebugData: ItemCatalog %s entry missing ItemDefinition EntryId=%s"), Category, *Entry.ItemId.ToString());
					continue;
				}

				UsefulEntries++;
				AddKnownItem(ItemDefinition);
				const FName ItemId = ResolveMatchDebugCatalogEntryId(Entry.ItemId, ItemDefinition);
				UE_LOG(LogGambit, Log, TEXT("DebugData: %s %s"), Category, *FormatItemDetails(ItemDefinition));
				if (ItemId.IsNone())
				{
					UE_LOG(LogGambit, Warning, TEXT("DebugData: %s item has empty item id Asset=%s"), Category, *ItemDefinition->GetPathName());
				}
				if (ItemDefinition->ItemType != ExpectedType)
				{
					UE_LOG(
						LogGambit,
						Warning,
						TEXT("DebugData: %s item %s has type %s, expected %s"),
						Category,
						*ItemId.ToString(),
						*MatchDebugItemTypeToString(ItemDefinition->ItemType),
						*MatchDebugItemTypeToString(ExpectedType));
				}
				if (ItemDefinition->bUsesSharedPool && ItemDefinition->SharedPoolMaxStock <= 0)
				{
					UE_LOG(LogGambit, Warning, TEXT("DebugData: shared pool item %s has MaxStock <= 0"), *ItemId.ToString());
				}
				if (const UGambitDiceItemDefinition* DiceItemDefinition = Cast<UGambitDiceItemDefinition>(ItemDefinition))
				{
					if (!DiceItemDefinition->GrantedDiceDefinition)
					{
						UE_LOG(LogGambit, Warning, TEXT("DebugData: dice item %s has missing GrantedDiceDefinition"), *ItemId.ToString());
					}
				}
			}
			UE_LOG(LogGambit, Log, TEXT("DebugData: ItemCatalog %s UsefulEntries=%d/%d"), Category, UsefulEntries, Entries.Num());
		};

		LogItemCatalogEntries(TEXT("Modules"), ItemCatalog->ModuleEntries, EGambitItemType::Module);
		LogItemCatalogEntries(TEXT("Consumables"), ItemCatalog->ConsumableEntries, EGambitItemType::Consumable);
		LogItemCatalogEntries(TEXT("DiceItems"), ItemCatalog->DiceItemEntries, EGambitItemType::Dice);
	}

	const UGambitPlayerLoadoutDefinition* PlayerLoadout = Settings->DefaultPlayerLoadout.LoadSynchronous();
	LogSoftAssetHeader(TEXT("DefaultPlayerLoadout"), Settings->DefaultPlayerLoadout.ToSoftObjectPath(), PlayerLoadout);
	if (PlayerLoadout)
	{
		int32 ValidStartingDiceCount = 0;
		UE_LOG(
			LogGambit,
			Log,
			TEXT("DebugData: PlayerLoadout Dice=%d Modules=%d Consumables=%d"),
			PlayerLoadout->Loadout.StartingDice.Num(),
			PlayerLoadout->Loadout.StartingModules.Num(),
			PlayerLoadout->Loadout.StartingConsumables.Num());
		if (PlayerLoadout->Loadout.StartingDice.Num() == 0)
		{
			UE_LOG(
				LogGambit,
				Warning,
				TEXT("DebugData: PlayerLoadout has no starting dice; runtime fallback dice will be used, but DefaultPlayerLoadout is incomplete"));
		}
		for (const UGambitDiceDefinition* DiceDefinition : PlayerLoadout->Loadout.StartingDice)
		{
			UE_LOG(LogGambit, Log, TEXT("DebugData: Loadout Dice %s"), *FormatDiceDefinition(DiceDefinition));
			if (!DiceDefinition)
			{
				UE_LOG(LogGambit, Warning, TEXT("DebugData: PlayerLoadout contains null starting dice"));
			}
			else
			{
				ValidStartingDiceCount++;
				AddKnownDice(DiceDefinition);
			}
		}
		if (ValidStartingDiceCount < DefaultActiveDiceCount)
		{
			UE_LOG(
				LogGambit,
				Warning,
				TEXT("DebugData: PlayerLoadout has %d valid starting dice, below DefaultActiveDiceCount=%d; runtime fallback will fill missing dice"),
				ValidStartingDiceCount,
				DefaultActiveDiceCount);
		}
		for (const UGambitModuleDefinition* ModuleDefinition : PlayerLoadout->Loadout.StartingModules)
		{
			AddKnownItem(ModuleDefinition);
			UE_LOG(LogGambit, Log, TEXT("DebugData: Loadout Module %s"), *FormatItemDetails(ModuleDefinition));
		}
		for (const UGambitConsumableDefinition* ConsumableDefinition : PlayerLoadout->Loadout.StartingConsumables)
		{
			AddKnownItem(ConsumableDefinition);
			UE_LOG(LogGambit, Log, TEXT("DebugData: Loadout Consumable %s"), *FormatItemDetails(ConsumableDefinition));
		}
	}

	const UGambitSharedPoolDefinition* SharedPoolDefinition = Settings->DefaultSharedPoolDefinition.LoadSynchronous();
	LogSoftAssetHeader(TEXT("DefaultSharedPoolDefinition"), Settings->DefaultSharedPoolDefinition.ToSoftObjectPath(), SharedPoolDefinition);
	if (SharedPoolDefinition)
	{
		const UGambitItemCatalogDataAsset* SharedPoolCatalog = SharedPoolDefinition->ItemCatalog ? SharedPoolDefinition->ItemCatalog.Get() : ItemCatalog;
		TArray<FGambitDataValidationIssue> SharedPoolIssues;
		GambitDataValidation::ValidateSharedPoolDefinition(SharedPoolDefinition, SharedPoolCatalog, SharedPoolIssues);
		LogValidationIssues(SharedPoolDefinition, TEXT("DefaultSharedPoolDefinition"), SharedPoolIssues);
		if (!SharedPoolCatalog)
		{
			UE_LOG(
				LogGambit,
				Warning,
				TEXT("DebugData: SharedPoolDefinition cannot resolve item ids because neither SharedPoolDefinition.ItemCatalog nor DefaultItemCatalog is assigned/loaded"));
		}
		UE_LOG(LogGambit, Log, TEXT("DebugData: SharedPool StockEntries=%d"), SharedPoolDefinition->StockEntries.Num());
		if (SharedPoolDefinition->StockEntries.Num() == 0)
		{
			UE_LOG(LogGambit, Warning, TEXT("DebugData: SharedPoolDefinition has no stock entries"));
		}
		for (const FGambitSharedStockEntry& Entry : SharedPoolDefinition->StockEntries)
		{
			UGambitItemDefinition* ItemDefinition = ResolveMatchDebugCatalogEntryDefinition(Entry.ItemId, Entry.ItemDefinition, SharedPoolCatalog);
			AddKnownItem(ItemDefinition);
			const FName ItemId = ResolveMatchDebugCatalogEntryId(Entry.ItemId, ItemDefinition);
			const UGambitItemDefinition* CatalogItemDefinition = ResolveMatchDebugItemFromCatalog(ItemId, SharedPoolCatalog);
			UE_LOG(
				LogGambit,
				Log,
				TEXT("DebugData: SharedPool Entry ItemId=%s Item=%s MaxStock=%d"),
				*ItemId.ToString(),
				*FormatMatchDebugItemName(ItemDefinition),
				Entry.MaxStock);
			if (!ItemDefinition)
			{
				UE_LOG(LogGambit, Warning, TEXT("DebugData: SharedPool entry %s cannot resolve item definition"), *ItemId.ToString());
			}
			if (!ItemId.IsNone() && SharedPoolCatalog && !CatalogItemDefinition)
			{
				UE_LOG(LogGambit, Warning, TEXT("DebugData: SharedPool entry %s is not present in the configured item catalog"), *ItemId.ToString());
			}
			if (ItemId.IsNone())
			{
				UE_LOG(LogGambit, Warning, TEXT("DebugData: SharedPool entry has empty item id"));
			}
			if (Entry.MaxStock <= 0 && (!ItemDefinition || ItemDefinition->SharedPoolMaxStock <= 0))
			{
				UE_LOG(LogGambit, Warning, TEXT("DebugData: SharedPool entry %s has no positive max stock"), *ItemId.ToString());
			}
		}
	}

	const UGambitShopLootTable* ShopLootTable = Settings->DefaultShopLootTable.LoadSynchronous();
	LogSoftAssetHeader(TEXT("DefaultShopLootTable"), Settings->DefaultShopLootTable.ToSoftObjectPath(), ShopLootTable);
	if (ShopLootTable)
	{
		const UGambitItemCatalogDataAsset* LootCatalog = ShopLootTable->ItemCatalog ? ShopLootTable->ItemCatalog.Get() : ItemCatalog;
		TArray<FGambitDataValidationIssue> ShopIssues;
		GambitDataValidation::ValidateShopLootTable(ShopLootTable, LootCatalog, ShopIssues);
		LogValidationIssues(ShopLootTable, TEXT("DefaultShopLootTable"), ShopIssues);
		TSet<FName> ValidWeightedItemIds;
		UE_LOG(LogGambit, Log, TEXT("DebugData: ShopLootTable OfferCountOverride=%d WeightedEntries=%d"), ShopLootTable->OfferCountOverride, ShopLootTable->WeightedItems.Num());
		if (!LootCatalog)
		{
			UE_LOG(
				LogGambit,
				Warning,
				TEXT("DebugData: ShopLootTable cannot resolve item ids because neither ShopLootTable.ItemCatalog nor DefaultItemCatalog is assigned/loaded"));
		}
		if (ShopLootTable->OfferCountOverride != 0 && ShopLootTable->OfferCountOverride != DefaultShopOfferCount)
		{
			UE_LOG(
				LogGambit,
				Warning,
				TEXT("DebugData: ShopLootTable OfferCountOverride=%d; expected 0 or configured ShopOfferCount=%d"),
				ShopLootTable->OfferCountOverride,
				DefaultShopOfferCount);
		}
		if (ShopLootTable->WeightedItems.Num() == 0)
		{
			UE_LOG(LogGambit, Warning, TEXT("DebugData: ShopLootTable has no weighted entries"));
		}
		for (const FGambitShopWeightedEntry& Entry : ShopLootTable->WeightedItems)
		{
			UGambitItemDefinition* ItemDefinition = ResolveMatchDebugCatalogEntryDefinition(Entry.ItemId, Entry.ItemDefinition, LootCatalog);
			AddKnownItem(ItemDefinition);
			const FName ItemId = ResolveMatchDebugCatalogEntryId(Entry.ItemId, ItemDefinition);
			UE_LOG(
				LogGambit,
				Log,
				TEXT("DebugData: ShopWeighted ItemId=%s Item=%s Weight=%.2f PriceOverride=%d"),
				*ItemId.ToString(),
				*FormatMatchDebugItemName(ItemDefinition),
				Entry.Weight,
				Entry.PriceOverride);
			if (!ItemDefinition)
			{
				UE_LOG(LogGambit, Warning, TEXT("DebugData: ShopWeighted entry %s cannot resolve item definition"), *ItemId.ToString());
			}
			else if (!ItemId.IsNone() && Entry.Weight > 0.0f)
			{
				ValidWeightedItemIds.Add(ItemId);
			}
			if (Entry.Weight <= 0.0f)
			{
				UE_LOG(LogGambit, Warning, TEXT("DebugData: ShopWeighted entry %s has non-positive weight"), *ItemId.ToString());
			}
			const int32 ResolvedPrice = Entry.PriceOverride >= 0 ? Entry.PriceOverride : (ItemDefinition ? ItemDefinition->Cost : 0);
			if (ResolvedPrice < 0)
			{
				UE_LOG(LogGambit, Warning, TEXT("DebugData: ShopWeighted entry %s resolves to a negative price"), *ItemId.ToString());
			}
		}

		const int32 RequiredOfferCount = ShopLootTable->OfferCountOverride > 0 ? ShopLootTable->OfferCountOverride : DefaultShopOfferCount;
		if (ValidWeightedItemIds.Num() < RequiredOfferCount)
		{
			UE_LOG(
				LogGambit,
				Warning,
				TEXT("DebugData: ShopLootTable has %d unique valid positive-weight items, below required offer count %d"),
				ValidWeightedItemIds.Num(),
				RequiredOfferCount);
		}
	}

	UE_LOG(LogGambit, Log, TEXT("DebugData: === KNOWN DICE (%d) ==="), KnownDiceDefinitions.Num());
	for (const UGambitDiceDefinition* DiceDefinition : KnownDiceDefinitions)
	{
		UE_LOG(LogGambit, Log, TEXT("DebugData: KnownDice %s"), *FormatDiceDefinition(DiceDefinition));
	}

	UE_LOG(LogGambit, Log, TEXT("DebugData: === KNOWN MODULES (%d) ==="), KnownModuleDefinitions.Num());
	for (const UGambitModuleDefinition* ModuleDefinition : KnownModuleDefinitions)
	{
		UE_LOG(
			LogGambit,
			Log,
			TEXT("DebugData: KnownModule %s"),
			*FormatItemDetails(ModuleDefinition));
	}

	UE_LOG(LogGambit, Log, TEXT("DebugData: === KNOWN CONSUMABLES (%d) ==="), KnownConsumableDefinitions.Num());
	for (const UGambitConsumableDefinition* ConsumableDefinition : KnownConsumableDefinitions)
	{
		UE_LOG(
			LogGambit,
			Log,
			TEXT("DebugData: KnownConsumable %s CanTargetOpponent=%s"),
			*FormatItemDetails(ConsumableDefinition),
			ConsumableDefinition && ConsumableDefinition->bCanTargetOpponent ? TEXT("Yes") : TEXT("No"));
	}

	UE_LOG(LogGambit, Log, TEXT("DebugData: === KNOWN DICE ITEMS (%d) ==="), KnownDiceItemDefinitions.Num());
	for (const UGambitItemDefinition* DiceItemDefinition : KnownDiceItemDefinitions)
	{
		const UGambitDiceItemDefinition* TypedDiceItem = Cast<UGambitDiceItemDefinition>(DiceItemDefinition);
		UE_LOG(
			LogGambit,
			Log,
			TEXT("DebugData: KnownDiceItem %s GrantedDice=%s"),
			*FormatItemDetails(DiceItemDefinition),
			TypedDiceItem && TypedDiceItem->GrantedDiceDefinition
				? *TypedDiceItem->GrantedDiceDefinition->GetResolvedDiceId().ToString()
				: TEXT("None"));
	}

	UE_LOG(
		LogGambit,
		Log,
		TEXT("DebugData: === EFFECTS USED Definitions=%d Classes=%d ==="),
		KnownEffectDefinitions.Num(),
		KnownEffectClasses.Num());
	for (const UGambitItemEffectDefinition* EffectDefinition : KnownEffectDefinitions)
	{
		UE_LOG(
			LogGambit,
			Log,
			TEXT("DebugData: EffectDefinition Id=%s Type=%s Hook=%s Target=%s TargetRule=%s Supported=%s Path=%s"),
			EffectDefinition && !EffectDefinition->EffectId.IsNone() ? *EffectDefinition->EffectId.ToString() : TEXT("None"),
			EffectDefinition ? *GambitDataValidation::EffectTypeToString(EffectDefinition->EffectType) : TEXT("None"),
			EffectDefinition ? *GambitDataValidation::EffectHookToString(EffectDefinition->Hook) : TEXT("None"),
			EffectDefinition ? *GambitDataValidation::EffectTargetToString(EffectDefinition->Target) : TEXT("None"),
			EffectDefinition && !EffectDefinition->TargetRuleId.IsNone() ? *EffectDefinition->TargetRuleId.ToString() : TEXT("None"),
			EffectDefinition && GambitDataValidation::IsSupportedEffectType(EffectDefinition->EffectType) ? TEXT("Yes") : TEXT("No"),
			EffectDefinition ? *EffectDefinition->GetPathName() : TEXT("None"));
	}
	for (const UClass* EffectClass : KnownEffectClasses)
	{
		UE_LOG(
			LogGambit,
			Log,
			TEXT("DebugData: EffectClass %s"),
			EffectClass ? *EffectClass->GetPathName() : TEXT("None"));
	}

	UE_LOG(LogGambit, Log, TEXT("DebugData: === OBJECTS THAT CANNOT CURRENTLY FUNCTION (%d) ==="), BlockingObjectLabels.Num());
	if (BlockingObjectLabels.Num() == 0)
	{
		UE_LOG(LogGambit, Log, TEXT("DebugData: No blocking data validation issues found for configured known content."));
	}
	else
	{
		for (const FString& Label : BlockingObjectLabels)
		{
			UE_LOG(LogGambit, Error, TEXT("DebugData: BlockingObject %s"), *Label);
		}
	}

	UE_LOG(LogGambit, Log, TEXT("DebugData: === STATIC DATA VALIDATION END ==="));
}

void UGambitMatchDebugComponent::DebugPrintInventory() const
{
	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	const int32 DefaultActiveDiceCount = FMath::Max(0, UGambitGameBalanceSettings::Get()->DefaultActiveDiceCount);
	const UGambitStaticDataSettings* Settings = UGambitStaticDataSettings::Get();
	const FSoftObjectPath DefaultLoadoutPath = Settings ? Settings->DefaultPlayerLoadout.ToSoftObjectPath() : FSoftObjectPath();
	const bool bDefaultLoadoutAssigned = !DefaultLoadoutPath.IsNull();
	const UGambitPlayerLoadoutDefinition* DefaultLoadout = Settings ? Settings->DefaultPlayerLoadout.LoadSynchronous() : nullptr;
	UE_LOG(LogGambit, Log, TEXT("DebugInventory: PlayerCount=%d"), Players.Num());

	for (const AGambitPlayerState* PlayerState : Players)
	{
		if (!PlayerState)
		{
			continue;
		}

		LogInventoryStateForPlayer(PlayerState, Players, TEXT("DebugInventory:"));

		const TArray<TObjectPtr<UGambitDiceDefinition>>& OwnedDice = PlayerState->GetOwnedDiceDefinitionsRef();
		if (OwnedDice.Num() == 0)
		{
			const bool bRuntimeFallbackActive = DefaultActiveDiceCount > 0 && PlayerState->GetDiceStatesRef().Num() >= DefaultActiveDiceCount;
			if (!bDefaultLoadoutAssigned)
			{
				UE_LOG(
					LogGambit,
					Warning,
					TEXT("DebugInventory: %s owns no dice definitions; runtime fallback %s because DefaultPlayerLoadout is not assigned. Configure DefaultPlayerLoadout for authored match data."),
					*BuildMatchDebugPlayerLabel(PlayerState, Players),
					bRuntimeFallbackActive ? TEXT("is active") : TEXT("is NOT active"));
			}
			else if (!DefaultLoadout)
			{
				UE_LOG(
					LogGambit,
					Warning,
					TEXT("DebugInventory: %s owns no dice definitions; DefaultPlayerLoadout is assigned (%s) but failed to load, runtime fallback %s."),
					*BuildMatchDebugPlayerLabel(PlayerState, Players),
					*DefaultLoadoutPath.ToString(),
					bRuntimeFallbackActive ? TEXT("is active") : TEXT("is NOT active"));
			}
			else
			{
				UE_LOG(
					LogGambit,
					Warning,
					TEXT("DebugInventory: %s owns no dice definitions; DefaultPlayerLoadout loaded with %d starting dice, so this is incomplete runtime state. Runtime fallback %s."),
					*BuildMatchDebugPlayerLabel(PlayerState, Players),
					DefaultLoadout->Loadout.StartingDice.Num(),
					bRuntimeFallbackActive ? TEXT("is active") : TEXT("is NOT active"));
			}
		}
		for (int32 Index = 0; Index < OwnedDice.Num(); ++Index)
		{
			UE_LOG(LogGambit, Log, TEXT("DebugInventory: %s OwnedDie[%d] %s"), *BuildMatchDebugPlayerLabel(PlayerState, Players), Index, *FormatDiceDefinition(OwnedDice[Index]));
		}

		const TArray<FGambitDieRuntimeState>& RuntimeDice = PlayerState->GetDiceStatesRef();
		for (int32 Index = 0; Index < RuntimeDice.Num(); ++Index)
		{
			const FGambitDieRuntimeState& DieState = RuntimeDice[Index];
			UE_LOG(
				LogGambit,
				Log,
				TEXT("DebugInventory: %s RuntimeDie[%d] InstanceId=%d Definition=%s FaceIndex=%d Raw=%d Effective=%d ScoreValue=%d ComboCount=%d Sum=%s Combos=%s Reroll=%s Lockable=%s Locked=%s DestroyAfterRound=%s"),
				*BuildMatchDebugPlayerLabel(PlayerState, Players),
				Index,
				DieState.InstanceId,
				DieState.DiceDefinition ? *DieState.DiceDefinition->GetResolvedDiceId().ToString() : TEXT("FallbackD6"),
				DieState.RolledFaceIndex,
				DieState.RawValue,
				DieState.EffectiveValue,
				DieState.ScoreContributionValue,
				DieState.ComboContributionCount,
				DieState.bCountsForScoreSum ? TEXT("Yes") : TEXT("No"),
				DieState.bCountsForCombinations ? TEXT("Yes") : TEXT("No"),
				DieState.bCanBeRerolled ? TEXT("Yes") : TEXT("No"),
				DieState.bCanBeLocked ? TEXT("Yes") : TEXT("No"),
				DieState.bLocked ? TEXT("Yes") : TEXT("No"),
				DieState.bDestroyedAfterRound ? TEXT("Yes") : TEXT("No"));
		}

		const TArray<TObjectPtr<UGambitModuleDefinition>>& Modules = PlayerState->GetActiveModulesRef();
		for (int32 Index = 0; Index < Modules.Num(); ++Index)
		{
			const UGambitModuleDefinition* Module = Modules[Index];
			UE_LOG(
				LogGambit,
				Log,
				TEXT("DebugInventory: %s Module[%d] %s"),
				*BuildMatchDebugPlayerLabel(PlayerState, Players),
				Index,
				*FormatItemDetails(Module));
		}

		const TArray<FGambitConsumableRuntimeSlot>& Consumables = PlayerState->GetConsumableSlotsRef();
		for (int32 Index = 0; Index < Consumables.Num(); ++Index)
		{
			const FGambitConsumableRuntimeSlot& Slot = Consumables[Index];
			const UGambitConsumableDefinition* Consumable = Slot.Definition;
			UE_LOG(
				LogGambit,
				Log,
				TEXT("DebugInventory: %s Consumable[%d] Instance=%s %s"),
				*BuildMatchDebugPlayerLabel(PlayerState, Players),
				Index,
				*Slot.InventoryInstanceId.ToString(EGuidFormats::DigitsWithHyphens),
				*FormatItemDetails(Consumable));
		}

		if (const UGambitInventoryComponent* InventoryComponent = PlayerState->GetInventoryComponent())
		{
			const TArray<FGambitInventoryItemInstance>& Instances = InventoryComponent->GetOwnedItemInstancesRef();
			for (int32 Index = 0; Index < Instances.Num(); ++Index)
			{
				const FGambitInventoryItemInstance& Instance = Instances[Index];
				UE_LOG(
					LogGambit,
					Log,
					TEXT("DebugInventory: %s ItemInstance[%d] Id=%s Type=%s Source=%s Purchase=%s Effect=%s Item=%s Dice=%s Equipped=%s Active=%s Stack=%d Tags=%d"),
					*BuildMatchDebugPlayerLabel(PlayerState, Players),
					Index,
					*Instance.InstanceId.ToString(EGuidFormats::DigitsWithHyphens),
					*MatchDebugItemTypeToString(Instance.ItemType),
					*Instance.SourceStableId.ToString(),
					*Instance.SourcePurchaseId.ToString(),
					*Instance.SourceEffectId.ToString(),
					*FormatItemDetails(Instance.ItemDefinition.Get()),
					*FormatDiceDefinition(Instance.DiceDefinition.Get()),
					Instance.bEquipped ? TEXT("Yes") : TEXT("No"),
					Instance.bActive ? TEXT("Yes") : TEXT("No"),
					Instance.StackCount,
					Instance.RuntimeTags.Num());
			}
		}
	}
}

void UGambitMatchDebugComponent::DebugPrintSharedPool() const
{
	const AGambitGameState* GameState = GetGambitGameState();
	const UGambitSharedPoolComponent* SharedPoolComponent = GameState ? GameState->GetSharedPoolComponent() : nullptr;
	if (!SharedPoolComponent)
	{
		UE_LOG(LogGambit, Warning, TEXT("DebugSharedPool: Missing shared pool component"));
		return;
	}

	const UGambitStaticDataSettings* Settings = UGambitStaticDataSettings::Get();
	const UGambitSharedPoolDefinition* SharedPoolDefinition = Settings ? Settings->DefaultSharedPoolDefinition.LoadSynchronous() : nullptr;
	const UGambitItemCatalogDataAsset* ItemCatalog = SharedPoolDefinition && SharedPoolDefinition->ItemCatalog
		? SharedPoolDefinition->ItemCatalog.Get()
		: (Settings ? Settings->DefaultItemCatalog.LoadSynchronous() : nullptr);
	if (!ItemCatalog)
	{
		UE_LOG(
			LogGambit,
			Warning,
			TEXT("DebugSharedPool: cannot resolve tracked item names because neither SharedPoolDefinition.ItemCatalog nor DefaultItemCatalog is assigned/loaded"));
	}

	const TMap<FName, int32>& RemainingStock = SharedPoolComponent->GetRemainingStockByItemIdRef();
	const TMap<FName, int32>& MaxStock = SharedPoolComponent->GetMaxStockByItemIdRef();
	const TMap<FName, int32>& ReservedStock = SharedPoolComponent->GetReservedStockByItemIdRef();
	const TMap<FName, int32>& PurchasedCount = SharedPoolComponent->GetPurchasedCountByItemIdRef();
	UE_LOG(LogGambit, Log, TEXT("DebugSharedPool: TrackedItems=%d"), RemainingStock.Num());
	if (RemainingStock.Num() == 0)
	{
		UE_LOG(LogGambit, Warning, TEXT("DebugSharedPool: no tracked shared pool items"));
	}

	for (const TPair<FName, int32>& Entry : RemainingStock)
	{
		const int32* MaxStockValue = MaxStock.Find(Entry.Key);
		const int32* ReservedStockValue = ReservedStock.Find(Entry.Key);
		const int32* PurchasedCountValue = PurchasedCount.Find(Entry.Key);
		const UGambitItemDefinition* ItemDefinition = ResolveMatchDebugItemFromCatalog(Entry.Key, ItemCatalog);
		UE_LOG(
			LogGambit,
			Log,
			TEXT("DebugSharedPool: ItemId=%s Item=%s Remaining=%d Reserved=%d Purchased=%d Max=%d"),
			*Entry.Key.ToString(),
			*FormatMatchDebugItemName(ItemDefinition),
			Entry.Value,
			ReservedStockValue ? *ReservedStockValue : 0,
			PurchasedCountValue ? *PurchasedCountValue : 0,
			MaxStockValue ? *MaxStockValue : 0);
		if (!ItemDefinition)
		{
			UE_LOG(
				LogGambit,
				Warning,
				TEXT("DebugSharedPool: tracked item %s could not be resolved; DefaultItemCatalog or SharedPoolDefinition.ItemCatalog is missing/incomplete"),
				*Entry.Key.ToString());
		}
	}

	for (const AGambitPlayerState* PlayerState : GetAllPlayers())
	{
		if (!PlayerState)
		{
			continue;
		}

		const TArray<AGambitPlayerState*> Players = GetAllPlayers();
		for (const FGambitShopOffer& Offer : PlayerState->GetCurrentShopOffersRef())
		{
			if (Offer.ItemDefinition && Offer.ItemDefinition->bUsesSharedPool && !SharedPoolComponent->IsItemTracked(Offer.ItemDefinition))
			{
				UE_LOG(
					LogGambit,
					Warning,
					TEXT("DebugSharedPool: shop warning Player=%s OfferId=%d ItemId=%s is shared-pool but not tracked"),
					*BuildMatchDebugPlayerLabel(PlayerState, Players),
					Offer.OfferId,
					*Offer.ItemDefinition->GetResolvedItemId().ToString());
			}
		}
	}
}

AGambitGameMode* UGambitMatchDebugComponent::GetGambitGameMode() const
{
	return Cast<AGambitGameMode>(GetOwner());
}

AGambitGameState* UGambitMatchDebugComponent::GetGambitGameState() const
{
	const AGambitGameMode* GameMode = GetGambitGameMode();
	return GameMode ? GameMode->GetGameState<AGambitGameState>() : nullptr;
}

TArray<AGambitPlayerState*> UGambitMatchDebugComponent::GetAllPlayers() const
{
	const AGambitGameState* GameState = GetGambitGameState();
	return GameState ? GameState->GetGambitPlayerStates() : TArray<AGambitPlayerState*>();
}

AGambitPlayerState* UGambitMatchDebugComponent::GetPlayerByIndex(const int32 PlayerIndex) const
{
	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	if (!Players.IsValidIndex(PlayerIndex))
	{
		UE_LOG(LogGambit, Log, TEXT("DebugMatch: Invalid PlayerIndex=%d PlayerCount=%d"), PlayerIndex, Players.Num());
		return nullptr;
	}

	return Players[PlayerIndex];
}

UGambitDebugAutoPlayer* UGambitMatchDebugComponent::GetOrCreateDebugAutoPlayer()
{
	if (!DebugAutoPlayer)
	{
		DebugAutoPlayer = NewObject<UGambitDebugAutoPlayer>(this);
	}

	return DebugAutoPlayer;
}

void UGambitMatchDebugComponent::PrintFinalMatchSummary() const
{
	const AGambitGameState* GameState = GetGambitGameState();
	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	TArray<AGambitPlayerState*> FinalRanking = Players;
	FinalRanking.Sort([](const AGambitPlayerState& A, const AGambitPlayerState& B)
	{
		if (A.GetTotalVictoryPoints() == B.GetTotalVictoryPoints())
		{
			return A.GetCurrentRoundScore() > B.GetCurrentRoundScore();
		}

		return A.GetTotalVictoryPoints() > B.GetTotalVictoryPoints();
	});
	const FString PhaseString = GameState ? MatchDebugPhaseToString(GameState->GetCurrentPhase()) : FString(TEXT("MissingGameState"));

	UE_LOG(
		LogGambit,
		Log,
		TEXT("DebugMatch: === AUTO FULL MATCH SUMMARY RoundPlayed=%d Phase=%s Players=%d ==="),
		GameState ? GameState->GetCurrentRoundIndex() : 0,
		*PhaseString,
		Players.Num());

	if (FinalRanking.Num() > 0 && FinalRanking[0])
	{
		UE_LOG(
			LogGambit,
			Log,
			TEXT("DebugMatch: Winner=%s VP=%d Gold=%d"),
			*BuildMatchDebugPlayerLabel(FinalRanking[0], Players),
			FinalRanking[0]->GetTotalVictoryPoints(),
			FinalRanking[0]->GetCurrentGold());
	}

	for (int32 RankIndex = 0; RankIndex < FinalRanking.Num(); ++RankIndex)
	{
		const AGambitPlayerState* PlayerState = FinalRanking[RankIndex];
		if (!PlayerState)
		{
			continue;
		}

		const FGambitPlayerSlotState SlotState = PlayerState->GetSlotState();
		UE_LOG(
			LogGambit,
			Log,
			TEXT("DebugMatch: FinalRank=%d Player=%s VP=%d Gold=%d LastScore=%d Dice=%d Modules=%d/%d Consumables=%d/%d"),
			RankIndex + 1,
			*BuildMatchDebugPlayerLabel(PlayerState, Players),
			PlayerState->GetTotalVictoryPoints(),
			PlayerState->GetCurrentGold(),
			PlayerState->GetCurrentRoundScore(),
			PlayerState->GetOwnedDiceDefinitionsRef().Num(),
			SlotState.ModuleSlotsUsed,
			SlotState.ModuleSlotsCapacity,
			SlotState.ConsumableSlotsUsed,
			SlotState.ConsumableSlotsCapacity);

		for (const UGambitDiceDefinition* DiceDefinition : PlayerState->GetOwnedDiceDefinitionsRef())
		{
			UE_LOG(LogGambit, Log, TEXT("DebugMatch: Final %s Dice %s"), *BuildMatchDebugPlayerLabel(PlayerState, Players), *FormatDiceDefinition(DiceDefinition));
		}
		for (const UGambitModuleDefinition* ModuleDefinition : PlayerState->GetActiveModulesRef())
		{
			UE_LOG(LogGambit, Log, TEXT("DebugMatch: Final %s Module %s"), *BuildMatchDebugPlayerLabel(PlayerState, Players), *FormatItemDetails(ModuleDefinition));
		}
		for (const FGambitConsumableRuntimeSlot& ConsumableSlot : PlayerState->GetConsumableSlotsRef())
		{
			const UGambitConsumableDefinition* ConsumableDefinition = ConsumableSlot.Definition;
			UE_LOG(LogGambit, Log, TEXT("DebugMatch: Final %s Consumable %s"), *BuildMatchDebugPlayerLabel(PlayerState, Players), *FormatItemDetails(ConsumableDefinition));
		}
	}
}

void UGambitMatchDebugComponent::SetAllPlayersReady(const bool bReady) const
{
	AGambitGameMode* GameMode = GetGambitGameMode();
	if (!GameMode)
	{
		UE_LOG(LogGambit, Log, TEXT("DebugMatch: SetAllPlayersReady failed, missing GambitGameMode"));
		return;
	}

	int32 ReadyCount = 0;
	for (AGambitPlayerState* PlayerState : GetAllPlayers())
	{
		if (!PlayerState)
		{
			continue;
		}

		GameMode->SetPlayerReady(PlayerState, bReady);
		ReadyCount++;
	}

	UE_LOG(LogGambit, Log, TEXT("DebugMatch: SetAllPlayersReady bReady=%s Count=%d"), bReady ? TEXT("true") : TEXT("false"), ReadyCount);
}
