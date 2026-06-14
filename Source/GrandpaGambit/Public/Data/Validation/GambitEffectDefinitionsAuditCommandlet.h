#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GambitEffectDefinitionsAuditCommandlet.generated.h"

UCLASS()
class GRANDPAGAMBIT_API UGambitEffectDefinitionsAuditCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UGambitEffectDefinitionsAuditCommandlet();

	virtual int32 Main(const FString& Params) override;
};
