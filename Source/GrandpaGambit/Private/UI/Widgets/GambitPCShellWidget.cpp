#include "UI/Widgets/GambitPCShellWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Dice/Data/GambitDiceDefinition.h"
#include "Engine/World.h"
#include "Game/Modes/GambitGameMode.h"
#include "Game/States/GambitGameState.h"
#include "Items/Data/GambitItemDefinition.h"
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

	FString ShellDiceValues(const TArray<FGambitDieRuntimeState>& DiceStates)
	{
		TArray<FString> Values;
		Values.Reserve(DiceStates.Num());
		for (const FGambitDieRuntimeState& DieState : DiceStates)
		{
			Values.Add(FString::Printf(
				TEXT("%d%s"),
				DieState.EffectiveValue,
				DieState.bLocked ? TEXT("L") : TEXT("")));
		}

		return Values.Num() > 0 ? FString::Join(Values, TEXT(", ")) : TEXT("-");
	}

	bool ShellIsReadyGatedPhase(const EGambitRoundPhase Phase)
	{
		return Phase == EGambitRoundPhase::SelectionReroll
			|| Phase == EGambitRoundPhase::Action
			|| Phase == EGambitRoundPhase::Shop;
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
	AddSpacerLine();

	BuildPlayerRows();

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
		const AGambitPlayerState* PlayerState = Players[PlayerIndex];
		if (!PlayerState)
		{
			continue;
		}

		AddText(FString::Printf(
			TEXT("P%d | VP %d | Score %d | Gold %d | Dice [%s]"),
			PlayerIndex + 1,
			PlayerState->GetTotalVictoryPoints(),
			PlayerState->GetCurrentRoundScore(),
			PlayerState->GetCurrentGold(),
			*ShellDiceValues(PlayerState->GetDiceStatesRef())));

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

UTextBlock* UGambitPCShellWidget::AddText(const FString& Text, const float FontSize)
{
	if (!RootContent || !WidgetTree)
	{
		return nullptr;
	}

	UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TextBlock->SetText(FText::FromString(Text));
	TextBlock->SetAutoWrapText(true);
	TextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));

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
