#pragma once

#include "CoreMinimal.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "GambitRoundFeedbackTypes.generated.h"

UENUM(BlueprintType)
enum class EGambitRoundFeedbackEventCategory : uint8
{
	System,
	Effect,
	Score,
	Dice,
	Gold,
	Shop,
	Inventory,
	Protection,
	Refusal
};

UENUM(BlueprintType)
enum class EGambitScoreBreakdownLineType : uint8
{
	BaseCombination,
	DiceSum,
	DiceContribution,
	Additive,
	Multiplier,
	Cap,
	Diminishing,
	TemporaryModifier,
	FinalScore
};

UENUM(BlueprintType)
enum class EGambitGoldBreakdownLineType : uint8
{
	BaseReward,
	Interest,
	RecurringIncome,
	Effect,
	Purchase,
	Cashback,
	Refund,
	Manual,
	Other
};

UENUM(BlueprintType)
enum class EGambitShopBreakdownLineType : uint8
{
	GeneratedOffer,
	PriceResolved,
	PurchaseSuccess,
	PurchaseFailure,
	PostPurchaseAdjustment
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitRoundFeedbackEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	int32 Sequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	EGambitRoundFeedbackEventCategory Category = EGambitRoundFeedbackEventCategory::Effect;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	EGambitRoundPhase Phase = EGambitRoundPhase::None;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FName HookId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FName SourceId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FString SourceName;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FName EffectId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FName EffectTypeId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FString EffectTypeName;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FName TargetRuleId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FString TargetName;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	bool bTriggered = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	bool bNegative = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	bool bPrevented = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FString Summary;
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitScoreBreakdownLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	int32 Sequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	EGambitScoreBreakdownLineType LineType = EGambitScoreBreakdownLineType::Additive;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	EGambitRoundPhase Phase = EGambitRoundPhase::None;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FName SourceId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FString SourceName;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FString Label;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	float AdditiveDelta = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	float DiceContributionDelta = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	float MultiplierValue = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	float ScoreBefore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	float ScoreAfter = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FString Summary;
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitGoldBreakdownLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	int32 Sequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	EGambitGoldBreakdownLineType LineType = EGambitGoldBreakdownLineType::Other;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	EGambitRoundPhase Phase = EGambitRoundPhase::None;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FName SourceId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FString SourceName;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	int32 RequestedDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	int32 ActualDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	int32 GoldBefore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	int32 GoldAfter = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	bool bClamped = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FString Summary;
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitShopBreakdownLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	int32 Sequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	EGambitShopBreakdownLineType LineType = EGambitShopBreakdownLineType::GeneratedOffer;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	EGambitRoundPhase Phase = EGambitRoundPhase::None;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	int32 OfferId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FString ItemName;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	EGambitItemType ItemType = EGambitItemType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	EGambitItemRarity Rarity = EGambitItemRarity::Common;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	int32 BasePrice = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	int32 PriceBeforeModifiers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	int32 ResolvedPrice = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	float DiscountPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	float SurchargePercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	int32 FlatPriceDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	bool bFree = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FString FreeReason;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	float CashbackPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	int32 CashbackGold = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	int32 GoldDeltaOnPurchase = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	bool bUsesSharedPool = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	bool bHasSharedPoolReservation = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FString SharedPoolState;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FString SharedPoolReason;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	bool bPurchaseSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	bool bRefused = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FString FailureReason;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	TArray<FString> PresentationLines;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FString Summary;
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitDiceSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	int32 HandIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	int32 InstanceId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FName DiceId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FName EffectiveDiceId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FName OriginalDiceId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	EGambitItemRarity Rarity = EGambitItemRarity::Common;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	EGambitDiceType DiceType = EGambitDiceType::D6;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	int32 RawValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	int32 EffectiveValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	int32 ScoreContributionValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	int32 ComboContributionCount = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	bool bLocked = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	bool bCountsForScoreSum = true;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	bool bCountsForCombinations = true;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	bool bCanBeRerolled = true;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	bool bCanBeLocked = true;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	bool bDestroyedAfterRound = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	bool bRemovedFromRound = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	bool bTemporaryDie = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	bool bTemporarilyTransformed = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	bool bHasRuntimeFaceOverride = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	TArray<int32> RuntimeFaces;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FName RuntimeSourceItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FName RuntimeSourceEffectId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	TArray<FName> DefinitionTags;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	TArray<FName> RuntimeTags;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	TArray<FName> EffectIds;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	TArray<FName> AppliedRuntimeEffectIds;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FString Summary;
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitItemSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FGuid InventoryInstanceId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FName SourceStableId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FName SourcePurchaseId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FName SourceEffectId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	EGambitItemType ItemType = EGambitItemType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	EGambitItemRarity Rarity = EGambitItemRarity::Common;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	int32 Cost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	bool bUsesSharedPool = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	int32 StackCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	bool bEquipped = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	TArray<FName> Tags;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	TArray<FName> RuntimeTags;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	TArray<FName> EffectIds;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FString Summary;
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitPlayerSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	int32 PlayerIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FString PlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	int32 CurrentGold = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	int32 CurrentRoundScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	int32 TotalVictoryPoints = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FGambitPlayerSlotState SlotState;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	FGambitScoreBreakdown LastScoreBreakdown;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	TArray<FGambitDiceSnapshot> Dice;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	TArray<FGambitItemSnapshot> ActiveModules;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	TArray<FGambitItemSnapshot> Consumables;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	TArray<FGambitRoundFeedbackEvent> EffectEvents;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	TArray<FGambitScoreBreakdownLine> ScoreLines;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	TArray<FGambitGoldBreakdownLine> GoldLines;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Feedback")
	TArray<FGambitShopBreakdownLine> ShopLines;
};
