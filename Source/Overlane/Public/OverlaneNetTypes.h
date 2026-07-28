#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "OverlaneNetTypes.generated.h"

/**
 * The one true simulation rate. The server, the owning client's prediction and
 * any replay all advance the vehicle in steps of exactly this size, so the same
 * inputs produce the same result regardless of frame rate.
 */
inline constexpr float OverlaneFixedDeltaSeconds = 1.0f / 60.0f;

/** Spiral-of-death guard: never run more than this many steps in one frame. */
inline constexpr int32 OverlaneMaxStepsPerFrame = 5;

/** 128 steps is 2.13 s of input history, comfortably beyond any playable ping. */
inline constexpr int32 OverlaneMoveRingSize = 128;

/** Redundancy per batch: 12 commands is 200 ms, so a lost packet is recoverable. */
inline constexpr int32 OverlaneMaxBatchCommands = 12;

UENUM()
enum class EOverlaneStepMode : uint8
{
    /** Normal forward simulation: collisions may cut speed and produce feedback. */
    Live,

    /**
     * Re-simulating unacknowledged input after a correction. The sweep still
     * blocks, but the collision speed cut is suppressed: it is a discrete branch
     * that would otherwise flip on a hairline geometric difference and swing the
     * speed by 55%, which is what makes naive replay unstable.
     */
    Replay
};

/** One fixed-step slice of player intent. Quantised because it goes on the wire. */
USTRUCT()
struct FOverlaneInputCommand
{
    GENERATED_BODY()

    UPROPERTY()
    uint16 Sequence = 0;

    UPROPERTY()
    uint8 Throttle = 0;

    UPROPERTY()
    uint8 Brake = 0;

    UPROPERTY()
    int8 Steering = 0;

    /** bit 0 = boost requested. */
    UPROPERTY()
    uint8 Flags = 0;

    float GetThrottle() const { return Throttle * (1.0f / 255.0f); }
    float GetBrake() const { return Brake * (1.0f / 255.0f); }
    float GetSteering() const { return FMath::Clamp(Steering * (1.0f / 127.0f), -1.0f, 1.0f); }
    bool IsBoostRequested() const { return (Flags & 0x01) != 0; }

    static FOverlaneInputCommand Make(uint16 InSequence, float InThrottle, float InBrake, float InSteering, bool bBoost)
    {
        FOverlaneInputCommand Command;
        Command.Sequence = InSequence;
        Command.Throttle = static_cast<uint8>(FMath::RoundToInt(FMath::Clamp(InThrottle, 0.0f, 1.0f) * 255.0f));
        Command.Brake = static_cast<uint8>(FMath::RoundToInt(FMath::Clamp(InBrake, 0.0f, 1.0f) * 255.0f));
        Command.Steering = static_cast<int8>(FMath::RoundToInt(FMath::Clamp(InSteering, -1.0f, 1.0f) * 127.0f));
        Command.Flags = bBoost ? 0x01 : 0x00;
        return Command;
    }
};

/**
 * Everything SimulateStep reads or writes. This struct IS the vehicle as far as
 * the simulation is concerned; anything not in here is cosmetic.
 */
USTRUCT()
struct FOverlaneVehicleSimState
{
    GENERATED_BODY()

    UPROPERTY()
    FVector Location = FVector::ZeroVector;

    UPROPERTY()
    float Yaw = 0.0f;

    /** cm/s, signed. */
    UPROPERTY()
    float CurrentSpeed = 0.0f;

    UPROPERTY()
    float BoostCharge = 1.0f;

    UPROPERTY()
    float CollisionCutCooldown = 0.0f;

    /** Wraps deliberately: it is only ever compared for equality across a step. */
    UPROPERTY()
    uint8 CollisionEventCount = 0;

    UPROPERTY()
    bool bBoostActive = false;
};

/** Server to owning client. Replaces the raw transform channel entirely. */
USTRUCT()
struct FOverlaneMoveAck
{
    GENERATED_BODY()

    /** The last command the server actually consumed. */
    UPROPERTY()
    uint16 Sequence = 0;

    UPROPERTY()
    FVector_NetQuantize100 Location = FVector::ZeroVector;

    /** Yaw in 65536ths of a turn. */
    UPROPERTY()
    uint16 YawQ = 0;

    /** Exact: the -1200..6800 cm/s range fits an int16 with room to spare. */
    UPROPERTY()
    int16 SpeedCms = 0;

    UPROPERTY()
    uint8 BoostChargeQ = 255;

    UPROPERTY()
    uint8 CollisionEventCount = 0;

    UPROPERTY()
    uint8 CorrectionEpoch = 0;

    /** b0 BoostActive, b1 DrivingAllowed, b2 InputStarved, b3 ForceSnap. */
    UPROPERTY()
    uint8 Flags = 0;
};

/** Owning client to server: every command the client has not seen acked yet. */
USTRUCT()
struct FOverlaneMoveBatch
{
    GENERATED_BODY()

    UPROPERTY()
    uint16 BaseSequence = 0;

    /** Echoed back so the server can tell a correction was received. */
    UPROPERTY()
    uint8 AckedCorrectionEpoch = 0;

    UPROPERTY()
    TArray<FOverlaneInputCommand> Commands;
};
