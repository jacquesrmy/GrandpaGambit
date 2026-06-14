#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GambitEffectTargetRulesAuditCommandlet.generated.h"

UCLASS()
class GRANDPAGAMBIT_API UGambitEffectTargetRulesAuditCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UGambitEffectTargetRulesAuditCommandlet();

	virtual int32 Main(const FString& Params) override;
};
