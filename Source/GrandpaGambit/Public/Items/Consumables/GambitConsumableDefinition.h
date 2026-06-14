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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable", meta = (DisplayName = "Legacy Action Score Modifier", ToolTip = "Legacy shortcut applied on ConsumableUse. Prefer EffectDefinitions for new object effects; do not use both unless preserving an existing asset during migration."))
	FGambitScoreModifierContext ActionScoreModifier;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable", meta = (ToolTip = "Player activation windows only. Effects still execute with the ConsumableUse hook; use delayed-effect state for later resolution."))
	TArray<EGambitRoundPhase> UsablePhases;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable")
	bool bCanTargetOpponent = false;
};
