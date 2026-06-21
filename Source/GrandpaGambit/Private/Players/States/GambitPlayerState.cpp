#include "Players/States/GambitPlayerState.h"

#include "Data/Assets/GambitItemEffectDefinition.h"
#include "Dice/Data/GambitDiceDefinition.h"
#include "Items/Consumables/GambitConsumableDefinition.h"
#include "Items/Data/GambitItemDefinition.h"
#include "Items/Modules/GambitModuleDefinition.h"
#include "Players/Components/GambitDiceComponent.h"
#include "Players/Components/GambitEconomyComponent.h"
#include "Players/Components/GambitInventoryComponent.h"
#include "Players/Components/GambitPlayerRoundStateComponent.h"
#include "Scoring/Calculators/GambitScoreModifierMath.h"
#include "Shop/Components/GambitShopComponent.h"

namespace
{
	FString ResolveTextOrName(const FText& Text, const FName Fallback)
	{
		return Text.IsEmpty() ? Fallback.ToString() : Text.ToString();
	}

	TArray<FName> BuildEffectIds(const TArray<TObjectPtr<UGambitItemEffectDefinition>>& EffectDefinitions)
	{
		TArray<FName> EffectIds;
		EffectIds.Reserve(EffectDefinitions.Num());
		for (const TObjectPtr<UGambitItemEffectDefinition>& EffectDefinition : EffectDefinitions)
		{
			if (!EffectDefinition)
			{
				continue;
			}

			EffectIds.Add(!EffectDefinition->EffectId.IsNone()
				? EffectDefinition->EffectId
				: FName(*EffectDefinition->GetName()));
		}

		return EffectIds;
	}

	FGambitDebugItemSnapshot BuildItemSnapshot(const UGambitItemDefinition* ItemDefinition)
	{
		FGambitDebugItemSnapshot Snapshot;
		if (!ItemDefinition)
		{
			Snapshot.DisplayName = TEXT("None");
			return Snapshot;
		}

		Snapshot.ItemId = ItemDefinition->GetResolvedItemId();
		Snapshot.DisplayName = ResolveTextOrName(ItemDefinition->DisplayName, Snapshot.ItemId);
		Snapshot.ItemType = ItemDefinition->ItemType;
		Snapshot.Rarity = ItemDefinition->Rarity;
		Snapshot.Cost = ItemDefinition->Cost;
		Snapshot.bUsesSharedPool = ItemDefinition->bUsesSharedPool;
		Snapshot.Tags = ItemDefinition->Tags;
		Snapshot.EffectIds = BuildEffectIds(ItemDefinition->EffectDefinitions);
		Snapshot.Summary = FString::Printf(
			TEXT("%s [%s] Cost=%d Effects=%d"),
			*Snapshot.DisplayName,
			*Snapshot.ItemId.ToString(),
			Snapshot.Cost,
			Snapshot.EffectIds.Num());
		return Snapshot;
	}
}

AGambitPlayerState::AGambitPlayerState()
{
	InventoryComponent = CreateDefaultSubobject<UGambitInventoryComponent>(TEXT("InventoryComponent"));
	EconomyComponent = CreateDefaultSubobject<UGambitEconomyComponent>(TEXT("EconomyComponent"));
	DiceComponent = CreateDefaultSubobject<UGambitDiceComponent>(TEXT("DiceComponent"));
	ShopComponent = CreateDefaultSubobject<UGambitShopComponent>(TEXT("ShopComponent"));
	RoundStateComponent = CreateDefaultSubobject<UGambitPlayerRoundStateComponent>(TEXT("RoundStateComponent"));
}

void AGambitPlayerState::InitializeForMatch()
{
	TotalVictoryPoints = 0;
	OnVictoryPointsChanged.Broadcast(TotalVictoryPoints);

	if (EconomyComponent)
	{
		EconomyComponent->InitializeForMatch();
	}

	if (InventoryComponent)
	{
		InventoryComponent->InitializeForMatch();
	}

	if (ShopComponent)
	{
		ShopComponent->InitializeForMatch();
	}

	if (DiceComponent)
	{
		DiceComponent->ResetMatchRuntimeState();
	}

	ResetRoundState();
}

void AGambitPlayerState::ResetRoundState()
{
	if (InventoryComponent)
	{
		InventoryComponent->ResetRoundState();
	}

	if (DiceComponent && InventoryComponent)
	{
		DiceComponent->ResetDiceForNewRound(InventoryComponent->GetOwnedDiceDefinitions());
	}

	if (ShopComponent)
	{
		ShopComponent->ClearOffers();
	}

	if (RoundStateComponent)
	{
		RoundStateComponent->ResetRoundState();
	}

	OnRoundScoreChanged.Broadcast(GetCurrentRoundScore());
}

void AGambitPlayerState::ApplyRoundScore(const FGambitScoreBreakdown& ScoreBreakdown)
{
	if (RoundStateComponent)
	{
		RoundStateComponent->ApplyRoundScore(ScoreBreakdown);
	}

	OnRoundScoreChanged.Broadcast(GetCurrentRoundScore());
}

int32 AGambitPlayerState::ApplyRoundGoldReward(const int32 BaseGoldReward)
{
	if (!EconomyComponent)
	{
		return 0;
	}

	TArray<FGambitDebugGoldLine> GoldLines;
	const int32 Interest = EconomyComponent->ApplyRoundEconomyDetailed(BaseGoldReward, GoldLines);
	AppendDebugGoldLines(GoldLines);
	return Interest;
}

int32 AGambitPlayerState::AddGold(const int32 DeltaGold)
{
	return EconomyComponent ? EconomyComponent->AddGold(DeltaGold) : 0;
}

void AGambitPlayerState::AddVictoryPoints(const int32 VictoryPointsToAdd)
{
	TotalVictoryPoints = FMath::Max(0, TotalVictoryPoints + VictoryPointsToAdd);
	OnVictoryPointsChanged.Broadcast(TotalVictoryPoints);
}

bool AGambitPlayerState::ConsumeConsumableDefinitionAtSlot(
	const int32 SlotIndex,
	UGambitConsumableDefinition*& OutDefinition)
{
	return InventoryComponent ? InventoryComponent->ConsumeConsumableDefinitionAtSlot(SlotIndex, OutDefinition) : false;
}

void AGambitPlayerState::ApplyTemporaryScoreModifier(const FGambitScoreModifierContext& Modifier)
{
	if (RoundStateComponent)
	{
		RoundStateComponent->ApplyTemporaryScoreModifier(Modifier);
	}
}

void AGambitPlayerState::AddRoundEvent(const FGambitRoundGameplayEvent& Event)
{
	if (RoundStateComponent)
	{
		RoundStateComponent->AddRoundEvent(Event);
	}
}

void AGambitPlayerState::AppendRoundEvents(const TArray<FGambitRoundGameplayEvent>& Events)
{
	if (RoundStateComponent)
	{
		RoundStateComponent->AppendRoundEvents(Events);
	}
}

FGambitScoreModifierContext AGambitPlayerState::BuildCombinedScoreModifier() const
{
	if (RoundStateComponent)
	{
		return RoundStateComponent->BuildCombinedScoreModifier();
	}

	return FGambitScoreModifierMath::MakeNeutral();
}

FGambitScoreModifierContext AGambitPlayerState::GetTemporaryScoreModifier() const
{
	return RoundStateComponent
		? RoundStateComponent->GetTemporaryScoreModifier()
		: FGambitScoreModifierMath::MakeNeutral();
}

void AGambitPlayerState::RollAllDice(FRandomStream& RandomStream)
{
	if (DiceComponent)
	{
		DiceComponent->RollAll(RandomStream);
	}
}

bool AGambitPlayerState::TryRerollUnlockedDice(FRandomStream& RandomStream)
{
	if (!DiceComponent)
	{
		return false;
	}

	return DiceComponent->RollUnlocked(RandomStream);
}

bool AGambitPlayerState::TrySetDieLocked(const int32 DieIndex, const bool bLocked)
{
	return DiceComponent ? DiceComponent->SetDieLocked(DieIndex, bLocked) : false;
}

int32 AGambitPlayerState::CommitDestroyedDiceAfterRound()
{
	if (!DiceComponent)
	{
		return 0;
	}

	int32 RemovedCount = 0;
	const TArray<FGambitDieRuntimeState> DiceStates = DiceComponent->GetDiceStates();
	for (int32 DieIndex = DiceStates.Num() - 1; DieIndex >= 0; --DieIndex)
	{
		if (!DiceStates[DieIndex].bDestroyedAfterRound)
		{
			continue;
		}

		if (InventoryComponent && DieIndex < InventoryComponent->GetOwnedDiceCount())
		{
			UGambitDiceDefinition* RemovedDieDefinition = nullptr;
			InventoryComponent->RemoveOwnedDieAtIndex(DieIndex, RemovedDieDefinition);
		}

		if (DiceComponent->RemoveDieAtIndex(DieIndex))
		{
			RemovedCount++;
		}
	}

	return RemovedCount;
}

TArray<FGambitDieRuntimeState> AGambitPlayerState::GetDiceStates() const
{
	return DiceComponent ? DiceComponent->GetDiceStates() : TArray<FGambitDieRuntimeState>();
}

const TArray<FGambitDieRuntimeState>& AGambitPlayerState::GetDiceStatesRef() const
{
	static const TArray<FGambitDieRuntimeState> EmptyDiceStates;
	return DiceComponent ? DiceComponent->GetDiceStatesRef() : EmptyDiceStates;
}

void AGambitPlayerState::RefreshShopOffers(FRandomStream& RandomStream, UGambitSharedPoolComponent* SharedPoolComponent)
{
	if (ShopComponent)
	{
		ShopComponent->RefreshOffers(RandomStream, SharedPoolComponent);
	}
}

void AGambitPlayerState::RefreshShopOffersWithCount(
	FRandomStream& RandomStream,
	UGambitSharedPoolComponent* SharedPoolComponent,
	const int32 OfferCountOverride)
{
	if (ShopComponent)
	{
		ShopComponent->RefreshOffersWithCount(RandomStream, SharedPoolComponent, OfferCountOverride);
	}
}

FGambitShopPurchaseContext AGambitPlayerState::BuildShopPurchaseContext(
	const int32 OfferId,
	const UGambitSharedPoolComponent* SharedPoolComponent) const
{
	return ShopComponent
		? ShopComponent->BuildPurchaseContext(OfferId, EconomyComponent, SharedPoolComponent)
		: FGambitShopPurchaseContext();
}

bool AGambitPlayerState::TryPurchaseOffer(const int32 OfferId, UGambitSharedPoolComponent* SharedPoolComponent)
{
	if (!ShopComponent || !EconomyComponent || !InventoryComponent)
	{
		return false;
	}

	return ShopComponent->PurchaseOffer(OfferId, EconomyComponent, InventoryComponent, SharedPoolComponent);
}

bool AGambitPlayerState::TryPurchaseOfferWithContext(
	FGambitShopPurchaseContext& PurchaseContext,
	UGambitSharedPoolComponent* SharedPoolComponent)
{
	if (!ShopComponent || !EconomyComponent || !InventoryComponent)
	{
		return false;
	}

	return ShopComponent->PurchaseOfferWithContext(PurchaseContext, EconomyComponent, InventoryComponent, SharedPoolComponent);
}

void AGambitPlayerState::ResolveShopPurchasePrice(FGambitShopPurchaseContext& PurchaseContext) const
{
	if (ShopComponent)
	{
		ShopComponent->ResolvePurchasePrice(PurchaseContext);
	}
}

void AGambitPlayerState::ApplyPostPurchaseAdjustments(FGambitShopPurchaseContext& PurchaseContext)
{
	if (ShopComponent)
	{
		ShopComponent->ApplyPostPurchaseAdjustments(PurchaseContext, EconomyComponent);
	}
}

void AGambitPlayerState::SkipShop(UGambitSharedPoolComponent* SharedPoolComponent)
{
	if (ShopComponent)
	{
		ShopComponent->SkipShop(SharedPoolComponent);
	}
}

TArray<FGambitShopOffer> AGambitPlayerState::GetCurrentShopOffers() const
{
	return ShopComponent ? ShopComponent->GetCurrentOffers() : TArray<FGambitShopOffer>();
}

const TArray<FGambitShopOffer>& AGambitPlayerState::GetCurrentShopOffersRef() const
{
	static const TArray<FGambitShopOffer> EmptyOffers;
	return ShopComponent ? ShopComponent->GetCurrentOffersRef() : EmptyOffers;
}

int32 AGambitPlayerState::GetCurrentRoundScore() const
{
	return RoundStateComponent ? RoundStateComponent->GetCurrentRoundScore() : 0;
}

FGambitScoreBreakdown AGambitPlayerState::GetLastScoreBreakdown() const
{
	return RoundStateComponent ? RoundStateComponent->GetLastScoreBreakdown() : FGambitScoreBreakdown();
}

int32 AGambitPlayerState::GetCurrentGold() const
{
	return EconomyComponent ? EconomyComponent->GetCurrentGold() : 0;
}

FGambitPlayerSlotState AGambitPlayerState::GetSlotState() const
{
	return InventoryComponent ? InventoryComponent->BuildSlotState() : FGambitPlayerSlotState();
}

const TArray<TObjectPtr<UGambitDiceDefinition>>& AGambitPlayerState::GetOwnedDiceDefinitionsRef() const
{
	static const TArray<TObjectPtr<UGambitDiceDefinition>> EmptyDiceDefinitions;
	return InventoryComponent ? InventoryComponent->GetOwnedDiceDefinitions() : EmptyDiceDefinitions;
}

const TArray<TObjectPtr<UGambitModuleDefinition>>& AGambitPlayerState::GetActiveModulesRef() const
{
	static const TArray<TObjectPtr<UGambitModuleDefinition>> EmptyModules;
	return InventoryComponent ? InventoryComponent->GetActiveModules() : EmptyModules;
}

const TArray<FGambitConsumableRuntimeSlot>& AGambitPlayerState::GetConsumableSlotsRef() const
{
	static const TArray<FGambitConsumableRuntimeSlot> EmptyConsumables;
	return InventoryComponent ? InventoryComponent->GetConsumableSlotsRef() : EmptyConsumables;
}

FGambitPlayerRuntimeSnapshot AGambitPlayerState::BuildRuntimeSnapshot() const
{
	FGambitPlayerRuntimeSnapshot Snapshot;
	Snapshot.CurrentGold = GetCurrentGold();
	Snapshot.CurrentRoundScore = GetCurrentRoundScore();
	Snapshot.TotalVictoryPoints = TotalVictoryPoints;
	Snapshot.SlotState = GetSlotState();
	return Snapshot;
}

TArray<FGambitRoundGameplayEvent> AGambitPlayerState::GetRoundEvents() const
{
	return RoundStateComponent ? RoundStateComponent->GetRoundEvents() : TArray<FGambitRoundGameplayEvent>();
}

const TArray<FGambitRoundGameplayEvent>& AGambitPlayerState::GetRoundEventsRef() const
{
	static const TArray<FGambitRoundGameplayEvent> EmptyRoundEvents;
	return RoundStateComponent ? RoundStateComponent->GetRoundEventsRef() : EmptyRoundEvents;
}

bool AGambitPlayerState::HasEventThisRound(const EGambitRoundGameplayEventType EventType) const
{
	return RoundStateComponent ? RoundStateComponent->HasEventThisRound(EventType) : false;
}

int32 AGambitPlayerState::CountEventsThisRound(const EGambitRoundGameplayEventType EventType) const
{
	return RoundStateComponent ? RoundStateComponent->CountEventsThisRound(EventType) : 0;
}

TArray<FGambitRoundGameplayEvent> AGambitPlayerState::GetRoundEventsByType(
	const EGambitRoundGameplayEventType EventType) const
{
	return RoundStateComponent
		? RoundStateComponent->GetRoundEventsByType(EventType)
		: TArray<FGambitRoundGameplayEvent>();
}

TArray<FGambitRoundGameplayEvent> AGambitPlayerState::GetRoundEventsBySourceItem(const FName SourceItemId) const
{
	return RoundStateComponent
		? RoundStateComponent->GetRoundEventsBySourceItem(SourceItemId)
		: TArray<FGambitRoundGameplayEvent>();
}

TArray<FGambitRoundGameplayEvent> AGambitPlayerState::GetRoundEventsByEffect(const FName EffectId) const
{
	return RoundStateComponent
		? RoundStateComponent->GetRoundEventsByEffect(EffectId)
		: TArray<FGambitRoundGameplayEvent>();
}

TArray<FGambitRoundGameplayEvent> AGambitPlayerState::GetRoundEventsByTargetPlayer(const int32 TargetPlayerId) const
{
	return RoundStateComponent
		? RoundStateComponent->GetRoundEventsByTargetPlayer(TargetPlayerId)
		: TArray<FGambitRoundGameplayEvent>();
}

void AGambitPlayerState::AddDebugEffectEvent(const FGambitDebugEffectEvent& Event)
{
	if (RoundStateComponent)
	{
		RoundStateComponent->AddDebugEffectEvent(Event);
	}
}

void AGambitPlayerState::AddDebugScoreLine(const FGambitDebugScoreLine& Line)
{
	if (RoundStateComponent)
	{
		RoundStateComponent->AddDebugScoreLine(Line);
	}
}

void AGambitPlayerState::AddDebugGoldLine(const FGambitDebugGoldLine& Line)
{
	if (RoundStateComponent)
	{
		RoundStateComponent->AddDebugGoldLine(Line);
	}
}

void AGambitPlayerState::AddDebugShopLine(const FGambitDebugShopLine& Line)
{
	if (RoundStateComponent)
	{
		RoundStateComponent->AddDebugShopLine(Line);
	}
}

void AGambitPlayerState::AppendDebugEffectEvents(const TArray<FGambitDebugEffectEvent>& Events)
{
	if (RoundStateComponent)
	{
		RoundStateComponent->AppendDebugEffectEvents(Events);
	}
}

void AGambitPlayerState::AppendDebugScoreLines(const TArray<FGambitDebugScoreLine>& Lines)
{
	if (RoundStateComponent)
	{
		RoundStateComponent->AppendDebugScoreLines(Lines);
	}
}

void AGambitPlayerState::AppendDebugGoldLines(const TArray<FGambitDebugGoldLine>& Lines)
{
	if (RoundStateComponent)
	{
		RoundStateComponent->AppendDebugGoldLines(Lines);
	}
}

void AGambitPlayerState::AppendDebugShopLines(const TArray<FGambitDebugShopLine>& Lines)
{
	if (RoundStateComponent)
	{
		RoundStateComponent->AppendDebugShopLines(Lines);
	}
}

TArray<FGambitDebugDieSnapshot> AGambitPlayerState::BuildDebugDiceSnapshot() const
{
	TArray<FGambitDebugDieSnapshot> Snapshots;
	const TArray<FGambitDieRuntimeState>& DiceStates = GetDiceStatesRef();
	Snapshots.Reserve(DiceStates.Num());

	for (const FGambitDieRuntimeState& DieState : DiceStates)
	{
		FGambitDebugDieSnapshot Snapshot;
		Snapshot.HandIndex = DieState.HandIndex;
		Snapshot.InstanceId = DieState.InstanceId;
		Snapshot.RawValue = DieState.RawValue;
		Snapshot.EffectiveValue = DieState.EffectiveValue;
		Snapshot.ScoreContributionValue = DieState.ScoreContributionValue;
		Snapshot.ComboContributionCount = DieState.ComboContributionCount;
		Snapshot.bLocked = DieState.bLocked;
		Snapshot.bCountsForScoreSum = DieState.bCountsForScoreSum;
		Snapshot.bCountsForCombinations = DieState.bCountsForCombinations;
		Snapshot.bCanBeRerolled = DieState.bCanBeRerolled;
		Snapshot.bCanBeLocked = DieState.bCanBeLocked;
		Snapshot.bDestroyedAfterRound = DieState.bDestroyedAfterRound;
		Snapshot.bRemovedFromRound = DieState.bRemovedFromRound;
		Snapshot.bTemporaryDie = DieState.bTemporaryDie;
		Snapshot.bTemporarilyTransformed = DieState.bTemporarilyTransformed;
		Snapshot.RuntimeSourceItemId = DieState.RuntimeSourceItemId;
		Snapshot.RuntimeSourceEffectId = DieState.RuntimeSourceEffectId;
		Snapshot.RuntimeTags = DieState.RuntimeTags;
		Snapshot.AppliedRuntimeEffectIds = DieState.AppliedRuntimeEffectIds;

		if (const UGambitDiceDefinition* DiceDefinition = DieState.DiceDefinition.Get())
		{
			Snapshot.DiceId = DiceDefinition->GetResolvedDiceId();
			Snapshot.DisplayName = ResolveTextOrName(DiceDefinition->DisplayName, Snapshot.DiceId);
			Snapshot.Rarity = DiceDefinition->Rarity;
			Snapshot.DiceType = DiceDefinition->DiceType;
			Snapshot.DefinitionTags = DiceDefinition->Tags;
			Snapshot.EffectIds = BuildEffectIds(DiceDefinition->EffectDefinitions);
		}
		else
		{
			Snapshot.DisplayName = TEXT("Unknown Die");
		}

		Snapshot.Summary = FString::Printf(
			TEXT("#%d %s Value=%d Raw=%d ScoreValue=%d ComboCount=%d Locked=%s Temp=%s Transformed=%s Tags=%d Effects=%d RuntimeEffects=%d"),
			Snapshot.HandIndex,
			*Snapshot.DisplayName,
			Snapshot.EffectiveValue,
			Snapshot.RawValue,
			Snapshot.ScoreContributionValue,
			Snapshot.ComboContributionCount,
			Snapshot.bLocked ? TEXT("Yes") : TEXT("No"),
			Snapshot.bTemporaryDie ? TEXT("Yes") : TEXT("No"),
			Snapshot.bTemporarilyTransformed ? TEXT("Yes") : TEXT("No"),
			Snapshot.RuntimeTags.Num() + Snapshot.DefinitionTags.Num(),
			Snapshot.EffectIds.Num(),
			Snapshot.AppliedRuntimeEffectIds.Num());
		Snapshots.Add(Snapshot);
	}

	return Snapshots;
}

TArray<FGambitDebugItemSnapshot> AGambitPlayerState::BuildDebugModuleSnapshot() const
{
	TArray<FGambitDebugItemSnapshot> Snapshots;
	const TArray<TObjectPtr<UGambitModuleDefinition>>& Modules = GetActiveModulesRef();
	Snapshots.Reserve(Modules.Num());
	for (const TObjectPtr<UGambitModuleDefinition>& ModuleDefinition : Modules)
	{
		Snapshots.Add(BuildItemSnapshot(ModuleDefinition.Get()));
	}
	return Snapshots;
}

TArray<FGambitDebugItemSnapshot> AGambitPlayerState::BuildDebugConsumableSnapshot() const
{
	TArray<FGambitDebugItemSnapshot> Snapshots;
	const TArray<FGambitConsumableRuntimeSlot>& Slots = GetConsumableSlotsRef();
	Snapshots.Reserve(Slots.Num());
	for (const FGambitConsumableRuntimeSlot& Slot : Slots)
	{
		Snapshots.Add(BuildItemSnapshot(Slot.Definition.Get()));
	}
	return Snapshots;
}

FGambitDebugPlayerSnapshot AGambitPlayerState::BuildDebugPlayerSnapshot(const int32 PlayerIndex) const
{
	FGambitDebugPlayerSnapshot Snapshot;
	Snapshot.PlayerIndex = PlayerIndex;
	Snapshot.PlayerName = GetPlayerName().IsEmpty() ? FString::Printf(TEXT("Player %d"), PlayerIndex) : GetPlayerName();
	Snapshot.CurrentGold = GetCurrentGold();
	Snapshot.CurrentRoundScore = GetCurrentRoundScore();
	Snapshot.TotalVictoryPoints = GetTotalVictoryPoints();
	Snapshot.SlotState = GetSlotState();
	Snapshot.LastScoreBreakdown = GetLastScoreBreakdown();
	Snapshot.Dice = BuildDebugDiceSnapshot();
	Snapshot.ActiveModules = BuildDebugModuleSnapshot();
	Snapshot.Consumables = BuildDebugConsumableSnapshot();
	Snapshot.EffectEvents = GetDebugEffectEvents();
	Snapshot.ScoreLines = GetDebugScoreLines();
	Snapshot.GoldLines = GetDebugGoldLines();
	Snapshot.ShopLines = GetDebugShopLines();
	return Snapshot;
}

TArray<FGambitDebugEffectEvent> AGambitPlayerState::GetDebugEffectEvents() const
{
	return RoundStateComponent ? RoundStateComponent->GetDebugEffectEvents() : TArray<FGambitDebugEffectEvent>();
}

TArray<FGambitDebugScoreLine> AGambitPlayerState::GetDebugScoreLines() const
{
	return RoundStateComponent ? RoundStateComponent->GetDebugScoreLines() : TArray<FGambitDebugScoreLine>();
}

TArray<FGambitDebugGoldLine> AGambitPlayerState::GetDebugGoldLines() const
{
	return RoundStateComponent ? RoundStateComponent->GetDebugGoldLines() : TArray<FGambitDebugGoldLine>();
}

TArray<FGambitDebugShopLine> AGambitPlayerState::GetDebugShopLines() const
{
	return RoundStateComponent ? RoundStateComponent->GetDebugShopLines() : TArray<FGambitDebugShopLine>();
}
