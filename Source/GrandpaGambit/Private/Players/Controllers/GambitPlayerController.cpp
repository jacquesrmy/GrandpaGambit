#include "Players/Controllers/GambitPlayerController.h"

#include "Debug/GambitDevMatchSandboxComponent.h"
#include "Debug/GambitDevMatchSandboxTypes.h"
#include "Core/Logging/GambitLog.h"
#include "Debug/GambitMatchDebugComponent.h"
#include "Engine/GameInstance.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Engine/LocalPlayer.h"
#include "Game/Modes/GambitGameMode.h"
#include "Game/States/GambitGameState.h"
#include "Items/Data/GambitItemDefinition.h"
#include "Players/States/GambitPlayerState.h"
#include "Players/Subsystems/GambitLocalMultiplayerSubsystem.h"

namespace
{
	FString ResolveControllerItemName(const UGambitItemDefinition* ItemDefinition)
	{
		if (!ItemDefinition)
		{
			return TEXT("None");
		}

		return ItemDefinition->DisplayName.IsEmpty()
			? ItemDefinition->GetResolvedItemId().ToString()
			: ItemDefinition->DisplayName.ToString();
	}

	int32 ResolveControllerPlayerId(const AGambitPlayerState* PlayerState)
	{
		return PlayerState ? PlayerState->GetPlayerId() : INDEX_NONE;
	}
}

void AGambitPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		if (UGambitLocalMultiplayerSubsystem* LocalMultiplayer = GetLocalMultiplayerSubsystem())
		{
			LocalMultiplayer->RefreshInputMappings();
		}
	}
}

void AGambitPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		return;
	}

	if (IsLocalController())
	{
		if (UGambitLocalMultiplayerSubsystem* LocalSubsystem = GetLocalMultiplayerSubsystem())
		{
			LocalSubsystem->RefreshInputMappings();
		}
	}

	auto BindActionIfValid = [EnhancedInput, this](UInputAction* Action, void (AGambitPlayerController::*Handler)(const FInputActionValue&))
	{
		if (Action)
		{
			EnhancedInput->BindAction(Action, ETriggerEvent::Started, this, Handler);
		}
	};

	BindActionIfValid(ResolveInputAction(JoinLocalPlayerInputAction, EGambitCoreInputAction::JoinLocalPlayer), &AGambitPlayerController::HandleJoinLocalPlayerInput);
	BindActionIfValid(ResolveInputAction(LeaveLocalPlayerInputAction, EGambitCoreInputAction::LeaveLocalPlayer), &AGambitPlayerController::HandleLeaveLocalPlayerInput);
	BindActionIfValid(ResolveInputAction(ReadyToggleInputAction, EGambitCoreInputAction::Ready), &AGambitPlayerController::HandleReadyToggleInput);
	BindActionIfValid(ResolveInputAction(RerollInputAction, EGambitCoreInputAction::Reroll), &AGambitPlayerController::HandleRerollInput);
	BindActionIfValid(ResolveInputAction(Consumable1InputAction, EGambitCoreInputAction::Consumable1), &AGambitPlayerController::HandleConsumable1Input);
	BindActionIfValid(ResolveInputAction(Consumable2InputAction, EGambitCoreInputAction::Consumable2), &AGambitPlayerController::HandleConsumable2Input);
	BindActionIfValid(ResolveInputAction(Consumable3InputAction, EGambitCoreInputAction::Consumable3), &AGambitPlayerController::HandleConsumable3Input);
	BindActionIfValid(ResolveInputAction(ShopOffer1InputAction, EGambitCoreInputAction::ShopOffer1), &AGambitPlayerController::HandleShopOffer1Input);
	BindActionIfValid(ResolveInputAction(ShopOffer2InputAction, EGambitCoreInputAction::ShopOffer2), &AGambitPlayerController::HandleShopOffer2Input);
	BindActionIfValid(ResolveInputAction(ShopOffer3InputAction, EGambitCoreInputAction::ShopOffer3), &AGambitPlayerController::HandleShopOffer3Input);
	BindActionIfValid(ResolveInputAction(LockDie1InputAction, EGambitCoreInputAction::LockDie1), &AGambitPlayerController::HandleLockDie1Input);
	BindActionIfValid(ResolveInputAction(LockDie2InputAction, EGambitCoreInputAction::LockDie2), &AGambitPlayerController::HandleLockDie2Input);
	BindActionIfValid(ResolveInputAction(LockDie3InputAction, EGambitCoreInputAction::LockDie3), &AGambitPlayerController::HandleLockDie3Input);
	BindActionIfValid(ResolveInputAction(LockDie4InputAction, EGambitCoreInputAction::LockDie4), &AGambitPlayerController::HandleLockDie4Input);
	BindActionIfValid(ResolveInputAction(LockDie5InputAction, EGambitCoreInputAction::LockDie5), &AGambitPlayerController::HandleLockDie5Input);
	BindActionIfValid(ResolveInputAction(LockDie6InputAction, EGambitCoreInputAction::LockDie6), &AGambitPlayerController::HandleLockDie6Input);
}

void AGambitPlayerController::RequestPlayerReady(const bool bReady)
{
	if (HasAuthority())
	{
		if (AGambitGameMode* GameMode = GetGambitGameMode())
		{
			GameMode->SetPlayerReady(GetGambitPlayerState(), bReady);
		}
		return;
	}

	ServerRequestPlayerReady(bReady);
}

void AGambitPlayerController::RequestSetDieLocked(const int32 DieIndex, const bool bLocked)
{
	if (HasAuthority())
	{
		if (AGambitGameMode* GameMode = GetGambitGameMode())
		{
			GameMode->RequestSetDieLocked(GetGambitPlayerState(), DieIndex, bLocked);
		}
		return;
	}

	ServerRequestSetDieLocked(DieIndex, bLocked);
}

void AGambitPlayerController::RequestReroll()
{
	if (HasAuthority())
	{
		if (AGambitGameMode* GameMode = GetGambitGameMode())
		{
			GameMode->RequestReroll(GetGambitPlayerState());
		}
		return;
	}

	ServerRequestReroll();
}

void AGambitPlayerController::RequestUseConsumable(const int32 SlotIndex)
{
	if (RequestBeginConsumableTargetSelection(SlotIndex))
	{
		return;
	}

	if (HasAuthority())
	{
		if (AGambitGameMode* GameMode = GetGambitGameMode())
		{
			GameMode->RequestUseConsumable(GetGambitPlayerState(), SlotIndex);
		}
		return;
	}

	ServerRequestUseConsumable(SlotIndex);
}

void AGambitPlayerController::RequestUseConsumableOnSelectedDie(const int32 SlotIndex, const int32 SelectedDieIndex)
{
	if (HasAuthority())
	{
		if (AGambitGameMode* GameMode = GetGambitGameMode())
		{
			GameMode->RequestUseConsumableOnTargetSelectedDie(GetGambitPlayerState(), SlotIndex, GetGambitPlayerState(), SelectedDieIndex);
		}
		return;
	}

	ServerRequestUseConsumableOnSelectedDie(SlotIndex, SelectedDieIndex);
}

bool AGambitPlayerController::RequestBeginConsumableTargetSelection(const int32 SlotIndex)
{
	AGambitGameMode* GameMode = GetGambitGameMode();
	if (!GameMode)
	{
		return false;
	}

	FGambitTargetSelectionRequest Request;
	if (!GameMode->BuildConsumableTargetSelectionRequest(GetGambitPlayerState(), SlotIndex, Request)
		|| !Request.bRequiresExplicitSelection)
	{
		return false;
	}

	return StartTargetSelection(Request);
}

bool AGambitPlayerController::StartTargetSelection(const FGambitTargetSelectionRequest& Request)
{
	if (!Request.bRequiresExplicitSelection)
	{
		return false;
	}

	PendingTargetSelectionRequest = Request;
	bHasPendingTargetSelection = true;
	PendingTargetSelectionSelectedOptionId = INDEX_NONE;

	for (const FGambitTargetSelectionOption& Option : PendingTargetSelectionRequest.Options)
	{
		if (Option.bValid)
		{
			PendingTargetSelectionSelectedOptionId = Option.OptionId;
			break;
		}
	}

	const FString Summary = Request.DebugText.IsEmpty()
		? FString::Printf(TEXT("%s requires target selection"), *ResolveControllerItemName(Request.SourceItemDefinition))
		: Request.DebugText;
	AddTargetSelectionFeedback(
		EGambitRoundGameplayEventType::TargetSelectionRequested,
		EGambitRoundGameplayEventOutcome::Applied,
		PendingTargetSelectionRequest,
		Summary);

	if (!PendingTargetSelectionRequest.HasValidOptions())
	{
		AddTargetSelectionFeedback(
			EGambitRoundGameplayEventType::TargetSelectionInvalid,
			EGambitRoundGameplayEventOutcome::Failed,
			PendingTargetSelectionRequest,
			PendingTargetSelectionRequest.InvalidReason.IsEmpty()
				? TEXT("Target selection has no valid options.")
				: PendingTargetSelectionRequest.InvalidReason);
	}

	OnTargetSelectionChanged.Broadcast(PendingTargetSelectionRequest);
	return true;
}

bool AGambitPlayerController::RequestCancelTargetSelection()
{
	if (!bHasPendingTargetSelection)
	{
		return false;
	}

	AddTargetSelectionFeedback(
		EGambitRoundGameplayEventType::TargetSelectionCancelled,
		EGambitRoundGameplayEventOutcome::Skipped,
		PendingTargetSelectionRequest,
		TEXT("Target selection cancelled."));
	ClearPendingTargetSelection();
	return true;
}

bool AGambitPlayerController::RequestSelectTargetSelectionOption(const int32 OptionId)
{
	if (!bHasPendingTargetSelection)
	{
		return false;
	}

	const FGambitTargetSelectionOption* Option = FindPendingTargetSelectionOption(OptionId);
	if (!Option || !Option->bValid)
	{
		AddTargetSelectionFeedback(
			EGambitRoundGameplayEventType::TargetSelectionInvalid,
			EGambitRoundGameplayEventOutcome::Failed,
			PendingTargetSelectionRequest,
			FString::Printf(TEXT("Target selection option %d is invalid."), OptionId));
		return false;
	}

	PendingTargetSelectionSelectedOptionId = OptionId;
	OnTargetSelectionChanged.Broadcast(PendingTargetSelectionRequest);
	return true;
}

bool AGambitPlayerController::RequestConfirmTargetSelection()
{
	if (!bHasPendingTargetSelection)
	{
		return false;
	}

	const FGambitTargetSelectionOption* Option = FindPendingTargetSelectionOption(PendingTargetSelectionSelectedOptionId);
	if (!Option || !Option->bValid)
	{
		AddTargetSelectionFeedback(
			EGambitRoundGameplayEventType::TargetSelectionInvalid,
			EGambitRoundGameplayEventOutcome::Failed,
			PendingTargetSelectionRequest,
			TEXT("Target selection confirmation failed: no valid option selected."));
		return false;
	}

	AGambitGameMode* GameMode = GetGambitGameMode();
	if (!GameMode)
	{
		AddTargetSelectionFeedback(
			EGambitRoundGameplayEventType::TargetSelectionInvalid,
			EGambitRoundGameplayEventOutcome::Failed,
			PendingTargetSelectionRequest,
			TEXT("Target selection confirmation failed: missing GameMode."),
			Option);
		return false;
	}

	const FGambitTargetSelectionResult Result = BuildTargetSelectionResult(*Option);
	const bool bSuccess = GameMode->RequestUseConsumableWithTargetSelectionResult(GetGambitPlayerState(), Result);
	if (!bSuccess)
	{
		AddTargetSelectionFeedback(
			EGambitRoundGameplayEventType::TargetSelectionInvalid,
			EGambitRoundGameplayEventOutcome::Failed,
			PendingTargetSelectionRequest,
			TEXT("Target selection confirmation was refused by round flow."),
			Option);
		return false;
	}

	ClearPendingTargetSelection();
	return true;
}

bool AGambitPlayerController::GetSelectedTargetSelectionOption(FGambitTargetSelectionOption& OutOption) const
{
	const FGambitTargetSelectionOption* Option = FindPendingTargetSelectionOption(PendingTargetSelectionSelectedOptionId);
	if (!Option)
	{
		OutOption = FGambitTargetSelectionOption();
		return false;
	}

	OutOption = *Option;
	return true;
}

void AGambitPlayerController::RequestPurchaseOffer(const int32 OfferId)
{
	if (HasAuthority())
	{
		if (AGambitGameMode* GameMode = GetGambitGameMode())
		{
			GameMode->RequestPurchaseOffer(GetGambitPlayerState(), OfferId);
		}
		return;
	}

	ServerRequestPurchaseOffer(OfferId);
}

bool AGambitPlayerController::RequestJoinLocalPlayer()
{
	if (!IsLocalController())
	{
		return false;
	}

	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (bOnlyPrimaryLocalPlayerCanManageRoster && LocalPlayer && LocalPlayer->GetLocalPlayerIndex() != 0)
	{
		return false;
	}

	UGambitLocalMultiplayerSubsystem* LocalMultiplayer = GetLocalMultiplayerSubsystem();
	return LocalMultiplayer ? LocalMultiplayer->RequestJoinLocalPlayer() : false;
}

bool AGambitPlayerController::RequestLeaveThisLocalPlayer()
{
	if (!IsLocalController())
	{
		return false;
	}

	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return false;
	}

	UGambitLocalMultiplayerSubsystem* LocalMultiplayer = GetLocalMultiplayerSubsystem();
	if (!LocalMultiplayer)
	{
		return false;
	}

	if (bOnlyPrimaryLocalPlayerCanManageRoster && LocalPlayer->GetLocalPlayerIndex() == 0)
	{
		const int32 LocalPlayerCount = LocalMultiplayer->GetLocalPlayerCount();
		return LocalPlayerCount > 1 ? LocalMultiplayer->RequestLeaveLocalPlayer(LocalPlayerCount - 1) : false;
	}

	if (bOnlyPrimaryLocalPlayerCanManageRoster)
	{
		return false;
	}

	return LocalMultiplayer->RequestLeaveLocalPlayer(LocalPlayer->GetLocalPlayerIndex());
}

void AGambitPlayerController::GambitPrintMatch()
{
	if (AGambitGameMode* GameMode = GetGambitGameMode())
	{
		if (UGambitMatchDebugComponent* MatchDebug = GameMode->GetMatchDebugComponent())
		{
			MatchDebug->PrintMatchSnapshot();
			MatchDebug->PrintAllPlayerSnapshots();
			return;
		}
	}

	UE_LOG(LogGambit, Log, TEXT("DebugMatch: GambitPrintMatch failed, missing debug component"));
}

void AGambitPlayerController::GambitValidateData()
{
	if (UGambitMatchDebugComponent* MatchDebug = GetMatchDebugComponent())
	{
		MatchDebug->DebugValidateData();
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DebugMatch: GambitValidateData failed, missing debug component"));
}

void AGambitPlayerController::GambitPrintInventory()
{
	if (UGambitMatchDebugComponent* MatchDebug = GetMatchDebugComponent())
	{
		MatchDebug->DebugPrintInventory();
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DebugMatch: GambitPrintInventory failed, missing debug component"));
}

void AGambitPlayerController::GambitPrintSharedPool()
{
	if (UGambitMatchDebugComponent* MatchDebug = GetMatchDebugComponent())
	{
		MatchDebug->DebugPrintSharedPool();
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DebugMatch: GambitPrintSharedPool failed, missing debug component"));
}

void AGambitPlayerController::GambitReadyAll()
{
	if (AGambitGameMode* GameMode = GetGambitGameMode())
	{
		if (UGambitMatchDebugComponent* MatchDebug = GameMode->GetMatchDebugComponent())
		{
			MatchDebug->DebugReadyAllPlayers();
			return;
		}
	}

	UE_LOG(LogGambit, Log, TEXT("DebugMatch: GambitReadyAll failed, missing debug component"));
}

void AGambitPlayerController::GambitAutoAdvance()
{
	if (AGambitGameMode* GameMode = GetGambitGameMode())
	{
		if (UGambitMatchDebugComponent* MatchDebug = GameMode->GetMatchDebugComponent())
		{
			MatchDebug->DebugAutoAdvanceCurrentPhase();
			return;
		}
	}

	UE_LOG(LogGambit, Log, TEXT("DebugMatch: GambitAutoAdvance failed, missing debug component"));
}

void AGambitPlayerController::GambitAutoToShop()
{
	if (AGambitGameMode* GameMode = GetGambitGameMode())
	{
		if (UGambitMatchDebugComponent* MatchDebug = GameMode->GetMatchDebugComponent())
		{
			MatchDebug->DebugAutoAdvanceUntilShopOrMatchEnd();
			return;
		}
	}

	UE_LOG(LogGambit, Log, TEXT("DebugMatch: GambitAutoToShop failed, missing debug component"));
}

void AGambitPlayerController::GambitBuyFirstOfferAll()
{
	if (AGambitGameMode* GameMode = GetGambitGameMode())
	{
		if (UGambitMatchDebugComponent* MatchDebug = GameMode->GetMatchDebugComponent())
		{
			MatchDebug->DebugBuyFirstAffordableOfferForAllPlayers();
			return;
		}
	}

	UE_LOG(LogGambit, Log, TEXT("DebugMatch: GambitBuyFirstOfferAll failed, missing debug component"));
}

void AGambitPlayerController::GambitSkipShop()
{
	if (AGambitGameMode* GameMode = GetGambitGameMode())
	{
		if (UGambitMatchDebugComponent* MatchDebug = GameMode->GetMatchDebugComponent())
		{
			MatchDebug->DebugSkipShopForAllPlayers();
			return;
		}
	}

	UE_LOG(LogGambit, Log, TEXT("DebugMatch: GambitSkipShop failed, missing debug component"));
}

void AGambitPlayerController::Gambit()
{
	if (UGambitMatchDebugComponent* MatchDebug = GetMatchDebugComponent())
	{
		MatchDebug->PrintDebugCommandHelp();
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DebugMatch: Gambit help failed, missing debug component"));
}

void AGambitPlayerController::GambitGrantGold(const int32 Amount)
{
	if (UGambitMatchDebugComponent* MatchDebug = GetMatchDebugComponent())
	{
		MatchDebug->DebugGrantGoldToAllPlayers(Amount);
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DebugMatch: GambitGrantGold failed, missing debug component"));
}

void AGambitPlayerController::GambitGrantConsumable(const int32 PlayerIndex, const FString& ConsumableId)
{
	if (UGambitMatchDebugComponent* MatchDebug = GetMatchDebugComponent())
	{
		MatchDebug->DebugGrantConsumable(PlayerIndex, FName(*ConsumableId));
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DebugMatch: GambitGrantConsumable failed, missing debug component"));
}

void AGambitPlayerController::GambitPrintShop()
{
	if (UGambitMatchDebugComponent* MatchDebug = GetMatchDebugComponent())
	{
		MatchDebug->DebugPrintShop();
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DebugMatch: GambitPrintShop failed, missing debug component"));
}

void AGambitPlayerController::GambitBuyOffer(const int32 PlayerIndex, const int32 OfferId)
{
	if (UGambitMatchDebugComponent* MatchDebug = GetMatchDebugComponent())
	{
		MatchDebug->DebugBuyOffer(PlayerIndex, OfferId);
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DebugMatch: GambitBuyOffer failed, missing debug component"));
}

void AGambitPlayerController::GambitRerollPlayer(const int32 PlayerIndex)
{
	if (UGambitMatchDebugComponent* MatchDebug = GetMatchDebugComponent())
	{
		MatchDebug->DebugRerollPlayer(PlayerIndex);
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DebugMatch: GambitRerollPlayer failed, missing debug component"));
}

void AGambitPlayerController::GambitLockDie(const int32 PlayerIndex, const int32 DieIndex)
{
	if (UGambitMatchDebugComponent* MatchDebug = GetMatchDebugComponent())
	{
		MatchDebug->DebugLockDie(PlayerIndex, DieIndex);
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DebugMatch: GambitLockDie failed, missing debug component"));
}

void AGambitPlayerController::GambitUseConsumable(const int32 PlayerIndex, const int32 SlotIndex)
{
	if (UGambitMatchDebugComponent* MatchDebug = GetMatchDebugComponent())
	{
		MatchDebug->DebugUseConsumable(PlayerIndex, SlotIndex);
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DebugMatch: GambitUseConsumable failed, missing debug component"));
}

void AGambitPlayerController::GambitUseConsumableOnDie(const int32 PlayerIndex, const int32 SlotIndex, const int32 DieIndex)
{
	if (UGambitMatchDebugComponent* MatchDebug = GetMatchDebugComponent())
	{
		MatchDebug->DebugUseConsumableOnDie(PlayerIndex, SlotIndex, DieIndex);
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DebugMatch: GambitUseConsumableOnDie failed, missing debug component"));
}

void AGambitPlayerController::GambitAutoFullMatch()
{
	if (UGambitMatchDebugComponent* MatchDebug = GetMatchDebugComponent())
	{
		MatchDebug->DebugAutoFullMatch();
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DebugMatch: GambitAutoFullMatch failed, missing debug component"));
}

void AGambitPlayerController::GambitAIDecideRerolls()
{
	if (UGambitMatchDebugComponent* MatchDebug = GetMatchDebugComponent())
	{
		MatchDebug->DebugAIDecideRerolls();
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DebugAI: GambitAIDecideRerolls failed, missing debug component"));
}

void AGambitPlayerController::GambitAIDecideActions()
{
	if (UGambitMatchDebugComponent* MatchDebug = GetMatchDebugComponent())
	{
		MatchDebug->DebugAIDecideActions();
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DebugAI: GambitAIDecideActions failed, missing debug component"));
}

void AGambitPlayerController::GambitAIDecideShop()
{
	if (UGambitMatchDebugComponent* MatchDebug = GetMatchDebugComponent())
	{
		MatchDebug->DebugAIDecideShop();
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DebugAI: GambitAIDecideShop failed, missing debug component"));
}

void AGambitPlayerController::GambitAIFullRound()
{
	if (UGambitMatchDebugComponent* MatchDebug = GetMatchDebugComponent())
	{
		MatchDebug->DebugAIFullRound();
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DebugAI: GambitAIFullRound failed, missing debug component"));
}

void AGambitPlayerController::GambitAIFullMatch()
{
	if (UGambitMatchDebugComponent* MatchDebug = GetMatchDebugComponent())
	{
		MatchDebug->DebugAIFullMatch();
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DebugAI: GambitAIFullMatch failed, missing debug component"));
}

void AGambitPlayerController::GambitDevStart(const int32 DesiredLocalPlayerCount)
{
	if (UGambitDevMatchSandboxComponent* Sandbox = GetDevMatchSandboxComponent())
	{
		const FGambitDevSandboxActionResult Result = Sandbox->StartDevMatchSandbox(DesiredLocalPlayerCount);
		UE_LOG(LogGambit, Log, TEXT("DevSandbox: Start Players=%d Result=%s Message=%s"), DesiredLocalPlayerCount, Result.bSuccess ? TEXT("Success") : TEXT("Failure"), *Result.Message);
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DevSandbox: GambitDevStart failed, missing sandbox component"));
}

void AGambitPlayerController::GambitDevSnapshot()
{
	if (UGambitDevMatchSandboxComponent* Sandbox = GetDevMatchSandboxComponent())
	{
		const FGambitDevSandboxSnapshot Snapshot = Sandbox->BuildSandboxSnapshot();
		UE_LOG(
			LogGambit,
			Log,
			TEXT("DevSandbox: Snapshot %s"),
			*Snapshot.Summary);
		for (const FGambitDevSandboxPlayerSlotSnapshot& Slot : Snapshot.PlayerSlots)
		{
			UE_LOG(LogGambit, Log, TEXT("DevSandbox: Slot %s"), *Slot.Summary);
		}
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DevSandbox: GambitDevSnapshot failed, missing sandbox component"));
}

void AGambitPlayerController::GambitDevInspect(const int32 PlayerIndex)
{
	if (UGambitDevMatchSandboxComponent* Sandbox = GetDevMatchSandboxComponent())
	{
		const FGambitDevSandboxActionResult Result = Sandbox->SetInspectedPlayerIndex(PlayerIndex);
		UE_LOG(LogGambit, Log, TEXT("DevSandbox: Inspect Player=%d Result=%s Message=%s"), PlayerIndex, Result.bSuccess ? TEXT("Success") : TEXT("Failure"), *Result.Message);
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DevSandbox: GambitDevInspect failed, missing sandbox component"));
}

void AGambitPlayerController::GambitDevHuman(const int32 PlayerIndex)
{
	if (UGambitDevMatchSandboxComponent* Sandbox = GetDevMatchSandboxComponent())
	{
		const FGambitDevSandboxActionResult Result = Sandbox->SetPlayerControlMode(PlayerIndex, EGambitDevSandboxPlayerControlMode::HumanLocal);
		UE_LOG(LogGambit, Log, TEXT("DevSandbox: Human Player=%d Result=%s Message=%s"), PlayerIndex, Result.bSuccess ? TEXT("Success") : TEXT("Failure"), *Result.Message);
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DevSandbox: GambitDevHuman failed, missing sandbox component"));
}

void AGambitPlayerController::GambitDevAI(const int32 PlayerIndex)
{
	if (UGambitDevMatchSandboxComponent* Sandbox = GetDevMatchSandboxComponent())
	{
		const FGambitDevSandboxActionResult Result = Sandbox->SetPlayerControlMode(PlayerIndex, EGambitDevSandboxPlayerControlMode::DebugAI);
		UE_LOG(LogGambit, Log, TEXT("DevSandbox: AI Player=%d Result=%s Message=%s"), PlayerIndex, Result.bSuccess ? TEXT("Success") : TEXT("Failure"), *Result.Message);
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DevSandbox: GambitDevAI failed, missing sandbox component"));
}

void AGambitPlayerController::GambitDevAdvance()
{
	if (UGambitDevMatchSandboxComponent* Sandbox = GetDevMatchSandboxComponent())
	{
		const FGambitDevSandboxActionResult Result = Sandbox->RequestAdvanceCurrentPhase();
		UE_LOG(LogGambit, Log, TEXT("DevSandbox: Advance Result=%s Message=%s"), Result.bSuccess ? TEXT("Success") : TEXT("Failure"), *Result.Message);
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DevSandbox: GambitDevAdvance failed, missing sandbox component"));
}

void AGambitPlayerController::GambitDevAIDecide()
{
	if (UGambitDevMatchSandboxComponent* Sandbox = GetDevMatchSandboxComponent())
	{
		const FGambitDevSandboxActionResult Result = Sandbox->RequestAIDecisionForAIPlayers();
		UE_LOG(LogGambit, Log, TEXT("DevSandbox: AIDecide Result=%s Message=%s"), Result.bSuccess ? TEXT("Success") : TEXT("Failure"), *Result.Message);
		return;
	}

	UE_LOG(LogGambit, Log, TEXT("DevSandbox: GambitDevAIDecide failed, missing sandbox component"));
}

void AGambitPlayerController::ServerRequestPlayerReady_Implementation(const bool bReady)
{
	RequestPlayerReady(bReady);
}

void AGambitPlayerController::ServerRequestSetDieLocked_Implementation(const int32 DieIndex, const bool bLocked)
{
	RequestSetDieLocked(DieIndex, bLocked);
}

void AGambitPlayerController::ServerRequestReroll_Implementation()
{
	RequestReroll();
}

void AGambitPlayerController::ServerRequestUseConsumable_Implementation(const int32 SlotIndex)
{
	RequestUseConsumable(SlotIndex);
}

void AGambitPlayerController::ServerRequestUseConsumableOnSelectedDie_Implementation(const int32 SlotIndex, const int32 SelectedDieIndex)
{
	RequestUseConsumableOnSelectedDie(SlotIndex, SelectedDieIndex);
}

void AGambitPlayerController::ServerRequestPurchaseOffer_Implementation(const int32 OfferId)
{
	RequestPurchaseOffer(OfferId);
}

void AGambitPlayerController::HandleJoinLocalPlayerInput(const FInputActionValue& Value)
{
	(void)Value;
	RequestJoinLocalPlayer();
}

void AGambitPlayerController::HandleLeaveLocalPlayerInput(const FInputActionValue& Value)
{
	(void)Value;
	RequestLeaveThisLocalPlayer();
}

void AGambitPlayerController::HandleReadyToggleInput(const FInputActionValue& Value)
{
	(void)Value;
	bReadyState = !bReadyState;
	RequestPlayerReady(bReadyState);
}

void AGambitPlayerController::HandleRerollInput(const FInputActionValue& Value)
{
	(void)Value;
	RequestReroll();
}

void AGambitPlayerController::HandleConsumable1Input(const FInputActionValue& Value)
{
	(void)Value;
	RequestUseConsumable(0);
}

void AGambitPlayerController::HandleConsumable2Input(const FInputActionValue& Value)
{
	(void)Value;
	RequestUseConsumable(1);
}

void AGambitPlayerController::HandleConsumable3Input(const FInputActionValue& Value)
{
	(void)Value;
	RequestUseConsumable(2);
}

void AGambitPlayerController::HandleShopOffer1Input(const FInputActionValue& Value)
{
	(void)Value;
	RequestPurchaseOffer(0);
}

void AGambitPlayerController::HandleShopOffer2Input(const FInputActionValue& Value)
{
	(void)Value;
	RequestPurchaseOffer(1);
}

void AGambitPlayerController::HandleShopOffer3Input(const FInputActionValue& Value)
{
	(void)Value;
	RequestPurchaseOffer(2);
}

void AGambitPlayerController::HandleLockDie1Input(const FInputActionValue& Value)
{
	(void)Value;
	ToggleDieLockByIndex(0);
}

void AGambitPlayerController::HandleLockDie2Input(const FInputActionValue& Value)
{
	(void)Value;
	ToggleDieLockByIndex(1);
}

void AGambitPlayerController::HandleLockDie3Input(const FInputActionValue& Value)
{
	(void)Value;
	ToggleDieLockByIndex(2);
}

void AGambitPlayerController::HandleLockDie4Input(const FInputActionValue& Value)
{
	(void)Value;
	ToggleDieLockByIndex(3);
}

void AGambitPlayerController::HandleLockDie5Input(const FInputActionValue& Value)
{
	(void)Value;
	ToggleDieLockByIndex(4);
}

void AGambitPlayerController::HandleLockDie6Input(const FInputActionValue& Value)
{
	(void)Value;
	ToggleDieLockByIndex(5);
}

UInputAction* AGambitPlayerController::ResolveInputAction(UInputAction* PreferredAction, const EGambitCoreInputAction FallbackAction) const
{
	if (PreferredAction)
	{
		return PreferredAction;
	}

	const UGambitLocalMultiplayerSubsystem* LocalSubsystem = GetLocalMultiplayerSubsystem();
	return LocalSubsystem ? LocalSubsystem->GetCoreInputAction(FallbackAction) : nullptr;
}

void AGambitPlayerController::ToggleDieLockByIndex(const int32 DieIndex)
{
	AGambitPlayerState* GambitPlayerState = GetGambitPlayerState();
	if (!GambitPlayerState)
	{
		return;
	}

	const TArray<FGambitDieRuntimeState>& DiceStates = GambitPlayerState->GetDiceStatesRef();
	if (!DiceStates.IsValidIndex(DieIndex))
	{
		return;
	}

	RequestSetDieLocked(DieIndex, !DiceStates[DieIndex].bLocked);
}

const FGambitTargetSelectionOption* AGambitPlayerController::FindPendingTargetSelectionOption(const int32 OptionId) const
{
	if (!bHasPendingTargetSelection)
	{
		return nullptr;
	}

	return PendingTargetSelectionRequest.Options.FindByPredicate([OptionId](const FGambitTargetSelectionOption& Option)
	{
		return Option.OptionId == OptionId;
	});
}

void AGambitPlayerController::ClearPendingTargetSelection()
{
	bHasPendingTargetSelection = false;
	PendingTargetSelectionRequest = FGambitTargetSelectionRequest();
	PendingTargetSelectionSelectedOptionId = INDEX_NONE;
	OnTargetSelectionChanged.Broadcast(PendingTargetSelectionRequest);
}

FGambitTargetSelectionResult AGambitPlayerController::BuildTargetSelectionResult(
	const FGambitTargetSelectionOption& Option) const
{
	FGambitTargetSelectionResult Result;
	Result.RequestId = PendingTargetSelectionRequest.RequestId;
	Result.bConfirmed = true;
	Result.SourceConsumableSlotIndex = PendingTargetSelectionRequest.SourceConsumableSlotIndex;
	Result.SelectedOptionId = Option.OptionId;
	Result.TargetPlayer = Option.TargetPlayer ? Option.TargetPlayer.Get() : PendingTargetSelectionRequest.RequestingPlayer.Get();
	Result.TargetPlayerIndex = Option.TargetPlayerIndex;
	Result.TargetDieHandIndex = Option.TargetDieHandIndex;
	Result.TargetDieInstanceId = Option.TargetDieInstanceId;
	Result.TargetRuleId = Option.TargetRuleId;
	return Result;
}

void AGambitPlayerController::AddTargetSelectionFeedback(
	const EGambitRoundGameplayEventType EventType,
	const EGambitRoundGameplayEventOutcome Outcome,
	const FGambitTargetSelectionRequest& Request,
	const FString& Summary,
	const FGambitTargetSelectionOption* Option) const
{
	AGambitPlayerState* TargetSelectionPlayerState = Request.RequestingPlayer
		? Request.RequestingPlayer.Get()
		: GetGambitPlayerState();
	if (!TargetSelectionPlayerState)
	{
		return;
	}

	const AGambitGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AGambitGameState>() : nullptr;
	const AGambitPlayerState* TargetPlayer = Option && Option->TargetPlayer
		? Option->TargetPlayer.Get()
		: TargetSelectionPlayerState;
	const EGambitRoundPhase Phase = GameState ? GameState->GetCurrentPhase() : Request.CurrentPhase;

	FGambitDebugEffectEvent DebugEvent;
	DebugEvent.Category = Outcome == EGambitRoundGameplayEventOutcome::Failed
		? EGambitDebugEventCategory::Refusal
		: EGambitDebugEventCategory::Effect;
	DebugEvent.Phase = Phase;
	DebugEvent.HookId = TEXT("TargetSelection");
	DebugEvent.SourceId = Request.SourceItemId;
	DebugEvent.SourceName = ResolveControllerItemName(Request.SourceItemDefinition);
	DebugEvent.EffectId = Request.EffectId;
	DebugEvent.TargetRuleId = Request.TargetRuleId;
	DebugEvent.TargetName = Option ? Option->Label : TEXT("Pending target");
	DebugEvent.bTriggered = Outcome == EGambitRoundGameplayEventOutcome::Applied;
	DebugEvent.Summary = Summary;
	TargetSelectionPlayerState->AddDebugEffectEvent(DebugEvent);

	FGambitRoundGameplayEvent RoundEvent;
	RoundEvent.EventType = EventType;
	RoundEvent.Outcome = Outcome;
	RoundEvent.RoundNumber = GameState ? GameState->GetCurrentRoundIndex() : 0;
	RoundEvent.Phase = Phase;
	RoundEvent.SourcePlayerId = ResolveControllerPlayerId(TargetSelectionPlayerState);
	RoundEvent.TargetPlayerId = ResolveControllerPlayerId(TargetPlayer);
	RoundEvent.SourceItemId = Request.SourceItemId;
	RoundEvent.EffectId = Request.EffectId;
	RoundEvent.TargetRuleId = Request.TargetRuleId;
	RoundEvent.TargetDieHandIndex = Option ? Option->TargetDieHandIndex : INDEX_NONE;
	RoundEvent.TargetDieInstanceId = Option ? Option->TargetDieInstanceId : INDEX_NONE;
	RoundEvent.Reason = Summary;
	TargetSelectionPlayerState->AddRoundEvent(RoundEvent);
}

AGambitPlayerState* AGambitPlayerController::GetGambitPlayerState() const
{
	return GetPlayerState<AGambitPlayerState>();
}

AGambitGameMode* AGambitPlayerController::GetGambitGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<AGambitGameMode>() : nullptr;
}

UGambitMatchDebugComponent* AGambitPlayerController::GetMatchDebugComponent() const
{
	const AGambitGameMode* GameMode = GetGambitGameMode();
	return GameMode ? GameMode->GetMatchDebugComponent() : nullptr;
}

UGambitDevMatchSandboxComponent* AGambitPlayerController::GetDevMatchSandboxComponent() const
{
	const AGambitGameMode* GameMode = GetGambitGameMode();
	return GameMode ? GameMode->GetDevMatchSandboxComponent() : nullptr;
}

UGambitLocalMultiplayerSubsystem* AGambitPlayerController::GetLocalMultiplayerSubsystem() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UGambitLocalMultiplayerSubsystem>() : nullptr;
}
