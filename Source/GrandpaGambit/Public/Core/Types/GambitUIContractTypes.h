#pragma once

#include "CoreMinimal.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "Core/Types/GambitRoundFeedbackTypes.h"
#include "Core/Types/GambitTargetSelectionTypes.h"
#include "GambitUIContractTypes.generated.h"

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitUITargetSelectionSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	bool bHasPendingSelection = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	FGambitTargetSelectionRequest Request;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	int32 SelectedOptionId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	bool bHasSelectedOption = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	FGambitTargetSelectionOption SelectedOption;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	bool bCanConfirmSelection = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	FString ConfirmUnavailableReason;
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitUIDieActionSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	FGambitDiceSnapshot Die;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	bool bCanToggleLock = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	bool bCanLock = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	bool bCanUnlock = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	FString LockUnavailableReason;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	bool bCanBeRerolledByNextReroll = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	FString RerollUnavailableReason;
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitUIConsumableActionSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	FGambitItemSnapshot Consumable;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	bool bSlotOccupied = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	bool bCanUse = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	FString UseUnavailableReason;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	bool bRequiresTargetSelection = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	bool bHasValidTargetOptions = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	FGambitTargetSelectionRequest TargetSelectionPreview;
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitUIPlayerActionSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	int32 PlayerIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	EGambitRoundPhase Phase = EGambitRoundPhase::None;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	bool bCanReadyForPhase = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	FString ReadyUnavailableReason;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	int32 RerollsUsed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	int32 RerollLimit = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	int32 RerollsRemaining = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	bool bCanReroll = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	FString RerollUnavailableReason;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	TArray<FGambitUIDieActionSnapshot> DiceActions;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	TArray<FGambitUIConsumableActionSnapshot> ConsumableActions;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	TArray<FGambitShopOfferSnapshot> ShopOffers;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	bool bCanPurchaseAnyOffer = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	FString ShopUnavailableReason;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	FGambitUITargetSelectionSnapshot TargetSelection;
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitUIPlayerSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	FGambitPlayerSnapshot Player;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	FGambitUIPlayerActionSnapshot Actions;
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitUIMatchSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	EGambitMatchLifecycleState LifecycleState = EGambitMatchLifecycleState::MainMenu;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	FGambitMatchSetupConfig MatchSetup;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	int32 CurrentRoundIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	EGambitRoundPhase CurrentPhase = EGambitRoundPhase::None;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	bool bIsReadyGatedPhase = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	TArray<FGambitRankingEntry> RoundRanking;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	TArray<FGambitFinalRankingEntry> FinalRanking;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|UI Contract")
	TArray<FGambitUIPlayerSnapshot> Players;
};
