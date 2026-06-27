#include "Game/Modes/GambitGameMode.h"

#include "Debug/GambitDevMatchSandboxComponent.h"
#include "Debug/GambitMatchDebugComponent.h"
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

	if (UGambitLocalMultiplayerSubsystem* LocalMultiplayer = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGambitLocalMultiplayerSubsystem>() : nullptr)
	{
		LocalMultiplayer->InitializeLocalMultiplayerSession();
	}

	StartMatchFlow();
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

	bMatchFlowStarted = true;

	if (RoundFlowComponent)
	{
		RoundFlowComponent->StartMatchFlow();
	}
}

void AGambitGameMode::RestartMatchFlow()
{
	bMatchFlowStarted = true;

	if (RoundFlowComponent)
	{
		RoundFlowComponent->StartMatchFlow();
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
