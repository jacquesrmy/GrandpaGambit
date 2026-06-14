#pragma once

#include "CoreMinimal.h"
#include "Items/Data/GambitItemDefinition.h"
#include "GambitConsumableDefinition.generated.h"

UCLASS(BlueprintType)
class GRANDPAGAMBIT_API UGambitConsumableDefinition : public UGambitItemDefinition
{
	GENERATED_BODY()

public:
	UGambitConsumableDefinition();

	UFUNCTION(BlueprintPure, Category = "Gambit|Consumable")
	bool CanBeUsedDuringPhase(EGambitRoundPhase Phase) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Consumable")
	bool HasNonNeutralActionScoreModifier() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Consumable")
	bool ShouldApplyLegacyActionScoreModifier() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable", meta = (DisplayName = "Legacy Action Score Modifier", ToolTip = "Legacy/migration-only shortcut. It is applied on ConsumableUse only when EffectDefinitions is empty; EffectDefinitions are the source of truth for authored content."))
	FGambitScoreModifierContext ActionScoreModifier;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable", meta = (ToolTip = "Player activation windows only. Effects still execute with the ConsumableUse hook; use delayed-effect state for later resolution."))
	TArray<EGambitRoundPhase> UsablePhases;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable")
	bool bCanTargetOpponent = false;
};
