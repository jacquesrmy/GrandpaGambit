#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GambitTableCameraDirector.generated.h"

class ULocalPlayer;

UCLASS(BlueprintType)
class GRANDPAGAMBIT_API AGambitTableCameraDirector : public AActor
{
	GENERATED_BODY()

public:
	AGambitTableCameraDirector();

	UFUNCTION(BlueprintCallable, Category = "Gambit|Camera")
	void ApplyCameraTargets(const TArray<ULocalPlayer*>& LocalPlayers, bool bUseSplitScreen, float BlendTime);

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Gambit|Camera")
	TObjectPtr<AActor> SharedCameraTarget = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Gambit|Camera")
	TArray<TObjectPtr<AActor>> PerPlayerCameraTargets;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Gambit|Camera")
	bool bFallbackToPawnIfMissingTarget = true;
};
