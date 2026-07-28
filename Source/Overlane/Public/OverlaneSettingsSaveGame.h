#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "OverlaneSettingsSaveGame.generated.h"

/** Small local-only settings payload for the prototype menus. */
UCLASS()
class OVERLANE_API UOverlaneSettingsSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY()
    int32 GraphicsQuality = 2;

    UPROPERTY()
    bool bVSyncEnabled = true;

    UPROPERTY()
    int32 FrameRateMode = 1;

    UPROPERTY()
    float CameraFovOffset = 0.0f;

    UPROPERTY()
    bool bTrafficDebugOverlay = false;
};
