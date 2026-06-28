#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "GambitPCShellWidget.generated.h"

class AGambitGameMode;
class AGambitGameState;
class UButton;
class UTextBlock;
class UVerticalBox;

UCLASS(BlueprintType, Blueprintable)
class GRANDPAGAMBIT_API UGambitPCShellWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Gambit|PC Shell")
	bool InitializeShellWidget();

	UFUNCTION(BlueprintCallable, Category = "Gambit|PC Shell")
	void RefreshShell();

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
	void BuildPlayerRows();
	void BuildFinalRankingRows();
	void ConfigureMatch(int32 LocalPlayerCount, int32 RoundCount);

	UTextBlock* AddText(const FString& Text, float FontSize = 18.0f);
	UButton* AddButton(const FString& Label);
	void AddSpacerLine();
	void ClearRoot();

	AGambitGameMode* GetGambitGameMode() const;
	AGambitGameState* GetGambitGameState() const;

	UPROPERTY(Transient)
	TObjectPtr<AGambitGameState> BoundGameState = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> RootContent = nullptr;

	EGambitMatchLifecycleState CachedLifecycleState = EGambitMatchLifecycleState::MainMenu;
	FGambitMatchSetupConfig CachedMatchSetup;
	EGambitRoundPhase CachedPhase = EGambitRoundPhase::None;
	int32 CachedRoundIndex = 0;
	float RefreshAccumulator = 0.0f;
};
