#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "Core/Types/GambitTargetSelectionTypes.h"
#include "Core/Types/GambitUIContractTypes.h"
#include "GambitGameMode.generated.h"

class AGambitPlayerState;
#if !UE_BUILD_SHIPPING
class UGambitDevMatchSandboxComponent;
class UGambitMatchDebugComponent;
#endif
class UGambitRoundFlowComponent;

UCLASS()
class GRANDPAGAMBIT_API AGambitGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AGambitGameMode();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

	UFUNCTION(BlueprintCallable, Category = "Gambit|GameMode")
	void StartMatchFlow();

	UFUNCTION(BlueprintCallable, Category = "Gambit|GameMode")
	void RestartMatchFlow();

	UFUNCTION(BlueprintCallable, Category = "Gambit|GameMode|Match Flow")
	void RequestMainMenu();

	UFUNCTION(BlueprintCallable, Category = "Gambit|GameMode|Match Flow")
	void RequestOpenMatchSetup();

	UFUNCTION(BlueprintCallable, Category = "Gambit|GameMode|Match Flow")
	bool RequestConfigureMatch(int32 LocalPlayerCount, int32 RoundCount);

	UFUNCTION(BlueprintCallable, Category = "Gambit|GameMode|Match Flow")
	bool RequestEnterLobby();

	UFUNCTION(BlueprintCallable, Category = "Gambit|GameMode|Match Flow")
	bool RequestStartConfiguredMatch();

	UFUNCTION(BlueprintCallable, Category = "Gambit|GameMode|Match Flow")
	bool RequestReadyAllPlayersForCurrentPhase();

	UFUNCTION(BlueprintPure, Category = "Gambit|GameMode|Match Flow")
	FGambitMatchSetupConfig GetPendingMatchSetup() const { return PendingMatchSetup; }

	UFUNCTION(BlueprintCallable, Category = "Gambit|GameMode")
	void SetPlayerReady(AGambitPlayerState* PlayerState, bool bReady);

	UFUNCTION(BlueprintCallable, Category = "Gambit|GameMode")
	bool RequestSetDieLocked(AGambitPlayerState* PlayerState, int32 DieIndex, bool bLocked);

	UFUNCTION(BlueprintCallable, Category = "Gambit|GameMode")
	FGambitRoundCommandResult RequestSetDieLockedDetailed(AGambitPlayerState* PlayerState, int32 DieIndex, bool bLocked);

	UFUNCTION(BlueprintCallable, Category = "Gambit|GameMode")
	bool RequestReroll(AGambitPlayerState* PlayerState);

	UFUNCTION(BlueprintCallable, Category = "Gambit|GameMode")
	FGambitRoundCommandResult RequestRerollDetailed(AGambitPlayerState* PlayerState);

	UFUNCTION(BlueprintCallable, Category = "Gambit|GameMode")
	bool RequestUseConsumable(AGambitPlayerState* PlayerState, int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|GameMode")
	bool RequestUseConsumableOnTarget(AGambitPlayerState* PlayerState, int32 SlotIndex, AGambitPlayerState* TargetPlayerState);

	UFUNCTION(BlueprintCallable, Category = "Gambit|GameMode")
	bool RequestUseConsumableOnTargetSelectedDie(
		AGambitPlayerState* PlayerState,
		int32 SlotIndex,
		AGambitPlayerState* TargetPlayerState,
		int32 SelectedDieIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|GameMode|Target Selection")
	bool BuildConsumableTargetSelectionRequest(
		AGambitPlayerState* PlayerState,
		int32 SlotIndex,
		UPARAM(ref) FGambitTargetSelectionRequest& OutRequest) const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|GameMode|Target Selection")
	bool RequestUseConsumableWithTargetSelectionResult(
		AGambitPlayerState* PlayerState,
		const FGambitTargetSelectionResult& TargetSelectionResult);

	UFUNCTION(BlueprintCallable, Category = "Gambit|GameMode")
	bool RequestPurchaseOffer(AGambitPlayerState* PlayerState, int32 OfferId);

	UFUNCTION(BlueprintCallable, Category = "Gambit|GameMode")
	FGambitRoundCommandResult RequestPurchaseOfferDetailed(AGambitPlayerState* PlayerState, int32 OfferId);

	UFUNCTION(BlueprintPure, Category = "Gambit|GameMode")
	TArray<FGambitShopOfferSnapshot> BuildShopOfferSnapshots(AGambitPlayerState* PlayerState) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|UI Contract")
	FGambitUIPlayerActionSnapshot BuildPlayerActionSnapshot(AGambitPlayerState* PlayerState) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|GameMode")
	UGambitRoundFlowComponent* GetRoundFlowComponent() const { return RoundFlowComponent; }

#if !UE_BUILD_SHIPPING
	UGambitMatchDebugComponent* GetMatchDebugComponent() const { return MatchDebugComponent; }

	UGambitDevMatchSandboxComponent* GetDevMatchSandboxComponent() const { return DevMatchSandboxComponent; }
#endif

private:
	FGambitMatchSetupConfig BuildDefaultMatchSetup() const;
	FGambitMatchSetupConfig BuildClampedMatchSetup(int32 LocalPlayerCount, int32 RoundCount) const;
	void PublishMatchSetup() const;
	void StartMatchWithSetup(const FGambitMatchSetupConfig& MatchSetup, bool bForceRestart);
	void ResetMatchShellState(EGambitMatchLifecycleState NewState);
	void InitializePlayerForMatch(APlayerController* PlayerController) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|GameMode", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGambitRoundFlowComponent> RoundFlowComponent;

#if !UE_BUILD_SHIPPING
	UGambitMatchDebugComponent* MatchDebugComponent = nullptr;

	UGambitDevMatchSandboxComponent* DevMatchSandboxComponent = nullptr;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gambit|GameMode|PC Shell", meta = (AllowPrivateAccess = "true"))
	bool bAutoStartMatchOnBeginPlay = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|GameMode|PC Shell", meta = (AllowPrivateAccess = "true"))
	FGambitMatchSetupConfig PendingMatchSetup;

	bool bMatchFlowStarted = false;
};
