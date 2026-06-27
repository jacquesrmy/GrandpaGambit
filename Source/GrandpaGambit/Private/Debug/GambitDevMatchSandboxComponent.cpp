#include "Debug/GambitDevMatchSandboxComponent.h"

#include "Core/Logging/GambitLog.h"
#include "Debug/GambitDebugAutoPlayer.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/Flow/GambitRoundFlowComponent.h"
#include "Game/Modes/GambitGameMode.h"
#include "Game/States/GambitGameState.h"
#include "Players/Controllers/GambitPlayerController.h"
#include "Players/States/GambitPlayerState.h"
#include "Players/Subsystems/GambitLocalMultiplayerSubsystem.h"

namespace
{
	FString DevSandboxPhaseToString(const EGambitRoundPhase Phase)
	{
		if (const UEnum* Enum = StaticEnum<EGambitRoundPhase>())
		{
			return Enum->GetNameStringByValue(static_cast<int64>(Phase));
		}

		return TEXT("Unknown");
	}

	FString ControlModeToString(const EGambitDevSandboxPlayerControlMode ControlMode)
	{
		switch (ControlMode)
		{
		case EGambitDevSandboxPlayerControlMode::HumanLocal: return TEXT("HumanLocal");
		case EGambitDevSandboxPlayerControlMode::DebugAI: return TEXT("DebugAI");
		default: return TEXT("Unknown");
		}
	}

	bool IsReadyGatedPhase(const EGambitRoundPhase Phase)
	{
		return Phase == EGambitRoundPhase::SelectionReroll
			|| Phase == EGambitRoundPhase::Action
			|| Phase == EGambitRoundPhase::Shop;
	}
}

UGambitDevMatchSandboxComponent::UGambitDevMatchSandboxComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::StartDevMatchSandbox(const int32 DesiredLocalPlayerCount)
{
	const int32 TargetPlayerCount = DesiredLocalPlayerCount > 0 ? DesiredLocalPlayerCount : DefaultDevLocalPlayerCount;
	const FGambitDevSandboxActionResult RosterResult = EnsureLocalPlayerCount(TargetPlayerCount);
	if (!RosterResult.bSuccess)
	{
		return RosterResult;
	}

	return RestartDevMatchSandbox();
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::RestartDevMatchSandbox()
{
	AGambitGameMode* GameMode = GetGambitGameMode();
	const AGambitGameState* GameState = GetGambitGameState();
	const EGambitRoundPhase PhaseBefore = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	if (!GameMode)
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::MissingGameMode, TEXT("Dev sandbox restart failed: missing GambitGameMode"), INDEX_NONE, PhaseBefore);
	}

	GameMode->RestartMatchFlow();
	NormalizeInspectedPlayerIndex();
	UE_LOG(LogGambit, Log, TEXT("DevSandbox: Restarted dev match"));
	return MakeResult(true, EGambitDevSandboxActionStatus::Success, TEXT("Dev match restarted"), INDEX_NONE, PhaseBefore);
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::EnsureLocalPlayerCount(const int32 DesiredLocalPlayerCount)
{
	UGambitLocalMultiplayerSubsystem* LocalMultiplayer = GetLocalMultiplayerSubsystem();
	if (!LocalMultiplayer)
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::MissingLocalMultiplayer, TEXT("Cannot ensure local players: missing local multiplayer subsystem"));
	}

	const int32 CountBefore = LocalMultiplayer->GetLocalPlayerCount();
	LocalMultiplayer->EnsureLocalPlayerCount(DesiredLocalPlayerCount);
	const int32 CountAfter = LocalMultiplayer->GetLocalPlayerCount();
	NormalizeInspectedPlayerIndex();

	const bool bSuccess = CountAfter == LocalMultiplayer->GetLocalPlayerCount() && CountAfter > 0;
	return MakeResult(
		bSuccess,
		bSuccess ? EGambitDevSandboxActionStatus::Success : EGambitDevSandboxActionStatus::Failed,
		FString::Printf(TEXT("Local player count %d -> %d"), CountBefore, CountAfter));
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::JoinLocalPlayer()
{
	UGambitLocalMultiplayerSubsystem* LocalMultiplayer = GetLocalMultiplayerSubsystem();
	if (!LocalMultiplayer)
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::MissingLocalMultiplayer, TEXT("Cannot join local player: missing local multiplayer subsystem"));
	}

	const int32 CountBefore = LocalMultiplayer->GetLocalPlayerCount();
	const bool bJoined = LocalMultiplayer->RequestJoinLocalPlayer();
	const int32 CountAfter = LocalMultiplayer->GetLocalPlayerCount();
	NormalizeInspectedPlayerIndex();
	return MakeResult(
		bJoined,
		bJoined ? EGambitDevSandboxActionStatus::Success : EGambitDevSandboxActionStatus::Failed,
		FString::Printf(TEXT("Join local player %s: %d -> %d"), bJoined ? TEXT("succeeded") : TEXT("failed"), CountBefore, CountAfter));
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::LeaveLastLocalPlayer()
{
	UGambitLocalMultiplayerSubsystem* LocalMultiplayer = GetLocalMultiplayerSubsystem();
	if (!LocalMultiplayer)
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::MissingLocalMultiplayer, TEXT("Cannot leave local player: missing local multiplayer subsystem"));
	}

	const int32 CountBefore = LocalMultiplayer->GetLocalPlayerCount();
	const bool bLeft = CountBefore > 0 && LocalMultiplayer->RequestLeaveLocalPlayer(CountBefore - 1);
	const int32 CountAfter = LocalMultiplayer->GetLocalPlayerCount();
	NormalizeInspectedPlayerIndex();
	return MakeResult(
		bLeft,
		bLeft ? EGambitDevSandboxActionStatus::Success : EGambitDevSandboxActionStatus::Failed,
		FString::Printf(TEXT("Leave local player %s: %d -> %d"), bLeft ? TEXT("succeeded") : TEXT("failed"), CountBefore, CountAfter));
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::SetInspectedPlayerIndex(const int32 PlayerIndex)
{
	if (!GetPlayerByIndex(PlayerIndex))
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::InvalidPlayer, FString::Printf(TEXT("Invalid inspected player index %d"), PlayerIndex), PlayerIndex);
	}

	InspectedPlayerIndex = PlayerIndex;
	return MakeResult(true, EGambitDevSandboxActionStatus::Success, FString::Printf(TEXT("Inspecting player %d"), PlayerIndex), PlayerIndex);
}

AGambitPlayerState* UGambitDevMatchSandboxComponent::GetInspectedPlayerState() const
{
	return GetPlayerByIndex(InspectedPlayerIndex);
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::SetPlayerControlMode(
	const int32 PlayerIndex,
	const EGambitDevSandboxPlayerControlMode ControlMode)
{
	if (!GetPlayerByIndex(PlayerIndex))
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::InvalidPlayer, FString::Printf(TEXT("Invalid player index %d"), PlayerIndex), PlayerIndex);
	}

	ControlModeByPlayerIndex.Add(PlayerIndex, ControlMode);
	return MakeResult(
		true,
		EGambitDevSandboxActionStatus::Success,
		FString::Printf(TEXT("Player %d control mode set to %s"), PlayerIndex, *ControlModeToString(ControlMode)),
		PlayerIndex);
}

EGambitDevSandboxPlayerControlMode UGambitDevMatchSandboxComponent::GetPlayerControlMode(const int32 PlayerIndex) const
{
	if (const EGambitDevSandboxPlayerControlMode* Mode = ControlModeByPlayerIndex.Find(PlayerIndex))
	{
		return *Mode;
	}

	return EGambitDevSandboxPlayerControlMode::HumanLocal;
}

FGambitDevSandboxSnapshot UGambitDevMatchSandboxComponent::BuildSandboxSnapshot() const
{
	FGambitDevSandboxSnapshot Snapshot;

	const AGambitGameState* GameState = GetGambitGameState();
	Snapshot.RoundIndex = GameState ? GameState->GetCurrentRoundIndex() : 0;
	Snapshot.Phase = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	Snapshot.InspectedPlayerIndex = InspectedPlayerIndex;
	Snapshot.LocalPlayerCount = GetLocalMultiplayerSubsystem() ? GetLocalMultiplayerSubsystem()->GetLocalPlayerCount() : 0;
	Snapshot.PlayerSlots = BuildPlayerSlotSnapshots();
	Snapshot.bHasInspectedPlayer = BuildDebugPlayerSnapshotByIndex(InspectedPlayerIndex, Snapshot.InspectedPlayer);
	Snapshot.Summary = FString::Printf(
		TEXT("Round=%d Phase=%s Players=%d Inspect=%d"),
		Snapshot.RoundIndex,
		*DevSandboxPhaseToString(Snapshot.Phase),
		Snapshot.PlayerSlots.Num(),
		Snapshot.InspectedPlayerIndex);
	return Snapshot;
}

TArray<FGambitDevSandboxPlayerSlotSnapshot> UGambitDevMatchSandboxComponent::BuildPlayerSlotSnapshots() const
{
	TArray<FGambitDevSandboxPlayerSlotSnapshot> Slots;
	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	Slots.Reserve(Players.Num());

	const AGambitGameMode* GameMode = GetGambitGameMode();
	const UGambitRoundFlowComponent* RoundFlow = GameMode ? GameMode->GetRoundFlowComponent() : nullptr;
	for (int32 PlayerIndex = 0; PlayerIndex < Players.Num(); ++PlayerIndex)
	{
		const AGambitPlayerState* PlayerState = Players[PlayerIndex];
		if (!PlayerState)
		{
			continue;
		}

		FGambitDevSandboxPlayerSlotSnapshot Slot;
		Slot.PlayerIndex = PlayerIndex;
		Slot.PlayerName = PlayerState->GetPlayerName().IsEmpty()
			? FString::Printf(TEXT("Player %d"), PlayerIndex)
			: PlayerState->GetPlayerName();
		Slot.ControlMode = GetPlayerControlMode(PlayerIndex);
		Slot.bIsInspected = PlayerIndex == InspectedPlayerIndex;
		Slot.CurrentGold = PlayerState->GetCurrentGold();
		Slot.CurrentRoundScore = PlayerState->GetCurrentRoundScore();
		Slot.TotalVictoryPoints = PlayerState->GetTotalVictoryPoints();
		Slot.RerollsUsed = RoundFlow ? RoundFlow->GetRerollsUsedForPlayer(const_cast<AGambitPlayerState*>(PlayerState)) : 0;
		Slot.MaxRerolls = RoundFlow ? RoundFlow->GetMaxRerollsForPlayer(const_cast<AGambitPlayerState*>(PlayerState)) : 0;
		Slot.SlotState = PlayerState->GetSlotState();
		if (const AGambitPlayerController* PlayerController = GetControllerForPlayer(const_cast<AGambitPlayerState*>(PlayerState)))
		{
			Slot.bHasPendingTargetSelection = PlayerController->HasPendingTargetSelection();
			Slot.PendingTargetSelection = PlayerController->GetPendingTargetSelectionRequest();
			Slot.SelectedTargetOptionId = PlayerController->GetPendingTargetSelectionSelectedOptionId();
		}
		Slot.Summary = FString::Printf(
			TEXT("P%d %s Mode=%s Gold=%d VP=%d Score=%d Rerolls=%d/%d TargetPending=%s"),
			PlayerIndex,
			*Slot.PlayerName,
			*ControlModeToString(Slot.ControlMode),
			Slot.CurrentGold,
			Slot.TotalVictoryPoints,
			Slot.CurrentRoundScore,
			Slot.RerollsUsed,
			Slot.MaxRerolls,
			Slot.bHasPendingTargetSelection ? TEXT("Yes") : TEXT("No"));
		Slots.Add(Slot);
	}

	return Slots;
}

TArray<FGambitDebugPlayerSnapshot> UGambitDevMatchSandboxComponent::BuildDebugPlayerSnapshots() const
{
	TArray<FGambitDebugPlayerSnapshot> Snapshots;
	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	Snapshots.Reserve(Players.Num());

	for (int32 PlayerIndex = 0; PlayerIndex < Players.Num(); ++PlayerIndex)
	{
		if (const AGambitPlayerState* PlayerState = Players[PlayerIndex])
		{
			Snapshots.Add(PlayerState->BuildDebugPlayerSnapshot(PlayerIndex));
		}
	}

	return Snapshots;
}

bool UGambitDevMatchSandboxComponent::BuildDebugPlayerSnapshotByIndex(
	const int32 PlayerIndex,
	FGambitDebugPlayerSnapshot& OutSnapshot) const
{
	const AGambitPlayerState* PlayerState = GetPlayerByIndex(PlayerIndex);
	if (!PlayerState)
	{
		OutSnapshot = FGambitDebugPlayerSnapshot();
		return false;
	}

	OutSnapshot = PlayerState->BuildDebugPlayerSnapshot(PlayerIndex);
	return true;
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::RequestPlayerReady(const int32 PlayerIndex, const bool bReady)
{
	AGambitGameMode* GameMode = GetGambitGameMode();
	const AGambitGameState* GameState = GetGambitGameState();
	const EGambitRoundPhase PhaseBefore = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	AGambitPlayerState* PlayerState = GetPlayerByIndex(PlayerIndex);
	if (!GameMode)
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::MissingGameMode, TEXT("Ready failed: missing GameMode"), PlayerIndex, PhaseBefore);
	}
	if (!PlayerState)
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::InvalidPlayer, TEXT("Ready failed: invalid player"), PlayerIndex, PhaseBefore);
	}

	GameMode->SetPlayerReady(PlayerState, bReady);
	return MakeResult(
		true,
		EGambitDevSandboxActionStatus::Success,
		FString::Printf(TEXT("Player %d ready=%s"), PlayerIndex, bReady ? TEXT("true") : TEXT("false")),
		PlayerIndex,
		PhaseBefore);
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::RequestSetDieLocked(
	const int32 PlayerIndex,
	const int32 DieIndex,
	const bool bLocked)
{
	AGambitGameMode* GameMode = GetGambitGameMode();
	const AGambitGameState* GameState = GetGambitGameState();
	const EGambitRoundPhase PhaseBefore = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	AGambitPlayerState* PlayerState = GetPlayerByIndex(PlayerIndex);
	if (!GameMode)
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::MissingGameMode, TEXT("Set die lock failed: missing GameMode"), PlayerIndex, PhaseBefore);
	}
	if (!PlayerState)
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::InvalidPlayer, TEXT("Set die lock failed: invalid player"), PlayerIndex, PhaseBefore);
	}

	const bool bSuccess = GameMode->RequestSetDieLocked(PlayerState, DieIndex, bLocked);
	return MakeResult(
		bSuccess,
		bSuccess ? EGambitDevSandboxActionStatus::Success : EGambitDevSandboxActionStatus::InvalidPhase,
		FString::Printf(TEXT("Set P%d die %d locked=%s"), PlayerIndex, DieIndex, bLocked ? TEXT("true") : TEXT("false")),
		PlayerIndex,
		PhaseBefore);
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::RequestToggleDieLock(const int32 PlayerIndex, const int32 DieIndex)
{
	const AGambitPlayerState* PlayerState = GetPlayerByIndex(PlayerIndex);
	if (!PlayerState)
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::InvalidPlayer, TEXT("Toggle die lock failed: invalid player"), PlayerIndex);
	}

	const TArray<FGambitDieRuntimeState>& DiceStates = PlayerState->GetDiceStatesRef();
	if (!DiceStates.IsValidIndex(DieIndex))
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::Failed, FString::Printf(TEXT("Toggle die lock failed: invalid die index %d"), DieIndex), PlayerIndex);
	}

	return RequestSetDieLocked(PlayerIndex, DieIndex, !DiceStates[DieIndex].bLocked);
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::RequestReroll(const int32 PlayerIndex)
{
	AGambitGameMode* GameMode = GetGambitGameMode();
	const AGambitGameState* GameState = GetGambitGameState();
	const EGambitRoundPhase PhaseBefore = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	AGambitPlayerState* PlayerState = GetPlayerByIndex(PlayerIndex);
	if (!GameMode)
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::MissingGameMode, TEXT("Reroll failed: missing GameMode"), PlayerIndex, PhaseBefore);
	}
	if (!PlayerState)
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::InvalidPlayer, TEXT("Reroll failed: invalid player"), PlayerIndex, PhaseBefore);
	}

	const bool bSuccess = GameMode->RequestReroll(PlayerState);
	return MakeResult(
		bSuccess,
		bSuccess ? EGambitDevSandboxActionStatus::Success : EGambitDevSandboxActionStatus::InvalidPhase,
		FString::Printf(TEXT("Reroll P%d"), PlayerIndex),
		PlayerIndex,
		PhaseBefore);
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::RequestUseConsumable(const int32 PlayerIndex, const int32 SlotIndex)
{
	AGambitGameMode* GameMode = GetGambitGameMode();
	const AGambitGameState* GameState = GetGambitGameState();
	const EGambitRoundPhase PhaseBefore = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	AGambitPlayerState* PlayerState = GetPlayerByIndex(PlayerIndex);
	if (!GameMode)
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::MissingGameMode, TEXT("Use consumable failed: missing GameMode"), PlayerIndex, PhaseBefore);
	}
	if (!PlayerState)
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::InvalidPlayer, TEXT("Use consumable failed: invalid player"), PlayerIndex, PhaseBefore);
	}

	const bool bSuccess = GameMode->RequestUseConsumable(PlayerState, SlotIndex);
	return MakeResult(
		bSuccess,
		bSuccess ? EGambitDevSandboxActionStatus::Success : EGambitDevSandboxActionStatus::InvalidPhase,
		FString::Printf(TEXT("Use P%d consumable slot %d"), PlayerIndex, SlotIndex),
		PlayerIndex,
		PhaseBefore);
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::RequestUseConsumableOnDie(
	const int32 PlayerIndex,
	const int32 SlotIndex,
	const int32 DieIndex)
{
	AGambitGameMode* GameMode = GetGambitGameMode();
	const AGambitGameState* GameState = GetGambitGameState();
	const EGambitRoundPhase PhaseBefore = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	AGambitPlayerState* PlayerState = GetPlayerByIndex(PlayerIndex);
	if (!GameMode)
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::MissingGameMode, TEXT("Use consumable on die failed: missing GameMode"), PlayerIndex, PhaseBefore);
	}
	if (!PlayerState)
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::InvalidPlayer, TEXT("Use consumable on die failed: invalid player"), PlayerIndex, PhaseBefore);
	}

	const bool bSuccess = GameMode->RequestUseConsumableOnTargetSelectedDie(PlayerState, SlotIndex, PlayerState, DieIndex);
	return MakeResult(
		bSuccess,
		bSuccess ? EGambitDevSandboxActionStatus::Success : EGambitDevSandboxActionStatus::InvalidPhase,
		FString::Printf(TEXT("Use P%d consumable slot %d on die %d"), PlayerIndex, SlotIndex, DieIndex),
		PlayerIndex,
		PhaseBefore);
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::RequestBeginConsumableTargetSelection(
	const int32 PlayerIndex,
	const int32 SlotIndex)
{
	const AGambitGameState* GameState = GetGambitGameState();
	const EGambitRoundPhase PhaseBefore = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	AGambitPlayerController* PlayerController = GetControllerForPlayerIndex(PlayerIndex);
	if (!PlayerController)
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::InvalidPlayer, TEXT("Begin target selection failed: missing player controller"), PlayerIndex, PhaseBefore);
	}

	const bool bSuccess = PlayerController->RequestBeginConsumableTargetSelection(SlotIndex);
	return MakeResult(
		bSuccess,
		bSuccess ? EGambitDevSandboxActionStatus::Success : EGambitDevSandboxActionStatus::Failed,
		FString::Printf(TEXT("Begin P%d consumable target selection slot %d"), PlayerIndex, SlotIndex),
		PlayerIndex,
		PhaseBefore);
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::RequestSelectTargetSelectionOption(
	const int32 PlayerIndex,
	const int32 OptionId)
{
	const AGambitGameState* GameState = GetGambitGameState();
	const EGambitRoundPhase PhaseBefore = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	AGambitPlayerController* PlayerController = GetControllerForPlayerIndex(PlayerIndex);
	if (!PlayerController)
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::InvalidPlayer, TEXT("Select target option failed: missing player controller"), PlayerIndex, PhaseBefore);
	}

	const bool bSuccess = PlayerController->RequestSelectTargetSelectionOption(OptionId);
	return MakeResult(
		bSuccess,
		bSuccess ? EGambitDevSandboxActionStatus::Success : EGambitDevSandboxActionStatus::Failed,
		FString::Printf(TEXT("Select P%d target option %d"), PlayerIndex, OptionId),
		PlayerIndex,
		PhaseBefore);
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::RequestConfirmTargetSelection(const int32 PlayerIndex)
{
	const AGambitGameState* GameState = GetGambitGameState();
	const EGambitRoundPhase PhaseBefore = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	AGambitPlayerController* PlayerController = GetControllerForPlayerIndex(PlayerIndex);
	if (!PlayerController)
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::InvalidPlayer, TEXT("Confirm target selection failed: missing player controller"), PlayerIndex, PhaseBefore);
	}

	const bool bSuccess = PlayerController->RequestConfirmTargetSelection();
	return MakeResult(
		bSuccess,
		bSuccess ? EGambitDevSandboxActionStatus::Success : EGambitDevSandboxActionStatus::Failed,
		FString::Printf(TEXT("Confirm P%d target selection"), PlayerIndex),
		PlayerIndex,
		PhaseBefore);
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::RequestCancelTargetSelection(const int32 PlayerIndex)
{
	const AGambitGameState* GameState = GetGambitGameState();
	const EGambitRoundPhase PhaseBefore = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	AGambitPlayerController* PlayerController = GetControllerForPlayerIndex(PlayerIndex);
	if (!PlayerController)
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::InvalidPlayer, TEXT("Cancel target selection failed: missing player controller"), PlayerIndex, PhaseBefore);
	}

	const bool bSuccess = PlayerController->RequestCancelTargetSelection();
	return MakeResult(
		bSuccess,
		bSuccess ? EGambitDevSandboxActionStatus::Success : EGambitDevSandboxActionStatus::Failed,
		FString::Printf(TEXT("Cancel P%d target selection"), PlayerIndex),
		PlayerIndex,
		PhaseBefore);
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::RequestPurchaseOffer(const int32 PlayerIndex, const int32 OfferId)
{
	AGambitGameMode* GameMode = GetGambitGameMode();
	const AGambitGameState* GameState = GetGambitGameState();
	const EGambitRoundPhase PhaseBefore = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	AGambitPlayerState* PlayerState = GetPlayerByIndex(PlayerIndex);
	if (!GameMode)
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::MissingGameMode, TEXT("Purchase failed: missing GameMode"), PlayerIndex, PhaseBefore);
	}
	if (!PlayerState)
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::InvalidPlayer, TEXT("Purchase failed: invalid player"), PlayerIndex, PhaseBefore);
	}

	const bool bSuccess = GameMode->RequestPurchaseOffer(PlayerState, OfferId);
	return MakeResult(
		bSuccess,
		bSuccess ? EGambitDevSandboxActionStatus::Success : EGambitDevSandboxActionStatus::Failed,
		FString::Printf(TEXT("Purchase P%d offer %d"), PlayerIndex, OfferId),
		PlayerIndex,
		PhaseBefore);
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::RequestSkipShop(const int32 PlayerIndex)
{
	const AGambitGameState* GameState = GetGambitGameState();
	const EGambitRoundPhase PhaseBefore = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	if (PhaseBefore != EGambitRoundPhase::Shop)
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::InvalidPhase, TEXT("Skip shop is only valid during Shop phase"), PlayerIndex, PhaseBefore);
	}

	return RequestPlayerReady(PlayerIndex, true);
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::RequestAdvanceCurrentPhase()
{
	const AGambitGameState* GameState = GetGambitGameState();
	if (!GameState)
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::MissingGameState, TEXT("Advance failed: missing GameState"));
	}

	const EGambitRoundPhase PhaseBefore = GameState->GetCurrentPhase();
	if (!IsReadyGatedPhase(PhaseBefore))
	{
		return MakeResult(
			false,
			EGambitDevSandboxActionStatus::Unsupported,
			FString::Printf(TEXT("Phase %s has no manual sandbox advance"), *DevSandboxPhaseToString(PhaseBefore)),
			INDEX_NONE,
			PhaseBefore);
	}

	SetAllPlayersReady(true);
	return MakeResult(
		true,
		EGambitDevSandboxActionStatus::Success,
		FString::Printf(TEXT("Advanced gated phase %s"), *DevSandboxPhaseToString(PhaseBefore)),
		INDEX_NONE,
		PhaseBefore);
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::RequestAdvanceToShopOrMatchEnd()
{
	constexpr int32 MaxIterations = 20;
	FGambitDevSandboxActionResult LastResult = MakeResult(true, EGambitDevSandboxActionStatus::Success, TEXT("Already at target"));

	for (int32 Iteration = 0; Iteration < MaxIterations; ++Iteration)
	{
		const AGambitGameState* GameState = GetGambitGameState();
		if (!GameState)
		{
			return MakeResult(false, EGambitDevSandboxActionStatus::MissingGameState, TEXT("Advance to shop failed: missing GameState"));
		}

		const EGambitRoundPhase PhaseBefore = GameState->GetCurrentPhase();
		if (PhaseBefore == EGambitRoundPhase::Shop || PhaseBefore == EGambitRoundPhase::None)
		{
			return MakeResult(true, EGambitDevSandboxActionStatus::Success, FString::Printf(TEXT("Stopped at phase %s"), *DevSandboxPhaseToString(PhaseBefore)), INDEX_NONE, PhaseBefore);
		}

		LastResult = RequestAdvanceCurrentPhase();
		const AGambitGameState* UpdatedGameState = GetGambitGameState();
		const EGambitRoundPhase PhaseAfter = UpdatedGameState ? UpdatedGameState->GetCurrentPhase() : EGambitRoundPhase::None;
		if (!LastResult.bSuccess || PhaseAfter == PhaseBefore)
		{
			return MakeResult(false, LastResult.Status, FString::Printf(TEXT("Advance to shop stopped at phase %s"), *DevSandboxPhaseToString(PhaseAfter)), INDEX_NONE, PhaseBefore);
		}
	}

	return MakeResult(false, EGambitDevSandboxActionStatus::Failed, TEXT("Advance to shop stopped at safety limit"));
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::RequestAIDecisionForPlayer(const int32 PlayerIndex)
{
	return RequestAIDecisionForPlayers({ PlayerIndex }, FString::Printf(TEXT("AI decision for player %d"), PlayerIndex));
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::RequestAIDecisionForAIPlayers()
{
	TArray<int32> AIPlayerIndices;
	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	for (int32 PlayerIndex = 0; PlayerIndex < Players.Num(); ++PlayerIndex)
	{
		if (GetPlayerControlMode(PlayerIndex) == EGambitDevSandboxPlayerControlMode::DebugAI)
		{
			AIPlayerIndices.Add(PlayerIndex);
		}
	}

	if (AIPlayerIndices.Num() == 0)
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::InvalidPlayer, TEXT("No players are marked as DebugAI"));
	}

	return RequestAIDecisionForPlayers(AIPlayerIndices, TEXT("AI decision for DebugAI players"));
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::RequestAIDecisionForAllPlayers()
{
	TArray<int32> PlayerIndices;
	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	for (int32 PlayerIndex = 0; PlayerIndex < Players.Num(); ++PlayerIndex)
	{
		PlayerIndices.Add(PlayerIndex);
	}

	return RequestAIDecisionForPlayers(PlayerIndices, TEXT("AI decision for all players"));
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::RequestAIShopDecisionForPlayer(const int32 PlayerIndex)
{
	return RequestAIShopDecisionForPlayers({ PlayerIndex }, FString::Printf(TEXT("AI shop decision for player %d"), PlayerIndex));
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::RequestAIShopDecisionForAIPlayers()
{
	TArray<int32> AIPlayerIndices;
	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	for (int32 PlayerIndex = 0; PlayerIndex < Players.Num(); ++PlayerIndex)
	{
		if (GetPlayerControlMode(PlayerIndex) == EGambitDevSandboxPlayerControlMode::DebugAI)
		{
			AIPlayerIndices.Add(PlayerIndex);
		}
	}

	if (AIPlayerIndices.Num() == 0)
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::InvalidPlayer, TEXT("No players are marked as DebugAI"));
	}

	return RequestAIShopDecisionForPlayers(AIPlayerIndices, TEXT("AI shop decision for DebugAI players"));
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::RequestAIShopDecisionForAllPlayers()
{
	TArray<int32> PlayerIndices;
	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	for (int32 PlayerIndex = 0; PlayerIndex < Players.Num(); ++PlayerIndex)
	{
		PlayerIndices.Add(PlayerIndex);
	}

	return RequestAIShopDecisionForPlayers(PlayerIndices, TEXT("AI shop decision for all players"));
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::MakeResult(
	const bool bSuccess,
	const EGambitDevSandboxActionStatus Status,
	const FString& Message,
	const int32 PlayerIndex,
	const EGambitRoundPhase PhaseBefore) const
{
	const AGambitGameState* GameState = GetGambitGameState();

	FGambitDevSandboxActionResult Result;
	Result.bSuccess = bSuccess;
	Result.Status = Status;
	Result.PlayerIndex = PlayerIndex;
	Result.PhaseBefore = PhaseBefore;
	Result.PhaseAfter = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	Result.Message = Message;
	return Result;
}

AGambitGameMode* UGambitDevMatchSandboxComponent::GetGambitGameMode() const
{
	return Cast<AGambitGameMode>(GetOwner());
}

AGambitGameState* UGambitDevMatchSandboxComponent::GetGambitGameState() const
{
	const AGambitGameMode* GameMode = GetGambitGameMode();
	return GameMode ? GameMode->GetGameState<AGambitGameState>() : nullptr;
}

UGambitLocalMultiplayerSubsystem* UGambitDevMatchSandboxComponent::GetLocalMultiplayerSubsystem() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UGambitLocalMultiplayerSubsystem>() : nullptr;
}

TArray<AGambitPlayerState*> UGambitDevMatchSandboxComponent::GetAllPlayers() const
{
	const AGambitGameState* GameState = GetGambitGameState();
	return GameState ? GameState->GetGambitPlayerStates() : TArray<AGambitPlayerState*>();
}

AGambitPlayerState* UGambitDevMatchSandboxComponent::GetPlayerByIndex(const int32 PlayerIndex) const
{
	const TArray<AGambitPlayerState*> Players = GetAllPlayers();
	return Players.IsValidIndex(PlayerIndex) ? Players[PlayerIndex] : nullptr;
}

AGambitPlayerController* UGambitDevMatchSandboxComponent::GetControllerForPlayer(AGambitPlayerState* PlayerState) const
{
	if (!PlayerState)
	{
		return nullptr;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		AGambitPlayerController* PlayerController = Cast<AGambitPlayerController>(It->Get());
		if (PlayerController && PlayerController->GetPlayerState<AGambitPlayerState>() == PlayerState)
		{
			return PlayerController;
		}
	}

	return nullptr;
}

AGambitPlayerController* UGambitDevMatchSandboxComponent::GetControllerForPlayerIndex(const int32 PlayerIndex) const
{
	return GetControllerForPlayer(GetPlayerByIndex(PlayerIndex));
}

UGambitDebugAutoPlayer* UGambitDevMatchSandboxComponent::GetOrCreateAutoPlayer()
{
	if (!AutoPlayer)
	{
		AutoPlayer = NewObject<UGambitDebugAutoPlayer>(this);
	}

	return AutoPlayer;
}

void UGambitDevMatchSandboxComponent::NormalizeInspectedPlayerIndex()
{
	const int32 PlayerCount = GetAllPlayers().Num();
	if (PlayerCount <= 0)
	{
		InspectedPlayerIndex = INDEX_NONE;
		return;
	}

	if (!FMath::IsWithinInclusive(InspectedPlayerIndex, 0, PlayerCount - 1))
	{
		InspectedPlayerIndex = 0;
	}
}

void UGambitDevMatchSandboxComponent::SetAllPlayersReady(const bool bReady) const
{
	AGambitGameMode* GameMode = GetGambitGameMode();
	if (!GameMode)
	{
		return;
	}

	for (AGambitPlayerState* PlayerState : GetAllPlayers())
	{
		if (PlayerState)
		{
			GameMode->SetPlayerReady(PlayerState, bReady);
		}
	}
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::RequestAIDecisionForPlayers(
	const TArray<int32>& PlayerIndices,
	const FString& Label)
{
	AGambitGameMode* GameMode = GetGambitGameMode();
	const AGambitGameState* GameState = GetGambitGameState();
	const EGambitRoundPhase PhaseBefore = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	UGambitDebugAutoPlayer* DebugAI = GetOrCreateAutoPlayer();
	if (!GameMode)
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::MissingGameMode, TEXT("AI decision failed: missing GameMode"), INDEX_NONE, PhaseBefore);
	}
	if (!GameState)
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::MissingGameState, TEXT("AI decision failed: missing GameState"), INDEX_NONE, PhaseBefore);
	}
	if (!DebugAI)
	{
		return MakeResult(false, EGambitDevSandboxActionStatus::Failed, TEXT("AI decision failed: missing auto player"), INDEX_NONE, PhaseBefore);
	}
	if (!IsReadyGatedPhase(PhaseBefore))
	{
		return MakeResult(
			false,
			EGambitDevSandboxActionStatus::InvalidPhase,
			FString::Printf(TEXT("AI decision skipped: phase %s is not a player decision phase"), *DevSandboxPhaseToString(PhaseBefore)),
			INDEX_NONE,
			PhaseBefore);
	}

	int32 HandledCount = 0;
	TArray<AGambitPlayerState*> PlayersToMarkReady;
	for (const int32 PlayerIndex : PlayerIndices)
	{
		AGambitPlayerState* PlayerState = GetPlayerByIndex(PlayerIndex);
		if (!PlayerState)
		{
			continue;
		}

		bool bHandledPlayer = false;
		switch (PhaseBefore)
		{
		case EGambitRoundPhase::SelectionReroll:
			DebugAI->DecideRerollsForPlayer(GameMode, PlayerState, PlayerIndex);
			bHandledPlayer = true;
			break;
		case EGambitRoundPhase::Action:
			DebugAI->DecideActionsForPlayer(GameMode, PlayerState, PlayerIndex);
			bHandledPlayer = true;
			break;
		case EGambitRoundPhase::Shop:
			DebugAI->DecideShopForPlayer(GameMode, PlayerState, PlayerIndex);
			bHandledPlayer = true;
			break;
		default:
			break;
		}

		if (bHandledPlayer)
		{
			PlayersToMarkReady.AddUnique(PlayerState);
			HandledCount++;
		}
	}

	for (AGambitPlayerState* PlayerState : PlayersToMarkReady)
	{
		GameMode->SetPlayerReady(PlayerState, true);
	}

	const bool bSuccess = HandledCount > 0;
	return MakeResult(
		bSuccess,
		bSuccess ? EGambitDevSandboxActionStatus::Success : EGambitDevSandboxActionStatus::InvalidPlayer,
		FString::Printf(TEXT("%s handled %d player(s)"), *Label, HandledCount),
		INDEX_NONE,
		PhaseBefore);
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxComponent::RequestAIShopDecisionForPlayers(
	const TArray<int32>& PlayerIndices,
	const FString& Label)
{
	const AGambitGameState* GameState = GetGambitGameState();
	const EGambitRoundPhase PhaseBefore = GameState ? GameState->GetCurrentPhase() : EGambitRoundPhase::None;
	if (PhaseBefore != EGambitRoundPhase::Shop)
	{
		return MakeResult(
			false,
			EGambitDevSandboxActionStatus::InvalidPhase,
			FString::Printf(TEXT("AI shop decision skipped: phase %s is not Shop"), *DevSandboxPhaseToString(PhaseBefore)),
			INDEX_NONE,
			PhaseBefore);
	}

	return RequestAIDecisionForPlayers(PlayerIndices, Label);
}
