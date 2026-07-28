#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "OverlaneHUD.generated.h"

UCLASS()
class OVERLANE_API AOverlaneHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;
};
