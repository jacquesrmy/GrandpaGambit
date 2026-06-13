#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Core/Types/GambitGameplayEvents.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "GambitGameState.generated.h"

class AGambitPlayerState;
class UGambitRankingComponent;
class UGambitSharedPoolComponent;

UCLASS()
class GRANDPAGAMBIT_API AGambitGameState : public AGameState
{
	GENERATED_BODY()

public:
	AGambitGameState();

	UFUNCTION(BlueprintCallable, Category = "Gambit|GameState")
	void SetCurrentPhase(EGambitRoundPhase NewPhase);

	UFUNCTION(BlueprintCallable, Category = "Gambit|GameState")
	void SetCurrentRoundIndex(int32 NewRoundIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|GameState")
	void SetRoundRankingSnapshot(const TArray<FGambitRankingEntry>& NewRanking);

	UFUNCTION(BlueprintPure, Category = "Gambit|GameState")
	EGambitRoundPhase GetCurrentPhase() const { return CurrentPhase; }

	UFUNCTION(BlueprintPure, Category = "Gambit|GameState")
	int32 GetCurrentRoundIndex() const { return CurrentRoundIndex; }

	UFUNCTION(BlueprintPure, Category = "Gambit|GameState")
	TArray<FGambitRankingEntry> GetRoundRankingSnapshot() const { return RoundRankingSnapshot; }

	const TArray<FGambitRankingEntry>& GetRoundRankingSnapshotRef() const { return RoundRankingSnapshot; }

	UFUNCTION(BlueprintPure, Category = "Gambit|GameState")
	UGambitSharedPoolComponent* GetSharedPoolComponent() const { return SharedPoolComponent; }

	UFUNCTION(BlueprintPure, Category = "Gambit|GameState")
	UGambitRankingComponent* GetRankingComponent() const { return RankingComponent; }

	UFUNCTION(BlueprintCallable, Category = "Gambit|GameState")
	TArray<AGambitPlayerState*> GetGambitPlayerStates() const;

	UPROPERTY(BlueprintAssignable, Category = "Gambit|GameState")
	FOnGambitPhaseChanged OnPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "Gambit|GameState")
	FOnGambitRoundChanged OnRoundChanged;

	UPROPERTY(BlueprintAssignable, Category = "Gambit|GameState")
	FOnGambitRankingUpdated OnRankingUpdated;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|GameState", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGambitSharedPoolComponent> SharedPoolComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|GameState", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGambitRankingComponent> RankingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|GameState", meta = (AllowPrivateAccess = "true"))
	EGambitRoundPhase CurrentPhase = EGambitRoundPhase::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|GameState", meta = (AllowPrivateAccess = "true"))
	int32 CurrentRoundIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|GameState", meta = (AllowPrivateAccess = "true"))
	TArray<FGambitRankingEntry> RoundRankingSnapshot;
};
