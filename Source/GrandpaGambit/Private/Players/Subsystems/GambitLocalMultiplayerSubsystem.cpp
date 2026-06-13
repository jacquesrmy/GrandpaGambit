#include "Players/Subsystems/GambitLocalMultiplayerSubsystem.h"

#include "Core/Logging/GambitLog.h"
#include "Core/Settings/GambitLocalMultiplayerSettings.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "Game/Camera/GambitTableCameraDirector.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "Players/Input/GambitLocalPlayerInputConfig.h"

void UGambitLocalMultiplayerSubsystem::InitializeLocalMultiplayerSession()
{
	const UGambitLocalMultiplayerSettings* Settings = UGambitLocalMultiplayerSettings::Get();
	EnsureLocalPlayerCount(Settings->DefaultLocalPlayerCount);
	RefreshLocalMultiplayerLayout();
}

UInputAction* UGambitLocalMultiplayerSubsystem::GetCoreInputAction(const EGambitCoreInputAction Action) const
{
	if (const TObjectPtr<UInputAction>* FoundAction = RuntimeCoreInputActions.Find(Action))
	{
		return FoundAction->Get();
	}

	return nullptr;
}

bool UGambitLocalMultiplayerSubsystem::RequestJoinLocalPlayer()
{
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return false;
	}

	const UGambitLocalMultiplayerSettings* Settings = UGambitLocalMultiplayerSettings::Get();
	if (GetLocalPlayerCount() >= Settings->MaxLocalPlayers)
	{
		return false;
	}

	FString OutError;
	ULocalPlayer* NewPlayer = GameInstance->CreateLocalPlayer(-1, OutError, true);
	if (!NewPlayer)
	{
		UE_LOG(LogGambit, Warning, TEXT("Local MP join failed: %s"), *OutError);
		return false;
	}

	RefreshLocalMultiplayerLayout();
	return true;
}

bool UGambitLocalMultiplayerSubsystem::RequestLeaveLocalPlayer(const int32 LocalPlayerIndex)
{
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return false;
	}

	TArray<ULocalPlayer*> LocalPlayers = GetLocalPlayers();
	if (!LocalPlayers.IsValidIndex(LocalPlayerIndex))
	{
		return false;
	}

	const UGambitLocalMultiplayerSettings* Settings = UGambitLocalMultiplayerSettings::Get();
	// Keep a valid minimum player count for session ownership and local flow.
	if (LocalPlayers.Num() <= Settings->MinLocalPlayers)
	{
		return false;
	}

	GameInstance->RemoveLocalPlayer(LocalPlayers[LocalPlayerIndex]);
	RefreshLocalMultiplayerLayout();
	return true;
}

void UGambitLocalMultiplayerSubsystem::EnsureLocalPlayerCount(const int32 DesiredPlayerCount)
{
	const UGambitLocalMultiplayerSettings* Settings = UGambitLocalMultiplayerSettings::Get();
	const int32 TargetCount = Settings->ClampLocalPlayerCount(DesiredPlayerCount);

	int32 CurrentCount = GetLocalPlayerCount();
	while (CurrentCount < TargetCount)
	{
		if (!RequestJoinLocalPlayer())
		{
			break;
		}
		CurrentCount = GetLocalPlayerCount();
	}

	while (CurrentCount > TargetCount)
	{
		if (!RequestLeaveLocalPlayer(CurrentCount - 1))
		{
			break;
		}
		CurrentCount = GetLocalPlayerCount();
	}
}

int32 UGambitLocalMultiplayerSubsystem::GetLocalPlayerCount() const
{
	return GetLocalPlayers().Num();
}

void UGambitLocalMultiplayerSubsystem::RefreshLocalMultiplayerLayout()
{
	const UGambitLocalMultiplayerSettings* Settings = UGambitLocalMultiplayerSettings::Get();
	const int32 LocalPlayerCount = GetLocalPlayerCount();
	const bool bUseSplitScreen = Settings->ShouldUseSplitScreen(LocalPlayerCount);

	if (UGameViewportClient* ViewportClient = GetViewportClient())
	{
		ViewportClient->SetForceDisableSplitscreen(!bUseSplitScreen);
	}

	if (Settings->bApplyInputMappingsOnJoinLeave)
	{
		RefreshInputMappings();
	}

	RefreshCameraTargets();
	BroadcastLocalPlayersChanged();
}

void UGambitLocalMultiplayerSubsystem::RefreshInputMappings()
{
	UGambitLocalPlayerInputConfig* InputConfig = ResolveInputConfig();
	if (!InputConfig)
	{
		return;
	}

	TArray<ULocalPlayer*> LocalPlayers = GetLocalPlayers();
	for (int32 LocalPlayerIndex = 0; LocalPlayerIndex < LocalPlayers.Num(); ++LocalPlayerIndex)
	{
		ULocalPlayer* LocalPlayer = LocalPlayers[LocalPlayerIndex];
		if (!LocalPlayer)
		{
			continue;
		}

		UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		if (!InputSubsystem)
		{
			continue;
		}

		InputSubsystem->ClearAllMappings();

		for (const FGambitInputMappingContextEntry& SharedContext : InputConfig->SharedMappingContexts)
		{
			if (SharedContext.MappingContext)
			{
				InputSubsystem->AddMappingContext(SharedContext.MappingContext, SharedContext.Priority);
			}
		}

		const FGambitIndexedInputMappingSet* PlayerMappingSet = InputConfig->FindPlayerSpecificMappingSet(LocalPlayerIndex);
		if (!PlayerMappingSet)
		{
			continue;
		}

		for (const FGambitInputMappingContextEntry& PlayerContext : PlayerMappingSet->MappingContexts)
		{
			if (PlayerContext.MappingContext)
			{
				InputSubsystem->AddMappingContext(PlayerContext.MappingContext, PlayerContext.Priority);
			}
		}
	}
}

void UGambitLocalMultiplayerSubsystem::RefreshCameraTargets()
{
	const UGambitLocalMultiplayerSettings* Settings = UGambitLocalMultiplayerSettings::Get();
	AGambitTableCameraDirector* ResolvedCameraDirector = ResolveCameraDirector();
	if (!ResolvedCameraDirector)
	{
		return;
	}

	const TArray<ULocalPlayer*> LocalPlayers = GetLocalPlayers();
	const bool bUseSplitScreen = Settings->ShouldUseSplitScreen(LocalPlayers.Num());
	ResolvedCameraDirector->ApplyCameraTargets(LocalPlayers, bUseSplitScreen, Settings->CameraBlendTime);
}

void UGambitLocalMultiplayerSubsystem::SetCameraDirector(AGambitTableCameraDirector* InCameraDirector)
{
	CameraDirector = InCameraDirector;
}

TArray<ULocalPlayer*> UGambitLocalMultiplayerSubsystem::GetLocalPlayers() const
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetLocalPlayers();
	}

	return {};
}

UGameViewportClient* UGambitLocalMultiplayerSubsystem::GetViewportClient() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetGameViewportClient() : nullptr;
}

UGambitLocalPlayerInputConfig* UGambitLocalMultiplayerSubsystem::ResolveInputConfig()
{
	const UGambitLocalMultiplayerSettings* Settings = UGambitLocalMultiplayerSettings::Get();
	if (!Settings->InputConfigAsset.IsNull())
	{
		return Settings->InputConfigAsset.LoadSynchronous();
	}

	return CreateRuntimeFallbackInputConfig();
}

AGambitTableCameraDirector* UGambitLocalMultiplayerSubsystem::ResolveCameraDirector()
{
	if (CameraDirector.IsValid())
	{
		return CameraDirector.Get();
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AGambitTableCameraDirector> It(World); It; ++It)
	{
		CameraDirector = *It;
		break;
	}

	return CameraDirector.Get();
}

void UGambitLocalMultiplayerSubsystem::BroadcastLocalPlayersChanged()
{
	OnLocalPlayersChanged.Broadcast(GetLocalPlayerCount());
}

UGambitLocalPlayerInputConfig* UGambitLocalMultiplayerSubsystem::CreateRuntimeFallbackInputConfig()
{
	if (RuntimeFallbackInputConfig)
	{
		return RuntimeFallbackInputConfig;
	}

	RuntimeFallbackInputConfig = NewObject<UGambitLocalPlayerInputConfig>(this, TEXT("RuntimeFallbackInputConfig"));
	RuntimeCoreInputActions.Reset();

	auto MakeAction = [this](const EGambitCoreInputAction Action, const TCHAR* Name) -> UInputAction*
	{
		return CreateCoreInputAction(Action, FName(Name));
	};

	UInputAction* JoinAction = MakeAction(EGambitCoreInputAction::JoinLocalPlayer, TEXT("IA_JoinLocalPlayer"));
	UInputAction* LeaveAction = MakeAction(EGambitCoreInputAction::LeaveLocalPlayer, TEXT("IA_LeaveLocalPlayer"));
	UInputAction* ReadyAction = MakeAction(EGambitCoreInputAction::Ready, TEXT("IA_Ready"));
	UInputAction* RerollAction = MakeAction(EGambitCoreInputAction::Reroll, TEXT("IA_Reroll"));
	UInputAction* Consumable1Action = MakeAction(EGambitCoreInputAction::Consumable1, TEXT("IA_Consumable1"));
	UInputAction* Consumable2Action = MakeAction(EGambitCoreInputAction::Consumable2, TEXT("IA_Consumable2"));
	UInputAction* Consumable3Action = MakeAction(EGambitCoreInputAction::Consumable3, TEXT("IA_Consumable3"));
	UInputAction* ShopOffer1Action = MakeAction(EGambitCoreInputAction::ShopOffer1, TEXT("IA_ShopOffer1"));
	UInputAction* ShopOffer2Action = MakeAction(EGambitCoreInputAction::ShopOffer2, TEXT("IA_ShopOffer2"));
	UInputAction* ShopOffer3Action = MakeAction(EGambitCoreInputAction::ShopOffer3, TEXT("IA_ShopOffer3"));
	UInputAction* LockDie1Action = MakeAction(EGambitCoreInputAction::LockDie1, TEXT("IA_LockDie1"));
	UInputAction* LockDie2Action = MakeAction(EGambitCoreInputAction::LockDie2, TEXT("IA_LockDie2"));
	UInputAction* LockDie3Action = MakeAction(EGambitCoreInputAction::LockDie3, TEXT("IA_LockDie3"));
	UInputAction* LockDie4Action = MakeAction(EGambitCoreInputAction::LockDie4, TEXT("IA_LockDie4"));
	UInputAction* LockDie5Action = MakeAction(EGambitCoreInputAction::LockDie5, TEXT("IA_LockDie5"));
	UInputAction* LockDie6Action = MakeAction(EGambitCoreInputAction::LockDie6, TEXT("IA_LockDie6"));

	// Shared controls available to any local player.
	AddMapping(RuntimeFallbackInputConfig, INDEX_NONE, JoinAction, EKeys::Enter, 100);
	AddMapping(RuntimeFallbackInputConfig, INDEX_NONE, LeaveAction, EKeys::BackSpace, 100);
	AddMapping(RuntimeFallbackInputConfig, INDEX_NONE, JoinAction, EKeys::Gamepad_Special_Right, 100);
	AddMapping(RuntimeFallbackInputConfig, INDEX_NONE, LeaveAction, EKeys::Gamepad_Special_Left, 100);

	// Player 1 keyboard layout.
	AddMapping(RuntimeFallbackInputConfig, 0, ReadyAction, EKeys::SpaceBar);
	AddMapping(RuntimeFallbackInputConfig, 0, RerollAction, EKeys::R);
	AddMapping(RuntimeFallbackInputConfig, 0, Consumable1Action, EKeys::One);
	AddMapping(RuntimeFallbackInputConfig, 0, Consumable2Action, EKeys::Two);
	AddMapping(RuntimeFallbackInputConfig, 0, Consumable3Action, EKeys::Three);
	AddMapping(RuntimeFallbackInputConfig, 0, ShopOffer1Action, EKeys::F1);
	AddMapping(RuntimeFallbackInputConfig, 0, ShopOffer2Action, EKeys::F2);
	AddMapping(RuntimeFallbackInputConfig, 0, ShopOffer3Action, EKeys::F3);
	AddMapping(RuntimeFallbackInputConfig, 0, LockDie1Action, EKeys::Q);
	AddMapping(RuntimeFallbackInputConfig, 0, LockDie2Action, EKeys::W);
	AddMapping(RuntimeFallbackInputConfig, 0, LockDie3Action, EKeys::E);
	AddMapping(RuntimeFallbackInputConfig, 0, LockDie4Action, EKeys::A);
	AddMapping(RuntimeFallbackInputConfig, 0, LockDie5Action, EKeys::S);
	AddMapping(RuntimeFallbackInputConfig, 0, LockDie6Action, EKeys::D);

	// Gamepad layout shared by players 2-4.
	for (int32 LocalPlayerIndex = 1; LocalPlayerIndex <= 3; ++LocalPlayerIndex)
	{
		AddMapping(RuntimeFallbackInputConfig, LocalPlayerIndex, ReadyAction, EKeys::Gamepad_RightThumbstick);
		AddMapping(RuntimeFallbackInputConfig, LocalPlayerIndex, RerollAction, EKeys::Gamepad_FaceButton_Bottom);
		AddMapping(RuntimeFallbackInputConfig, LocalPlayerIndex, Consumable1Action, EKeys::Gamepad_FaceButton_Left);
		AddMapping(RuntimeFallbackInputConfig, LocalPlayerIndex, Consumable2Action, EKeys::Gamepad_FaceButton_Top);
		AddMapping(RuntimeFallbackInputConfig, LocalPlayerIndex, Consumable3Action, EKeys::Gamepad_FaceButton_Right);
		AddMapping(RuntimeFallbackInputConfig, LocalPlayerIndex, ShopOffer1Action, EKeys::Gamepad_LeftTrigger);
		AddMapping(RuntimeFallbackInputConfig, LocalPlayerIndex, ShopOffer2Action, EKeys::Gamepad_RightTrigger);
		AddMapping(RuntimeFallbackInputConfig, LocalPlayerIndex, ShopOffer3Action, EKeys::Gamepad_LeftThumbstick);
		AddMapping(RuntimeFallbackInputConfig, LocalPlayerIndex, LockDie1Action, EKeys::Gamepad_DPad_Up);
		AddMapping(RuntimeFallbackInputConfig, LocalPlayerIndex, LockDie2Action, EKeys::Gamepad_DPad_Right);
		AddMapping(RuntimeFallbackInputConfig, LocalPlayerIndex, LockDie3Action, EKeys::Gamepad_DPad_Down);
		AddMapping(RuntimeFallbackInputConfig, LocalPlayerIndex, LockDie4Action, EKeys::Gamepad_DPad_Left);
		AddMapping(RuntimeFallbackInputConfig, LocalPlayerIndex, LockDie5Action, EKeys::Gamepad_LeftShoulder);
		AddMapping(RuntimeFallbackInputConfig, LocalPlayerIndex, LockDie6Action, EKeys::Gamepad_RightShoulder);
	}

	UE_LOG(LogGambit, Log, TEXT("Created runtime fallback local multiplayer input config."));
	return RuntimeFallbackInputConfig;
}

UInputAction* UGambitLocalMultiplayerSubsystem::CreateCoreInputAction(const EGambitCoreInputAction Action, const FName& ActionName)
{
	if (TObjectPtr<UInputAction>* FoundAction = RuntimeCoreInputActions.Find(Action))
	{
		return FoundAction->Get();
	}

	UInputAction* NewAction = NewObject<UInputAction>(this, ActionName);
	NewAction->ValueType = EInputActionValueType::Boolean;
	RuntimeCoreInputActions.Add(Action, NewAction);
	return NewAction;
}

void UGambitLocalMultiplayerSubsystem::AddMapping(
	UGambitLocalPlayerInputConfig* ConfigAsset,
	const int32 LocalPlayerIndex,
	UInputAction* Action,
	const FKey& Key,
	const int32 Priority)
{
	if (!ConfigAsset || !Action)
	{
		return;
	}

	FGambitInputMappingContextEntry ContextEntry;
	ContextEntry.Priority = Priority;
	ContextEntry.MappingContext = NewObject<UInputMappingContext>(ConfigAsset);
	ContextEntry.MappingContext->MapKey(Action, Key);

	if (LocalPlayerIndex == INDEX_NONE)
	{
		ConfigAsset->SharedMappingContexts.Add(ContextEntry);
		return;
	}

	FGambitIndexedInputMappingSet* MappingSet = ConfigAsset->PlayerSpecificMappingContexts.FindByPredicate(
		[LocalPlayerIndex](const FGambitIndexedInputMappingSet& Entry)
		{
			return Entry.LocalPlayerIndex == LocalPlayerIndex;
		});

	if (!MappingSet)
	{
		FGambitIndexedInputMappingSet NewSet;
		NewSet.LocalPlayerIndex = LocalPlayerIndex;
		NewSet.MappingContexts.Add(ContextEntry);
		ConfigAsset->PlayerSpecificMappingContexts.Add(NewSet);
	}
	else
	{
		MappingSet->MappingContexts.Add(ContextEntry);
	}
}
