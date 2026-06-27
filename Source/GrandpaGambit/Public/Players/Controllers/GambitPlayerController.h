#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Core/Types/GambitTargetSelectionTypes.h"
#include "GambitPlayerController.generated.h"

class AGambitGameMode;
class AGambitPlayerState;
class UInputAction;
class UGambitDevMatchSandboxComponent;
class UGambitMatchDebugComponent;
class UGambitLocalMultiplayerSubsystem;
enum class EGambitCoreInputAction : uint8;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGambitTargetSelectionChanged, FGambitTargetSelectionRequest, Request);

UCLASS()
class GRANDPAGAMBIT_API AGambitPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Gambit|Input")
	void RequestPlayerReady(bool bReady);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Input")
	void RequestSetDieLocked(int32 DieIndex, bool bLocked);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Input")
	void RequestReroll();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Input")
	void RequestUseConsumable(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Input")
	void RequestUseConsumableOnSelectedDie(int32 SlotIndex, int32 SelectedDieIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Target Selection")
	bool RequestBeginConsumableTargetSelection(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Target Selection")
	bool StartTargetSelection(const FGambitTargetSelectionRequest& Request);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Target Selection")
	bool RequestCancelTargetSelection();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Target Selection")
	bool RequestSelectTargetSelectionOption(int32 OptionId);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Target Selection")
	bool RequestConfirmTargetSelection();

	UFUNCTION(BlueprintPure, Category = "Gambit|Target Selection")
	bool HasPendingTargetSelection() const { return bHasPendingTargetSelection; }

	UFUNCTION(BlueprintPure, Category = "Gambit|Target Selection")
	FGambitTargetSelectionRequest GetPendingTargetSelectionRequest() const { return PendingTargetSelectionRequest; }

	UFUNCTION(BlueprintPure, Category = "Gambit|Target Selection")
	int32 GetPendingTargetSelectionSelectedOptionId() const { return PendingTargetSelectionSelectedOptionId; }

	UFUNCTION(BlueprintPure, Category = "Gambit|Target Selection")
	bool GetSelectedTargetSelectionOption(UPARAM(ref) FGambitTargetSelectionOption& OutOption) const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Input")
	void RequestPurchaseOffer(int32 OfferId);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Local Multiplayer")
	bool RequestJoinLocalPlayer();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Local Multiplayer")
	bool RequestLeaveThisLocalPlayer();

	UPROPERTY(BlueprintAssignable, Category = "Gambit|Target Selection")
	FOnGambitTargetSelectionChanged OnTargetSelectionChanged;

	UFUNCTION(Exec)
	void GambitPrintMatch();

	UFUNCTION(Exec)
	void GambitValidateData();

	UFUNCTION(Exec)
	void GambitPrintInventory();

	UFUNCTION(Exec)
	void GambitPrintSharedPool();

	UFUNCTION(Exec)
	void GambitReadyAll();

	UFUNCTION(Exec)
	void GambitAutoAdvance();

	UFUNCTION(Exec)
	void GambitAutoToShop();

	UFUNCTION(Exec)
	void GambitBuyFirstOfferAll();

	UFUNCTION(Exec)
	void GambitSkipShop();

	UFUNCTION(Exec)
	void Gambit();

	UFUNCTION(Exec)
	void GambitGrantGold(int32 Amount);

	UFUNCTION(Exec)
	void GambitGrantConsumable(int32 PlayerIndex, const FString& ConsumableId);

	UFUNCTION(Exec)
	void GambitPrintShop();

	UFUNCTION(Exec)
	void GambitBuyOffer(int32 PlayerIndex, int32 OfferId);

	UFUNCTION(Exec)
	void GambitRerollPlayer(int32 PlayerIndex);

	UFUNCTION(Exec)
	void GambitLockDie(int32 PlayerIndex, int32 DieIndex);

	UFUNCTION(Exec)
	void GambitUseConsumable(int32 PlayerIndex, int32 SlotIndex);

	UFUNCTION(Exec)
	void GambitUseConsumableOnDie(int32 PlayerIndex, int32 SlotIndex, int32 DieIndex);

	UFUNCTION(Exec)
	void GambitAutoFullMatch();

	UFUNCTION(Exec)
	void GambitAIDecideRerolls();

	UFUNCTION(Exec)
	void GambitAIDecideActions();

	UFUNCTION(Exec)
	void GambitAIDecideShop();

	UFUNCTION(Exec)
	void GambitAIFullRound();

	UFUNCTION(Exec)
	void GambitAIFullMatch();

	UFUNCTION(Exec)
	void GambitDevStart(int32 DesiredLocalPlayerCount);

	UFUNCTION(Exec)
	void GambitDevSnapshot();

	UFUNCTION(Exec)
	void GambitDevInspect(int32 PlayerIndex);

	UFUNCTION(Exec)
	void GambitDevHuman(int32 PlayerIndex);

	UFUNCTION(Exec)
	void GambitDevAI(int32 PlayerIndex);

	UFUNCTION(Exec)
	void GambitDevAdvance();

	UFUNCTION(Exec)
	void GambitDevAIDecide();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

protected:
	UFUNCTION(Server, Reliable)
	void ServerRequestPlayerReady(bool bReady);

	UFUNCTION(Server, Reliable)
	void ServerRequestSetDieLocked(int32 DieIndex, bool bLocked);

	UFUNCTION(Server, Reliable)
	void ServerRequestReroll();

	UFUNCTION(Server, Reliable)
	void ServerRequestUseConsumable(int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerRequestUseConsumableOnSelectedDie(int32 SlotIndex, int32 SelectedDieIndex);

	UFUNCTION(Server, Reliable)
	void ServerRequestPurchaseOffer(int32 OfferId);

	void HandleJoinLocalPlayerInput(const FInputActionValue& Value);
	void HandleLeaveLocalPlayerInput(const FInputActionValue& Value);
	void HandleReadyToggleInput(const FInputActionValue& Value);
	void HandleRerollInput(const FInputActionValue& Value);
	void HandleConsumable1Input(const FInputActionValue& Value);
	void HandleConsumable2Input(const FInputActionValue& Value);
	void HandleConsumable3Input(const FInputActionValue& Value);
	void HandleShopOffer1Input(const FInputActionValue& Value);
	void HandleShopOffer2Input(const FInputActionValue& Value);
	void HandleShopOffer3Input(const FInputActionValue& Value);
	void HandleLockDie1Input(const FInputActionValue& Value);
	void HandleLockDie2Input(const FInputActionValue& Value);
	void HandleLockDie3Input(const FInputActionValue& Value);
	void HandleLockDie4Input(const FInputActionValue& Value);
	void HandleLockDie5Input(const FInputActionValue& Value);
	void HandleLockDie6Input(const FInputActionValue& Value);

private:
	UInputAction* ResolveInputAction(UInputAction* PreferredAction, EGambitCoreInputAction FallbackAction) const;
	void ToggleDieLockByIndex(int32 DieIndex);
	const FGambitTargetSelectionOption* FindPendingTargetSelectionOption(int32 OptionId) const;
	void ClearPendingTargetSelection();
	FGambitTargetSelectionResult BuildTargetSelectionResult(const FGambitTargetSelectionOption& Option) const;
	void AddTargetSelectionFeedback(
		EGambitRoundGameplayEventType EventType,
		EGambitRoundGameplayEventOutcome Outcome,
		const FGambitTargetSelectionRequest& Request,
		const FString& Summary,
		const FGambitTargetSelectionOption* Option = nullptr) const;

	AGambitPlayerState* GetGambitPlayerState() const;
	AGambitGameMode* GetGambitGameMode() const;
	UGambitMatchDebugComponent* GetMatchDebugComponent() const;
	UGambitDevMatchSandboxComponent* GetDevMatchSandboxComponent() const;
	UGambitLocalMultiplayerSubsystem* GetLocalMultiplayerSubsystem() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gambit|Local Multiplayer Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> JoinLocalPlayerInputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gambit|Local Multiplayer Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> LeaveLocalPlayerInputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gambit|Gameplay Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> ReadyToggleInputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gambit|Gameplay Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> RerollInputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gambit|Gameplay Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> Consumable1InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gambit|Gameplay Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> Consumable2InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gambit|Gameplay Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> Consumable3InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gambit|Gameplay Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> ShopOffer1InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gambit|Gameplay Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> ShopOffer2InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gambit|Gameplay Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> ShopOffer3InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gambit|Gameplay Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> LockDie1InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gambit|Gameplay Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> LockDie2InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gambit|Gameplay Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> LockDie3InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gambit|Gameplay Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> LockDie4InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gambit|Gameplay Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> LockDie5InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gambit|Gameplay Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> LockDie6InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gambit|Local Multiplayer Input", meta = (AllowPrivateAccess = "true"))
	bool bOnlyPrimaryLocalPlayerCanManageRoster = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Gameplay Input", meta = (AllowPrivateAccess = "true"))
	bool bReadyState = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Target Selection", meta = (AllowPrivateAccess = "true"))
	bool bHasPendingTargetSelection = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Target Selection", meta = (AllowPrivateAccess = "true"))
	FGambitTargetSelectionRequest PendingTargetSelectionRequest;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Target Selection", meta = (AllowPrivateAccess = "true"))
	int32 PendingTargetSelectionSelectedOptionId = INDEX_NONE;
};
