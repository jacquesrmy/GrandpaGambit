#pragma once

#include "CoreMinimal.h"
#include "Items/Data/GambitItemDefinition.h"
#include "GambitModuleDefinition.generated.h"

UCLASS(BlueprintType)
class GRANDPAGAMBIT_API UGambitModuleDefinition : public UGambitItemDefinition
{
	GENERATED_BODY()

public:
	UGambitModuleDefinition();

	UFUNCTION(BlueprintPure, Category = "Gambit|Module")
	bool HasNonNeutralPersistentScoreModifier() const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Module")
	bool ShouldApplyLegacyPersistentScoreModifier() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Module", meta = (DisplayName = "Legacy Persistent Score Modifier", ToolTip = "Legacy/migration-only shortcut. It is applied during ScoreModifier only when EffectDefinitions is empty; EffectDefinitions are the source of truth for authored content."))
	FGambitScoreModifierContext PersistentScoreModifier;
};
