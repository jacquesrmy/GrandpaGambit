#include "Dice/Data/GambitDiceDefinition.h"

#if WITH_EDITOR
#include "Data/Validation/GambitDataValidation.h"
#endif

namespace
{
	const TArray<int32>& GetFallbackD6Faces()
	{
		static const TArray<int32> FallbackFaces = { 1, 2, 3, 4, 5, 6 };
		return FallbackFaces;
	}
}

#if WITH_EDITOR
EDataValidationResult UGambitDiceDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult SuperResult = Super::IsDataValid(Context);
	TArray<FGambitDataValidationIssue> Issues;
	GambitDataValidation::ValidateDiceDefinition(this, Issues);
	return GambitDataValidation::ApplyIssuesToContext(Issues, Context, SuperResult);
}
#endif

FName UGambitDiceDefinition::GetStableDiceId() const
{
	return DiceId;
}

FName UGambitDiceDefinition::GetResolvedDiceId() const
{
	if (!DiceId.IsNone())
	{
		return DiceId;
	}

	const FPrimaryAssetId PrimaryAssetId = GetPrimaryAssetId();
	if (PrimaryAssetId.IsValid())
	{
		return PrimaryAssetId.PrimaryAssetName;
	}

	return FName(*GetName());
}

TArray<int32> UGambitDiceDefinition::GetResolvedFaces() const
{
	return Faces.Num() > 0 ? Faces : GetFallbackD6Faces();
}

int32 UGambitDiceDefinition::GetFaceValue(const int32 FaceIndex) const
{
	const TArray<int32>& ResolvedFaces = Faces.Num() > 0 ? Faces : GetFallbackD6Faces();
	if (!ResolvedFaces.IsValidIndex(FaceIndex))
	{
		return ResolvedFaces[0];
	}

	return ResolvedFaces[FaceIndex];
}

float UGambitDiceDefinition::GetFaceWeight(const int32 FaceIndex) const
{
	if (!FaceWeights.IsValidIndex(FaceIndex))
	{
		return 1.0f;
	}

	return FMath::Max(0.0f, FaceWeights[FaceIndex]);
}

int32 UGambitDiceDefinition::RollFaceIndex(FRandomStream& RandomStream) const
{
	const TArray<int32>& ResolvedFaces = Faces.Num() > 0 ? Faces : GetFallbackD6Faces();
	if (ResolvedFaces.Num() <= 1)
	{
		return 0;
	}

	float TotalWeight = 0.0f;
	for (int32 FaceIndex = 0; FaceIndex < ResolvedFaces.Num(); ++FaceIndex)
	{
		TotalWeight += GetFaceWeight(FaceIndex);
	}

	if (TotalWeight <= 0.0f)
	{
		return RandomStream.RandRange(0, ResolvedFaces.Num() - 1);
	}

	const float Roll = RandomStream.FRandRange(0.0f, TotalWeight);
	float AccumulatedWeight = 0.0f;
	for (int32 FaceIndex = 0; FaceIndex < ResolvedFaces.Num(); ++FaceIndex)
	{
		AccumulatedWeight += GetFaceWeight(FaceIndex);
		if (Roll <= AccumulatedWeight)
		{
			return FaceIndex;
		}
	}

	return ResolvedFaces.Num() - 1;
}
