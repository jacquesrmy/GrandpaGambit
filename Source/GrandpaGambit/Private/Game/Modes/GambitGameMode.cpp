#include "Game/Modes/GambitGameMode.h"

#include "Debug/GambitDevMatchSandboxComponent.h"
#include "Debug/GambitMatchDebugComponent.h"
#include "Core/Settings/GambitGameBalanceSettings.h"
#include "Engine/GameInstance.h"
#include "Game/Flow/GambitRoundFlowComponent.h"
#include "Game/States/GambitGameState.h"
#include "Players/Controllers/GambitPlayerController.h"
#include "Players/States/GambitPlayerState.h"
#include "Players/Subsystems/GambitLocalMultiplayerSubsystem.h"

AGambitGameMode::AGambitGameMode()
{
	GameStateClass = AGambitGameState::StaticClass();
	PlayerStateClass = AGambitPlayerState::StaticClass();
	PlayerControllerClass = AGambitPlayerController::StaticClass();

	RoundFlowComponent = CreateDefaultSubobject<UGambitRoundFlowComponent>(TEXT("RoundFlowComponent"));
	MatchDebugComponent = CreateDefaultSubobject<UGambitMatchDebugComponent>(TEXT("MatchDebugComponent"));
	DevMatchSandboxComponent = CreateDefaultSubobject<UGambitDevMatchSandboxComponent>(TEXT("DevMatchSandboxComponent"));
}

void AGambitGameMode::BeginPlay()
{
	Super::BeginPlay();

	PendingMatchSetup = BuildDefaultMatchSetup();
	PublishMatchSetup();
	ResetMatchShellState(EGambitMatchLifecycleState::MainMenu);

	if (bAutoStartMatchOnBeginPlay)
	{
		RequestStartConfiguredMatch();
	}
}

void AGambitGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (bMatchFlowStarted)
	{
		InitializePlayerForMatch(NewPlayer);
	}

	if (UGambitLocalMultiplayerSubsystem* LocalMultiplayer = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGambitLocalMultiplayerSubsystem>() : nullptr)
	{
		LocalMultiplayer->RefreshLocalMultiplayerLayout();
	}
}

void AGambitGameMode::StartMatchFlow()
{
	if (bMatchFlowStarted)
	{
		return;
	}

	StartMatchWithSetup(PendingMatchSetup, false);
}

void AGambitGameMode::RestartMatchFlow()
{
	if (UGambitLocalMultiplayerSubsystem* LocalMultiplayer = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGambitLocalMultiplayerSubsystem>() : nullptr)
	{
		PendingMatchSetup = BuildClampedMatchSetup(LocalMultiplayer->GetLocalPlayerCount(), PendingMatchSetup.RoundCount);
		PublishMatchSetup();
	}

	StartMatchWithSetup(PendingMatchSetup, true);
}

void AGambitGameMode::RequestMainMenu()
{
	PendingMatchSetup = BuildDefaultMatchSetup();
	PublishMatchSetup();
	ResetMatchShellState(EGambitMatchLifecycleState::MainMenu);
}

void AGambitGameMode::RequestOpenMatchSetup()
{
	ResetMatchShellState(EGambitMatchLifecycleState::MatchSetup);
}

bool AGambitGameMode::RequestConfigureMatch(const int32 LocalPlayerCount, const int32 RoundCount)
{
	PendingMatchSetup = BuildClampedMatchSetup(LocalPlayerCount, RoundCount);
	PublishMatchSetup();
	return true;
}

bool AGambitGameMode::RequestEnterLobby()
{
	UGambitLocalMultiplayerSubsystem* LocalMultiplayer = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UGambitLocalMultiplayerSubsystem>()
		: nullptr;
	if (!LocalMultiplayer)
	{
		return false;
	}

	LocalMultiplayer->EnsureLocalPlayerCount(PendingMatchSetup.LocalPlayerCount);
	if (LocalMultiplayer->GetLocalPlayerCount() != PendingMatchSetup.LocalPlayerCount)
	{
		return false;
	}

	LocalMultiplayer->RefreshLocalMultiplayerLayout();
	ResetMatchShellState(EGambitMatchLifecycleState::Lobby);
	return true;
}

bool AGambitGameMode::RequestStartConfiguredMatch()
{
	if (!RequestEnterLobby())
	{
		return false;
	}

	StartMatchWithSetup(PendingMatchSetup, true);
	return true;
}

bool AGambitGameMode::RequestReadyAllPlayersForCurrentPhase()
{
	AGambitGameState* GambitGameState = GetGameState<AGambitGameState>();
	if (!GambitGameState)
	{
		return false;
	}

	const EGambitRoundPhase CurrentPhase = GambitGameState->GetCurrentPhase();
	const bool bReadyGatedPhase = CurrentPhase == EGambitRoundPhase::SelectionReroll
		|| CurrentPhase == EGambitRoundPhase::Action
		|| CurrentPhase == EGambitRoundPhase::Shop;
	if (!bReadyGatedPhase)
	{
		return false;
	}

	for (AGambitPlayerState* PlayerState : GambitGameState->GetGambitPlayerStates())
	{
		SetPlayerReady(PlayerState, true);
	}

	return true;
}

FGambitMatchSetupConfig AGambitGameMode::BuildDefaultMatchSetup() const
{
	const UGambitGameBalanceSettings* Settings = UGambitGameBalanceSettings::Get();
	return BuildClampedMatchSetup(Settings->MinLocalPlayers, Settings->RoundCount);
}

FGambitMatchSetupConfig AGambitGameMode::BuildClampedMatchSetup(const int32 LocalPlayerCount, const int32 RoundCount) const
{
	const UGambitGameBalanceSettings* Settings = UGambitGameBalanceSettings::Get();

	FGambitMatchSetupConfig MatchSetup;
	MatchSetup.LocalPlayerCount = Settings->ClampLocalPlayerCount(LocalPlayerCount);
	MatchSetup.RoundCount = FMath::Max(1, RoundCount);
	return MatchSetup;
}

void AGambitGameMode::PublishMatchSetup() const
{
	if (AGambitGameState* GambitGameState = GetGameState<AGambitGameState>())
	{
		GambitGameState->SetMatchSetupConfig(PendingMatchSetup);
	}
}

void AGambitGameMode::StartMatchWithSetup(const FGambitMatchSetupConfig& MatchSetup, const bool bForceRestart)
{
	if (bMatchFlowStarted && !bForceRestart)
	{
		return;
	}

	PendingMatchSetup = BuildClampedMatchSetup(MatchSetup.LocalPlayerCount, MatchSetup.RoundCount);
	PublishMatchSetup();
	bMatchFlowStarted = true;

	if (AGambitGameState* GambitGameState = GetGameState<AGambitGameState>())
	{
		GambitGameState->SetFinalRankingSnapshot({});
		GambitGameState->SetMatchLifecycleState(EGambitMatchLifecycleState::InMatch);
	}

	if (RoundFlowComponent)
	{
		RoundFlowComponent->ApplyMatchSetup(PendingMatchSetup);
		RoundFlowComponent->StartMatchFlow();
	}
}

void AGambitGameMode::ResetMatchShellState(const EGambitMatchLifecycleState NewState)
{
	bMatchFlowStarted = false;

	if (AGambitGameState* GambitGameState = GetGameState<AGambitGameState>())
	{
		GambitGameState->SetCurrentRoundIndex(0);
		GambitGameState->SetCurrentPhase(EGambitRoundPhase::None);
		GambitGameState->SetRoundRankingSnapshot({});
		GambitGameState->SetFinalRankingSnapshot({});
		GambitGameState->SetMatchLifecycleState(NewState);
	}
}

void AGambitGameMode::SetPlayerReady(AGambitPlayerState* PlayerState, const bool bReady)
{
	if (RoundFlowComponent)
	{
		RoundFlowComponent->SetPlayerReady(PlayerState, bReady);
	}
}

bool AGambitGameMode::RequestSetDieLocked(AGambitPlayerState* PlayerState, const int32 DieIndex, const bool bLocked)
{
	return RoundFlowComponent ? RoundFlowComponent->RequestSetDieLocked(PlayerState, DieIndex, bLocked) : false;
}

bool AGambitGameMode::RequestReroll(AGambitPlayerState* PlayerState)
{
	return RoundFlowComponent ? RoundFlowComponent->RequestReroll(PlayerState) : false;
}

bool AGambitGameMode::RequestUseConsumable(AGambitPlayerState* PlayerState, const int32 SlotIndex)
{
	return RoundFlowComponent ? RoundFlowComponent->RequestUseConsumable(PlayerState, SlotIndex) : false;
}

bool AGambitGameMode::RequestUseConsumableOnTarget(
	AGambitPlayerState* PlayerState,
	const int32 SlotIndex,
	AGambitPlayerState* TargetPlayerState)
{
	return RoundFlowComponent ? RoundFlowComponent->RequestUseConsumableOnTarget(PlayerState, SlotIndex, TargetPlayerState) : false;
}

bool AGambitGameMode::RequestUseConsumableOnTargetSelectedDie(
	AGambitPlayerState* PlayerState,
	const int32 SlotIndex,
	AGambitPlayerState* TargetPlayerState,
	const int32 SelectedDieIndex)
{
	return RoundFlowComponent
		? RoundFlowComponent->RequestUseConsumableOnTargetSelectedDie(PlayerState, SlotIndex, TargetPlayerState, SelectedDieIndex)
		: false;
}

bool AGambitGameMode::BuildConsumableTargetSelectionRequest(
	AGambitPlayerState* PlayerState,
	const int32 SlotIndex,
	FGambitTargetSelectionRequest& OutRequest) const
{
	return RoundFlowComponent
		? RoundFlowComponent->BuildConsumableTargetSelectionRequest(PlayerState, SlotIndex, OutRequest)
		: false;
}

bool AGambitGameMode::RequestUseConsumableWithTargetSelectionResult(
	AGambitPlayerState* PlayerState,
	const FGambitTargetSelectionResult& TargetSelectionResult)
{
	return RoundFlowComponent
		? RoundFlowComponent->RequestUseConsumableWithTargetSelectionResult(PlayerState, TargetSelectionResult)
		: false;
}

bool AGambitGameMode::RequestPurchaseOffer(AGambitPlayerState* PlayerState, const int32 OfferId)
{
	return RoundFlowComponent ? RoundFlowComponent->RequestPurchaseOffer(PlayerState, OfferId) : false;
}

void AGambitGameMode::InitializePlayerForMatch(APlayerController* PlayerController) const
{
	if (!PlayerController)
	{
		return;
	}

	AGambitPlayerState* PlayerState = PlayerController->GetPlayerState<AGambitPlayerState>();
	if (PlayerState)
	{
		PlayerState->InitializeForMatch();
	}
}
