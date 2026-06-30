#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Types/GambitRoundFeedbackTypes.h"
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

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player|Round Feedback")
	void AddRoundFeedbackEvent(const FGambitRoundFeedbackEvent& Event);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player|Round Feedback")
	void AddScoreBreakdownLine(const FGambitScoreBreakdownLine& Line);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player|Round Feedback")
	void AddGoldBreakdownLine(const FGambitGoldBreakdownLine& Line);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Player|Round Feedback")
	void AddShopBreakdownLine(const FGambitShopBreakdownLine& Line);

	void AppendRoundFeedbackEvents(const TArray<FGambitRoundFeedbackEvent>& Events);
	void AppendScoreBreakdownLines(const TArray<FGambitScoreBreakdownLine>& Lines);
	void AppendGoldBreakdownLines(const TArray<FGambitGoldBreakdownLine>& Lines);
	void AppendShopBreakdownLines(const TArray<FGambitShopBreakdownLine>& Lines);
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

	UFUNCTION(BlueprintPure, Category = "Gambit|Player|Round Feedback")
	TArray<FGambitRoundFeedbackEvent> GetRoundFeedbackEvents() const { return RoundFeedbackEvents; }

	const TArray<FGambitRoundFeedbackEvent>& GetRoundFeedbackEventsRef() const { return RoundFeedbackEvents; }

	UFUNCTION(BlueprintPure, Category = "Gambit|Player|Round Feedback")
	TArray<FGambitScoreBreakdownLine> GetScoreBreakdownLines() const { return ScoreBreakdownLines; }

	const TArray<FGambitScoreBreakdownLine>& GetScoreBreakdownLinesRef() const { return ScoreBreakdownLines; }

	UFUNCTION(BlueprintPure, Category = "Gambit|Player|Round Feedback")
	TArray<FGambitGoldBreakdownLine> GetGoldBreakdownLines() const { return GoldBreakdownLines; }

	const TArray<FGambitGoldBreakdownLine>& GetGoldBreakdownLinesRef() const { return GoldBreakdownLines; }

	UFUNCTION(BlueprintPure, Category = "Gambit|Player|Round Feedback")
	TArray<FGambitShopBreakdownLine> GetShopBreakdownLines() const { return ShopBreakdownLines; }

	const TArray<FGambitShopBreakdownLine>& GetShopBreakdownLinesRef() const { return ShopBreakdownLines; }

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Player|Round Feedback", meta = (AllowPrivateAccess = "true"))
	TArray<FGambitRoundFeedbackEvent> RoundFeedbackEvents;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Player|Round Feedback", meta = (AllowPrivateAccess = "true"))
	TArray<FGambitScoreBreakdownLine> ScoreBreakdownLines;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Player|Round Feedback", meta = (AllowPrivateAccess = "true"))
	TArray<FGambitGoldBreakdownLine> GoldBreakdownLines;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Player|Round Feedback", meta = (AllowPrivateAccess = "true"))
	TArray<FGambitShopBreakdownLine> ShopBreakdownLines;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Player|Round Feedback", meta = (AllowPrivateAccess = "true"))
	int32 NextFeedbackSequence = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Player|Round Events", meta = (AllowPrivateAccess = "true"))
	int32 NextRoundEventSequence = 1;
};
