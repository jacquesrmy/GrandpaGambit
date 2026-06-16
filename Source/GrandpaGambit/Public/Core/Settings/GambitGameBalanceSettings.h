#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "GambitGameBalanceSettings.generated.h"

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitCombinationScoreEntry
{
	GENERATED_BODY()

	FGambitCombinationScoreEntry() = default;

	FGambitCombinationScoreEntry(const EGambitCombinationType InCombination, const int32 InBaseScore)
		: Combination(InCombination)
		, BaseScore(InBaseScore)
	{
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scoring")
	EGambitCombinationType Combination = EGambitCombinationType::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scoring", meta = (ClampMin = "0"))
	int32 BaseScore = 0;
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Grandpa Gambit Balance"))
class GRANDPAGAMBIT_API UGambitGameBalanceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGambitGameBalanceSettings();

	static const UGambitGameBalanceSettings* Get();

	UFUNCTION(BlueprintPure, Category = "Gambit|Settings")
	int32 GetVictoryPointsForRank(int32 Rank) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Settings")
	int32 GetInterestGoldBonus(int32 CurrentGold) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Settings")
	bool IsSupportedLocalPlayerCount(int32 LocalPlayerCount) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Settings")
	int32 ClampLocalPlayerCount(int32 RequestedLocalPlayerCount) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Settings")
	int32 GetRoundGoldRewardFromScore(int32 RoundScore) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Settings")
	int32 GetBaseScoreForCombination(EGambitCombinationType Combination) const;

	void ValidateCombinationBaseScores(TArray<FString>& OutWarnings) const;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "2", ClampMax = "4"))
	int32 MinLocalPlayers = 2;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "2", ClampMax = "4"))
	int32 MaxLocalPlayers = 4;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "1"))
	int32 RoundCount = 8;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Dice", meta = (ClampMin = "1"))
	int32 DefaultActiveDiceCount = 4;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Dice", meta = (ClampMin = "0"))
	int32 MaxRerollsPerRound = 2;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "1"))
	int32 ModuleSlotCount = 4;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "1"))
	int32 ConsumableSlotCount = 3;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Economy", meta = (ClampMin = "0"))
	int32 StartingGold = 0;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Economy", meta = (ClampMin = "1"))
	int32 MaxGold = 50;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Economy", meta = (ClampMin = "1"))
	int32 InterestIntervalGold = 10;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Economy", meta = (ClampMin = "0"))
	int32 MaxInterestGold = 5;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Reward", meta = (ClampMin = "0"))
	int32 BaseRoundGoldReward = 1;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Reward", meta = (ClampMin = "1"))
	int32 ScorePerGoldStep = 20;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Reward", meta = (ClampMin = "1"))
	int32 MaxRoundGoldReward = 10;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = "1"))
	int32 ShopOfferCount = 3;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Scoring", meta = (ClampMin = "0.0"))
	float DefaultScoreCap = 0.0f;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Scoring", meta = (ClampMin = "0.0"))
	float DefaultDiminishingThreshold = 0.0f;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Scoring", meta = (ClampMin = "0.0"))
	float DefaultDiminishingFactor = 1.0f;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Scoring")
	TArray<FGambitCombinationScoreEntry> CombinationBaseScores;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Ranking")
	TArray<FGambitVictoryPointReward> VictoryPointTable;
};
