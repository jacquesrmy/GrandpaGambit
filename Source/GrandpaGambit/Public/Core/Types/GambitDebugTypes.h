#pragma once

#include "CoreMinimal.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "GambitDebugTypes.generated.h"

UENUM(BlueprintType)
enum class EGambitDebugEventCategory : uint8
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
enum class EGambitDebugScoreLineType : uint8
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
enum class EGambitDebugGoldLineType : uint8
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
enum class EGambitDebugShopLineType : uint8
{
	GeneratedOffer,
	PriceResolved,
	PurchaseSuccess,
	PurchaseFailure,
	PostPurchaseAdjustment
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitDebugEffectEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	int32 Sequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	EGambitDebugEventCategory Category = EGambitDebugEventCategory::Effect;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	EGambitRoundPhase Phase = EGambitRoundPhase::None;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FName HookId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FName SourceId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FString SourceName;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FName EffectId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FName EffectTypeId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FString EffectTypeName;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FName TargetRuleId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FString TargetName;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	bool bTriggered = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	bool bNegative = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	bool bPrevented = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FString Summary;
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitDebugScoreLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	int32 Sequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	EGambitDebugScoreLineType LineType = EGambitDebugScoreLineType::Additive;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	EGambitRoundPhase Phase = EGambitRoundPhase::None;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FName SourceId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FString SourceName;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FString Label;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	float AdditiveDelta = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	float DiceContributionDelta = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	float MultiplierValue = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	float ScoreBefore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	float ScoreAfter = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FString Summary;
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitDebugGoldLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	int32 Sequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	EGambitDebugGoldLineType LineType = EGambitDebugGoldLineType::Other;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	EGambitRoundPhase Phase = EGambitRoundPhase::None;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FName SourceId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FString SourceName;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	int32 RequestedDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	int32 ActualDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	int32 GoldBefore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	int32 GoldAfter = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	bool bClamped = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FString Summary;
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitDebugShopLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	int32 Sequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	EGambitDebugShopLineType LineType = EGambitDebugShopLineType::GeneratedOffer;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	EGambitRoundPhase Phase = EGambitRoundPhase::None;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	int32 OfferId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FString ItemName;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	EGambitItemType ItemType = EGambitItemType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	EGambitItemRarity Rarity = EGambitItemRarity::Common;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	int32 BasePrice = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	int32 PriceBeforeModifiers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	int32 ResolvedPrice = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	float DiscountPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	float SurchargePercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	int32 FlatPriceDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	bool bFree = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FString FreeReason;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	float CashbackPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	int32 CashbackGold = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	int32 GoldDeltaOnPurchase = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	bool bUsesSharedPool = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	bool bHasSharedPoolReservation = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FString SharedPoolState;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FString SharedPoolReason;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	bool bPurchaseSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	bool bRefused = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FString FailureReason;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	TArray<FString> DebugLines;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FString Summary;
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitDebugDieSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	int32 HandIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	int32 InstanceId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FName DiceId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FName EffectiveDiceId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FName OriginalDiceId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	EGambitItemRarity Rarity = EGambitItemRarity::Common;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	EGambitDiceType DiceType = EGambitDiceType::D6;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	int32 RawValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	int32 EffectiveValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	int32 ScoreContributionValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	int32 ComboContributionCount = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	bool bLocked = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	bool bCountsForScoreSum = true;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	bool bCountsForCombinations = true;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	bool bCanBeRerolled = true;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	bool bCanBeLocked = true;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	bool bDestroyedAfterRound = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	bool bRemovedFromRound = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	bool bTemporaryDie = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	bool bTemporarilyTransformed = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	bool bHasRuntimeFaceOverride = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	TArray<int32> RuntimeFaces;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FName RuntimeSourceItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FName RuntimeSourceEffectId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	TArray<FName> DefinitionTags;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	TArray<FName> RuntimeTags;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	TArray<FName> EffectIds;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	TArray<FName> AppliedRuntimeEffectIds;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FString Summary;
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitDebugItemSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	EGambitItemType ItemType = EGambitItemType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	EGambitItemRarity Rarity = EGambitItemRarity::Common;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	int32 Cost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	bool bUsesSharedPool = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	TArray<FName> Tags;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	TArray<FName> EffectIds;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FString Summary;
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitDebugPlayerSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	int32 PlayerIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FString PlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	int32 CurrentGold = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	int32 CurrentRoundScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	int32 TotalVictoryPoints = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FGambitPlayerSlotState SlotState;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	FGambitScoreBreakdown LastScoreBreakdown;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	TArray<FGambitDebugDieSnapshot> Dice;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	TArray<FGambitDebugItemSnapshot> ActiveModules;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	TArray<FGambitDebugItemSnapshot> Consumables;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	TArray<FGambitDebugEffectEvent> EffectEvents;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	TArray<FGambitDebugScoreLine> ScoreLines;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	TArray<FGambitDebugGoldLine> GoldLines;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Debug")
	TArray<FGambitDebugShopLine> ShopLines;
};
