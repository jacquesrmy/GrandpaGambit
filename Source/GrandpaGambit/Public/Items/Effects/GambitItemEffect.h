#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "Items/Effects/GambitEffectExecutionTypes.h"
#include "GambitItemEffect.generated.h"

UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class GRANDPAGAMBIT_API UGambitItemEffect : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Gambit|Item Effect")
	bool CanExecute(const FGambitEffectExecutionContext& Context) const;
	virtual bool CanExecute_Implementation(const FGambitEffectExecutionContext& Context) const;

	UFUNCTION(BlueprintNativeEvent, Category = "Gambit|Item Effect")
	bool ExecuteEffect(FGambitEffectExecutionContext& Context) const;
	virtual bool ExecuteEffect_Implementation(FGambitEffectExecutionContext& Context) const;

	UFUNCTION(BlueprintNativeEvent, Category = "Gambit|Item Effect")
	void ApplyToScoreModifier(FGambitScoreModifierContext& InOutModifier) const;
	virtual void ApplyToScoreModifier_Implementation(FGambitScoreModifierContext& InOutModifier) const;

	UFUNCTION(BlueprintNativeEvent, Category = "Gambit|Item Effect")
	void ApplyToScoreBreakdown(FGambitScoreBreakdown& InOutBreakdown) const;
	virtual void ApplyToScoreBreakdown_Implementation(FGambitScoreBreakdown& InOutBreakdown) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gambit|Item Effect")
	EGambitEffectHook Hook = EGambitEffectHook::ScoreModifier;
};
