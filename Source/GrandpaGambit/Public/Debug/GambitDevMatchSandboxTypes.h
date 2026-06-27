#pragma once

#include "CoreMinimal.h"
#include "Core/Types/GambitDebugTypes.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "Core/Types/GambitTargetSelectionTypes.h"
#include "GambitDevMatchSandboxTypes.generated.h"

UENUM(BlueprintType)
enum class EGambitDevSandboxActionStatus : uint8
{
	Success,
	Failed,
	InvalidPlayer,
	InvalidPhase,
	MissingGameMode,
	MissingGameState,
	MissingLocalMultiplayer,
	Unsupported
};

UENUM(BlueprintType)
enum class EGambitDevSandboxPlayerControlMode : uint8
{
	HumanLocal,
	DebugAI
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitDevSandboxActionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Dev Sandbox")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Dev Sandbox")
	EGambitDevSandboxActionStatus Status = EGambitDevSandboxActionStatus::Failed;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Dev Sandbox")
	int32 PlayerIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Dev Sandbox")
	EGambitRoundPhase PhaseBefore = EGambitRoundPhase::None;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Dev Sandbox")
	EGambitRoundPhase PhaseAfter = EGambitRoundPhase::None;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Dev Sandbox")
	FString Message;
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitDevSandboxPlayerSlotSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Dev Sandbox")
	int32 PlayerIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Dev Sandbox")
	FString PlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Dev Sandbox")
	EGambitDevSandboxPlayerControlMode ControlMode = EGambitDevSandboxPlayerControlMode::HumanLocal;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Dev Sandbox")
	bool bIsInspected = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Dev Sandbox")
	int32 CurrentGold = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Dev Sandbox")
	int32 CurrentRoundScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Dev Sandbox")
	int32 TotalVictoryPoints = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Dev Sandbox")
	int32 RerollsUsed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Dev Sandbox")
	int32 MaxRerolls = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Dev Sandbox")
	FGambitPlayerSlotState SlotState;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Dev Sandbox")
	bool bHasPendingTargetSelection = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Dev Sandbox")
	FGambitTargetSelectionRequest PendingTargetSelection;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Dev Sandbox")
	int32 SelectedTargetOptionId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Dev Sandbox")
	FString Summary;
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitDevSandboxSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Dev Sandbox")
	int32 RoundIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Dev Sandbox")
	EGambitRoundPhase Phase = EGambitRoundPhase::None;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Dev Sandbox")
	int32 LocalPlayerCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Dev Sandbox")
	int32 InspectedPlayerIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Dev Sandbox")
	bool bHasInspectedPlayer = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Dev Sandbox")
	TArray<FGambitDevSandboxPlayerSlotSnapshot> PlayerSlots;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Dev Sandbox")
	FGambitDebugPlayerSnapshot InspectedPlayer;

	UPROPERTY(BlueprintReadOnly, Category = "Gambit|Dev Sandbox")
	FString Summary;
};
