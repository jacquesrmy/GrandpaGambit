#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Core/Types/GambitRoundFeedbackTypes.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "Core/Types/GambitRoundGameplayEventTypes.h"
#include "GambitPlayerState.generated.h"

class UGambitDiceComponent;
class UGambitConsumableDefinition;
class UGambitDiceDefinition;
class UGambitEconomyComponent;
class UGambitInventoryComponent;
class UGambitModuleDefinition;
class UGambitPlayerRoundStateComponent;
class UGambitShopComponent;
class UGambitSharedPoolComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGambitRoundScoreChanged, int32, NewRoundScore);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGambitVictoryPointsChanged, int32, NewTotalVictoryPoints);

UCLASS()
class GRANDPAGAMBIT_API AGambitPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AGambitPlayerState();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player")
	void InitializeForMatch();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player")
	void ResetRoundState();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player")
	void ApplyRoundScore(const FGambitScoreBreakdown& ScoreBreakdown);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player")
	int32 ApplyRoundGoldReward(int32 BaseGoldReward);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player")
	int32 AddGold(int32 DeltaGold);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player")
	void AddVictoryPoints(int32 VictoryPointsToAdd);

	bool ConsumeConsumableDefinitionAtSlot(int32 SlotIndex, UGambitConsumableDefinition*& OutDefinition);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player")
	void ApplyTemporaryScoreModifier(const FGambitScoreModifierContext& Modifier);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player|Round Events")
	void AddRoundEvent(const FGambitRoundGameplayEvent& Event);

	void AppendRoundEvents(const TArray<FGambitRoundGameplayEvent>& Events);

	UFUNCTION(BlueprintPure, Category = "Gambit|Player")
	FGambitScoreModifierContext BuildCombinedScoreModifier() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Player")
	FGambitScoreModifierContext GetTemporaryScoreModifier() const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player")
	void RollAllDice(FRandomStream& RandomStream);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player")
	bool TryRerollUnlockedDice(FRandomStream& RandomStream);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player")
	bool TrySetDieLocked(int32 DieIndex, bool bLocked);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player")
	int32 CommitDestroyedDiceAfterRound();

	UFUNCTION(BlueprintPure, Category = "Gambit|Player")
	TArray<FGambitDieRuntimeState> GetDiceStates() const;

	const TArray<FGambitDieRuntimeState>& GetDiceStatesRef() const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player")
	void RefreshShopOffers(FRandomStream& RandomStream, UGambitSharedPoolComponent* SharedPoolComponent);

	void RefreshShopOffersWithCount(FRandomStream& RandomStream, UGambitSharedPoolComponent* SharedPoolComponent, int32 OfferCountOverride);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player")
	FGambitShopPurchaseContext BuildShopPurchaseContext(int32 OfferId, const UGambitSharedPoolComponent* SharedPoolComponent) const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player")
	FGambitShopPurchaseContext BuildShopPurchasePreviewContext(int32 OfferId, const UGambitSharedPoolComponent* SharedPoolComponent) const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player")
	bool TryPurchaseOffer(int32 OfferId, UGambitSharedPoolComponent* SharedPoolComponent);

	bool TryPurchaseOfferWithContext(FGambitShopPurchaseContext& PurchaseContext, UGambitSharedPoolComponent* SharedPoolComponent);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player")
	void ResolveShopPurchasePrice(UPARAM(ref) FGambitShopPurchaseContext& PurchaseContext) const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player")
	void ApplyPostPurchaseAdjustments(UPARAM(ref) FGambitShopPurchaseContext& PurchaseContext);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player")
	void SkipShop(UGambitSharedPoolComponent* SharedPoolComponent);

	UFUNCTION(BlueprintPure, Category = "Gambit|Player")
	TArray<FGambitShopOffer> GetCurrentShopOffers() const;

	const TArray<FGambitShopOffer>& GetCurrentShopOffersRef() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Player")
	int32 GetCurrentRoundScore() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Player")
	int32 GetTotalVictoryPoints() const { return TotalVictoryPoints; }

	UFUNCTION(BlueprintPure, Category = "Gambit|Player")
	FGambitScoreBreakdown GetLastScoreBreakdown() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Player")
	int32 GetCurrentGold() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Player")
	FGambitPlayerSlotState GetSlotState() const;

	const TArray<TObjectPtr<UGambitDiceDefinition>>& GetOwnedDiceDefinitionsRef() const;

	const TArray<TObjectPtr<UGambitModuleDefinition>>& GetActiveModulesRef() const;

	const TArray<FGambitConsumableRuntimeSlot>& GetConsumableSlotsRef() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Player")
	FGambitPlayerRuntimeSnapshot BuildRuntimeSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Player|Round Events")
	TArray<FGambitRoundGameplayEvent> GetRoundEvents() const;

	const TArray<FGambitRoundGameplayEvent>& GetRoundEventsRef() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Player|Round Events")
	bool HasEventThisRound(EGambitRoundGameplayEventType EventType) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Player|Round Events")
	int32 CountEventsThisRound(EGambitRoundGameplayEventType EventType) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Player|Round Events")
	TArray<FGambitRoundGameplayEvent> GetRoundEventsByType(EGambitRoundGameplayEventType EventType) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Player|Round Events")
	TArray<FGambitRoundGameplayEvent> GetRoundEventsBySourceItem(FName SourceItemId) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Player|Round Events")
	TArray<FGambitRoundGameplayEvent> GetRoundEventsByEffect(FName EffectId) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Player|Round Events")
	TArray<FGambitRoundGameplayEvent> GetRoundEventsByTargetPlayer(int32 TargetPlayerId) const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player|Feedback")
	void AddRoundFeedbackEvent(const FGambitRoundFeedbackEvent& Event);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player|Feedback")
	void AddScoreBreakdownLine(const FGambitScoreBreakdownLine& Line);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player|Feedback")
	void AddGoldBreakdownLine(const FGambitGoldBreakdownLine& Line);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player|Feedback")
	void AddShopBreakdownLine(const FGambitShopBreakdownLine& Line);

	void AppendRoundFeedbackEvents(const TArray<FGambitRoundFeedbackEvent>& Events);
	void AppendScoreBreakdownLines(const TArray<FGambitScoreBreakdownLine>& Lines);
	void AppendGoldBreakdownLines(const TArray<FGambitGoldBreakdownLine>& Lines);
	void AppendShopBreakdownLines(const TArray<FGambitShopBreakdownLine>& Lines);

	UFUNCTION(BlueprintPure, Category = "Gambit|Player|Feedback")
	TArray<FGambitDiceSnapshot> BuildDiceSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Player|Feedback")
	TArray<FGambitItemSnapshot> BuildModuleSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Player|Feedback")
	TArray<FGambitItemSnapshot> BuildConsumableSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Player|Feedback")
	FGambitPlayerSnapshot BuildPlayerSnapshot(int32 PlayerIndex) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Player|Feedback")
	TArray<FGambitRoundFeedbackEvent> GetRoundFeedbackEvents() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Player|Feedback")
	TArray<FGambitScoreBreakdownLine> GetScoreBreakdownLines() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Player|Feedback")
	TArray<FGambitGoldBreakdownLine> GetGoldBreakdownLines() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Player|Feedback")
	TArray<FGambitShopBreakdownLine> GetShopBreakdownLines() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Player")
	UGambitInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	UFUNCTION(BlueprintPure, Category = "Gambit|Player")
	UGambitEconomyComponent* GetEconomyComponent() const { return EconomyComponent; }

	UFUNCTION(BlueprintPure, Category = "Gambit|Player")
	UGambitDiceComponent* GetDiceComponent() const { return DiceComponent; }

	UFUNCTION(BlueprintPure, Category = "Gambit|Player")
	UGambitShopComponent* GetShopComponent() const { return ShopComponent; }

	UFUNCTION(BlueprintPure, Category = "Gambit|Player")
	UGambitPlayerRoundStateComponent* GetRoundStateComponent() const { return RoundStateComponent; }

	UPROPERTY(BlueprintAssignable, Category = "Gambit|Player")
	FOnGambitRoundScoreChanged OnRoundScoreChanged;

	UPROPERTY(BlueprintAssignable, Category = "Gambit|Player")
	FOnGambitVictoryPointsChanged OnVictoryPointsChanged;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Player", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGambitInventoryComponent> InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Player", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGambitEconomyComponent> EconomyComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Player", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGambitDiceComponent> DiceComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Player", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGambitShopComponent> ShopComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Player", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGambitPlayerRoundStateComponent> RoundStateComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Player", meta = (AllowPrivateAccess = "true"))
	int32 TotalVictoryPoints = 0;
};
