#pragma once

#include "CoreMinimal.h"
#include "Core/Types/GambitTargetSelectionTypes.h"
#include "UObject/Object.h"
#include "GambitTargetSelectionService.generated.h"

class AGambitPlayerState;
class UGambitConsumableDefinition;

UCLASS(BlueprintType)
class GRANDPAGAMBIT_API UGambitTargetSelectionService : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Gambit|Target Selection")
	bool BuildConsumableTargetSelectionRequest(
		AGambitPlayerState* RequestingPlayer,
		int32 SlotIndex,
		const TArray<AGambitPlayerState*>& MatchPlayers,
		EGambitRoundPhase CurrentPhase,
		const FGambitEffectExecutionContext& BaseContext,
		UPARAM(ref) FGambitTargetSelectionRequest& OutRequest) const;

	UFUNCTION(BlueprintPure, Category = "Gambit|Target Selection")
	static bool ConsumableRequiresExplicitTargetSelection(const UGambitConsumableDefinition* ConsumableDefinition);
};
