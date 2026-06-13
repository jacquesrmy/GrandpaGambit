#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GambitDebugAutoPlayer.generated.h"

class AGambitGameMode;
class AGambitPlayerState;
class UGambitDiceCombinationEvaluator;
class UGambitScoreCalculator;

UCLASS(BlueprintType)
class GRANDPAGAMBIT_API UGambitDebugAutoPlayer : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug AI")
	void DecideRerolls(AGambitGameMode* GameMode);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug AI")
	void DecideRerollsForPlayer(AGambitGameMode* GameMode, AGambitPlayerState* PlayerState, int32 PlayerIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug AI")
	void DecideActions(AGambitGameMode* GameMode);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug AI")
	void DecideActionsForPlayer(AGambitGameMode* GameMode, AGambitPlayerState* PlayerState, int32 PlayerIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug AI")
	void DecideShop(AGambitGameMode* GameMode);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug AI")
	void DecideShopForPlayer(AGambitGameMode* GameMode, AGambitPlayerState* PlayerState, int32 PlayerIndex);

private:
	void EnsureScoringServices();

	UPROPERTY(Transient)
	TObjectPtr<UGambitDiceCombinationEvaluator> DiceEvaluator;

	UPROPERTY(Transient)
	TObjectPtr<UGambitScoreCalculator> ScoreCalculator;
};
