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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Module", meta = (DisplayName = "Legacy Persistent Score Modifier", ToolTip = "Legacy shortcut applied during ScoreModifier. Prefer EffectDefinitions for new object effects; do not use both unless preserving an existing asset during migration."))
	FGambitScoreModifierContext PersistentScoreModifier;
};
