#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GambitEffectTargetRulesFixupCommandlet.generated.h"

UCLASS()
class GRANDPAGAMBIT_API UGambitEffectTargetRulesFixupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UGambitEffectTargetRulesFixupCommandlet();

	virtual int32 Main(const FString& Params) override;
};
