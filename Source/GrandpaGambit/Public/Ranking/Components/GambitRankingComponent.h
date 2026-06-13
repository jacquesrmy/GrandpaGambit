#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Types/GambitGameplayTypes.h"
#include "GambitRankingComponent.generated.h"

class AGambitPlayerState;

UCLASS(ClassGroup = (Gambit), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class GRANDPAGAMBIT_API UGambitRankingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGambitRankingComponent();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Ranking")
	TArray<FGambitRankingEntry> BuildRoundRanking(const TArray<AGambitPlayerState*>& PlayerStates) const;

	UFUNCTION(BlueprintCallable, Category = "Gambit|Ranking")
	void ApplyVictoryPoints(TArray<FGambitRankingEntry>& InOutRanking) const;
};
