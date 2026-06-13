#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GambitLocalMultiplayerSubsystem.generated.h"

class AGambitTableCameraDirector;
class UGameViewportClient;
class UGambitLocalPlayerInputConfig;
class UInputAction;
class ULocalPlayer;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGambitLocalPlayersChanged, int32, LocalPlayerCount);

UENUM(BlueprintType)
enum class EGambitCoreInputAction : uint8
{
	JoinLocalPlayer,
	LeaveLocalPlayer,
	Ready,
	Reroll,
	Consumable1,
	Consumable2,
	Consumable3,
	ShopOffer1,
	ShopOffer2,
	ShopOffer3,
	LockDie1,
	LockDie2,
	LockDie3,
	LockDie4,
	LockDie5,
	LockDie6
};

UCLASS()
class GRANDPAGAMBIT_API UGambitLocalMultiplayerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Gambit|Local Multiplayer")
	void InitializeLocalMultiplayerSession();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Local Multiplayer")
	bool RequestJoinLocalPlayer();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Local Multiplayer")
	bool RequestLeaveLocalPlayer(int32 LocalPlayerIndex);

	UFUNCTION(BlueprintCallable, Category = "Gambit|Local Multiplayer")
	void EnsureLocalPlayerCount(int32 DesiredPlayerCount);

	UFUNCTION(BlueprintPure, Category = "Gambit|Local Multiplayer")
	int32 GetLocalPlayerCount() const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Local Multiplayer")
	void RefreshLocalMultiplayerLayout();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Local Multiplayer")
	void RefreshInputMappings();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Local Multiplayer")
	void RefreshCameraTargets();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Local Multiplayer")
	void SetCameraDirector(AGambitTableCameraDirector* InCameraDirector);

	UFUNCTION(BlueprintPure, Category = "Gambit|Local Multiplayer")
	UInputAction* GetCoreInputAction(EGambitCoreInputAction Action) const;

	UPROPERTY(BlueprintAssignable, Category = "Gambit|Local Multiplayer")
	FOnGambitLocalPlayersChanged OnLocalPlayersChanged;

private:
	TArray<ULocalPlayer*> GetLocalPlayers() const;
	UGameViewportClient* GetViewportClient() const;
	UGambitLocalPlayerInputConfig* ResolveInputConfig();
	AGambitTableCameraDirector* ResolveCameraDirector();
	UGambitLocalPlayerInputConfig* CreateRuntimeFallbackInputConfig();
	UInputAction* CreateCoreInputAction(EGambitCoreInputAction Action, const FName& ActionName);
	void AddMapping(UGambitLocalPlayerInputConfig* ConfigAsset, int32 LocalPlayerIndex, UInputAction* Action, const FKey& Key, int32 Priority = 0);
	void BroadcastLocalPlayersChanged();

	UPROPERTY(Transient)
	TWeakObjectPtr<AGambitTableCameraDirector> CameraDirector;

	UPROPERTY(Transient)
	TObjectPtr<UGambitLocalPlayerInputConfig> RuntimeFallbackInputConfig = nullptr;

	UPROPERTY(Transient)
	TMap<EGambitCoreInputAction, TObjectPtr<UInputAction>> RuntimeCoreInputActions;
};
