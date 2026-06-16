#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Core/Types/GambitGameplayTypes.h"

struct GRANDPAGAMBIT_API FGambitScoreModifierMath
{
	static FGambitScoreModifierContext MakeNeutral();
	static FGambitScoreModifierContext Normalize(FGambitScoreModifierContext Modifier);
	static bool IsNeutral(const FGambitScoreModifierContext& Modifier);
	static FGambitScoreModifierContext Merge(
		const FGambitScoreModifierContext& A,
		const FGambitScoreModifierContext& B);
	static FGambitScoreModifierContext MergeAll(TConstArrayView<FGambitScoreModifierContext> Modifiers);
};
