#pragma once

#include "CoreMinimal.h"
#include "Items/Effects/GambitEffectExecutionTypes.h"
#include "GambitEffectTargetResolver.generated.h"

class AGambitPlayerState;
class UGambitDiceComponent;
class UGambitEconomyComponent;
class UGambitInventoryComponent;
class UGambitItemEffectDefinition;
class UGambitShopComponent;

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitResolvedEffectTarget
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Effect Target")
	EGambitEffectTarget TargetSide = EGambitEffectTarget::Source;

	UPROPERTY(BlueprintReadOnly, Category = "Effect Target")
	TObjectPtr<AGambitPlayerState> Player = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Effect Target")
	TObjectPtr<UGambitDiceComponent> DiceComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Effect Target")
	TObjectPtr<UGambitEconomyComponent> EconomyComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Effect Target")
	TObjectPtr<UGambitInventoryComponent> InventoryComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Effect Target")
	TObjectPtr<UGambitShopComponent> ShopComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Effect Target")
	TArray<int32> DiceHandIndexes;

	UPROPERTY(BlueprintReadOnly, Category = "Effect Target")
	FName TargetRuleId;
};

USTRUCT(BlueprintType)
struct GRANDPAGAMBIT_API FGambitEffectTargetResolveResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Effect Target")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Effect Target")
	FString FailureReason;

	UPROPERTY(BlueprintReadOnly, Category = "Effect Target")
	FName TargetRuleId;

	UPROPERTY(BlueprintReadOnly, Category = "Effect Target")
	TArray<FGambitResolvedEffectTarget> Targets;

	bool HasResolvedTargets() const
	{
		return bSuccess && Targets.Num() > 0;
	}
};

namespace GambitEffectTargetResolver
{
	GRANDPAGAMBIT_API FGambitEffectTargetResolveResult ResolveEffectTargets(
		const UGambitItemEffectDefinition* EffectDefinition,
		const FGambitEffectExecutionContext& Context);

	GRANDPAGAMBIT_API FGambitEffectTargetResolveResult ResolveEffectTarget(
		const UGambitItemEffectDefinition* EffectDefinition,
		const FGambitEffectExecutionContext& Context,
		EGambitEffectTarget RequestedTarget);

	GRANDPAGAMBIT_API int32 ResolveSelectedDieHandIndex(
		const FGambitEffectExecutionContext& Context,
		EGambitEffectTarget RequestedTarget,
		FName TargetRuleId);
}
