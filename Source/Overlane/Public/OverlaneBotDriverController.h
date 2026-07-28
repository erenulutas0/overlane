#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Controller.h"
#include "OverlaneBotDriverController.generated.h"

class ATrafficLanePath;

/**
 * A deliberately small first-pass driver for the solo practice bot.  It owns
 * a normal replicated vehicle pawn and follows one author-placed lane spline;
 * race scoring and finish authority remain exclusively with human players.
 */
UCLASS()
class OVERLANE_API AOverlaneBotDriverController : public AController
{
    GENERATED_BODY()

public:
    AOverlaneBotDriverController();

    void ConfigurePracticeRoute(ATrafficLanePath* InLanePath, float InStartingDistance);

protected:
    virtual void Tick(float DeltaSeconds) override;

private:
    void StopVehicleInput() const;

    UPROPERTY(Transient)
    TObjectPtr<ATrafficLanePath> LanePath;

    float StartingDistance = 0.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot", meta = (ClampMin = "100.0"))
    float LookAheadDistance = 1200.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Practice Bot", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DriveThrottle = 0.82f;
};
