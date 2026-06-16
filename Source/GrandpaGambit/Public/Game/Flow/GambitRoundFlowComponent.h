#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Types/GambitGameplayEvents.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "Items/Effects/GambitEffectExecutionTypes.h"
#include "GambitRoundFlowComponent.generated.h"

class AGambitGameState;
class AGambitPlayerState;
class UGambitDiceEvaluatorContract;
class UGambitEffectResolver;
class UGambitItemDefinition;
class UGambitRoundEffectPipeline;
class UGambitScoreCalculatorContract;
struct FGambitEffectExecutionContext;
struct FGambitRoundEffectCommitRequest;
struct FGambitRoundEffectContextRequest;

UCLASS(ClassGroup = (Gambit), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class GRANDPAGAMBIT_API UGambitRoundFlowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGambitRoundFlowComponent();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Round Flow")
	void StartMatchFlow();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Round Flow")
	void SetPlayerReady(AGambitPlayerState* PlayerState, bool bReady);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Round Flow")
	bool RequestSetDieLocked(AGambitPlayerState* PlayerState, int32 DieIndex, bool bLocked);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Round Flow")
	bool RequestReroll(AGambitPlayerState* PlayerState);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Round Flow")
	bool RequestUseConsumable(AGambitPlayerState* PlayerState, int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Round Flow")
	bool RequestUseConsumableOnTarget(AGambitPlayerState* PlayerState, int32 SlotIndex, AGambitPlayerState* TargetPlayerState);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Round Flow")
	bool RequestUseConsumableOnTargetSelectedDie(
		AGambitPlayerState* PlayerState,
		int32 SlotIndex,
		AGambitPlayerState* TargetPlayerState,
		int32 SelectedDieIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Round Flow")
	bool RequestPurchaseOffer(AGambitPlayerState* PlayerState, int32 OfferId);

	UFUNCTION(BlueprintPure, Category = "Gambit|Round Flow")
	int32 GetRerollsUsedForPlayer(AGambitPlayerState* PlayerState) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Round Flow")
	int32 GetMaxRerollsForPlayer(AGambitPlayerState* PlayerState) const;

	UPROPERTY(BlueprintAssignable, Category = "Gambit|Round Flow")
	FOnGambitRoundFlowPhaseEntered OnPhaseEntered;

protected:
	virtual void BeginPlay() override;

private:
	bool TransitionToPhase(EGambitRoundPhase NewPhase);
	bool IsTransitionAllowed(EGambitRoundPhase CurrentPhase, EGambitRoundPhase NextPhase) const;

	void StartNextRound();
	void EnterRollPhase();
	void EnterSelectionRerollPhase();
	void EnterActionPhase();
	void EnterResolutionPhase();
	void EnterRewardPhase();
	void EnterRankingPhase();
	void EnterShopPhase();
	void EnterRoundEndPhase();

	void AdvanceWhenAllPlayersReady();
	bool AreAllPlayersReady() const;
	void ResetReadiness();

	FGambitRoundEffectContextRequest BuildEffectContextRequest(EGambitEffectHook Hook, AGambitPlayerState* SourcePlayer, AGambitPlayerState* TargetPlayer = nullptr) const;
	FGambitRoundEffectCommitRequest BuildEffectCommitRequest();
	FGambitEffectExecutionContext CreateEffectContext(EGambitEffectHook Hook, AGambitPlayerState* SourcePlayer, AGambitPlayerState* TargetPlayer = nullptr) const;
	void ExecuteActiveEffectsAndCommit(FGambitEffectExecutionContext& Context);
	FGambitScoreModifierContext BuildScoreModifierForResolution(
		AGambitPlayerState* PlayerState,
		const FGambitDiceCombinationResult& CombinationResult);

	int32 GetRerollCount(AGambitPlayerState* PlayerState) const;
	void IncrementRerollCount(AGambitPlayerState* PlayerState);
	TArray<FGambitRerollDieDelta> BuildRerollDeltas(
		const TArray<FGambitDieRuntimeState>& DiceBefore,
		const TArray<FGambitDieRuntimeState>& DiceAfter) const;
	void TrackRerollDeltas(AGambitPlayerState* PlayerState, TArray<FGambitRerollDieDelta>& RerollDeltas);
	int32 GetFirstRerolledDieHandIndex(AGambitPlayerState* PlayerState) const;
	int32 GetMaxRerollCountForAnyDie(AGambitPlayerState* PlayerState) const;
	int32 GetEffectiveRerollLimit(AGambitPlayerState* PlayerState) const;
	int32 GetRankForPlayer(AGambitPlayerState* PlayerState) const;
	int32 ComputeBaseGoldReward(const AGambitPlayerState* PlayerState) const;

	AGambitGameState* GetGambitGameState() const;
	TArray<AGambitPlayerState*> GetAllPlayers() const;

	UPROPERTY(EditDefaultsOnly, Category = "Gambit|Round Flow")
	int32 RandomSeed = 1337;

	UPROPERTY(EditDefaultsOnly, Category = "Gambit|Round Flow")
	TSubclassOf<UGambitDiceEvaluatorContract> DiceEvaluatorClass;

	UPROPERTY(EditDefaultsOnly, Category = "Gambit|Round Flow")
	TSubclassOf<UGambitScoreCalculatorContract> ScoreCalculatorClass;

	UPROPERTY(Transient)
	TObjectPtr<UGambitDiceEvaluatorContract> DiceEvaluator;

	UPROPERTY(Transient)
	TObjectPtr<UGambitScoreCalculatorContract> ScoreCalculator;

	UPROPERTY(Transient)
	TObjectPtr<UGambitEffectResolver> EffectResolver;

	UPROPERTY(Transient)
	TObjectPtr<UGambitRoundEffectPipeline> EffectPipeline;

	UPROPERTY(VisibleAnywhere, Category = "Gambit|Round Flow")
	FRandomStream MatchRandomStream;

	UPROPERTY(VisibleAnywhere, Category = "Gambit|Round Flow")
	TMap<TObjectPtr<AGambitPlayerState>, bool> PlayerReadyMap;

	UPROPERTY(VisibleAnywhere, Category = "Gambit|Round Flow")
	TMap<TObjectPtr<AGambitPlayerState>, int32> RerollsUsedByPlayer;

	UPROPERTY(VisibleAnywhere, Category = "Gambit|Round Flow")
	TMap<TObjectPtr<AGambitPlayerState>, int32> RerollLimitDeltaByPlayer;

	TMap<AGambitPlayerState*, int32> FirstRerolledDieInstanceByPlayer;

	TMap<AGambitPlayerState*, TMap<int32, int32>> RerollCountByDieInstanceByPlayer;
};
