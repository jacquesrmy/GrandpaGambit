#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GambitLocalMultiplayerSettings.generated.h"

class UGambitLocalPlayerInputConfig;

UENUM(BlueprintType)
enum class EGambitViewportLayoutMode : uint8
{
	Auto UMETA(DisplayName = "Auto"),
	ForceSplitScreen UMETA(DisplayName = "Force Split Screen"),
	ForceSharedCamera UMETA(DisplayName = "Force Shared Camera")
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Grandpa Gambit Local Multiplayer"))
class GRANDPAGAMBIT_API UGambitLocalMultiplayerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGambitLocalMultiplayerSettings();

	static const UGambitLocalMultiplayerSettings* Get();

	UFUNCTION(BlueprintPure, Category = "Gambit|Local Multiplayer")
	bool ShouldUseSplitScreen(int32 LocalPlayerCount) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Local Multiplayer")
	bool IsSupportedLocalPlayerCount(int32 LocalPlayerCount) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Local Multiplayer")
	int32 ClampLocalPlayerCount(int32 RequestedLocalPlayerCount) const;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Players", meta = (ClampMin = "1", ClampMax = "4"))
	int32 MinLocalPlayers = 1;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Players", meta = (ClampMin = "1", ClampMax = "4"))
	int32 DefaultLocalPlayerCount = 2;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Players", meta = (ClampMin = "1", ClampMax = "4"))
	int32 MaxLocalPlayers = 4;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Viewport")
	EGambitViewportLayoutMode ViewportLayoutMode = EGambitViewportLayoutMode::Auto;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Viewport")
	bool bEnableSplitScreenInAuto = true;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0.0"))
	float CameraBlendTime = 0.0f;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	bool bApplyInputMappingsOnJoinLeave = true;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TSoftObjectPtr<UGambitLocalPlayerInputConfig> InputConfigAsset;
};
