#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Debug/GambitDevMatchSandboxTypes.h"
#include "GambitDevMatchSandboxWidget.generated.h"

class UGambitDevMatchSandboxComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGambitDevSandboxWidgetSnapshotUpdated, FGambitDevSandboxSnapshot, Snapshot);

UCLASS(Abstract, BlueprintType, Blueprintable)
class GRANDPAGAMBIT_API UGambitDevMatchSandboxWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox")
	bool InitializeSandboxWidget();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox")
	bool RefreshSandboxSnapshot();

	UFUNCTION(BlueprintPure, Category = "Gambit|Dev Sandbox")
	UGambitDevMatchSandboxComponent* GetSandboxComponent() const { return SandboxComponent; }

	UFUNCTION(BlueprintPure, Category = "Gambit|Dev Sandbox")
	FGambitDevSandboxSnapshot GetCachedSandboxSnapshot() const { return CachedSnapshot; }

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|Commands")
	FGambitDevSandboxActionResult StartDevMatchSandbox(int32 DesiredLocalPlayerCount);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|Commands")
	FGambitDevSandboxActionResult SetInspectedPlayerIndex(int32 PlayerIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|Commands")
	FGambitDevSandboxActionResult SetPlayerControlMode(int32 PlayerIndex, EGambitDevSandboxPlayerControlMode ControlMode);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|Commands")
	FGambitDevSandboxActionResult RequestPlayerReady(int32 PlayerIndex, bool bReady);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|Commands")
	FGambitDevSandboxActionResult RequestToggleDieLock(int32 PlayerIndex, int32 DieIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|Commands")
	FGambitDevSandboxActionResult RequestReroll(int32 PlayerIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|Commands")
	FGambitDevSandboxActionResult RequestUseConsumable(int32 PlayerIndex, int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|Commands")
	FGambitDevSandboxActionResult RequestUseConsumableOnDie(int32 PlayerIndex, int32 SlotIndex, int32 DieIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|Commands")
	FGambitDevSandboxActionResult RequestPurchaseOffer(int32 PlayerIndex, int32 OfferId);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|Commands")
	FGambitDevSandboxActionResult RequestSkipShop(int32 PlayerIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Dev Sandbox|Commands")
	FGambitDevSandboxActionResult RequestAdvanceCurrentPhase();

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

	UPROPERTY(BlueprintAssignable, Category = "Gambit|Dev Sandbox")
	FOnGambitDevSandboxWidgetSnapshotUpdated OnSandboxSnapshotUpdated;

private:
	FGambitDevSandboxActionResult RefreshAfterCommand(const FGambitDevSandboxActionResult& Result);
	FGambitDevSandboxActionResult MakeMissingSandboxResult(const FString& Message) const;
	UGambitDevMatchSandboxComponent* ResolveSandboxComponent() const;

	UPROPERTY(Transient)
	TObjectPtr<UGambitDevMatchSandboxComponent> SandboxComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gambit|Dev Sandbox", meta = (AllowPrivateAccess = "true"))
	FGambitDevSandboxSnapshot CachedSnapshot;
};
