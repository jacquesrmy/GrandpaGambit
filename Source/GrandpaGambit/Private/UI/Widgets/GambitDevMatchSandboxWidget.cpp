#include "UI/Widgets/GambitDevMatchSandboxWidget.h"

#include "Debug/GambitDevMatchSandboxComponent.h"
#include "Engine/World.h"
#include "Game/Modes/GambitGameMode.h"

void UGambitDevMatchSandboxWidget::NativeConstruct()
{
	Super::NativeConstruct();
	InitializeSandboxWidget();
}

bool UGambitDevMatchSandboxWidget::InitializeSandboxWidget()
{
	SandboxComponent = ResolveSandboxComponent();
	return RefreshSandboxSnapshot();
}

bool UGambitDevMatchSandboxWidget::RefreshSandboxSnapshot()
{
	if (!SandboxComponent)
	{
		SandboxComponent = ResolveSandboxComponent();
	}

	if (!SandboxComponent)
	{
		CachedSnapshot = FGambitDevSandboxSnapshot();
		OnSandboxSnapshotUpdated.Broadcast(CachedSnapshot);
		return false;
	}

	CachedSnapshot = SandboxComponent->BuildSandboxSnapshot();
	OnSandboxSnapshotUpdated.Broadcast(CachedSnapshot);
	return true;
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxWidget::StartDevMatchSandbox(const int32 DesiredLocalPlayerCount)
{
	if (!SandboxComponent && !InitializeSandboxWidget())
	{
		return MakeMissingSandboxResult(TEXT("Start dev match failed: missing sandbox component"));
	}

	return RefreshAfterCommand(SandboxComponent->StartDevMatchSandbox(DesiredLocalPlayerCount));
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxWidget::SetInspectedPlayerIndex(const int32 PlayerIndex)
{
	if (!SandboxComponent && !InitializeSandboxWidget())
	{
		return MakeMissingSandboxResult(TEXT("Inspect player failed: missing sandbox component"));
	}

	return RefreshAfterCommand(SandboxComponent->SetInspectedPlayerIndex(PlayerIndex));
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxWidget::SetPlayerControlMode(
	const int32 PlayerIndex,
	const EGambitDevSandboxPlayerControlMode ControlMode)
{
	if (!SandboxComponent && !InitializeSandboxWidget())
	{
		return MakeMissingSandboxResult(TEXT("Set control mode failed: missing sandbox component"));
	}

	return RefreshAfterCommand(SandboxComponent->SetPlayerControlMode(PlayerIndex, ControlMode));
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxWidget::RequestPlayerReady(const int32 PlayerIndex, const bool bReady)
{
	if (!SandboxComponent && !InitializeSandboxWidget())
	{
		return MakeMissingSandboxResult(TEXT("Ready failed: missing sandbox component"));
	}

	return RefreshAfterCommand(SandboxComponent->RequestPlayerReady(PlayerIndex, bReady));
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxWidget::RequestToggleDieLock(const int32 PlayerIndex, const int32 DieIndex)
{
	if (!SandboxComponent && !InitializeSandboxWidget())
	{
		return MakeMissingSandboxResult(TEXT("Toggle die lock failed: missing sandbox component"));
	}

	return RefreshAfterCommand(SandboxComponent->RequestToggleDieLock(PlayerIndex, DieIndex));
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxWidget::RequestReroll(const int32 PlayerIndex)
{
	if (!SandboxComponent && !InitializeSandboxWidget())
	{
		return MakeMissingSandboxResult(TEXT("Reroll failed: missing sandbox component"));
	}

	return RefreshAfterCommand(SandboxComponent->RequestReroll(PlayerIndex));
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxWidget::RequestUseConsumable(const int32 PlayerIndex, const int32 SlotIndex)
{
	if (!SandboxComponent && !InitializeSandboxWidget())
	{
		return MakeMissingSandboxResult(TEXT("Use consumable failed: missing sandbox component"));
	}

	return RefreshAfterCommand(SandboxComponent->RequestUseConsumable(PlayerIndex, SlotIndex));
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxWidget::RequestUseConsumableOnDie(
	const int32 PlayerIndex,
	const int32 SlotIndex,
	const int32 DieIndex)
{
	if (!SandboxComponent && !InitializeSandboxWidget())
	{
		return MakeMissingSandboxResult(TEXT("Use consumable on die failed: missing sandbox component"));
	}

	return RefreshAfterCommand(SandboxComponent->RequestUseConsumableOnDie(PlayerIndex, SlotIndex, DieIndex));
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxWidget::RequestBeginConsumableTargetSelection(
	const int32 PlayerIndex,
	const int32 SlotIndex)
{
	if (!SandboxComponent && !InitializeSandboxWidget())
	{
		return MakeMissingSandboxResult(TEXT("Begin target selection failed: missing sandbox component"));
	}

	return RefreshAfterCommand(SandboxComponent->RequestBeginConsumableTargetSelection(PlayerIndex, SlotIndex));
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxWidget::RequestSelectTargetSelectionOption(
	const int32 PlayerIndex,
	const int32 OptionId)
{
	if (!SandboxComponent && !InitializeSandboxWidget())
	{
		return MakeMissingSandboxResult(TEXT("Select target option failed: missing sandbox component"));
	}

	return RefreshAfterCommand(SandboxComponent->RequestSelectTargetSelectionOption(PlayerIndex, OptionId));
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxWidget::RequestConfirmTargetSelection(const int32 PlayerIndex)
{
	if (!SandboxComponent && !InitializeSandboxWidget())
	{
		return MakeMissingSandboxResult(TEXT("Confirm target selection failed: missing sandbox component"));
	}

	return RefreshAfterCommand(SandboxComponent->RequestConfirmTargetSelection(PlayerIndex));
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxWidget::RequestCancelTargetSelection(const int32 PlayerIndex)
{
	if (!SandboxComponent && !InitializeSandboxWidget())
	{
		return MakeMissingSandboxResult(TEXT("Cancel target selection failed: missing sandbox component"));
	}

	return RefreshAfterCommand(SandboxComponent->RequestCancelTargetSelection(PlayerIndex));
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxWidget::RequestPurchaseOffer(const int32 PlayerIndex, const int32 OfferId)
{
	if (!SandboxComponent && !InitializeSandboxWidget())
	{
		return MakeMissingSandboxResult(TEXT("Purchase offer failed: missing sandbox component"));
	}

	return RefreshAfterCommand(SandboxComponent->RequestPurchaseOffer(PlayerIndex, OfferId));
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxWidget::RequestSkipShop(const int32 PlayerIndex)
{
	if (!SandboxComponent && !InitializeSandboxWidget())
	{
		return MakeMissingSandboxResult(TEXT("Skip shop failed: missing sandbox component"));
	}

	return RefreshAfterCommand(SandboxComponent->RequestSkipShop(PlayerIndex));
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxWidget::RequestAdvanceCurrentPhase()
{
	if (!SandboxComponent && !InitializeSandboxWidget())
	{
		return MakeMissingSandboxResult(TEXT("Advance phase failed: missing sandbox component"));
	}

	return RefreshAfterCommand(SandboxComponent->RequestAdvanceCurrentPhase());
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxWidget::RequestAIDecisionForAIPlayers()
{
	if (!SandboxComponent && !InitializeSandboxWidget())
	{
		return MakeMissingSandboxResult(TEXT("AI decision failed: missing sandbox component"));
	}

	return RefreshAfterCommand(SandboxComponent->RequestAIDecisionForAIPlayers());
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxWidget::RequestAIDecisionForAllPlayers()
{
	if (!SandboxComponent && !InitializeSandboxWidget())
	{
		return MakeMissingSandboxResult(TEXT("AI decision failed: missing sandbox component"));
	}

	return RefreshAfterCommand(SandboxComponent->RequestAIDecisionForAllPlayers());
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxWidget::RequestAIShopDecisionForPlayer(const int32 PlayerIndex)
{
	if (!SandboxComponent && !InitializeSandboxWidget())
	{
		return MakeMissingSandboxResult(TEXT("AI shop decision failed: missing sandbox component"));
	}

	return RefreshAfterCommand(SandboxComponent->RequestAIShopDecisionForPlayer(PlayerIndex));
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxWidget::RequestAIShopDecisionForAIPlayers()
{
	if (!SandboxComponent && !InitializeSandboxWidget())
	{
		return MakeMissingSandboxResult(TEXT("AI shop decision failed: missing sandbox component"));
	}

	return RefreshAfterCommand(SandboxComponent->RequestAIShopDecisionForAIPlayers());
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxWidget::RequestAIShopDecisionForAllPlayers()
{
	if (!SandboxComponent && !InitializeSandboxWidget())
	{
		return MakeMissingSandboxResult(TEXT("AI shop decision failed: missing sandbox component"));
	}

	return RefreshAfterCommand(SandboxComponent->RequestAIShopDecisionForAllPlayers());
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxWidget::RefreshAfterCommand(const FGambitDevSandboxActionResult& Result)
{
	RefreshSandboxSnapshot();
	return Result;
}

FGambitDevSandboxActionResult UGambitDevMatchSandboxWidget::MakeMissingSandboxResult(const FString& Message) const
{
	FGambitDevSandboxActionResult Result;
	Result.bSuccess = false;
	Result.Status = EGambitDevSandboxActionStatus::MissingGameMode;
	Result.Message = Message;
	return Result;
}

UGambitDevMatchSandboxComponent* UGambitDevMatchSandboxWidget::ResolveSandboxComponent() const
{
#if !UE_BUILD_SHIPPING
	const UWorld* World = GetWorld();
	AGambitGameMode* GameMode = World ? World->GetAuthGameMode<AGambitGameMode>() : nullptr;
	return GameMode ? GameMode->GetDevMatchSandboxComponent() : nullptr;
#else
	return nullptr;
#endif
}
