#include "Core/Settings/GambitStaticDataSettings.h"

UGambitStaticDataSettings::UGambitStaticDataSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("GrandpaGambit");
}

const UGambitStaticDataSettings* UGambitStaticDataSettings::Get()
{
	return GetDefault<UGambitStaticDataSettings>();
}
