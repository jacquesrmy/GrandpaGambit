#pragma once

#include "CoreMinimal.h"
#include "Core/Types/GambitDebugTypes.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "Core/Types/GambitRoundGameplayEventTypes.h"
#include "GambitEffectExecutionTypes.generated.h"

class AGambitPlayerState;
class UGambitConsumableDefinition;
class UGambitDiceComponent;
class UGambitDiceDefinition;
class UGambitEconomyComponent;
class UGambitInventoryComponent;
class UGambitItemDefinition;
class UGambitItemEffectDefinition;
class UGambitSharedPoolComponent;
class UGambitShopComponent;

UENUM(BlueprintType)
enum class EGambitEffectHook : uint8
{
	RoundStart,
	PreRoll,
	PostRoll,
	PreReroll,
	PostReroll,
	ConsumableUse,
	PreCombinationEvaluation,
	PostCombinationEvaluation,
	PreScoreCalculation,
	ScoreModifier,
	PostScoreCalculation,
	Reward,
	Ranking,
	PreShopGenerate,
	PostShopGenerate,
	PrePriceResolve,
	PostPriceResolve,
	PrePurchase,
	PostPurchase,
	ShopSkipped,
	RoundEnd
};

UENUM(BlueprintType)
enum class EGambitEffectTarget : uint8
{
	Source,
	Target,
	SourceAndTarget
};

UENUM(BlueprintType)
enum class EGambitItemEffectType : uint8
{
	None,
	ScoreModifier,
	AddScoreFlat,
	MultiplyScore,
	MultiplyDiceContribution,
	AddGold,
	SpendGold,
	ModifyDieValue,
	SetDieValue,
	LockDie,
	RerollDie,
	AddReroll,
	ModifyRerollLimit,
	GrantConsumable,
	AddTemporaryScoreModifier,
	StealScore,
	StealGold,
	PreventNegativeEffect,
	DestroyOrRemoveDiceChance,
	TransformDiceForRound,
	AddTemporaryDie,
	SetDieScoreContributionValue,
	SetDieComboContributionCount,
	SetDieCountsForScoreSum,
	SetDieCountsForCombinations,
	SetDieCanBeRerolled,
	SetDieCanBeLocked,
	MarkDieDestroyedAfterRound,
	AddDieRuntimeTags,
	RemoveDieRuntimeTags,
	ModifyShopOfferCount,
	AddShopDiscountPercent,
	AddShopSurchargePercent,
	AddShopFlatPriceDelta,
	MakeShopOfferFree,
	AddShopCashbackPercent,
	AddPurchaseGoldDelta,
	BlockShopPurchase,
	ModifyDebtLimit,
	ModifyMaxGold,
	ModifyInterestInterval,
	ModifyMaxInterest,
	ModifyInterestBonus,
	AddRecurringGoldIncome,
	SellItem,
	SellDie,
	AddSharedPoolStock,
	SetSharedPoolPurchaseLimit,
	SetSharedPoolItemUnavailable,
	CopyLastTriggeredEffect
};

UENUM(BlueprintType)
enum class EGambitNegativeEffectCategory : uint8
{
	None,
	Generic,
	GoldSteal,
	GoldLoss,
	ScorePenalty,
	ScoreSteal,
	DieValueReduction,
	DieDestroyOrRemove,
	ForcedReroll,
	LockModification,
	ShopBlock,
	SharedPoolSabotage
};

UENUM(BlueprintType)
enum class EGambitEffectConditionType : uint8
{
	None,
	DieValueEquals,
	DieValueGreater,
	DieValueLower,
	HasAtLeastMatchingDice,
	HasCombinationType,
	EvenDiceCount,
	OddDiceCount,
	AllDiceEven,
	AllDiceOdd,
	AllDiceValueGreaterOrEqual,
	AllDiceValueLowerOrEqual,
	AllDiceValueBetween,
	DiceAverage,
	RawScore,
	FirstDieValue,
	SourceDieValue,
	SourceDieLocked,
	NoComboOrHighDice,
	AllDiceUsedBySelectedCombination,
	DiceValuesPalindrome,
	OwnedDiceDistinctTypeCount,
	OwnedDiceMostRepeatedTypeCount,
	OwnedDiceRarityCount,
	OwnedDiceRarityAtLeastCount,
	OwnedItemRarityCount,
	OwnedItemRarityAtLeastCount,
	OwnedModuleRarityCount,
	OwnedModuleRarityAtLeastCount,
	RerollsRemaining,
	RerollsUsedEqualsLimit,
	LastAffectedDieValueIncreased,
	SourceDieHasMatchingValue,
	GoldThreshold,
	RankCondition,
	RerollsUsedCondition,
	ItemRarity,
	ItemTag,
	ShopItemRarity,
	ShopItemTag,
	ShopPrice,
	ShopPurchaseCount,
	ShopOfferUsesSharedPool,
	ChancePercentage,
	AllDiceValueInSet,
	SourceDieComparedToAverage,
	LowestGold,
	HighestGold,
	LastRank,
	RerollValueIncreased,
	RerollValueDecreased,
	RerollNoValueChanged,
	AnyDieRerolledAtLeastCount,
	ShopGlobalPurchaseTypeCount,
	ShopGlobalPurchaseRarityCount,
	ShopGlobalPurchaseTypeAndRarityCount
};

UENUM(BlueprintType)
enum class EGambitEffectComparison : uint8
{
	Equal,
	NotEqual,
	Greater,
	GreaterOrEqual,
	Lower,
	LowerOrEqual
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitEffectConditionDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Condition")
	EGambitEffectConditionType ConditionType = EGambitEffectConditionType::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Condition")
	EGambitEffectTarget ConditionTarget = EGambitEffectTarget::Source;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Condition")
	EGambitEffectComparison Comparison = EGambitEffectComparison::GreaterOrEqual;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Condition")
	EGambitCombinationType CombinationType = EGambitCombinationType::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Condition")
	EGambitItemRarity Rarity = EGambitItemRarity::Common;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Condition")
	FName Tag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Condition")
	int32 Value = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Condition", meta = (ClampMin = "0"))
	int32 Count = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Condition", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float ChancePercent = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Condition")
	bool bInvert = false;
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitNegativeEffectProtection
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Effect|Protection")
	TArray<EGambitNegativeEffectCategory> ProtectedCategories;

	UPROPERTY(BlueprintReadWrite, Category = "Effect|Protection")
	bool bProtectsAllCategories = false;

	UPROPERTY(BlueprintReadWrite, Category = "Effect|Protection")
	bool bUnlimitedCharges = true;

	UPROPERTY(BlueprintReadWrite, Category = "Effect|Protection")
	int32 RemainingCharges = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Effect|Protection")
	FName SourceEffectId;

	UPROPERTY(BlueprintReadWrite, Category = "Effect|Protection")
	FString SourceName;

	bool HasRemainingCharges() const
	{
		return bUnlimitedCharges || RemainingCharges > 0;
	}

	bool CanPrevent(const TArray<EGambitNegativeEffectCategory>& NegativeCategories) const
	{
		if (!HasRemainingCharges())
		{
			return false;
		}

		if (bProtectsAllCategories)
		{
			return true;
		}

		for (const EGambitNegativeEffectCategory Category : NegativeCategories)
		{
			if (Category != EGambitNegativeEffectCategory::None && ProtectedCategories.Contains(Category))
			{
				return true;
			}
		}

		return false;
	}

	bool TryPrevent(const TArray<EGambitNegativeEffectCategory>& NegativeCategories)
	{
		if (!CanPrevent(NegativeCategories))
		{
			return false;
		}

		if (!bUnlimitedCharges)
		{
			RemainingCharges = FMath::Max(0, RemainingCharges - 1);
		}

		return true;
	}
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitNegativeEffectProtectionState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Effect|Protection")
	TArray<FGambitNegativeEffectProtection> Protections;

	void AddProtection(
		const TArray<EGambitNegativeEffectCategory>& ProtectedCategories,
		const bool bProtectsAllCategories,
		const int32 ChargeCount,
		const FName SourceEffectId,
		const FString& SourceName)
	{
		FGambitNegativeEffectProtection Protection;
		Protection.ProtectedCategories = ProtectedCategories;
		Protection.bProtectsAllCategories = bProtectsAllCategories;
		Protection.bUnlimitedCharges = ChargeCount <= 0;
		Protection.RemainingCharges = Protection.bUnlimitedCharges ? 0 : ChargeCount;
		Protection.SourceEffectId = SourceEffectId;
		Protection.SourceName = SourceName;
		Protections.Add(Protection);
	}

	bool CanPrevent(const TArray<EGambitNegativeEffectCategory>& NegativeCategories) const
	{
		for (const FGambitNegativeEffectProtection& Protection : Protections)
		{
			if (Protection.CanPrevent(NegativeCategories))
			{
				return true;
			}
		}

		return false;
	}

	bool TryPrevent(
		const TArray<EGambitNegativeEffectCategory>& NegativeCategories,
		FGambitNegativeEffectProtection& OutProtection)
	{
		for (FGambitNegativeEffectProtection& Protection : Protections)
		{
			if (Protection.TryPrevent(NegativeCategories))
			{
				OutProtection = Protection;
				return true;
			}
		}

		return false;
	}
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitEffectExecutionContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	EGambitEffectHook Hook = EGambitEffectHook::ScoreModifier;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	EGambitRoundPhase CurrentPhase = EGambitRoundPhase::None;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	int32 RoundNumber = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	TObjectPtr<AGambitPlayerState> SourcePlayer = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	TObjectPtr<AGambitPlayerState> TargetPlayer = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	TObjectPtr<UGambitItemDefinition> SourceItem = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	TObjectPtr<UGambitDiceDefinition> SourceDiceDefinition = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	int32 SourceDieInstanceId = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	int32 SourceDieHandIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	int32 TargetDieInstanceId = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	int32 TargetDieHandIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	TObjectPtr<UGambitDiceComponent> SourceDiceComponent = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	TObjectPtr<UGambitDiceComponent> TargetDiceComponent = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	TObjectPtr<UGambitEconomyComponent> SourceEconomyComponent = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	TObjectPtr<UGambitEconomyComponent> TargetEconomyComponent = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	TObjectPtr<UGambitInventoryComponent> SourceInventoryComponent = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	TObjectPtr<UGambitInventoryComponent> TargetInventoryComponent = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	TObjectPtr<UGambitShopComponent> SourceShopComponent = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	TObjectPtr<UGambitShopComponent> TargetShopComponent = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	TObjectPtr<UGambitSharedPoolComponent> SharedPoolComponent = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	TArray<TObjectPtr<AGambitPlayerState>> MatchPlayerStates;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	TArray<TObjectPtr<UGambitEconomyComponent>> MatchEconomyComponents;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	int32 MatchPlayerCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	int32 ShopOfferCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	int32 ShopOfferCountDelta = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	TArray<FGambitShopOffer> GeneratedShopOffers;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	FGambitShopPurchaseContext ShopPurchase;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	TArray<FGambitDieRuntimeState> SourceDice;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	TArray<FGambitDieRuntimeState> TargetDice;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	FGambitDiceCombinationResult CurrentCombinationResult;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	FGambitScoreBreakdown CurrentScoreBreakdown;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	FGambitScoreModifierContext CurrentScoreModifier;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	FGambitScoreModifierContext ScoreModifierDelta;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	FGambitScoreModifierContext TemporaryScoreModifierDelta;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	FGambitScoreModifierContext TargetTemporaryScoreModifierDelta;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	int32 BaseGoldReward = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	int32 InterestGoldReward = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	int32 SourceRank = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	int32 TargetRank = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	int32 RerollsUsed = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	int32 RerollLimit = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	int32 RerollLimitDelta = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	TArray<FGambitRerollDieDelta> RerollDeltas;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	int32 FirstRerolledDieHandIndexThisRound = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	int32 MaxRerollCountForAnyDieThisRound = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	int32 LastAffectedDieIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	int32 LastAffectedDieValueBefore = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	int32 LastAffectedDieValueAfter = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	FGambitNegativeEffectProtectionState NegativeEffectProtection;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	FRandomStream RandomStream;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	TObjectPtr<UGambitItemEffectDefinition> LastTriggeredEffectDefinition = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	TObjectPtr<UGambitConsumableDefinition> GrantedConsumable = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	TObjectPtr<UGambitDiceDefinition> TransformDiceDefinition = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Effect|Debug")
	TArray<FGambitDebugEffectEvent> DebugEffectEvents;

	UPROPERTY(BlueprintReadWrite, Category = "Effect|Debug")
	TArray<FGambitDebugScoreLine> DebugScoreLines;

	UPROPERTY(BlueprintReadWrite, Category = "Effect|Debug")
	TArray<FGambitDebugGoldLine> DebugGoldLines;

	UPROPERTY(BlueprintReadWrite, Category = "Effect|Debug")
	TArray<FGambitDebugShopLine> DebugShopLines;

	UPROPERTY(BlueprintReadWrite, Category = "Effect|Round Events")
	TArray<FGambitRoundGameplayEvent> RoundEvents;
};
