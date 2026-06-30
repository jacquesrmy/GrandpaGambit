#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "GambitPCShellWidget.generated.h"

class AGambitGameMode;
class AGambitGameState;
class AGambitPlayerController;
class AGambitPlayerState;
class UGambitRoundFlowComponent;
class UGambitPCShellWidget;
class UHorizontalBox;
class UTextBlock;
class UVerticalBox;

// Temporary V0.1 shell-only action verbs. Do not treat this enum as the final UI
// command contract; final UI should call the stable Request... APIs directly.
UENUM()
enum class EGambitPCShellActionKind : uint8
{
	ToggleDieLock,
	Reroll,
	UseConsumable,
	BuyShopOffer,
	SelectTargetSelectionOption,
	ConfirmTargetSelection,
	CancelTargetSelection
};

UCLASS()
class GRANDPAGAMBIT_API UGambitPCShellActionButton : public UButton
{
	GENERATED_BODY()

public:
	void ConfigureAction(
		UGambitPCShellWidget* InOwnerShell,
		EGambitPCShellActionKind InActionKind,
		AGambitPlayerState* InPlayerState,
		int32 InPlayerIndex,
		int32 InActionIndex = INDEX_NONE);

private:
	UFUNCTION()
	void HandleClicked();

	TWeakObjectPtr<UGambitPCShellWidget> OwnerShell;
	TWeakObjectPtr<AGambitPlayerState> PlayerState;
	EGambitPCShellActionKind ActionKind = EGambitPCShellActionKind::ToggleDieLock;
	int32 PlayerIndex = INDEX_NONE;
	int32 ActionIndex = INDEX_NONE;
};

/**
 * Temporary playable PC keyboard/mouse shell for V0.1.
 *
 * This widget is runtime because V0.1 must remain playable, but it is not the
 * final UI architecture. Keep gameplay truth in GameMode/RoundFlow/Controller
 * services, consume stable production UI contract snapshots where they fit, and
 * replace this shell instead of growing final layout/navigation here.
 */
UCLASS(BlueprintType, Blueprintable)
class GRANDPAGAMBIT_API UGambitPCShellWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Gambit|V0.1 PC Shell")
	bool InitializeShellWidget();

	UFUNCTION(BlueprintCallable, Category = "Gambit|V0.1 PC Shell")
	void RefreshShell();

	void ExecutePlayerAction(
		EGambitPCShellActionKind ActionKind,
		AGambitPlayerState* PlayerState,
		int32 PlayerIndex,
		int32 ActionIndex);

	static TArray<FString> BuildConsumableFeedbackLines(const AGambitPlayerState* PlayerState, EGambitRoundPhase CurrentPhase);
	static TArray<FString> BuildScoreFeedbackLines(const AGambitPlayerState* PlayerState);
	static TArray<FString> BuildRewardFeedbackLines(const AGambitPlayerState* PlayerState, int32 VictoryPointsGranted);
	static TArray<FString> BuildRankingFeedbackLines(const AGambitGameState* GameState);
	static TArray<FString> BuildLedgerFeedbackLines(const AGambitPlayerState* PlayerState, int32 MaxEvents = 6);
	static TArray<FString> BuildTargetSelectionFeedbackLines(const AGambitPlayerController* PlayerController);
	static TArray<FString> BuildShopFeedbackLines(
		const AGambitPlayerState* PlayerState,
		const TArray<FGambitShopOfferSnapshot>& OfferSnapshots);
	static TArray<FString> BuildInventorySummaryLines(const AGambitPlayerState* PlayerState);

private:
	UFUNCTION()
	void HandleMatchLifecycleChanged(EGambitMatchLifecycleState OldState, EGambitMatchLifecycleState NewState);

	UFUNCTION()
	void HandleMatchSetupChanged(FGambitMatchSetupConfig NewSetup);

	UFUNCTION()
	void HandlePhaseChanged(EGambitRoundPhase OldPhase, EGambitRoundPhase NewPhase);

	UFUNCTION()
	void HandleRoundChanged(int32 NewRoundIndex);

	UFUNCTION()
	void HandleRankingUpdated();

	UFUNCTION()
	void HandleNewMatchClicked();

	UFUNCTION()
	void HandleBackToMainMenuClicked();

	UFUNCTION()
	void HandlePlayerCount2Clicked();

	UFUNCTION()
	void HandlePlayerCount3Clicked();

	UFUNCTION()
	void HandlePlayerCount4Clicked();

	UFUNCTION()
	void HandleRoundCountMinusClicked();

	UFUNCTION()
	void HandleRoundCountPlusClicked();

	UFUNCTION()
	void HandleEnterLobbyClicked();

	UFUNCTION()
	void HandleStartMatchClicked();

	UFUNCTION()
	void HandleReadyAllClicked();

	void BindGameState(AGambitGameState* GameState);
	void UnbindGameState();
	void SyncCachedState();
	void RebuildShell();
	void BuildMainMenu();
	void BuildMatchSetup();
	void BuildLobby();
	void BuildMatchHud();
	void BuildMatchComplete();
	void BuildLobbyPlayerRows();
	void BuildPlayerRows();
	void BuildPlayerRoundHud(int32 PlayerIndex, AGambitPlayerState* PlayerState);
	void BuildPlayerActionHud(int32 PlayerIndex, AGambitPlayerState* PlayerState);
	void BuildPlayerShopHud(int32 PlayerIndex, AGambitPlayerState* PlayerState);
	void BuildTargetSelectionHud(AGambitPlayerController* PlayerController, AGambitPlayerState* PlayerState, int32 PlayerIndex);
	void BuildFinalRankingRows();
	void ConfigureMatch(int32 LocalPlayerCount, int32 RoundCount);
	void SetLastActionFeedback(const FGambitRoundCommandResult& Result);
	void SetLastActionFeedbackMessage(bool bSuccess, const FString& Message);
	FString ResolvePlayerDisplayName(const AGambitPlayerState* PlayerState, int32 PlayerIndex) const;

	UTextBlock* AddText(const FString& Text, float FontSize = 18.0f, const FLinearColor& Color = FLinearColor::White);
	UButton* AddButton(const FString& Label);
	UHorizontalBox* AddHorizontalBox(float TopPadding = 0.0f, float BottomPadding = 0.0f);
	UButton* AddButtonToBox(UHorizontalBox* Box, const FString& Label);
	UGambitPCShellActionButton* AddActionButtonToBox(
		UHorizontalBox* Box,
		const FString& Label,
		EGambitPCShellActionKind ActionKind,
		AGambitPlayerState* PlayerState,
		int32 PlayerIndex,
		int32 ActionIndex = INDEX_NONE);
	void AddSpacerLine();
	void ClearRoot();

	AGambitGameMode* GetGambitGameMode() const;
	AGambitGameState* GetGambitGameState() const;
	UGambitRoundFlowComponent* GetRoundFlowComponent() const;
	AGambitPlayerController* ResolvePlayerControllerForPlayerState(const AGambitPlayerState* PlayerState) const;

	UPROPERTY(Transient)
	TObjectPtr<AGambitGameState> BoundGameState = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> RootContent = nullptr;

	EGambitMatchLifecycleState CachedLifecycleState = EGambitMatchLifecycleState::MainMenu;
	FGambitMatchSetupConfig CachedMatchSetup;
	EGambitRoundPhase CachedPhase = EGambitRoundPhase::None;
	int32 CachedRoundIndex = 0;
	float RefreshAccumulator = 0.0f;
	FString LastActionFeedback;
	bool bLastActionSucceeded = false;
};
