#pragma once

#include "CoreMinimal.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "GambitRoundGameplayEventTypes.generated.h"

UENUM(BlueprintType)
enum class EGambitRoundGameplayEventType : uint8
{
	None,
	EffectApplied,
	EffectPrevented,
	ConsumableUsed,
	ModuleTriggered,
	DiceEffectApplied,
	GoldChanged,
	ScoreModifierApplied,
	DieModified,
	DieDestroyedOrRemoved,
	ShopPurchase,
	RerollUsed,
	RoundScored
};

UENUM(BlueprintType)
enum class EGambitRoundGameplayEventOutcome : uint8
{
	None,
	Applied,
	Prevented,
	Failed,
	Skipped
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitRoundGameplayEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Round Event")
	int32 Sequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Round Event")
	EGambitRoundGameplayEventType EventType = EGambitRoundGameplayEventType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Round Event")
	EGambitRoundGameplayEventOutcome Outcome = EGambitRoundGameplayEventOutcome::None;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Round Event")
	int32 RoundNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Round Event")
	EGambitRoundPhase Phase = EGambitRoundPhase::None;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Round Event")
	int32 SourcePlayerId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Round Event")
	int32 TargetPlayerId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Round Event")
	FName SourceItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Round Event")
	FName EffectId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Round Event")
	FName EffectTypeId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Round Event")
	FName TargetRuleId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Round Event")
	TArray<FName> NegativeCategoryIds;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Round Event")
	float NumericDelta = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Round Event")
	int32 SourceDieInstanceId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Round Event")
	int32 SourceDieHandIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Round Event")
	int32 TargetDieInstanceId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Round Event")
	int32 TargetDieHandIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Round Event")
	FString Reason;
};
