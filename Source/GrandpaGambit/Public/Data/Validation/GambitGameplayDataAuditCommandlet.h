#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GambitGameplayDataAuditCommandlet.generated.h"

UCLASS()
class GRANDPAGAMBIT_API UGambitGameplayDataAuditCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UGambitGameplayDataAuditCommandlet();

	virtual int32 Main(const FString& Params) override;
};
