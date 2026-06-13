#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GambitMatchDebugComponent.generated.h"

class AGambitGameMode;
class AGambitGameState;
class AGambitPlayerState;
class UGambitDebugAutoPlayer;

UCLASS(ClassGroup = (Gambit), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class GRANDPAGAMBIT_API UGambitMatchDebugComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGambitMatchDebugComponent();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug")
	void PrintMatchSnapshot() const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug")
	void PrintPlayerSnapshot(AGambitPlayerState* PlayerState) const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug")
	void PrintAllPlayerSnapshots() const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug")
	void PrintPlayerDebugReport(AGambitPlayerState* PlayerState) const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug")
	void PrintAllPlayerDebugReports() const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug")
	void DebugReadyAllPlayers();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug")
	void DebugAutoAdvanceCurrentPhase();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug")
	void DebugAutoAdvanceUntilShopOrMatchEnd();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug")
	void DebugBuyFirstAffordableOfferForAllPlayers();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug")
	void DebugSkipShopForAllPlayers();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug")
	void PrintDebugCommandHelp() const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug")
	void DebugGrantGoldToAllPlayers(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug")
	void DebugGrantConsumable(int32 PlayerIndex, FName ConsumableId);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug")
	void DebugPrintShop() const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug")
	void DebugBuyOffer(int32 PlayerIndex, int32 OfferId);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug")
	void DebugRerollPlayer(int32 PlayerIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug")
	void DebugLockDie(int32 PlayerIndex, int32 DieIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug")
	void DebugUseConsumable(int32 PlayerIndex, int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug")
	void DebugUseConsumableOnDie(int32 PlayerIndex, int32 SlotIndex, int32 DieIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug")
	void DebugAutoFullMatch();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug")
	void DebugAIDecideRerolls();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug")
	void DebugAIDecideActions();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug")
	void DebugAIDecideShop();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug")
	void DebugAIFullRound();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug")
	void DebugAIFullMatch();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug")
	void DebugValidateData() const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug")
	void DebugPrintInventory() const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Debug")
	void DebugPrintSharedPool() const;

private:
	AGambitGameMode* GetGambitGameMode() const;
	AGambitGameState* GetGambitGameState() const;
	TArray<AGambitPlayerState*> GetAllPlayers() const;
	AGambitPlayerState* GetPlayerByIndex(int32 PlayerIndex) const;
	UGambitDebugAutoPlayer* GetOrCreateDebugAutoPlayer();
	void PrintFinalMatchSummary() const;
	void SetAllPlayersReady(bool bReady) const;

	UPROPERTY(Transient)
	TObjectPtr<UGambitDebugAutoPlayer> DebugAutoPlayer;
};
