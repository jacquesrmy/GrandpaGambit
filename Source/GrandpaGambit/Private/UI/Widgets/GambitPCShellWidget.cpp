#include "UI/Widgets/GambitPCShellWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/Types/GambitDebugTypes.h"
#include "Core/Types/GambitRoundGameplayEventTypes.h"
#include "Core/Types/GambitTargetSelectionTypes.h"
#include "Dice/Data/GambitDiceDefinition.h"
#include "Engine/World.h"
#include "Game/Flow/GambitRoundFlowComponent.h"
#include "Game/Modes/GambitGameMode.h"
#include "Game/States/GambitGameState.h"
#include "Items/Data/GambitItemDefinition.h"
#include "Items/Consumables/GambitConsumableDefinition.h"
#include "Players/Components/GambitInventoryComponent.h"
#include "Players/Controllers/GambitPlayerController.h"
#include "Players/States/GambitPlayerState.h"

namespace
{
	FString ShellEnumToString(const UEnum* Enum, const int64 Value)
	{
		return Enum ? Enum->GetDisplayNameTextByValue(Value).ToString() : TEXT("Unknown");
	}

	FString ShellLifecycleToString(const EGambitMatchLifecycleState State)
	{
		return ShellEnumToString(StaticEnum<EGambitMatchLifecycleState>(), static_cast<int64>(State));
	}

	FString ShellPhaseToString(const EGambitRoundPhase Phase)
	{
		return ShellEnumToString(StaticEnum<EGambitRoundPhase>(), static_cast<int64>(Phase));
	}

	FString ShellCombinationToString(const EGambitCombinationType Combination)
	{
		return ShellEnumToString(StaticEnum<EGambitCombinationType>(), static_cast<int64>(Combination));
	}

	FString ShellTargetSelectionTypeToString(const EGambitTargetSelectionType SelectionType)
	{
		return ShellEnumToString(StaticEnum<EGambitTargetSelectionType>(), static_cast<int64>(SelectionType));
	}

	FString ShellRoundEventTypeToString(const EGambitRoundGameplayEventType EventType)
	{
		if (const UEnum* Enum = StaticEnum<EGambitRoundGameplayEventType>())
		{
			return Enum->GetNameStringByValue(static_cast<int64>(EventType));
		}

		return TEXT("Unknown");
	}

	FString ShellFormatSignedInt(const int32 Value)
	{
		return FString::Printf(TEXT("%+d"), Value);
	}

	FString ShellFormatSignedFloat(const float Value)
	{
		return FString::Printf(TEXT("%+0.2f"), Value);
	}

	FString ShellJoinNames(const TArray<FName>& Names)
	{
		TArray<FString> Parts;
		Parts.Reserve(Names.Num());
		for (const FName Name : Names)
		{
			if (!Name.IsNone())
			{
				Parts.Add(Name.ToString());
			}
		}

		return FString::Join(Parts, TEXT(", "));
	}

	FString ShellItemDisplayName(const UGambitItemDefinition* ItemDefinition)
	{
		if (!ItemDefinition)
		{
			return TEXT("None");
		}

		return ItemDefinition->DisplayName.IsEmpty()
			? ItemDefinition->GetResolvedItemId().ToString()
			: ItemDefinition->DisplayName.ToString();
	}

	bool ShellIsReadyGatedPhase(const EGambitRoundPhase Phase)
	{
		return Phase == EGambitRoundPhase::SelectionReroll
			|| Phase == EGambitRoundPhase::Action
			|| Phase == EGambitRoundPhase::Shop;
	}

	bool ShellIsImportantLedgerEvent(const EGambitRoundGameplayEventType EventType)
	{
		switch (EventType)
		{
		case EGambitRoundGameplayEventType::EffectApplied:
		case EGambitRoundGameplayEventType::EffectPrevented:
		case EGambitRoundGameplayEventType::ConsumableUsed:
		case EGambitRoundGameplayEventType::GoldChanged:
		case EGambitRoundGameplayEventType::ScoreModifierApplied:
		case EGambitRoundGameplayEventType::DieModified:
		case EGambitRoundGameplayEventType::DieDestroyedOrRemoved:
		case EGambitRoundGameplayEventType::TargetSelectionRequested:
		case EGambitRoundGameplayEventType::TargetSelectionCancelled:
		case EGambitRoundGameplayEventType::TargetSelectionInvalid:
			return true;
		default:
			return false;
		}
	}

	bool ShellIsConsumableUsableInPhase(const FGambitConsumableRuntimeSlot& Slot, const EGambitRoundPhase Phase)
	{
		return Slot.Definition && Slot.Definition->CanBeUsedDuringPhase(Phase);
	}

	FString ShellFormatConsumableLine(
		const AGambitPlayerState* PlayerState,
		const FGambitConsumableRuntimeSlot& Slot,
		const int32 SlotIndex,
		const EGambitRoundPhase Phase)
	{
		const UGambitConsumableDefinition* Definition = Slot.Definition.Get();
		const FName ItemId = Definition ? Definition->GetResolvedItemId() : NAME_None;
		const FString ItemName = Definition
			? (Definition->DisplayName.IsEmpty() ? Definition->GetResolvedItemId().ToString() : Definition->DisplayName.ToString())
			: FString(TEXT("None"));
		const FGambitInventoryItemInstance* Instance = nullptr;
		if (const UGambitInventoryComponent* InventoryComponent = PlayerState ? PlayerState->GetInventoryComponent() : nullptr)
		{
			Instance = InventoryComponent->FindItemInstanceById(Slot.InventoryInstanceId);
		}

		const int32 StackCount = Instance ? FMath::Max(1, Instance->StackCount) : 1;
		const bool bUsable = ShellIsConsumableUsableInPhase(Slot, Phase);
		const FString StateText = bUsable ? TEXT("Ready") : TEXT("Unavailable in phase");
		return FString::Printf(
			TEXT("  Slot %d: %s [%s] | Stack %d | Uses 1 | %s"),
			SlotIndex + 1,
			*ItemName,
			*ItemId.ToString(),
			StackCount,
			*StateText);
	}

	FString ShellFormatDebugScoreLine(const FGambitDebugScoreLine& Line)
	{
		const FString SourcePrefix = Line.SourceName.IsEmpty()
			? FString()
			: FString::Printf(TEXT("%s: "), *Line.SourceName);
		if (!Line.Summary.IsEmpty())
		{
			return FString::Printf(TEXT("  - %s%s"), *SourcePrefix, *Line.Summary);
		}

		return FString::Printf(
			TEXT("  - %s%s | Add %s | Dice %s | Mult x%0.2f | %.2f -> %.2f"),
			*SourcePrefix,
			*Line.Label,
			*ShellFormatSignedFloat(Line.AdditiveDelta),
			*ShellFormatSignedFloat(Line.DiceContributionDelta),
			Line.MultiplierValue,
			Line.ScoreBefore,
			Line.ScoreAfter);
	}

	FString ShellFormatDebugGoldLine(const FGambitDebugGoldLine& Line)
	{
		if (!Line.Summary.IsEmpty())
		{
			return FString::Printf(
				TEXT("  - %s | Actual %s | Gold %d -> %d%s"),
				*Line.Summary,
				*ShellFormatSignedInt(Line.ActualDelta),
				Line.GoldBefore,
				Line.GoldAfter,
				Line.bClamped ? TEXT(" | clamped") : TEXT(""));
		}

		return FString::Printf(
			TEXT("  - Gold %s | Requested %s | Gold %d -> %d%s"),
			*ShellFormatSignedInt(Line.ActualDelta),
			*ShellFormatSignedInt(Line.RequestedDelta),
			Line.GoldBefore,
			Line.GoldAfter,
			Line.bClamped ? TEXT(" | clamped") : TEXT(""));
	}

	FString ShellFormatLedgerEvent(const FGambitRoundGameplayEvent& Event)
	{
		FString Details = Event.Reason;
		if (Details.IsEmpty())
		{
			Details = !Event.EffectId.IsNone() ? Event.EffectId.ToString() : Event.EffectTypeId.ToString();
		}
		if (Details.IsEmpty())
		{
			Details = TEXT("No details");
		}

		FString TargetPart;
		if (Event.SourcePlayerId != INDEX_NONE || Event.TargetPlayerId != INDEX_NONE)
		{
			TargetPart = FString::Printf(
				TEXT(" | P%d -> P%d"),
				Event.SourcePlayerId,
				Event.TargetPlayerId);
		}

		FString DeltaPart;
		if (!FMath::IsNearlyZero(Event.NumericDelta))
		{
			DeltaPart = FString::Printf(TEXT(" | Delta %s"), *ShellFormatSignedFloat(Event.NumericDelta));
		}

		FString DiePart;
		if (Event.TargetDieHandIndex != INDEX_NONE)
		{
			DiePart = FString::Printf(TEXT(" | Die %d"), Event.TargetDieHandIndex + 1);
		}

		FString CategoryPart;
		if (Event.NegativeCategoryIds.Num() > 0)
		{
			CategoryPart = FString::Printf(TEXT(" | Categories %s"), *ShellJoinNames(Event.NegativeCategoryIds));
		}

		return FString::Printf(
			TEXT("  - %s: %s%s%s%s%s"),
			*ShellRoundEventTypeToString(Event.EventType),
			*Details,
			*TargetPart,
			*DeltaPart,
			*DiePart,
			*CategoryPart);
	}

	FString ShellResolveRankingPlayerName(const APlayerState* PlayerState, const int32 FallbackIndex)
	{
		if (PlayerState && !PlayerState->GetPlayerName().IsEmpty())
		{
			return PlayerState->GetPlayerName();
		}

		return FString::Printf(TEXT("Player %d"), FallbackIndex + 1);
	}

	int32 ShellFindVictoryPointsGrantedForPlayer(
		const AGambitGameState* GameState,
		const AGambitPlayerState* PlayerState)
	{
		if (!GameState || !PlayerState)
		{
			return INDEX_NONE;
		}

		for (const FGambitRankingEntry& Entry : GameState->GetRoundRankingSnapshotRef())
		{
			if (Entry.PlayerState == PlayerState)
			{
				return Entry.VictoryPointsGranted;
			}
		}

		return INDEX_NONE;
	}
}

void UGambitPCShellActionButton::ConfigureAction(
	UGambitPCShellWidget* InOwnerShell,
	const EGambitPCShellActionKind InActionKind,
	AGambitPlayerState* InPlayerState,
	const int32 InPlayerIndex,
	const int32 InActionIndex)
{
	OwnerShell = InOwnerShell;
	ActionKind = InActionKind;
	PlayerState = InPlayerState;
	PlayerIndex = InPlayerIndex;
	ActionIndex = InActionIndex;
	OnClicked.AddUniqueDynamic(this, &UGambitPCShellActionButton::HandleClicked);
}

void UGambitPCShellActionButton::HandleClicked()
{
	if (UGambitPCShellWidget* Shell = OwnerShell.Get())
	{
		Shell->ExecutePlayerAction(ActionKind, PlayerState.Get(), PlayerIndex, ActionIndex);
	}
}

void UGambitPCShellWidget::NativeConstruct()
{
	Super::NativeConstruct();
	InitializeShellWidget();
}

void UGambitPCShellWidget::NativeDestruct()
{
	UnbindGameState();
	Super::NativeDestruct();
}

void UGambitPCShellWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (CachedLifecycleState != EGambitMatchLifecycleState::InMatch)
	{
		return;
	}

	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator >= 0.35f)
	{
		RefreshAccumulator = 0.0f;
		RefreshShell();
	}
}

bool UGambitPCShellWidget::InitializeShellWidget()
{
	if (!WidgetTree)
	{
		return false;
	}

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PCShellBackground"));
	Background->SetBrushColor(FLinearColor(0.015f, 0.015f, 0.015f, 0.94f));
	Background->SetPadding(FMargin(32.0f, 28.0f));

	UScrollBox* ScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("PCShellScroll"));
	RootContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PCShellRoot"));
	ScrollBox->AddChild(RootContent);
	Background->SetContent(ScrollBox);
	WidgetTree->RootWidget = Background;

	BindGameState(GetGambitGameState());
	SyncCachedState();
	RebuildShell();
	return true;
}

void UGambitPCShellWidget::RefreshShell()
{
	if (!RootContent)
	{
		InitializeShellWidget();
		return;
	}

	BindGameState(GetGambitGameState());
	SyncCachedState();
	RebuildShell();
}

void UGambitPCShellWidget::HandleMatchLifecycleChanged(
	const EGambitMatchLifecycleState OldState,
	const EGambitMatchLifecycleState NewState)
{
	(void)OldState;
	LastActionFeedback.Reset();
	CachedLifecycleState = NewState;
	RefreshShell();
}

void UGambitPCShellWidget::HandleMatchSetupChanged(const FGambitMatchSetupConfig NewSetup)
{
	CachedMatchSetup = NewSetup;
	RefreshShell();
}

void UGambitPCShellWidget::HandlePhaseChanged(const EGambitRoundPhase OldPhase, const EGambitRoundPhase NewPhase)
{
	(void)OldPhase;
	LastActionFeedback.Reset();
	CachedPhase = NewPhase;
	RefreshShell();
}

void UGambitPCShellWidget::HandleRoundChanged(const int32 NewRoundIndex)
{
	CachedRoundIndex = NewRoundIndex;
	RefreshShell();
}

void UGambitPCShellWidget::HandleRankingUpdated()
{
	RefreshShell();
}

void UGambitPCShellWidget::HandleNewMatchClicked()
{
	if (AGambitGameMode* GameMode = GetGambitGameMode())
	{
		GameMode->RequestOpenMatchSetup();
	}
	RefreshShell();
}

void UGambitPCShellWidget::HandleBackToMainMenuClicked()
{
	if (AGambitGameMode* GameMode = GetGambitGameMode())
	{
		GameMode->RequestMainMenu();
	}
	RefreshShell();
}

void UGambitPCShellWidget::HandlePlayerCount2Clicked()
{
	ConfigureMatch(2, CachedMatchSetup.RoundCount);
}

void UGambitPCShellWidget::HandlePlayerCount3Clicked()
{
	ConfigureMatch(3, CachedMatchSetup.RoundCount);
}

void UGambitPCShellWidget::HandlePlayerCount4Clicked()
{
	ConfigureMatch(4, CachedMatchSetup.RoundCount);
}

void UGambitPCShellWidget::HandleRoundCountMinusClicked()
{
	ConfigureMatch(CachedMatchSetup.LocalPlayerCount, FMath::Max(1, CachedMatchSetup.RoundCount - 1));
}

void UGambitPCShellWidget::HandleRoundCountPlusClicked()
{
	ConfigureMatch(CachedMatchSetup.LocalPlayerCount, CachedMatchSetup.RoundCount + 1);
}

void UGambitPCShellWidget::HandleEnterLobbyClicked()
{
	if (AGambitGameMode* GameMode = GetGambitGameMode())
	{
		GameMode->RequestEnterLobby();
	}
	RefreshShell();
}

void UGambitPCShellWidget::HandleStartMatchClicked()
{
	if (AGambitGameMode* GameMode = GetGambitGameMode())
	{
		GameMode->RequestStartConfiguredMatch();
	}
	RefreshShell();
}

void UGambitPCShellWidget::HandleReadyAllClicked()
{
	if (AGambitGameMode* GameMode = GetGambitGameMode())
	{
		GameMode->RequestReadyAllPlayersForCurrentPhase();
	}
	RefreshShell();
}

void UGambitPCShellWidget::ExecutePlayerAction(
	const EGambitPCShellActionKind ActionKind,
	AGambitPlayerState* PlayerState,
	const int32 PlayerIndex,
	const int32 ActionIndex)
{
	FGambitRoundCommandResult Result;
	Result.Phase = CachedPhase;
	Result.PlayerIndex = PlayerIndex;
	Result.DieIndex = ActionIndex;

	AGambitGameMode* GameMode = GetGambitGameMode();
	if (!GameMode)
	{
		Result.Status = EGambitRoundCommandStatus::Failed;
		Result.Message = TEXT("Action refused: missing GameMode.");
		SetLastActionFeedback(Result);
		RefreshShell();
		return;
	}

	switch (ActionKind)
	{
	case EGambitPCShellActionKind::ToggleDieLock:
	{
		bool bTargetLocked = true;
		if (PlayerState)
		{
			const TArray<FGambitDieRuntimeState>& DiceStates = PlayerState->GetDiceStatesRef();
			if (DiceStates.IsValidIndex(ActionIndex))
			{
				bTargetLocked = !DiceStates[ActionIndex].bLocked;
			}
		}
		Result = GameMode->RequestSetDieLockedDetailed(PlayerState, ActionIndex, bTargetLocked);
		break;
	}
	case EGambitPCShellActionKind::Reroll:
		Result = GameMode->RequestRerollDetailed(PlayerState);
		break;
	case EGambitPCShellActionKind::UseConsumable:
	{
		AGambitPlayerController* PlayerController = ResolvePlayerControllerForPlayerState(PlayerState);
		if (!PlayerController)
		{
			SetLastActionFeedbackMessage(false, TEXT("Consumable use refused: missing player controller."));
			RefreshShell();
			return;
		}

		const bool bSuccess = PlayerController->RequestUseConsumable(ActionIndex);
		if (PlayerController->HasPendingTargetSelection())
		{
			const FGambitTargetSelectionRequest Request = PlayerController->GetPendingTargetSelectionRequest();
			const bool bHasValidOptions = Request.HasValidOptions();
			SetLastActionFeedbackMessage(
				bSuccess && bHasValidOptions,
				bHasValidOptions
					? FString::Printf(TEXT("Target selection opened for slot %d."), ActionIndex + 1)
					: FString::Printf(
						TEXT("Target selection has no valid option: %s"),
						Request.InvalidReason.IsEmpty() ? TEXT("unknown reason") : *Request.InvalidReason));
		}
		else
		{
			SetLastActionFeedbackMessage(
				bSuccess,
				bSuccess
					? FString::Printf(TEXT("Consumable slot %d used."), ActionIndex + 1)
					: FString::Printf(TEXT("Consumable slot %d refused."), ActionIndex + 1));
		}
		RefreshShell();
		return;
	}
	case EGambitPCShellActionKind::SelectTargetSelectionOption:
	{
		AGambitPlayerController* PlayerController = ResolvePlayerControllerForPlayerState(PlayerState);
		const bool bSuccess = PlayerController && PlayerController->RequestSelectTargetSelectionOption(ActionIndex);
		SetLastActionFeedbackMessage(
			bSuccess,
			bSuccess
				? FString::Printf(TEXT("Target option %d selected."), ActionIndex)
				: FString::Printf(TEXT("Target option %d refused."), ActionIndex));
		RefreshShell();
		return;
	}
	case EGambitPCShellActionKind::ConfirmTargetSelection:
	{
		AGambitPlayerController* PlayerController = ResolvePlayerControllerForPlayerState(PlayerState);
		const bool bSuccess = PlayerController && PlayerController->RequestConfirmTargetSelection();
		SetLastActionFeedbackMessage(
			bSuccess,
			bSuccess ? TEXT("Target confirmed.") : TEXT("Target confirmation refused."));
		RefreshShell();
		return;
	}
	case EGambitPCShellActionKind::CancelTargetSelection:
	{
		AGambitPlayerController* PlayerController = ResolvePlayerControllerForPlayerState(PlayerState);
		const bool bSuccess = PlayerController && PlayerController->RequestCancelTargetSelection();
		SetLastActionFeedbackMessage(
			bSuccess,
			bSuccess ? TEXT("Target selection cancelled.") : TEXT("Target cancellation refused."));
		RefreshShell();
		return;
	}
	default:
		Result.Status = EGambitRoundCommandStatus::Failed;
		Result.Message = TEXT("Action refused: unsupported shell command.");
		break;
	}

	SetLastActionFeedback(Result);
	RefreshShell();
}

TArray<FString> UGambitPCShellWidget::BuildConsumableFeedbackLines(
	const AGambitPlayerState* PlayerState,
	const EGambitRoundPhase CurrentPhase)
{
	TArray<FString> Lines;
	if (!PlayerState)
	{
		return Lines;
	}

	const FGambitPlayerSlotState SlotState = PlayerState->GetSlotState();
	Lines.Add(FString::Printf(
		TEXT("Consumables: %d/%d"),
		SlotState.ConsumableSlotsUsed,
		SlotState.ConsumableSlotsCapacity));

	const TArray<FGambitConsumableRuntimeSlot>& ConsumableSlots = PlayerState->GetConsumableSlotsRef();
	if (ConsumableSlots.Num() == 0)
	{
		Lines.Add(TEXT("  None"));
		return Lines;
	}

	for (int32 SlotIndex = 0; SlotIndex < ConsumableSlots.Num(); ++SlotIndex)
	{
		Lines.Add(ShellFormatConsumableLine(PlayerState, ConsumableSlots[SlotIndex], SlotIndex, CurrentPhase));
	}

	return Lines;
}

TArray<FString> UGambitPCShellWidget::BuildScoreFeedbackLines(const AGambitPlayerState* PlayerState)
{
	TArray<FString> Lines;
	if (!PlayerState || !PlayerState->HasEventThisRound(EGambitRoundGameplayEventType::RoundScored))
	{
		return Lines;
	}

	const FGambitScoreBreakdown Breakdown = PlayerState->GetLastScoreBreakdown();
	Lines.Add(FString::Printf(
		TEXT("Resolution: Combination %s | Raw %d | Final %d"),
		*ShellCombinationToString(Breakdown.Combination),
		Breakdown.RawScore,
		Breakdown.FinalScore));
	Lines.Add(FString::Printf(
		TEXT("Score totals: Base %d | Dice %d | Dice Bonus %s | Add %s | Mult x%0.2f | Before Cap %0.2f | After Cap %0.2f"),
		Breakdown.BaseCombinationScore,
		Breakdown.DiceSum,
		*ShellFormatSignedFloat(Breakdown.DiceContributionMultiplierBonus),
		*ShellFormatSignedFloat(Breakdown.AdditiveBonus),
		Breakdown.Multiplier,
		Breakdown.ScoreBeforeCap,
		Breakdown.ScoreAfterCap));

	const TArray<FGambitDebugScoreLine> ScoreLines = PlayerState->GetDebugScoreLines();
	if (ScoreLines.Num() > 0)
	{
		Lines.Add(TEXT("Scoring breakdown:"));
		for (const FGambitDebugScoreLine& Line : ScoreLines)
		{
			Lines.Add(ShellFormatDebugScoreLine(Line));
		}
	}

	return Lines;
}

TArray<FString> UGambitPCShellWidget::BuildRewardFeedbackLines(
	const AGambitPlayerState* PlayerState,
	const int32 VictoryPointsGranted)
{
	TArray<FString> Lines;
	if (!PlayerState)
	{
		return Lines;
	}

	const TArray<FGambitDebugGoldLine> GoldLines = PlayerState->GetDebugGoldLines();
	if (GoldLines.Num() == 0 && VictoryPointsGranted == INDEX_NONE)
	{
		return Lines;
	}

	int32 TotalGoldDelta = 0;
	for (const FGambitDebugGoldLine& Line : GoldLines)
	{
		TotalGoldDelta += Line.ActualDelta;
	}

	const FString VictoryPointPart = VictoryPointsGranted == INDEX_NONE
		? FString(TEXT(" | VP pending"))
		: FString::Printf(TEXT(" | VP %s"), *ShellFormatSignedInt(VictoryPointsGranted));
	Lines.Add(FString::Printf(
		TEXT("Rewards: Gold %s%s | Current Gold %d"),
		*ShellFormatSignedInt(TotalGoldDelta),
		*VictoryPointPart,
		PlayerState->GetCurrentGold()));

	if (GoldLines.Num() > 0)
	{
		Lines.Add(TEXT("Gold breakdown:"));
		for (const FGambitDebugGoldLine& Line : GoldLines)
		{
			Lines.Add(ShellFormatDebugGoldLine(Line));
		}
	}

	return Lines;
}

TArray<FString> UGambitPCShellWidget::BuildRankingFeedbackLines(const AGambitGameState* GameState)
{
	TArray<FString> Lines;
	if (!GameState || GameState->GetRoundRankingSnapshotRef().Num() == 0)
	{
		return Lines;
	}

	Lines.Add(TEXT("Round Ranking:"));
	const TArray<FGambitRankingEntry>& Ranking = GameState->GetRoundRankingSnapshotRef();
	for (int32 Index = 0; Index < Ranking.Num(); ++Index)
	{
		const FGambitRankingEntry& Entry = Ranking[Index];
		const AGambitPlayerState* RankedPlayer = Cast<AGambitPlayerState>(Entry.PlayerState);
		const int32 TotalVictoryPoints = RankedPlayer ? RankedPlayer->GetTotalVictoryPoints() : 0;
		Lines.Add(FString::Printf(
			TEXT("#%d %s | Score %d | VP %s | Total VP %d"),
			Entry.Rank,
			*ShellResolveRankingPlayerName(Entry.PlayerState, Index),
			Entry.RoundScore,
			*ShellFormatSignedInt(Entry.VictoryPointsGranted),
			TotalVictoryPoints));
	}

	return Lines;
}

TArray<FString> UGambitPCShellWidget::BuildLedgerFeedbackLines(
	const AGambitPlayerState* PlayerState,
	const int32 MaxEvents)
{
	TArray<FString> Lines;
	if (!PlayerState || MaxEvents <= 0)
	{
		return Lines;
	}

	TArray<const FGambitRoundGameplayEvent*> ImportantEvents;
	for (const FGambitRoundGameplayEvent& Event : PlayerState->GetRoundEventsRef())
	{
		if (ShellIsImportantLedgerEvent(Event.EventType))
		{
			ImportantEvents.Add(&Event);
		}
	}

	if (ImportantEvents.Num() == 0)
	{
		return Lines;
	}

	Lines.Add(TEXT("Ledger:"));
	const int32 StartIndex = FMath::Max(0, ImportantEvents.Num() - MaxEvents);
	for (int32 Index = StartIndex; Index < ImportantEvents.Num(); ++Index)
	{
		Lines.Add(ShellFormatLedgerEvent(*ImportantEvents[Index]));
	}

	return Lines;
}

TArray<FString> UGambitPCShellWidget::BuildTargetSelectionFeedbackLines(const AGambitPlayerController* PlayerController)
{
	TArray<FString> Lines;
	if (!PlayerController || !PlayerController->HasPendingTargetSelection())
	{
		return Lines;
	}

	const FGambitTargetSelectionRequest Request = PlayerController->GetPendingTargetSelectionRequest();
	const int32 SelectedOptionId = PlayerController->GetPendingTargetSelectionSelectedOptionId();
	const FString SourceName = ShellItemDisplayName(Request.SourceItemDefinition);
	const FString RequestSummary = Request.DebugText.IsEmpty()
		? FString::Printf(TEXT("%s requires target selection."), *SourceName)
		: Request.DebugText;

	Lines.Add(FString::Printf(
		TEXT("Target Selection: %s | Type %s | Selected %s"),
		*RequestSummary,
		*ShellTargetSelectionTypeToString(Request.SelectionType),
		SelectedOptionId == INDEX_NONE ? TEXT("None") : *FString::FromInt(SelectedOptionId)));

	if (!Request.InvalidReason.IsEmpty())
	{
		Lines.Add(FString::Printf(TEXT("Target Selection Invalid: %s"), *Request.InvalidReason));
	}

	if (Request.Options.Num() == 0)
	{
		Lines.Add(TEXT("  No target options."));
		return Lines;
	}

	for (const FGambitTargetSelectionOption& Option : Request.Options)
	{
		const FString SelectedPrefix = Option.OptionId == SelectedOptionId ? TEXT("[Selected] ") : TEXT("");
		const FString ValidText = Option.bValid ? TEXT("Valid") : TEXT("Invalid");
		FString Line = FString::Printf(
			TEXT("  %sOption %d: %s | %s"),
			*SelectedPrefix,
			Option.OptionId,
			*Option.Label,
			*ValidText);
		if (!Option.TargetRuleId.IsNone())
		{
			Line += FString::Printf(TEXT(" | Rule %s"), *Option.TargetRuleId.ToString());
		}
		if (Option.TargetDieHandIndex != INDEX_NONE)
		{
			Line += FString::Printf(TEXT(" | Die %d"), Option.TargetDieHandIndex + 1);
		}
		if (!Option.DebugText.IsEmpty())
		{
			Line += FString::Printf(TEXT(" | %s"), *Option.DebugText);
		}
		Lines.Add(Line);
	}

	return Lines;
}

void UGambitPCShellWidget::BindGameState(AGambitGameState* GameState)
{
	if (BoundGameState == GameState)
	{
		return;
	}

	UnbindGameState();
	BoundGameState = GameState;
	if (!BoundGameState)
	{
		return;
	}

	BoundGameState->OnMatchLifecycleChanged.AddDynamic(this, &UGambitPCShellWidget::HandleMatchLifecycleChanged);
	BoundGameState->OnMatchSetupChanged.AddDynamic(this, &UGambitPCShellWidget::HandleMatchSetupChanged);
	BoundGameState->OnPhaseChanged.AddDynamic(this, &UGambitPCShellWidget::HandlePhaseChanged);
	BoundGameState->OnRoundChanged.AddDynamic(this, &UGambitPCShellWidget::HandleRoundChanged);
	BoundGameState->OnRankingUpdated.AddDynamic(this, &UGambitPCShellWidget::HandleRankingUpdated);
	BoundGameState->OnFinalRankingUpdated.AddDynamic(this, &UGambitPCShellWidget::HandleRankingUpdated);
}

void UGambitPCShellWidget::UnbindGameState()
{
	if (!BoundGameState)
	{
		return;
	}

	BoundGameState->OnMatchLifecycleChanged.RemoveDynamic(this, &UGambitPCShellWidget::HandleMatchLifecycleChanged);
	BoundGameState->OnMatchSetupChanged.RemoveDynamic(this, &UGambitPCShellWidget::HandleMatchSetupChanged);
	BoundGameState->OnPhaseChanged.RemoveDynamic(this, &UGambitPCShellWidget::HandlePhaseChanged);
	BoundGameState->OnRoundChanged.RemoveDynamic(this, &UGambitPCShellWidget::HandleRoundChanged);
	BoundGameState->OnRankingUpdated.RemoveDynamic(this, &UGambitPCShellWidget::HandleRankingUpdated);
	BoundGameState->OnFinalRankingUpdated.RemoveDynamic(this, &UGambitPCShellWidget::HandleRankingUpdated);
	BoundGameState = nullptr;
}

void UGambitPCShellWidget::SyncCachedState()
{
	if (const AGambitGameState* GameState = BoundGameState.Get())
	{
		CachedLifecycleState = GameState->GetMatchLifecycleState();
		CachedMatchSetup = GameState->GetMatchSetupConfig();
		CachedPhase = GameState->GetCurrentPhase();
		CachedRoundIndex = GameState->GetCurrentRoundIndex();
		return;
	}

	if (const AGambitGameMode* GameMode = GetGambitGameMode())
	{
		CachedMatchSetup = GameMode->GetPendingMatchSetup();
	}
}

void UGambitPCShellWidget::RebuildShell()
{
	ClearRoot();
	AddText(TEXT("Grandpa Gambit"));
	AddText(FString::Printf(TEXT("State: %s"), *ShellLifecycleToString(CachedLifecycleState)));
	AddSpacerLine();

	switch (CachedLifecycleState)
	{
	case EGambitMatchLifecycleState::MatchSetup:
		BuildMatchSetup();
		break;
	case EGambitMatchLifecycleState::Lobby:
		BuildLobby();
		break;
	case EGambitMatchLifecycleState::InMatch:
		BuildMatchHud();
		break;
	case EGambitMatchLifecycleState::MatchComplete:
		BuildMatchComplete();
		break;
	case EGambitMatchLifecycleState::MainMenu:
	default:
		BuildMainMenu();
		break;
	}
}

void UGambitPCShellWidget::BuildMainMenu()
{
	AddText(TEXT("PC local shell"));
	UButton* NewMatchButton = AddButton(TEXT("New PC Match"));
	NewMatchButton->OnClicked.AddDynamic(this, &UGambitPCShellWidget::HandleNewMatchClicked);
}

void UGambitPCShellWidget::BuildMatchSetup()
{
	AddText(TEXT("Create Match"));
	AddText(FString::Printf(TEXT("Players: %d"), CachedMatchSetup.LocalPlayerCount));

	UButton* Players2Button = AddButton(TEXT("2 Players"));
	Players2Button->OnClicked.AddDynamic(this, &UGambitPCShellWidget::HandlePlayerCount2Clicked);

	UButton* Players3Button = AddButton(TEXT("3 Players"));
	Players3Button->OnClicked.AddDynamic(this, &UGambitPCShellWidget::HandlePlayerCount3Clicked);

	UButton* Players4Button = AddButton(TEXT("4 Players"));
	Players4Button->OnClicked.AddDynamic(this, &UGambitPCShellWidget::HandlePlayerCount4Clicked);

	AddSpacerLine();
	AddText(FString::Printf(TEXT("Rounds: %d"), CachedMatchSetup.RoundCount));

	UButton* RoundMinusButton = AddButton(TEXT("- Round"));
	RoundMinusButton->OnClicked.AddDynamic(this, &UGambitPCShellWidget::HandleRoundCountMinusClicked);

	UButton* RoundPlusButton = AddButton(TEXT("+ Round"));
	RoundPlusButton->OnClicked.AddDynamic(this, &UGambitPCShellWidget::HandleRoundCountPlusClicked);

	AddSpacerLine();
	UButton* LobbyButton = AddButton(TEXT("Continue To Lobby"));
	LobbyButton->OnClicked.AddDynamic(this, &UGambitPCShellWidget::HandleEnterLobbyClicked);

	UButton* BackButton = AddButton(TEXT("Back To Main Menu"));
	BackButton->OnClicked.AddDynamic(this, &UGambitPCShellWidget::HandleBackToMainMenuClicked);
}

void UGambitPCShellWidget::BuildLobby()
{
	AddText(TEXT("Table Setup"));
	AddText(FString::Printf(
		TEXT("%d local players, %d rounds"),
		CachedMatchSetup.LocalPlayerCount,
		CachedMatchSetup.RoundCount));
	BuildLobbyPlayerRows();

	UButton* StartButton = AddButton(TEXT("Start Match"));
	StartButton->OnClicked.AddDynamic(this, &UGambitPCShellWidget::HandleStartMatchClicked);

	UButton* BackButton = AddButton(TEXT("Back To Setup"));
	BackButton->OnClicked.AddDynamic(this, &UGambitPCShellWidget::HandleNewMatchClicked);
}

void UGambitPCShellWidget::BuildMatchHud()
{
	AddText(FString::Printf(
		TEXT("Round %d / %d"),
		CachedRoundIndex,
		CachedMatchSetup.RoundCount));
	AddText(FString::Printf(TEXT("Phase: %s"), *ShellPhaseToString(CachedPhase)));
	if (!LastActionFeedback.IsEmpty())
	{
		AddText(
			FString::Printf(TEXT("Feedback: %s"), *LastActionFeedback),
			16.0f,
			bLastActionSucceeded ? FLinearColor(0.35f, 0.95f, 0.45f) : FLinearColor(1.0f, 0.45f, 0.25f));
	}
	AddSpacerLine();

	BuildPlayerRows();

	const TArray<FString> RankingLines = BuildRankingFeedbackLines(BoundGameState.Get());
	if (RankingLines.Num() > 0)
	{
		AddSpacerLine();
		for (int32 LineIndex = 0; LineIndex < RankingLines.Num(); ++LineIndex)
		{
			AddText(RankingLines[LineIndex], LineIndex == 0 ? 18.0f : 15.0f);
		}
	}

	if (ShellIsReadyGatedPhase(CachedPhase))
	{
		UButton* ContinueButton = AddButton(TEXT("Continue Phase"));
		ContinueButton->OnClicked.AddDynamic(this, &UGambitPCShellWidget::HandleReadyAllClicked);
	}
	else
	{
		AddText(TEXT("Automatic phase resolution in progress."));
	}
}

void UGambitPCShellWidget::BuildMatchComplete()
{
	AddText(TEXT("Match Complete"));
	BuildFinalRankingRows();

	UButton* NewMatchButton = AddButton(TEXT("New Match"));
	NewMatchButton->OnClicked.AddDynamic(this, &UGambitPCShellWidget::HandleNewMatchClicked);

	UButton* MainMenuButton = AddButton(TEXT("Main Menu"));
	MainMenuButton->OnClicked.AddDynamic(this, &UGambitPCShellWidget::HandleBackToMainMenuClicked);
}

void UGambitPCShellWidget::BuildLobbyPlayerRows()
{
	const AGambitGameState* GameState = BoundGameState.Get();
	const TArray<AGambitPlayerState*> Players = GameState ? GameState->GetGambitPlayerStates() : TArray<AGambitPlayerState*>();
	if (Players.Num() == 0)
	{
		AddText(TEXT("No local players seated yet."));
		return;
	}

	for (int32 PlayerIndex = 0; PlayerIndex < Players.Num(); ++PlayerIndex)
	{
		const AGambitPlayerState* PlayerState = Players[PlayerIndex];
		const FString PlayerName = PlayerState && !PlayerState->GetPlayerName().IsEmpty()
			? PlayerState->GetPlayerName()
			: FString::Printf(TEXT("Player %d"), PlayerIndex + 1);
		AddText(FString::Printf(TEXT("Seat %d: %s"), PlayerIndex + 1, *PlayerName));
	}
}

void UGambitPCShellWidget::BuildPlayerRows()
{
	const AGambitGameState* GameState = BoundGameState.Get();
	const TArray<AGambitPlayerState*> Players = GameState ? GameState->GetGambitPlayerStates() : TArray<AGambitPlayerState*>();
	if (Players.Num() == 0)
	{
		AddText(TEXT("No local players registered yet."));
		return;
	}

	for (int32 PlayerIndex = 0; PlayerIndex < Players.Num(); ++PlayerIndex)
	{
		AGambitPlayerState* PlayerState = Players[PlayerIndex];
		if (!PlayerState)
		{
			continue;
		}

		BuildPlayerRoundHud(PlayerIndex, PlayerState);

		if (CachedPhase == EGambitRoundPhase::Shop)
		{
			const TArray<FGambitShopOffer>& Offers = PlayerState->GetCurrentShopOffersRef();
			for (const FGambitShopOffer& Offer : Offers)
			{
				AddText(FString::Printf(
					TEXT("  Offer %d: %s - %d gold"),
					Offer.OfferId + 1,
					*ShellItemDisplayName(Offer.ItemDefinition),
					Offer.Price));
			}
		}
	}
}

void UGambitPCShellWidget::BuildPlayerRoundHud(const int32 PlayerIndex, AGambitPlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return;
	}

	const UGambitRoundFlowComponent* RoundFlow = GetRoundFlowComponent();
	const int32 RerollsUsed = RoundFlow ? RoundFlow->GetRerollsUsedForPlayer(PlayerState) : 0;
	const int32 RerollLimit = RoundFlow ? RoundFlow->GetMaxRerollsForPlayer(PlayerState) : 0;
	const int32 RerollsRemaining = FMath::Max(0, RerollLimit - RerollsUsed);

	AddText(FString::Printf(
		TEXT("P%d %s | Phase %s"),
		PlayerIndex + 1,
		*ResolvePlayerDisplayName(PlayerState, PlayerIndex),
		*ShellPhaseToString(CachedPhase)),
		20.0f);
	AddText(FString::Printf(
		TEXT("VP %d | Score %d | Gold %d | Rerolls %d/%d | Remaining %d"),
		PlayerState->GetTotalVictoryPoints(),
		PlayerState->GetCurrentRoundScore(),
		PlayerState->GetCurrentGold(),
		RerollsUsed,
		RerollLimit,
		RerollsRemaining),
		16.0f);

	const int32 VictoryPointsGranted = ShellFindVictoryPointsGrantedForPlayer(BoundGameState.Get(), PlayerState);
	for (const FString& Line : BuildScoreFeedbackLines(PlayerState))
	{
		AddText(Line, Line.StartsWith(TEXT("Resolution")) ? 15.0f : 14.0f);
	}
	for (const FString& Line : BuildRewardFeedbackLines(PlayerState, VictoryPointsGranted))
	{
		AddText(Line, Line.StartsWith(TEXT("Rewards")) ? 15.0f : 14.0f);
	}
	for (const FString& Line : BuildLedgerFeedbackLines(PlayerState))
	{
		AddText(Line, Line.StartsWith(TEXT("Ledger")) ? 15.0f : 14.0f);
	}
	if (CachedPhase == EGambitRoundPhase::Action)
	{
		BuildPlayerActionHud(PlayerIndex, PlayerState);
	}

	const TArray<FGambitDieRuntimeState>& DiceStates = PlayerState->GetDiceStatesRef();
	if (DiceStates.Num() == 0)
	{
		AddText(TEXT("Dice: -"), 16.0f);
	}
	else
	{
		UHorizontalBox* DiceBox = AddHorizontalBox(2.0f, 4.0f);
		for (int32 DieIndex = 0; DieIndex < DiceStates.Num(); ++DieIndex)
		{
			const FGambitDieRuntimeState& DieState = DiceStates[DieIndex];
			const FString LockState = DieState.bLocked ? TEXT("Locked") : TEXT("Open");
			FString ButtonLabel = FString::Printf(
				TEXT("D%d  %d  %s"),
				DieIndex + 1,
				DieState.EffectiveValue,
				*LockState);
			if (!DieState.bCanBeLocked)
			{
				ButtonLabel += TEXT("  NoLock");
			}
			if (!DieState.bCanBeRerolled)
			{
				ButtonLabel += TEXT("  NoReroll");
			}

			AddActionButtonToBox(
				DiceBox,
				ButtonLabel,
				EGambitPCShellActionKind::ToggleDieLock,
				PlayerState,
				PlayerIndex,
				DieIndex);
		}
	}

	UHorizontalBox* ActionBox = AddHorizontalBox(0.0f, 8.0f);
	AddActionButtonToBox(
		ActionBox,
		TEXT("Reroll Unlocked"),
		EGambitPCShellActionKind::Reroll,
		PlayerState,
		PlayerIndex);
}

void UGambitPCShellWidget::BuildPlayerActionHud(const int32 PlayerIndex, AGambitPlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return;
	}

	AGambitPlayerController* PlayerController = ResolvePlayerControllerForPlayerState(PlayerState);
	const TArray<FGambitConsumableRuntimeSlot>& ConsumableSlots = PlayerState->GetConsumableSlotsRef();
	for (const FString& Line : BuildConsumableFeedbackLines(PlayerState, CachedPhase))
	{
		AddText(Line, Line.StartsWith(TEXT("Consumables")) ? 15.0f : 14.0f);
	}

	if (ConsumableSlots.Num() > 0)
	{
		UHorizontalBox* ConsumableBox = AddHorizontalBox(2.0f, 6.0f);
		for (int32 SlotIndex = 0; SlotIndex < ConsumableSlots.Num(); ++SlotIndex)
		{
			const FGambitConsumableRuntimeSlot& ConsumableSlot = ConsumableSlots[SlotIndex];
			const FString Label = FString::Printf(TEXT("Use Slot %d"), SlotIndex + 1);
			UGambitPCShellActionButton* Button = AddActionButtonToBox(
				ConsumableBox,
				Label,
				EGambitPCShellActionKind::UseConsumable,
				PlayerState,
				PlayerIndex,
				SlotIndex);
			if (Button)
			{
				Button->SetIsEnabled(PlayerController && ShellIsConsumableUsableInPhase(ConsumableSlot, CachedPhase));
			}
		}
	}

	BuildTargetSelectionHud(PlayerController, PlayerState, PlayerIndex);
}

void UGambitPCShellWidget::BuildTargetSelectionHud(
	AGambitPlayerController* PlayerController,
	AGambitPlayerState* PlayerState,
	const int32 PlayerIndex)
{
	if (!PlayerController || !PlayerController->HasPendingTargetSelection())
	{
		return;
	}

	for (const FString& Line : BuildTargetSelectionFeedbackLines(PlayerController))
	{
		AddText(Line, Line.StartsWith(TEXT("Target Selection")) ? 15.0f : 14.0f);
	}

	const FGambitTargetSelectionRequest Request = PlayerController->GetPendingTargetSelectionRequest();
	const int32 SelectedOptionId = PlayerController->GetPendingTargetSelectionSelectedOptionId();
	if (Request.Options.Num() > 0)
	{
		UHorizontalBox* OptionsBox = AddHorizontalBox(2.0f, 4.0f);
		for (const FGambitTargetSelectionOption& Option : Request.Options)
		{
			const FString Label = FString::Printf(
				TEXT("%sOption %d"),
				Option.OptionId == SelectedOptionId ? TEXT("* ") : TEXT(""),
				Option.OptionId);
			UGambitPCShellActionButton* Button = AddActionButtonToBox(
				OptionsBox,
				Label,
				EGambitPCShellActionKind::SelectTargetSelectionOption,
				PlayerState,
				PlayerIndex,
				Option.OptionId);
			if (Button)
			{
				Button->SetIsEnabled(Option.bValid);
			}
		}
	}

	UHorizontalBox* ConfirmBox = AddHorizontalBox(0.0f, 8.0f);
	UGambitPCShellActionButton* ConfirmButton = AddActionButtonToBox(
		ConfirmBox,
		TEXT("Confirm Target"),
		EGambitPCShellActionKind::ConfirmTargetSelection,
		PlayerState,
		PlayerIndex);
	if (ConfirmButton)
	{
		ConfirmButton->SetIsEnabled(Request.HasValidOptions() && SelectedOptionId != INDEX_NONE);
	}
	AddActionButtonToBox(
		ConfirmBox,
		TEXT("Cancel"),
		EGambitPCShellActionKind::CancelTargetSelection,
		PlayerState,
		PlayerIndex);
}

void UGambitPCShellWidget::BuildFinalRankingRows()
{
	const AGambitGameState* GameState = BoundGameState.Get();
	if (!GameState)
	{
		AddText(TEXT("No final ranking available."));
		return;
	}

	const TArray<FGambitFinalRankingEntry>& FinalRanking = GameState->GetFinalRankingSnapshotRef();
	if (FinalRanking.Num() == 0)
	{
		AddText(TEXT("No final ranking available."));
		return;
	}

	for (const FGambitFinalRankingEntry& Entry : FinalRanking)
	{
		AddText(FString::Printf(
			TEXT("#%d %s%s | VP %d | Last Score %d | Gold %d"),
			Entry.Rank,
			*Entry.PlayerName,
			Entry.bWinner ? TEXT(" - Winner") : TEXT(""),
			Entry.TotalVictoryPoints,
			Entry.LastRoundScore,
			Entry.Gold));
	}
}

void UGambitPCShellWidget::ConfigureMatch(const int32 LocalPlayerCount, const int32 RoundCount)
{
	if (AGambitGameMode* GameMode = GetGambitGameMode())
	{
		GameMode->RequestConfigureMatch(LocalPlayerCount, RoundCount);
	}
	RefreshShell();
}

void UGambitPCShellWidget::SetLastActionFeedback(const FGambitRoundCommandResult& Result)
{
	bLastActionSucceeded = Result.bSuccess;
	LastActionFeedback = Result.Message.IsEmpty()
		? (Result.bSuccess ? TEXT("Action accepted.") : TEXT("Action refused."))
		: Result.Message;
}

void UGambitPCShellWidget::SetLastActionFeedbackMessage(const bool bSuccess, const FString& Message)
{
	bLastActionSucceeded = bSuccess;
	LastActionFeedback = Message.IsEmpty()
		? (bSuccess ? TEXT("Action accepted.") : TEXT("Action refused."))
		: Message;
}

FString UGambitPCShellWidget::ResolvePlayerDisplayName(const AGambitPlayerState* PlayerState, const int32 PlayerIndex) const
{
	if (PlayerState && !PlayerState->GetPlayerName().IsEmpty())
	{
		return PlayerState->GetPlayerName();
	}

	return FString::Printf(TEXT("Player %d"), PlayerIndex + 1);
}

UTextBlock* UGambitPCShellWidget::AddText(const FString& Text, const float FontSize, const FLinearColor& Color)
{
	if (!RootContent || !WidgetTree)
	{
		return nullptr;
	}

	UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TextBlock->SetText(FText::FromString(Text));
	TextBlock->SetAutoWrapText(true);
	TextBlock->SetColorAndOpacity(FSlateColor(Color));

	FSlateFontInfo Font = TextBlock->GetFont();
	Font.Size = FMath::Max(8, FMath::RoundToInt(FontSize));
	TextBlock->SetFont(Font);

	if (UVerticalBoxSlot* TextSlot = RootContent->AddChildToVerticalBox(TextBlock))
	{
		TextSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 2.0f));
	}
	return TextBlock;
}

UButton* UGambitPCShellWidget::AddButton(const FString& Label)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ButtonText->SetText(FText::FromString(Label));
	ButtonText->SetAutoWrapText(true);
	ButtonText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Button->SetContent(ButtonText);
	if (UVerticalBoxSlot* ButtonSlot = RootContent->AddChildToVerticalBox(Button))
	{
		ButtonSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 4.0f));
	}
	return Button;
}

UHorizontalBox* UGambitPCShellWidget::AddHorizontalBox(const float TopPadding, const float BottomPadding)
{
	if (!RootContent || !WidgetTree)
	{
		return nullptr;
	}

	UHorizontalBox* Box = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UVerticalBoxSlot* BoxSlot = RootContent->AddChildToVerticalBox(Box))
	{
		BoxSlot->SetPadding(FMargin(0.0f, TopPadding, 0.0f, BottomPadding));
	}
	return Box;
}

UButton* UGambitPCShellWidget::AddButtonToBox(UHorizontalBox* Box, const FString& Label)
{
	if (!Box || !WidgetTree)
	{
		return nullptr;
	}

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ButtonText->SetText(FText::FromString(Label));
	ButtonText->SetAutoWrapText(true);
	ButtonText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Button->SetContent(ButtonText);
	if (UHorizontalBoxSlot* ButtonSlot = Box->AddChildToHorizontalBox(Button))
	{
		ButtonSlot->SetPadding(FMargin(0.0f, 2.0f, 8.0f, 2.0f));
	}
	return Button;
}

UGambitPCShellActionButton* UGambitPCShellWidget::AddActionButtonToBox(
	UHorizontalBox* Box,
	const FString& Label,
	const EGambitPCShellActionKind ActionKind,
	AGambitPlayerState* PlayerState,
	const int32 PlayerIndex,
	const int32 ActionIndex)
{
	if (!Box || !WidgetTree)
	{
		return nullptr;
	}

	UGambitPCShellActionButton* Button = WidgetTree->ConstructWidget<UGambitPCShellActionButton>(UGambitPCShellActionButton::StaticClass());
	UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ButtonText->SetText(FText::FromString(Label));
	ButtonText->SetAutoWrapText(true);
	ButtonText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Button->SetContent(ButtonText);
	Button->ConfigureAction(this, ActionKind, PlayerState, PlayerIndex, ActionIndex);
	if (UHorizontalBoxSlot* ButtonSlot = Box->AddChildToHorizontalBox(Button))
	{
		ButtonSlot->SetPadding(FMargin(0.0f, 2.0f, 8.0f, 2.0f));
	}
	return Button;
}

void UGambitPCShellWidget::AddSpacerLine()
{
	AddText(TEXT(" "));
}

void UGambitPCShellWidget::ClearRoot()
{
	if (RootContent)
	{
		RootContent->ClearChildren();
	}
}

AGambitGameMode* UGambitPCShellWidget::GetGambitGameMode() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetAuthGameMode<AGambitGameMode>() : nullptr;
}

AGambitGameState* UGambitPCShellWidget::GetGambitGameState() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetGameState<AGambitGameState>() : nullptr;
}

UGambitRoundFlowComponent* UGambitPCShellWidget::GetRoundFlowComponent() const
{
	const AGambitGameMode* GameMode = GetGambitGameMode();
	return GameMode ? GameMode->GetRoundFlowComponent() : nullptr;
}

AGambitPlayerController* UGambitPCShellWidget::ResolvePlayerControllerForPlayerState(
	const AGambitPlayerState* PlayerState) const
{
	if (!PlayerState)
	{
		return nullptr;
	}

	if (AGambitPlayerController* OwningController = Cast<AGambitPlayerController>(PlayerState->GetOwner()))
	{
		return OwningController;
	}

	UWorld* World = GetWorld();
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
