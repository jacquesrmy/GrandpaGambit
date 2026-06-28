#pragma once

#include "CoreMinimal.h"
#include "GambitGameplayTypes.generated.h"

class APlayerState;
class UGambitConsumableDefinition;
class UGambitDiceDefinition;
class UGambitItemDefinition;
class UGambitModuleDefinition;

UENUM(BlueprintType)
enum class EGambitRoundPhase : uint8
{
	None UMETA(DisplayName = "None"),
	Roll UMETA(DisplayName = "Roll"),
	SelectionReroll UMETA(DisplayName = "Selection / Reroll"),
	Action UMETA(DisplayName = "Action"),
	Resolution UMETA(DisplayName = "Resolution"),
	Reward UMETA(DisplayName = "Reward"),
	Ranking UMETA(DisplayName = "Ranking"),
	Shop UMETA(DisplayName = "Shop"),
	RoundEnd UMETA(DisplayName = "Round End")
};

UENUM(BlueprintType)
enum class EGambitMatchLifecycleState : uint8
{
	MainMenu UMETA(DisplayName = "Main Menu"),
	MatchSetup UMETA(DisplayName = "Match Setup"),
	Lobby UMETA(DisplayName = "Lobby"),
	InMatch UMETA(DisplayName = "In Match"),
	MatchComplete UMETA(DisplayName = "Match Complete")
};

UENUM(BlueprintType)
enum class EGambitItemRarity : uint8
{
	Common,
	Uncommon,
	Rare,
	Epic,
	Legendary
};

UENUM(BlueprintType)
enum class EGambitItemType : uint8
{
	None,
	Dice,
	Module,
	Consumable
};

UENUM(BlueprintType)
enum class EGambitSharedPoolAvailabilityState : uint8
{
	Available,
	NotSharedPoolItem,
	MissingItem,
	UntrackedItem,
	NoStockRemaining,
	PurchaseLimitReached,
	ReservedOut
};

UENUM(BlueprintType)
enum class EGambitDiceType : uint8
{
	D6,
	D8,
	D10,
	D12,
	Wildcard
};

UENUM(BlueprintType)
enum class EGambitCombinationType : uint8
{
	None,
	HighDice,
	Pair,
	TwoPair,
	ThreeOfAKind,
	StraightSmall,
	FullHouse,
	FourOfAKind,
	StraightLarge,
	FiveOfAKind
};

USTRUCT(BlueprintType)
struct FGambitDieRuntimeState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Dice")
	TObjectPtr<UGambitDiceDefinition> DiceDefinition = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Dice")
	TObjectPtr<UGambitDiceDefinition> OriginalDiceDefinition = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Dice")
	TArray<int32> RuntimeFaces;

	UPROPERTY(BlueprintReadOnly, Category = "Dice")
	bool bHasRuntimeFaceOverride = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dice")
	int32 InstanceId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Dice")
	int32 HandIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Dice")
	int32 RolledFaceIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Dice")
	int32 RawValue = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Dice")
	int32 EffectiveValue = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Dice")
	int32 ScoreContributionValue = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Dice")
	int32 ComboContributionCount = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Dice")
	bool bCountsForScoreSum = true;

	UPROPERTY(BlueprintReadOnly, Category = "Dice")
	bool bCountsForCombinations = true;

	UPROPERTY(BlueprintReadOnly, Category = "Dice")
	bool bCanBeRerolled = true;

	UPROPERTY(BlueprintReadOnly, Category = "Dice")
	bool bCanBeLocked = true;

	UPROPERTY(BlueprintReadOnly, Category = "Dice")
	bool bDestroyedAfterRound = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dice")
	bool bRemovedFromRound = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dice")
	bool bTemporaryDie = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dice")
	bool bTemporarilyTransformed = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dice")
	FName RuntimeSourceItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Dice")
	FName RuntimeSourceEffectId;

	UPROPERTY(BlueprintReadOnly, Category = "Dice")
	TArray<FName> AppliedRuntimeEffectIds;

	UPROPERTY(BlueprintReadOnly, Category = "Dice")
	int32 PreviousRoundValue = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Dice")
	TArray<FName> RuntimeTags;

	UPROPERTY(BlueprintReadOnly, Category = "Dice")
	bool bLocked = false;
};

USTRUCT(BlueprintType)
struct FGambitRerollDieDelta
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Reroll")
	int32 DieInstanceId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Reroll")
	int32 HandIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Reroll")
	int32 ValueBefore = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Reroll")
	int32 ValueAfter = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Reroll")
	int32 RerollCountForDieThisRound = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Reroll")
	bool bFirstRerolledDieThisRound = false;

	bool DidValueChange() const
	{
		return ValueBefore != INDEX_NONE && ValueAfter != INDEX_NONE && ValueBefore != ValueAfter;
	}
};

USTRUCT(BlueprintType)
struct FGambitDiceCombinationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	EGambitCombinationType Combination = EGambitCombinationType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 BaseCombinationScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 DiceSum = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 RawScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 PairCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	TArray<int32> MatchedValues;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	TArray<int32> MatchedDieIndexes;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	bool bAllDiceUsedBySelectedCombo = false;
};

USTRUCT(BlueprintType)
struct FGambitScoreModifierContext
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Score")
	float AdditiveBonus = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Score")
	float DiceContributionMultiplierBonus = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Score")
	float Multiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Score")
	float ScoreCap = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Score")
	float DiminishingThreshold = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Score")
	float DiminishingFactor = 1.0f;
};

USTRUCT(BlueprintType)
struct FGambitScoreBreakdown
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	EGambitCombinationType Combination = EGambitCombinationType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 BaseCombinationScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 DiceSum = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	float DiceContributionMultiplierBonus = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	float AdjustedDiceSum = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 RawScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	float AdditiveBonus = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	float Multiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	float ScoreBeforeCap = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	float ScoreAfterCap = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 FinalScore = 0;
};

USTRUCT(BlueprintType)
struct FGambitRoundPlayerResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	TObjectPtr<APlayerState> PlayerState = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	FGambitScoreBreakdown ScoreBreakdown;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 GoldGranted = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Round")
	int32 VictoryPointsGranted = 0;
};

USTRUCT(BlueprintType)
struct FGambitRankingEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ranking")
	int32 Rank = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Ranking")
	TObjectPtr<APlayerState> PlayerState = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Ranking")
	int32 RoundScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ranking")
	int32 VictoryPointsGranted = 0;
};

USTRUCT(BlueprintType)
struct FGambitFinalRankingEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ranking")
	int32 Rank = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Ranking")
	TObjectPtr<APlayerState> PlayerState = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Ranking")
	FString PlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "Ranking")
	int32 TotalVictoryPoints = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ranking")
	int32 LastRoundScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ranking")
	int32 Gold = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ranking")
	bool bWinner = false;
};

USTRUCT(BlueprintType)
struct FGambitMatchSetupConfig
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Match Setup")
	int32 LocalPlayerCount = 2;

	UPROPERTY(BlueprintReadOnly, Category = "Match Setup")
	int32 RoundCount = 8;
};

USTRUCT(BlueprintType)
struct FGambitShopOffer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	int32 OfferId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	TObjectPtr<UGambitItemDefinition> ItemDefinition = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	int32 BasePrice = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	int32 Price = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	bool bUsesSharedPool = false;

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	bool bHasSharedPoolReservation = false;

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	FGuid SharedPoolReservationId;

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	bool bFree = false;

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	FString FreeReason;
};

USTRUCT(BlueprintType)
struct FGambitShopWeightedEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop")
	FName ItemId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop")
	TObjectPtr<UGambitItemDefinition> ItemDefinition = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = "-1"))
	int32 PriceOverride = -1;
};

USTRUCT(BlueprintType)
struct FGambitSharedStockEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shared Pool")
	FName ItemId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shared Pool")
	TObjectPtr<UGambitItemDefinition> ItemDefinition = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shared Pool", meta = (ClampMin = "0"))
	int32 MaxStock = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shared Pool", meta = (ClampMin = "0"))
	int32 GlobalPurchaseLimit = 0;
};

USTRUCT(BlueprintType)
struct FGambitSharedPoolAvailabilityResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Shared Pool")
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Shared Pool")
	EGambitSharedPoolAvailabilityState State = EGambitSharedPoolAvailabilityState::MissingItem;

	UPROPERTY(BlueprintReadOnly, Category = "Shared Pool")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Shared Pool")
	int32 AvailableStock = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Shared Pool")
	int32 ReservedStock = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Shared Pool")
	int32 MaxStock = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Shared Pool")
	int32 PurchasedCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Shared Pool")
	int32 PurchaseLimit = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Shared Pool")
	FString Reason;
};

USTRUCT(BlueprintType)
struct FGambitRecurringGoldIncome
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	FName SourceId;

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 GoldPerRound = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 RemainingRounds = 0;
};

USTRUCT(BlueprintType)
struct FGambitShopPurchaseContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Shop")
	int32 OfferId = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "Shop")
	TObjectPtr<UGambitItemDefinition> ItemDefinition = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Shop")
	int32 BasePrice = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Shop")
	int32 PriceBeforeModifiers = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Shop")
	int32 ResolvedPrice = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Shop")
	float DiscountPercent = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Shop")
	float SurchargePercent = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Shop")
	int32 FlatPriceDelta = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Shop")
	bool bFree = false;

	UPROPERTY(BlueprintReadWrite, Category = "Shop")
	FString FreeReason;

	UPROPERTY(BlueprintReadWrite, Category = "Shop")
	float CashbackPercent = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Shop")
	int32 CashbackGold = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Shop")
	int32 GoldDeltaOnPurchase = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Shop")
	int32 PurchasesMadeBefore = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Shop")
	int32 GlobalPurchasesForItemType = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Shop")
	int32 GlobalPurchasesForItemRarity = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Shop")
	int32 GlobalPurchasesForItemTypeAndRarity = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Shop")
	bool bUsesSharedPool = false;

	UPROPERTY(BlueprintReadWrite, Category = "Shop")
	bool bHasSharedPoolReservation = false;

	UPROPERTY(BlueprintReadWrite, Category = "Shop")
	FGuid SharedPoolReservationId;

	UPROPERTY(BlueprintReadWrite, Category = "Shop")
	FGambitSharedPoolAvailabilityResult SharedPoolAvailability;

	UPROPERTY(BlueprintReadWrite, Category = "Shop")
	bool bBlockedByEffect = false;

	UPROPERTY(BlueprintReadWrite, Category = "Shop")
	bool bCanPurchase = false;

	UPROPERTY(BlueprintReadWrite, Category = "Shop")
	bool bPurchaseSucceeded = false;

	UPROPERTY(BlueprintReadWrite, Category = "Shop")
	FString FailureReason;

	UPROPERTY(BlueprintReadWrite, Category = "Shop")
	TArray<FString> DebugLines;
};

USTRUCT(BlueprintType)
struct FGambitItemCatalogEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Catalog")
	FName ItemId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Catalog")
	TObjectPtr<UGambitItemDefinition> ItemDefinition = nullptr;
};

USTRUCT(BlueprintType)
struct FGambitDiceCatalogEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Catalog")
	FName DiceId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Catalog")
	TObjectPtr<UGambitDiceDefinition> DiceDefinition = nullptr;
};

USTRUCT(BlueprintType)
struct FGambitPlayerLoadout
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout")
	TArray<TObjectPtr<UGambitDiceDefinition>> StartingDice;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout")
	TArray<TObjectPtr<UGambitModuleDefinition>> StartingModules;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout")
	TArray<TObjectPtr<UGambitConsumableDefinition>> StartingConsumables;
};

USTRUCT(BlueprintType)
struct FGambitVictoryPointReward
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranking", meta = (ClampMin = "1"))
	int32 Rank = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranking", meta = (ClampMin = "0"))
	int32 VictoryPoints = 0;
};

USTRUCT(BlueprintType)
struct FGambitConsumableRuntimeSlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Consumable")
	FGuid InventoryInstanceId;

	UPROPERTY(BlueprintReadOnly, Category = "Consumable")
	TObjectPtr<UGambitConsumableDefinition> Definition = nullptr;

	bool IsValid() const
	{
		return Definition != nullptr;
	}
};

USTRUCT(BlueprintType)
struct FGambitPlayerSlotState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Slots")
	int32 OwnedDiceCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slots")
	int32 ModuleSlotsUsed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slots")
	int32 ModuleSlotsCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slots")
	int32 ConsumableSlotsUsed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slots")
	int32 ConsumableSlotsCapacity = 0;
};

USTRUCT(BlueprintType)
struct FGambitPlayerRuntimeSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Player")
	int32 CurrentGold = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Player")
	int32 CurrentRoundScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Player")
	int32 TotalVictoryPoints = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Player")
	FGambitPlayerSlotState SlotState;
};
