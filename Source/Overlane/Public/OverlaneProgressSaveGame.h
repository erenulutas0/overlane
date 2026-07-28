#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "OverlaneProgressSaveGame.generated.h"

/** Local solo-race records. Kept separate from display/control preferences. */
UCLASS()
class OVERLANE_API UOverlaneProgressSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY()
    float BestSoloTimeSeconds = 0.0f;

    UPROPERTY()
    int32 BestNearMissCount = 0;

    UPROPERTY()
    int32 BestSoloScore = 0;

    UPROPERTY()
    int32 CompletedSoloRaces = 0;
};
