#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Types/GambitDebugTypes.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "Core/Types/GambitRoundGameplayEventTypes.h"
#include "GambitPlayerRoundStateComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGambitPlayerRoundScoreChanged, int32, NewRoundScore);

UCLASS(ClassGroup = (Gambit), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class GRANDPAGAMBIT_API UGambitPlayerRoundStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGambitPlayerRoundStateComponent();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player|Round State")
	void InitializeForMatch();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player|Round State")
	void ResetRoundState();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player|Round State")
	void ApplyRoundScore(const FGambitScoreBreakdown& ScoreBreakdown);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player|Round State")
	void ApplyTemporaryScoreModifier(const FGambitScoreModifierContext& Modifier);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player|Round Events")
	void AddRoundEvent(const FGambitRoundGameplayEvent& Event);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player|Round Debug")
	void AddDebugEffectEvent(const FGambitDebugEffectEvent& Event);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player|Round Debug")
	void AddDebugScoreLine(const FGambitDebugScoreLine& Line);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player|Round Debug")
	void AddDebugGoldLine(const FGambitDebugGoldLine& Line);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player|Round Debug")
	void AddDebugShopLine(const FGambitDebugShopLine& Line);

	void AppendDebugEffectEvents(const TArray<FGambitDebugEffectEvent>& Events);
	void AppendDebugScoreLines(const TArray<FGambitDebugScoreLine>& Lines);
	void AppendDebugGoldLines(const TArray<FGambitDebugGoldLine>& Lines);
	void AppendDebugShopLines(const TArray<FGambitDebugShopLine>& Lines);
	void AppendRoundEvents(const TArray<FGambitRoundGameplayEvent>& Events);

	UFUNCTION(BlueprintPure, Category = "Gambit|Player|Round State")
	FGambitScoreModifierContext GetTemporaryScoreModifier() const { return RoundConsumableModifier; }

	UFUNCTION(BlueprintPure, Category = "Gambit|Player|Round State")
	FGambitScoreModifierContext BuildCombinedScoreModifier() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Player|Round State")
	int32 GetCurrentRoundScore() const { return CurrentRoundScore; }

	UFUNCTION(BlueprintPure, Category = "Gambit|Player|Round State")
	FGambitScoreBreakdown GetLastScoreBreakdown() const { return LastScoreBreakdown; }

	const FGambitScoreBreakdown& GetLastScoreBreakdownRef() const { return LastScoreBreakdown; }

	UFUNCTION(BlueprintPure, Category = "Gambit|Player|Round Events")
	TArray<FGambitRoundGameplayEvent> GetRoundEvents() const { return RoundEvents; }

	const TArray<FGambitRoundGameplayEvent>& GetRoundEventsRef() const { return RoundEvents; }

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

	UFUNCTION(BlueprintPure, Category = "Gambit|Player|Round Debug")
	TArray<FGambitDebugEffectEvent> GetDebugEffectEvents() const { return DebugEffectEvents; }

	const TArray<FGambitDebugEffectEvent>& GetDebugEffectEventsRef() const { return DebugEffectEvents; }

	UFUNCTION(BlueprintPure, Category = "Gambit|Player|Round Debug")
	TArray<FGambitDebugScoreLine> GetDebugScoreLines() const { return DebugScoreLines; }

	const TArray<FGambitDebugScoreLine>& GetDebugScoreLinesRef() const { return DebugScoreLines; }

	UFUNCTION(BlueprintPure, Category = "Gambit|Player|Round Debug")
	TArray<FGambitDebugGoldLine> GetDebugGoldLines() const { return DebugGoldLines; }

	const TArray<FGambitDebugGoldLine>& GetDebugGoldLinesRef() const { return DebugGoldLines; }

	UFUNCTION(BlueprintPure, Category = "Gambit|Player|Round Debug")
	TArray<FGambitDebugShopLine> GetDebugShopLines() const { return DebugShopLines; }

	const TArray<FGambitDebugShopLine>& GetDebugShopLinesRef() const { return DebugShopLines; }

	UPROPERTY(BlueprintAssignable, Category = "Gambit|Player|Round State")
	FOnGambitPlayerRoundScoreChanged OnRoundScoreChanged;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Player|Round State", meta = (AllowPrivateAccess = "true"))
	int32 CurrentRoundScore = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Player|Round State", meta = (AllowPrivateAccess = "true"))
	FGambitScoreBreakdown LastScoreBreakdown;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Player|Round State", meta = (AllowPrivateAccess = "true"))
	FGambitScoreModifierContext RoundConsumableModifier;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Player|Round Events", meta = (AllowPrivateAccess = "true"))
	TArray<FGambitRoundGameplayEvent> RoundEvents;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Player|Round Debug", meta = (AllowPrivateAccess = "true"))
	TArray<FGambitDebugEffectEvent> DebugEffectEvents;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Player|Round Debug", meta = (AllowPrivateAccess = "true"))
	TArray<FGambitDebugScoreLine> DebugScoreLines;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Player|Round Debug", meta = (AllowPrivateAccess = "true"))
	TArray<FGambitDebugGoldLine> DebugGoldLines;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Player|Round Debug", meta = (AllowPrivateAccess = "true"))
	TArray<FGambitDebugShopLine> DebugShopLines;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Player|Round Debug", meta = (AllowPrivateAccess = "true"))
	int32 NextDebugSequence = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Player|Round Events", meta = (AllowPrivateAccess = "true"))
	int32 NextRoundEventSequence = 1;
};
