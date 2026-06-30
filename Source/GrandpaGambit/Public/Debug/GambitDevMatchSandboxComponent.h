#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Debug/GambitDevMatchSandboxTypes.h"
#include "GambitDevMatchSandboxComponent.generated.h"

class AGambitGameMode;
class AGambitGameState;
class AGambitPlayerController;
class AGambitPlayerState;
class UGambitDebugAutoPlayer;
class UGambitLocalMultiplayerSubsystem;

UCLASS(ClassGroup = (Gambit), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class GRANDPAGAMBIT_API UGambitDevMatchSandboxComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGambitDevMatchSandboxComponent();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox")
	FGambitDevSandboxActionResult StartDevMatchSandbox(int32 DesiredLocalPlayerCount);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox")
	FGambitDevSandboxActionResult RestartDevMatchSandbox();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox")
	FGambitDevSandboxActionResult EnsureLocalPlayerCount(int32 DesiredLocalPlayerCount);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox")
	FGambitDevSandboxActionResult JoinLocalPlayer();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox")
	FGambitDevSandboxActionResult LeaveLastLocalPlayer();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox")
	FGambitDevSandboxActionResult SetInspectedPlayerIndex(int32 PlayerIndex);

	UFUNCTION(BlueprintPure, Category = "Gambit|Dev Sandbox")
	int32 GetInspectedPlayerIndex() const { return InspectedPlayerIndex; }

	UFUNCTION(BlueprintPure, Category = "Gambit|Dev Sandbox")
	AGambitPlayerState* GetInspectedPlayerState() const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox")
	FGambitDevSandboxActionResult SetPlayerControlMode(int32 PlayerIndex, EGambitDevSandboxPlayerControlMode ControlMode);

	UFUNCTION(BlueprintPure, Category = "Gambit|Dev Sandbox")
	EGambitDevSandboxPlayerControlMode GetPlayerControlMode(int32 PlayerIndex) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Dev Sandbox")
	FGambitDevSandboxSnapshot BuildSandboxSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Dev Sandbox")
	TArray<FGambitDevSandboxPlayerSlotSnapshot> BuildPlayerSlotSnapshots() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Dev Sandbox")
	TArray<FGambitPlayerSnapshot> BuildPlayerSnapshots() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Dev Sandbox")
	bool BuildPlayerSnapshotByIndex(int32 PlayerIndex, UPARAM(ref) FGambitPlayerSnapshot& OutSnapshot) const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|Commands")
	FGambitDevSandboxActionResult RequestPlayerReady(int32 PlayerIndex, bool bReady);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|Commands")
	FGambitDevSandboxActionResult RequestSetDieLocked(int32 PlayerIndex, int32 DieIndex, bool bLocked);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|Commands")
	FGambitDevSandboxActionResult RequestToggleDieLock(int32 PlayerIndex, int32 DieIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|Commands")
	FGambitDevSandboxActionResult RequestReroll(int32 PlayerIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|Commands")
	FGambitDevSandboxActionResult RequestUseConsumable(int32 PlayerIndex, int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|Commands")
	FGambitDevSandboxActionResult RequestUseConsumableOnDie(int32 PlayerIndex, int32 SlotIndex, int32 DieIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|Commands")
	FGambitDevSandboxActionResult RequestBeginConsumableTargetSelection(int32 PlayerIndex, int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|Commands")
	FGambitDevSandboxActionResult RequestSelectTargetSelectionOption(int32 PlayerIndex, int32 OptionId);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|Commands")
	FGambitDevSandboxActionResult RequestConfirmTargetSelection(int32 PlayerIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|Commands")
	FGambitDevSandboxActionResult RequestCancelTargetSelection(int32 PlayerIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|Commands")
	FGambitDevSandboxActionResult RequestPurchaseOffer(int32 PlayerIndex, int32 OfferId);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|Commands")
	FGambitDevSandboxActionResult RequestSkipShop(int32 PlayerIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|Commands")
	FGambitDevSandboxActionResult RequestAdvanceCurrentPhase();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|Commands")
	FGambitDevSandboxActionResult RequestAdvanceToShopOrMatchEnd();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|AI")
	FGambitDevSandboxActionResult RequestAIDecisionForPlayer(int32 PlayerIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|AI")
	FGambitDevSandboxActionResult RequestAIDecisionForAIPlayers();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|AI")
	FGambitDevSandboxActionResult RequestAIDecisionForAllPlayers();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|AI")
	FGambitDevSandboxActionResult RequestAIShopDecisionForPlayer(int32 PlayerIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|AI")
	FGambitDevSandboxActionResult RequestAIShopDecisionForAIPlayers();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|AI")
	FGambitDevSandboxActionResult RequestAIShopDecisionForAllPlayers();

private:
	FGambitDevSandboxActionResult MakeResult(
		bool bSuccess,
		EGambitDevSandboxActionStatus Status,
		const FString& Message,
		int32 PlayerIndex = INDEX_NONE,
		EGambitRoundPhase PhaseBefore = EGambitRoundPhase::None) const;

	AGambitGameMode* GetGambitGameMode() const;
	AGambitGameState* GetGambitGameState() const;
	UGambitLocalMultiplayerSubsystem* GetLocalMultiplayerSubsystem() const;
	TArray<AGambitPlayerState*> GetAllPlayers() const;
	AGambitPlayerState* GetPlayerByIndex(int32 PlayerIndex) const;
	AGambitPlayerController* GetControllerForPlayer(AGambitPlayerState* PlayerState) const;
	AGambitPlayerController* GetControllerForPlayerIndex(int32 PlayerIndex) const;
	UGambitDebugAutoPlayer* GetOrCreateAutoPlayer();
	void NormalizeInspectedPlayerIndex();
	void SetAllPlayersReady(bool bReady) const;
	FGambitDevSandboxActionResult RequestAIDecisionForPlayers(const TArray<int32>& PlayerIndices, const FString& Label);
	FGambitDevSandboxActionResult RequestAIShopDecisionForPlayers(const TArray<int32>& PlayerIndices, const FString& Label);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gambit|Dev Sandbox", meta = (AllowPrivateAccess = "true", ClampMin = "1", ClampMax = "4"))
	int32 DefaultDevLocalPlayerCount = 2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Dev Sandbox", meta = (AllowPrivateAccess = "true"))
	int32 InspectedPlayerIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Dev Sandbox", meta = (AllowPrivateAccess = "true"))
	TMap<int32, EGambitDevSandboxPlayerControlMode> ControlModeByPlayerIndex;

	UPROPERTY(Transient)
	TObjectPtr<UGambitDebugAutoPlayer> AutoPlayer;
};
