#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GambitStaticDataSettings.generated.h"

class UGambitDiceCatalogDataAsset;
class UGambitItemCatalogDataAsset;
class UGambitPlayerLoadoutDefinition;
class UGambitSharedPoolDefinition;
class UGambitShopLootTable;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Grandpa Gambit Static Data"))
class GRANDPAGAMBIT_API UGambitStaticDataSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGambitStaticDataSettings();

	static const UGambitStaticDataSettings* Get();

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Catalogs")
	TSoftObjectPtr<UGambitDiceCatalogDataAsset> DefaultDiceCatalog;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Catalogs")
	TSoftObjectPtr<UGambitItemCatalogDataAsset> DefaultItemCatalog;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Match Defaults")
	TSoftObjectPtr<UGambitPlayerLoadoutDefinition> DefaultPlayerLoadout;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Match Defaults")
	TSoftObjectPtr<UGambitSharedPoolDefinition> DefaultSharedPoolDefinition;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Match Defaults")
	TSoftObjectPtr<UGambitShopLootTable> DefaultShopLootTable;
};
