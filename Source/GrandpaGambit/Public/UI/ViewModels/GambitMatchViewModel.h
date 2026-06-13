#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/Types/GambitDebugTypes.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "GambitMatchViewModel.generated.h"

class AGambitGameState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGambitViewModelUpdated);

UCLASS(BlueprintType)
class GRANDPAGAMBIT_API UGambitMatchViewModel : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Gambit|UI")
	void InitializeFromGameState(AGambitGameState* InGameState);

	UFUNCTION(BlueprintPure, Category = "Gambit|UI")
	int32 GetRoundIndex() const { return RoundIndex; }

	UFUNCTION(BlueprintPure, Category = "Gambit|UI")
	EGambitRoundPhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "Gambit|UI")
	TArray<FGambitRankingEntry> GetRankingSnapshot() const { return RankingSnapshot; }

	const TArray<FGambitRankingEntry>& GetRankingSnapshotRef() const { return RankingSnapshot; }

	UFUNCTION(BlueprintPure, Category = "Gambit|UI|Debug")
	TArray<FGambitDebugPlayerSnapshot> BuildDebugPlayerSnapshots() const;

	UPROPERTY(BlueprintAssignable, Category = "Gambit|UI")
	FOnGambitViewModelUpdated OnViewModelUpdated;

private:
	UFUNCTION()
	void HandlePhaseChanged(EGambitRoundPhase OldPhase, EGambitRoundPhase NewPhase);

	UFUNCTION()
	void HandleRoundChanged(int32 NewRoundIndex);

	UFUNCTION()
	void HandleRankingUpdated();

	UPROPERTY(Transient)
	TObjectPtr<AGambitGameState> BoundGameState = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|UI", meta = (AllowPrivateAccess = "true"))
	int32 RoundIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|UI", meta = (AllowPrivateAccess = "true"))
	EGambitRoundPhase Phase = EGambitRoundPhase::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|UI", meta = (AllowPrivateAccess = "true"))
	TArray<FGambitRankingEntry> RankingSnapshot;
};
