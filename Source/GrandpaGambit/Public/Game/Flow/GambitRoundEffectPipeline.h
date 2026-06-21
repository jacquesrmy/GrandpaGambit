#pragma once

#include "CoreMinimal.h"
#include "Items/Effects/GambitEffectExecutionTypes.h"
#include "UObject/Object.h"
#include "GambitRoundEffectPipeline.generated.h"

class AGambitPlayerState;
class UGambitEffectResolver;
class UGambitItemDefinition;
class UGambitSharedPoolComponent;

struct GRANDPAGAMBIT_API FGambitRoundEffectContextRequest
{
	EGambitEffectHook Hook = EGambitEffectHook::ScoreModifier;
	EGambitRoundPhase CurrentPhase = EGambitRoundPhase::None;
	int32 RoundNumber = 0;
	AGambitPlayerState* SourcePlayer = nullptr;
	AGambitPlayerState* TargetPlayer = nullptr;
	UGambitSharedPoolComponent* SharedPoolComponent = nullptr;
	TArray<AGambitPlayerState*> MatchPlayers;
	FRandomStream RandomStream;
	int32 RerollsUsed = 0;
	int32 RerollLimit = 0;
	int32 SourceRank = INDEX_NONE;
	int32 TargetRank = INDEX_NONE;
	int32 FirstRerolledDieHandIndexThisRound = INDEX_NONE;
	int32 MaxRerollCountForAnyDieThisRound = 0;
};

struct GRANDPAGAMBIT_API FGambitRoundEffectCommitRequest
{
	FRandomStream* MatchRandomStream = nullptr;
	TMap<TObjectPtr<AGambitPlayerState>, int32>* RerollLimitDeltaByPlayer = nullptr;
};

struct GRANDPAGAMBIT_API FGambitRoundScoreModifierEffectRequest
{
	FGambitRoundEffectContextRequest ContextRequest;
	FGambitRoundEffectCommitRequest CommitRequest;
	FGambitDiceCombinationResult CombinationResult;
};

struct GRANDPAGAMBIT_API FGambitRoundScoreModifierEffectResult
{
	FGambitScoreModifierContext ScoreModifier;
	FGambitEffectExecutionContext ExecutionContext;
	int32 TriggeredEffectCount = 0;
};

UCLASS()
class GRANDPAGAMBIT_API UGambitRoundEffectPipeline : public UObject
{
	GENERATED_BODY()

public:
	void SetEffectResolver(UGambitEffectResolver* InEffectResolver);

	FGambitEffectExecutionContext MakeEffectContext(const FGambitRoundEffectContextRequest& Request) const;
	void CommitEffectContext(const FGambitEffectExecutionContext& Context, const FGambitRoundEffectCommitRequest& CommitRequest) const;

	int32 ExecuteActiveSourceEffects(FGambitEffectExecutionContext& Context) const;
	int32 ExecuteActiveModuleEffects(FGambitEffectExecutionContext& Context) const;
	int32 ExecuteActiveDiceEffects(FGambitEffectExecutionContext& Context) const;
	int32 ExecuteItemEffects(UGambitItemDefinition* ItemDefinition, FGambitEffectExecutionContext& Context) const;

	FGambitRoundScoreModifierEffectResult BuildScoreModifierFromEffects(const FGambitRoundScoreModifierEffectRequest& Request) const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UGambitEffectResolver> EffectResolver;
};
