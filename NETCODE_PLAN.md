# OVERLANE NETCODE: DECISION AND PLAN

## 1. DECISION

**Build Design A's spine — fixed-timestep prediction with input replay against a sequenced server acknowledgement — but delete its Phase 3 entirely (the traffic pose recording and `StagePoses`/`RestoreRenderPoses` staging), replacing it with Design C's client-side traffic extrapolation plus a *collision-suppressed* replay rule.** Call it **Predictive Reconciliation with a Scalar Ack (PRSA)**.

**The judges disagreed on every design, and each one was disqualified by someone.** Judge 1 (correctness) disqualified B/OSV because the client authors the 0.45× collision cut — the quantity that literally decides a dense-traffic race — and the cumulative `MaxPossibleX` bound that was supposed to backstop it is arithmetically wrong in the direction that steals wins. Judge 2 (feasibility) disqualified A because its distinctive mechanism lands last, costs ~72 `SetActorTransform` calls per correction frame, and rests on an unverified assumption that Chaos scene queries see a kinematic body moved within the same frame — a failure that produces no error, only slightly-wrong jitter, which is exactly what a solo dev cannot attribute. Judge 3 conditionally disqualified C because it makes the predicted sweep `ECR_Ignore` rivals, so racing wheel-to-wheel with a friend produces drive-throughs followed by snaps.

**I resolved it by observing that the three disqualifications are not symmetric.** Judge 1's kill on B is *structural* — you cannot bolt server authority over collisions onto a design whose premise is that the client's collision verdict wins. Judge 3's kill on C is a *one-line tunable* (don't ignore rivals). Judge 2's kill on A is a *removable phase* — A's ring buffer, ack channel and fixed step are cheap and land early; only §5 is expensive and risky, and §5 exists solely to make replay self-consistent. I found a cheaper way to get replay self-consistency (§3 below), which deletes the disqualifying phase and leaves A's genuinely-correct core.

**The single reason:** only input replay reconstructs the client's un-acknowledged state *correctly during transients*, and transients — accelerating, braking, clipping a van — are the entire content of this game. Judge 1's arithmetic on C is decisive and I verified the inputs: C's headline mechanism assigns a `CurrentSpeed` that is one-way-latency stale, which at 150 ms RTT and `BrakingDeceleration = 7200` (`ArcadeHandlingComponent.h:48`) injects a ~540 cm/s sawtooth every packet, and it silently violates C's own stated invariant that nothing may write `CurrentSpeed`. C's claim that an input ring buffer *"would buy you those 5-20 cm and nothing else"* is the one load-bearing assertion in the three documents that is simply false. The ring buffer is ~40 lines. Buy it.

### Grafted, with attribution

| From | What | Why |
|---|---|---|
| **C** | Replicate traffic lane speed (`int16`), client-side kinematic extrapolation capped at 250 ms, **no** client-side traffic AI | All three judges named this the highest-value single graft. Collapses client traffic error from ~175 cm to ~15 cm. Ships on its own merits. |
| **C** | Server-ghost debug overlay (`overlane.Net.DrawCorrection`) | All three judges demanded it. Ship it *before* enforcement. |
| **C** | Inverted visual absorber — smooth the collision **root**, lag the mesh only on hard snaps | Correct for this game: the player threads gaps by looking at their own car. |
| **C** | Absolute rule: the correction path never writes `CurrentSpeed` | It gates boost eligibility (`ArcadeHandlingComponent.cpp:76`), camera FOV (`OverlaneVehiclePawn.cpp:243-248`), and the near-miss speed floors. |
| **C** | Behaviour-neutral wire-format migration step | Makes "did I break replication" answerable in isolation. |
| **C** | Carry `bDrivingAllowed` in the ack, not just the GameState | Removes a two-channel race on the pause gate. |
| **B** | Per-player `AOverlanePlayerState` race state | The largest live race-integrity defect. Verified: `OverlaneGameModeBase.cpp:153` scores only `GetPlayerPawn(this, 0)`, and the winner scan at `:162-171` is **nested inside that `if`** — if the host's pawn is momentarily null, nobody can win and the race hangs. |
| **B** | `AckedCorrectionId` echo + 200 ms client send-suppression after a force-snap | Prevents the infinite rubber-band. A's `bForceSnap` has no handshake. |
| **B** | Log-only-before-enforcing discipline, applied to the dead zone | Thresholds in this class of system can only be measured, not derived. |
| **B** | Client control RPCs; `ReturnToMainMenu` `ServerTravel` branch | Today a joined client's only exit is Alt+F4 (`OverlanePlayerController.cpp:229-349` all no-op; `OverlaneGameModeBase.cpp:818-821` uses `OpenLevel` unlike `RestartRace` at `:806-813`). |
| **B + C** | Cooldown on the 0.45× cut, making it a countable discrete event | Fixes the live `0.45^N` frame-rate bug **and** makes A's `CollisionEventCount` a robust event match. |
| **Judge 3** | Evaluate `ShouldHoldForPlayer` client-side for the **local player only**, feeding the extrapolator | Sharp catch: hold lateral clearance is 310 uu (`TrafficVehicleBase.cpp:595`) vs `MaximumNearMissLateralDistance` 300 uu — the server freezes traffic during essentially *every* near miss, so naive extrapolation overshoots ~70 cm in the dangerous direction, precisely during scoring. |
| **Judge 3** | Rivals: extrapolate to now, don't interpolate at a fixed 100 ms delay; and **do** block against them in prediction | Rival contact is cosmetic-only (`OverlaneVehiclePawn.cpp:396-401`), so an approximate block costs nothing and beats driving through your friend. |

**Rejected outright:** B's owner-authored collision verdict; B's `MaxPossibleX` route integral (the bound integrates the *ceiling* while the player averages far less, so headroom grows monotonically — ~250-300 m of banked free teleport over a 3-minute run on the 5995 m route); A's `TrafficPoses` recording and pose staging; C's raw scalar assignment; C's `ECR_Ignore` against rivals.

**One correction to the code map, verified in the engine source:** the claim that the owner also pays for `FRepMovement` on its own pawn is **wrong**. `E:\EPIC_GAMES-games\UE_5.8\Engine\Source\Runtime\Engine\Private\ActorReplication.cpp:580-581` registers `ReplicatedMovement` with `COND_SimulatedOrPhysics`. Design A was right. There is no double-send to fix — do not spend a day "optimising" it.

---

## 2. THE DETERMINISM PREREQUISITE

Step zero regardless of architecture: split `UArcadeHandlingComponent::TickComponent` (`E:\Overlane\Source\Overlane\Private\ArcadeHandlingComponent.cpp:57-130`) into an **accumulator** and a **pure step function**.

### 2.1 New file: `E:\Overlane\Source\Overlane\Public\OverlaneNetTypes.h`

```cpp
#pragma once
#include "CoreMinimal.h"
#include "OverlaneNetTypes.generated.h"

/** The one true simulation rate. Server, client prediction and replay all use this. */
inline constexpr float OverlaneFixedDeltaSeconds = 1.0f / 60.0f;
inline constexpr int32 OverlaneMaxStepsPerFrame  = 5;    // spiral-of-death guard
inline constexpr int32 OverlaneMoveRingSize      = 128;  // 2.13 s of history
inline constexpr int32 OverlaneMaxBatchCommands  = 12;   // 200 ms of redundancy

UENUM()
enum class EOverlaneStepMode : uint8 { Live, Replay };

USTRUCT()
struct FOverlaneInputCommand
{
    GENERATED_BODY()
    UPROPERTY() uint16 Sequence = 0;
    UPROPERTY() uint8  Throttle = 0;    // 0..255  -> 0..1
    UPROPERTY() uint8  Brake    = 0;    // 0..255  -> 0..1
    UPROPERTY() int8   Steering = 0;    // -127..127 -> -1..1
    UPROPERTY() uint8  Flags    = 0;    // bit0 = boost requested

    float GetThrottle() const { return Throttle * (1.0f / 255.0f); }
    float GetBrake()    const { return Brake    * (1.0f / 255.0f); }
    float GetSteering() const { return FMath::Clamp(Steering * (1.0f / 127.0f), -1.0f, 1.0f); }
    bool  IsBoostRequested() const { return (Flags & 0x01) != 0; }

    static FOverlaneInputCommand Make(uint16 Seq, float T, float B, float S, bool bBoost);
};

/** Everything SimulateStep reads or writes. This IS the vehicle. */
USTRUCT()
struct FOverlaneVehicleSimState
{
    GENERATED_BODY()
    FVector Location             = FVector::ZeroVector;
    float   Yaw                  = 0.0f;
    float   CurrentSpeed         = 0.0f;   // cm/s, signed
    float   BoostCharge          = 1.0f;
    float   CollisionCutCooldown = 0.0f;
    uint8   CollisionEventCount  = 0;      // wrapping
    bool    bBoostActive         = false;
};

/** Server -> owning client. Replaces OwnerServerTransform entirely. ~15 bytes. */
USTRUCT()
struct FOverlaneMoveAck
{
    GENERATED_BODY()
    UPROPERTY() uint16 Sequence            = 0;   // last command the server consumed
    UPROPERTY() FVector_NetQuantize100 Location = FVector::ZeroVector;
    UPROPERTY() uint16 YawQ                = 0;   // 65536ths of a turn
    UPROPERTY() int16  SpeedCms            = 0;   // exact: -1200..6800 fits int16
    UPROPERTY() uint8  BoostChargeQ        = 255;
    UPROPERTY() uint8  CollisionEventCount = 0;
    UPROPERTY() uint8  CorrectionEpoch     = 0;
    UPROPERTY() uint8  Flags               = 0;   // b0 BoostActive, b1 DrivingAllowed,
                                                  // b2 InputStarved, b3 ForceSnap
};

USTRUCT()
struct FOverlaneMoveBatch
{
    GENERATED_BODY()
    UPROPERTY() uint16 BaseSequence = 0;
    UPROPERTY() uint8  AckedCorrectionEpoch = 0;   // graft from B: the anti-rubber-band echo
    UPROPERTY() TArray<FOverlaneInputCommand> Commands;   // <= OverlaneMaxBatchCommands
};
```

Ship these as plain `USTRUCT`s. **Do not write `NetSerialize` bit-packing yet** — measure first (see §5).

### 2.2 New public API on `UArcadeHandlingComponent`

```cpp
// ArcadeHandlingComponent.h
public:
    void SimulateStep(const FOverlaneInputCommand& Cmd, float FixedDt, EOverlaneStepMode Mode);
    FOverlaneVehicleSimState GetSimState() const;
    void SetSimState(const FOverlaneVehicleSimState& In);   // used by reconcile; teleports the pawn
    uint8 GetCollisionEventCount() const { return CollisionEventCount; }

protected:
    UPROPERTY(EditAnywhere, Category="Arcade Handling|Collision", meta=(ClampMin="0.0"))
    float CollisionCutCooldownDuration = 0.12f;

private:
    float StepAccumulator       = 0.0f;
    float CollisionCutCooldown  = 0.0f;
    uint8 CollisionEventCount   = 0;
```

### 2.3 The accumulator

`TickComponent` (`ArcadeHandlingComponent.cpp:57`) becomes only this:

```cpp
void UArcadeHandlingComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                             FActorComponentTickFunction* Fn)
{
    Super::TickComponent(DeltaTime, TickType, Fn);

    if (!IsDrivingAllowedHere())      // replaces :61-66, see Step 1
    {
        bBoostActive = false;
        StepAccumulator = 0.0f;       // do not bank paused time
        return;
    }
    if (!ShouldSimulateHere())        // replaces :68-72, see Step 7
    {
        return;
    }

    StepAccumulator += FMath::Min(DeltaTime, 0.25f);   // clamp a hitch or a breakpoint

    int32 Steps = 0;
    while (StepAccumulator >= OverlaneFixedDeltaSeconds && Steps < OverlaneMaxStepsPerFrame)
    {
        FOverlaneInputCommand Cmd;
        if (!ConsumeNextCommand(Cmd))   // server: jitter queue. client/standalone: live cache.
            break;
        SimulateStep(Cmd, OverlaneFixedDeltaSeconds, EOverlaneStepMode::Live);
        StepAccumulator -= OverlaneFixedDeltaSeconds;
        ++Steps;
    }
    if (Steps >= OverlaneMaxStepsPerFrame)
        StepAccumulator = 0.0f;         // drop the backlog rather than spiral
}
```

`SimulateStep` is `ArcadeHandlingComponent.cpp:74-129` **verbatim**, with `DeltaTime` → `FixedDt`, the four input members read from `Cmd`, and exactly one behavioural change at the tail:

```cpp
    FHitResult Hit;
    VehiclePawn->AddActorWorldOffset(
        VehiclePawn->GetActorForwardVector() * CurrentSpeed * FixedDt, /*bSweep=*/true, &Hit);

    CollisionCutCooldown = FMath::Max(0.0f, CollisionCutCooldown - FixedDt);
    if (Mode == EOverlaneStepMode::Live
        && Hit.bBlockingHit
        && FMath::Abs(Hit.ImpactNormal.Z) < 0.7f
        && CollisionCutCooldown <= 0.0f)
    {
        CurrentSpeed *= CollisionSpeedMultiplier;          // 0.45f, .h:75
        CollisionCutCooldown = CollisionCutCooldownDuration;
        ++CollisionEventCount;                             // wrapping uint8
    }
```

The `Mode == Live` guard is the mechanism that makes §3 work. Note the sweep stays on in `Replay` — it only stops the move; it never cuts speed.

### 2.4 What happens to every `FInterpTo` currently in the integrator

There are exactly two, and **neither is removed**. Fixed dt freezes their step size, which is the whole fix:

| Call | Line | Disposition |
|---|---|---|
| `FMath::FInterpTo(CurrentSpeed, MaxForwardSpeed, DeltaTime, 1.35f)` — turbo-release decay | `ArcadeHandlingComponent.cpp:103` | `DeltaTime` → `FixedDt`. Becomes a fixed 2.25%-toward-target lerp per step. Deterministic by construction. |
| `FMath::FInterpConstantTo(CurrentSpeed, 0.0f, DeltaTime, CoastDeceleration)` — coast | `ArcadeHandlingComponent.cpp:115` | `DeltaTime` → `FixedDt`. Already near-exact; now also step-invariant at the terminal clamp. |
| `FInterpTo` on `CameraBoom->TargetArmLength` / `FieldOfView` | `OverlaneVehiclePawn.cpp:247-248` | **Stays out of the step function**, on variable render dt. Cosmetic. Must never enter sim state or a 144 fps player's camera becomes a 60 Hz staircase. |
| `FMath::FInterpTo(CurrentSpeed, TargetSpeed, DeltaSeconds, ResponseSpeed)` in traffic | `TrafficVehicleBase.cpp:642` | **Stays on variable dt, deliberately.** Traffic is never re-simulated by a client under this design. Explicit non-goal. |

### 2.5 How much does this actually change solo feel? Less than all three designs claimed.

I ran the numbers, because all three plans budget a retune pass and two of them treat it as a project risk.

- **`FInterpTo` at `:103`:** `1.35 × dt` is small at both rates. Over one second the remaining fraction is `e^-1.357 ≈ 0.257` at 144 fps and `e^-1.366 ≈ 0.255` at 60 fps. **Under 1% difference.** Invisible.
- **Euler yaw-then-translate turn radius:** the realised path is a regular polygon; `R_polygon / R_true = θ / (2 sin(θ/2)) ≈ 1 + θ²/24`. At cruise (`5000 cm/s`, `SpeedRatio = 0.735`, `SteeringScale = Lerp(1.0, 0.32, 0.735) = 0.50`, so `52.5 °/s`), one 60 Hz step is `θ = 0.875° = 0.0153 rad` → radius error **1 part in 10⁵**. The code map's "a 144 fps player literally corners differently" is true and quantitatively negligible.
- **The 0.45× cut:** this is the *only* material one, and it is large. It fires once per rendered frame, so a 30 ms clip costs 4 cuts at 144 fps (`0.45⁴ = 0.041×` speed) and 2 at 60 fps (`0.20×`). **~2.4× harsher at high frame rate.** The `CollisionCutCooldownDuration = 0.12f` makes that clip cost exactly one cut at any frame rate.

**Therefore:** the retune is a single-knob job — `CollisionSpeedMultiplier` and `CollisionCutCooldownDuration` — not a whole-model rebalance. Budget half a day, not a week. This materially de-risks Step 0 relative to what all three plans assumed.

One caveat verified from the header: `MaxForwardSpeed = 5000.0f` (`ArcadeHandlingComponent.h:39`) is hardcoded a second time as `5000.0f * RubberBandedScale` in `OverlaneBotDriverController.cpp:447`. Change both or neither.

---

## 3. THE TRAFFIC PROBLEM — DECIDED

**The client predicts collisions by sweeping against extrapolated traffic. Replay re-sweeps against live traffic with the speed cut suppressed. There is no traffic recording, no pose staging, and no client-side traffic AI.**

Five decided rules:

### 3.1 Shrink the disagreement first (from C — do this before anything else)

Add `UPROPERTY(Replicated) int16 ReplicatedLaneSpeed` to `ATrafficVehicleBase`, register it alongside the existing five at `TrafficVehicleBase.cpp:679-687`, write it from `CurrentSpeed` each server tick. Replace the bare `if (!HasAuthority()) return;` at `TrafficVehicleBase.cpp:624-627` with `TickClientExtrapolation(DeltaSeconds)`: pure kinematics along the actor's forward vector, capped at 250 ms, exponentially blended back onto each new snapshot, with `OnRep_ReplicatedMovement` resetting the base.

This is strictly better than A's plan to infer velocity from two position samples, because `ReplicatedLaneSpeed` already encodes the `ShouldHoldForPlayer` freeze (`MoveAlongTrafficPath` sets `CurrentSpeed = 0.0f` at `TrafficVehicleBase.cpp:578`) and the `FInterpTo` acceleration at `:642`. Client traffic error drops from ~175 cm (velocity × latency) to ~15 cm (acceleration × latency²).

**Do NOT re-run `ComputeFollowSpeedLimit` or the director client-side.** `ShouldHoldForPlayer` iterates `World->GetPlayerControllerIterator()` (`TrafficVehicleBase.cpp:598`) — the host sees every player, a client sees only itself. Identical code, permanently different answer.

### 3.2 One exception: evaluate the hold locally, for the local player only

Judge 3's catch, and it is the sharpest in the set. Hold clearance is 540 × 310 uu (`TrafficVehicleBase.cpp:594-595`); the near-miss lateral window is 300 uu. **The server freezes a traffic car dead during essentially every near miss.** Naive extrapolation overshoots by up to ~70 cm at the Sport profile, and it overshoots in the dangerous direction — the client believes it has clearance it does not have, precisely during the scoring moment.

So: in `TickClientExtrapolation`, zero the extrapolation velocity when the *locally-predicted* player pose falls inside the 540 × 310 uu box. That is exactly what the server will compute for that player. Rival-induced holds remain unpredicted and arrive one packet late as `ReplicatedLaneSpeed → 0`.

### 3.3 Live prediction sweeps and cuts; replay sweeps and does not cut

- **Live forward step** (`EOverlaneStepMode::Live`, every fixed tick on the owning client): sweep ON against extrapolated traffic, 0.45× cut ON, local cosmetic feedback ON, scoring OFF.
- **Replay step** (`EOverlaneStepMode::Replay`, only when reconcile exceeds the dead zone): sweep ON against *live* traffic, **cut OFF**, feedback OFF, scoring OFF.

**Why this is correct and not a hack.** A's §5 exists to guarantee replay self-consistency (its R1). The instability it was solving comes from one thing only: the 0.45× cut is a *discrete branch* that can flip between the original prediction and the replay, producing a 55% speed swing from a hairline geometric difference. Remove the cut from replay and the only remaining nondeterminism is a continuous, monotone position clamp — "did the sweep stop me short." Replay becomes stable by construction. And being stopped short by a car that is there *right now* is the correct answer regardless of what was true 100 ms ago.

The speed information the cut would have produced is not lost: `FOverlaneMoveAck::SpeedCms` carries the server's authoritative post-cut speed at the acked sequence, and replay re-integrates forward from it using inputs only. That is exactly the reconstruction C claimed was unnecessary and Judge 1 proved was necessary.

Cost: ~12 iterations of scalar math plus 12 sweeps on the correction frame. No `SetActorTransform` storm, no restore pass, no assumption about scene-query freshness.

### 3.4 Bias mispredictions toward false negatives (from A, kept)

1. **Client-only lateral inset.** In `ApplyTrafficVisualState` (`TrafficVehicleBase.cpp:304`), when `!HasAuthority()`, set the box extent to `ReplicatedCollisionExtent - FVector(0, 12.0f, 0)`. Marginal grazes stop being predicted; would-be ghost collisions become late server collisions.
2. **Freshness gate.** Do not predict against a car whose `bTrafficActive` flipped within the last net interval. `OnRep_TrafficActive → ApplyTrafficActiveState` toggles visibility and collision in one shot (`TrafficVehicleBase.cpp:294-298`), so a recycled car can materialise solid inside the predicted path with zero lead time.
3. **Cancellable feedback.** Tag the predicted impact flash with the step that produced it; if a later ack shows `CollisionEventCount` did not advance across it, cancel the flash instead of running the full duration.
4. **`SetGenerateOverlapEvents(false)` on `NearMissTrigger` when `!HasAuthority()`** (`TrafficVehicleBase.cpp:49-57`). Near-miss scoring is server-only anyway; this stops prediction from generating overlap storms.

### 3.5 Rivals block, and are extrapolated to now

Reject C's `ECR_Ignore`. Rival contact routes to `RegisterRivalImpact` (`OverlaneVehiclePawn.cpp:396-401`) — a feedback colour and nothing else: no speed change, no score. So a predicted rival block is *purely cosmetic* and costs at worst a small position error that reconciliation erases. Driving through your friend is worse.

Render rivals by extrapolating `ReplicatedSpeedKph` (already replicated unconditionally, `OverlaneVehiclePawn.cpp:270`) to *now*, not by interpolating at a fixed 100 ms delay. Two cars going the same direction have small relative velocity, so extrapolation error is small, whereas a hard 100 ms delay is a fixed ~1.4 m misplacement during exactly the drafting and side-by-side battles the mode exists for. Reserve the delay buffer for starvation.

**This decision has a shelf life, and I am naming it now:** if rival contact ever affects speed or position (ramming, blocking, drafting), rivals must move into the replayed simulation with replicated rival inputs, and that is a real project. The signal is a product decision, not a bug report.

---

## 4. IMPLEMENTATION PLAN

Two kill switches exist from Step 1 onward: `overlane.Net.PredictLocalVehicle` (default 0) and `overlane.Net.ExtrapolateTraffic` (default 0). **Single-player takes the `NM_Standalone` branch at every gate and never enters any new network path.**

---

### Step 0 — Fixed timestep, cut cooldown, event counter
**Files:** `Public/OverlaneNetTypes.h` (new), `Public/ArcadeHandlingComponent.h`, `Private/ArcadeHandlingComponent.cpp:57-130`
**Change:** §2 in full. `SimulateStep` + accumulator + `GetSimState`/`SetSimState` + `CollisionCutCooldownDuration = 0.12f` + wrapping `CollisionEventCount`. Retune `CollisionSpeedMultiplier` (`.h:75`) against the new cadence; if you touch `MaxForwardSpeed` (`.h:39`), also touch `OverlaneBotDriverController.cpp:447`.
**Why:** every design needs it, and it fixes a live single-player bug — the `0.45^N` scrape penalty is ~2.4× harsher at 144 fps than at 60 fps.
**Accept:** a 30 ms barrier scrape costs exactly one speed cut at both `t.MaxFPS 60` and `t.MaxFPS 144`, and lap times over the route match within 1%.
**Single-player: YES — improved.** ~1.5 days.

---

### Step 1 — Gate migration + CVars
**Files:** `Private/ArcadeHandlingComponent.cpp:61-66`
**Change:** new `IsDrivingAllowedHere()` — try `GetAuthGameMode<AOverlaneGameModeBase>()->IsDrivingAllowed()`; fall back to `GetGameState<AOverlaneRaceGameState>()->IsRaceActive() && !IsRacePaused()` (`OverlaneRaceGameState.h:16-18`); return true if neither exists. Register both CVars.
**Why:** `GetAuthGameMode` is null on clients, so the pause gate is silently a no-op there today, masked only by the authority gate two lines below. The moment the component ticks on a client (Step 7) that becomes a real bug.
**Accept:** host pauses mid-race → both windows' cars stop; unpause → both resume. Solo pause unchanged.
**Single-player: YES — unchanged.** 0.5 day.

---

### Step 2 — Sequenced, redundant input batches
**Files:** `Public/OverlanePlayerController.h:23-24,44-47`, `Private/OverlanePlayerController.cpp:149-201`
**Change:** the four `Handle*` functions keep writing the cached scalars but **stop calling `SubmitVehicleInput`** (`:152,158,164,170`). Instead sample the cache once per fixed tick into an `UnackedCommands` ring. Delete `ServerSetVehicleInput`; add `UFUNCTION(Server, Unreliable, WithValidation) ServerSendMoveBatch(FOverlaneMoveBatch)` at 30 Hz carrying **every** unacked command (cap 12). Server side: merge into a per-player jitter queue keyed on sequence, drop `Sequence <= LastConsumed`, reject beyond `LastConsumed + 24`. `ConsumeNextCommand` pops one per fixed step (two when the queue exceeds 8, capped) and sets `bInputStarved` when repeating the last command. Keep the clamps at `:196-199`.
**Why:** highest value-per-hour in the plan. Kills the stuck-throttle-on-one-dropped-packet bug (releasing the last input produces exactly one unreliable RPC with no resend, timeout or heartbeat), kills the out-of-order reapplication bug, and kills the ~432 RPC/s flood from unthrottled `ETriggerEvent::Triggered`.
**Accept:** with `NetEmulation.PktLoss 20`, hold W then release — the client car coasts to a stop; it never runs away.
**Single-player: YES — standalone bypasses the RPC and reads the cache directly.** 2 days.

---

### Step 3 — Traffic lane speed + client extrapolation
**Files:** `Public/TrafficVehicleBase.h`, `Private/TrafficVehicleBase.cpp:624-627, 679-687`
**Change:** §3.1 and §3.2. Behind `overlane.Net.ExtrapolateTraffic`.
**Why:** makes multiplayer traffic look right *today*, independent of prediction, and is the precondition for prediction agreeing with the server about contact.
**Accept:** at `NetEmulation.PktLag 75`, a client's view of a traffic car passing a fixed world marker lags the host's by under 20 cm (measure with the overlay in Step 5, or `stat` a debug line).
**Single-player: YES — the `HasAuthority()` path is untouched.** 1 day.

---

### Step 4 — Wire format swap, behaviour-neutral
**Files:** `Public/OverlaneVehiclePawn.h:173-174`, `Private/OverlaneVehiclePawn.cpp:36, 235-241, 273, 276-282`
**Change:** delete `OwnerServerTransform`, its `DOREPLIFETIME_CONDITION` at `:273` and `OnRep_OwnerServerTransform` at `:276-282`. Add `UPROPERTY(ReplicatedUsing=OnRep_ServerMoveAck) FOverlaneMoveAck ServerMoveAck` with `COND_OwnerOnly`, written in the `HasAuthority()` block at `:235-241`. Drop `SetNetUpdateFrequency` from 60 to 30 (`:36`). `OnRep_ServerMoveAck` for now does exactly what the old one did: `SetSimState` + hard `SetActorTransform`.
**Why:** isolates "did I break replication" from "did I break the feel." Bandwidth: an unquantised `FTransform` under LWC is 10 doubles = 80 B at 60 Hz (~4.8 KB/s); the ack is ~15 B at 30 Hz (~450 B/s) and carries strictly more — speed, boost, collision count, driving-allowed, epoch. ~10× on the channel that most needs headroom. (Do **not** also chase `FRepMovement` for the owner — `ActorReplication.cpp:580-581` is `COND_SimulatedOrPhysics`; there is nothing to save.)
**Accept:** two-window PIE looks pixel-identical to before; `stat net` shows owner-channel bytes down ~10×.
**Single-player: YES — untouched.** 1.5 days.

---

### Step 5 — Debug overlay / server ghost
**Files:** `Private/OverlaneHUD.cpp` (extend the existing traffic debug overlay path), `Private/OverlaneVehiclePawn.cpp`
**Change:** `overlane.Net.DrawCorrection` — a wireframe box at the acked server transform, plus longitudinal / lateral / yaw error, corrections/sec, snaps/min, replay length, measured RTT, and per-connection ack rate.
**Why:** all three judges independently insisted on this, and they are right. Without the ghost you cannot distinguish a genuine divergence from a projection artefact, and the next three steps are pure tuning. **Ship the overlay before the correction, not after.**
**Accept:** with `overlane.Net.PredictLocalVehicle 0` the ghost sits exactly on the car (error ≈ 0) because the client is a pure echo.
**Single-player: YES — overlay is inert in standalone.** 1 day.

---

### Step 6 — Authority hygiene (prerequisite; prediction turns each of these into a disaster)
**Files:** `Private/OverlaneVehiclePawn.cpp:332-336, 388-402, 404-438`, `Private/ArcadeHandlingComponent.cpp:36-45`, `Private/OverlanePlayerController.cpp:203-225`
**Change:**
1. Split `RegisterTrafficImpact` into `PlayPredictedImpactFeedback()` (local; colour, timer, camera kick — runs anywhere) and an authoritative half wrapped in `if (HasAuthority())` covering `MarkPlayerCollision()` at `:420`, `ActiveTrafficCollisionContacts` at `:423-428`, and `GameMode->RegisterTrafficCollision()` at `:434-437`. **This fixes a live bug:** `MarkPlayerCollision` is guarded only by `!bIsAIRacer`, never by `HasAuthority()`, so a client permanently disarms that car's near-miss locally.
2. Route recovery through `UFUNCTION(Server, Reliable) ServerRequestRecovery()` with a 5 s cooldown; remove the unguarded client fall-through at `OverlanePlayerController.cpp:221-224`. Replicate `RecoveryTransform` from the server (today it is captured per-machine at `OverlaneVehiclePawn.cpp:385`, *before* `RestartPlayer`'s lane offset at `OverlaneGameModeBase.cpp:113-120`). Server replies with `bForceSnap`.
3. Stop `ResetState()` refilling `BoostCharge = 1.0f` (`ArcadeHandlingComponent.cpp:43`) during an active race — pressing R is a free full turbo bar today, and on the host it runs server-side with no cooldown.
**Accept:** on a client, R teleports to the correct lane once, with no snap-back, no boost refill, and 5 s of cooldown. Spamming R changes nothing.
**Single-player: YES — the exploit fix is a solo improvement.** 1.5 days.

---

### Step 7 — Prediction ON, reconcile in LOG-ONLY mode
**Files:** `Private/ArcadeHandlingComponent.cpp:68-72`, `Private/OverlanePlayerController.cpp:173-183`, `Public/OverlaneMovePredictionComponent.h` (new) + `Private/` counterpart, `Private/OverlaneVehiclePawn.cpp:204`
**Change:** `ShouldSimulateHere()` returns `HasAuthority() || NM_Standalone || (IsLocallyControlled() && GetLocalRole() == ROLE_AutonomousProxy && CVarPredict)`. `SubmitVehicleInput` applies locally whenever `IsLocalController()`. New `UOverlaneMovePredictionComponent` created next to `ArcadeHandling` at `OverlaneVehiclePawn.cpp:204`, holding the 128-entry ring of `{Command, StateAfter}`, registered with `AddTickPrerequisiteComponent(ArcadeHandling)` — tick order is undeclared today and with prediction that ambiguity is a frame of latency you cannot see. Switch `GetSpeedKph`/`GetBoostChargeRatio`/`IsBoostActive` (`:251-264`) and `GetForwardSpeedCms` (`:296-299`, currently missing the branch and returning 0 on clients) onto `ShouldSimulateHere()`.
**`Reconcile()` measures and logs the error distribution but does not correct.** The old hard snap stays.
**Why:** zero input latency lands here. Log-only is not optional: the dead-zone thresholds can only be measured.
**Accept:** a week of real sessions with friends; the overlay's error histogram shows a tight cluster near zero with outliers only around traffic contact and packet loss.
**Single-player: YES — standalone takes the `NM_Standalone` branch.** 2 days + a week of calendar.

---

### Step 8 — Reconcile enforcement + replay + smoothing
**Files:** `Private/OverlaneMovePredictionComponent.cpp`
**Change:** dead zone from the measured distribution (start at 3 cm / 0.3° / 8 cm/s, plus `CollisionEventCount` must match). Outside it: `SetSimState(Ack)`, then `SimulateStep(..., EOverlaneStepMode::Replay)` for every command after `Ack.Sequence` still in the ring. Hard snap above 500 cm, on `CorrectionEpoch` change, or on `bForceSnap`. Event-driven snap when `Ack.CollisionEventCount` exceeds what the client can account for — correction latency drops from ~5 frames to ~1 packet. Echo `AckedCorrectionEpoch` in every batch and suppress sends for 200 ms after a forced snap; the server ignores divergence until it sees the echo. Asymmetric smoothing: forward corrections 150 ms, backward 300 ms. **The correction path never writes `CurrentSpeed`** — speed is adopted at the acked sequence and re-integrated, never nudged.
**Accept:** at `NetEmulation.PktLag 75 / PktLagVariance 20 / PktLoss 5`, clean traffic-free driving produces zero snaps per minute; dense traffic produces under 3 snaps/min; no oscillation after a forced snap.
**Single-player: YES.** 2.5 days.

---

### Step 9 — VisualRoot absorber
**Files:** `Private/OverlaneVehiclePawn.cpp:45-100, 206-211`
**Change:** insert `USceneComponent* VisualRoot` between `VehicleCollision` and the visuals; reparent `VehicleMesh`, `CabinMesh`, `FrontLightBar`, `RearLightBar`, the four wheels **and `CameraBoom`**. Smooth the collision **root** to the corrected pose; engage `VisualRoot` mesh lag (clamped 250 cm / 20°, decayed over 0.15 s) **only on a hard snap**.
**Why:** C's inversion of the `CharacterMovement` pattern is correct here — the player threads gaps by looking at their own car, so the collision root and the visible body must never disagree except when a visible teleport is the worse alternative.
**Accept:** a forced snap is a visible glide, not a jump; the car's rendered position is where its collision is during all non-snap corrections.
**Single-player: YES — `VisualRoot` offset is permanently zero in standalone.** 1 day.

---

### Step 10 — Per-player race state + client control RPCs
**Files:** `Public/OverlanePlayerState.h` (new), `Private/OverlaneGameModeBase.cpp:141-178, 818-821`, `Private/OverlanePlayerController.cpp:229-349`, `Public/OverlaneVehiclePawn.h:144-159`
**Change:** move `RaceScore`, `NearMissCount`, `CollisionCount`, `MaxSpeedKph` onto a replicated `AOverlanePlayerState`; iterate all controllers in `Tick`. **Lift the winner scan at `:162-171` out of the `if (PlayerVehicle = GetPlayerPawn(this, 0))` block at `:153`** — today a null host pawn means nobody can ever win and the race hangs forever. Add `Server, Reliable` RPCs for pause / settings / return-to-menu, each gated on `IsLocalController()` plus a host check for the destructive ones. Fix `ReturnToMainMenu` (`:818-821`) to branch to `ServerTravel` on `NM_ListenServer`, matching `RestartRace` at `:806-813`.
**Why:** the largest race-integrity defect in the codebase, and the reason a joined client's only exit is Alt+F4.
**Accept:** a joined client finishes with a non-zero score, a correct near-miss count, can pause, and can return to the menu without stranding the host.
**Single-player: YES — single controller, same numbers.** 2 days.

---

### Step 11 — Rival proxies + boost visibility
**Files:** `Private/OverlaneVehiclePawn.cpp:271-272`, new `OnRep_ReplicatedMovement` override
**Change:** 3-sample pose buffer extrapolated to now from `ReplicatedSpeedKph`, falling back to a delay buffer only on starvation. Lift `ReplicatedBoostCharge` and `bReplicatedBoostActive` off `COND_OwnerOnly` so rival turbo VFX and audio become possible at all.
**Accept:** at 75 ms lag, a rival drafting 5 m ahead does not visibly stutter, and their boost flare is visible.
**Single-player: YES — no proxies exist.** 1.5 days.

**Total: ~17-18 working days plus one calendar week of log-only play.** Steps 0-6 and 10 all improve the game with `overlane.Net.PredictLocalVehicle 0`, so the working build is never blocked behind Steps 7-9.

---

## 5. WHAT NOT TO BUILD YET

| Do not build | Signal that makes it necessary |
|---|---|
| **Deterministic/seeded traffic; removing `bAlwaysRelevant` (`TrafficVehicleBase.cpp:37`)** | The traffic pool target exceeds ~20 cars, **or** `stat net` shows traffic above ~50% of downstream at 4 players. Today it is 6 cars (`TrafficDirector.cpp:29-40`) and affordable. This is the single biggest deferred item and it is genuinely the right call if "DENSE" becomes literal — but it composes cleanly on top of PRSA, because prediction against a deterministic world collapses §3 to "just simulate traffic locally." |
| **A's `TrafficPoses` recording + `StagePoses`/`RestoreRenderPoses`** | The Step 5 overlay shows replay-attributable jitter that survives Step 3's extrapolation *and* the collision-suppressed replay rule. Measure before building. Judge 2's silent-failure argument stands: verify Chaos scene-query freshness with a standalone test before ever committing to this. |
| **B's validator: `MaxPossibleX` route integral, corridor checks, yaw-rate caps, `ValidateCollisionPlausibility`** | Leaderboards, ranked play, public matchmaking, or unlocks gated on results. Under PRSA the client sends only clamped inputs (`OverlanePlayerController.cpp:196-199`) and the server owns every collision, so the entire class this validator defends against does not exist. Building it now is anti-cheat for a threat model with two to four friends in it. |
| **`NetSerialize` bit-packing on the three structs** | `stat net` shows the move batch or ack material against your budget. Ship plain `USTRUCT`s; B is right that you measure first. |
| **Rival contact physics (ramming, blocking, drafting with speed effects)** | A product decision, not a bug. It inverts §3.5 and forces rivals into the replayed simulation with replicated rival inputs. Answer "will racing my friends ever involve *touching* my friends?" before Step 7, because the answer changes Step 11. |
| **Dedicated servers, host input delay, replays, spectating** | Competitive fairness becomes a stated requirement. PRSA narrows the host advantage (zero latency *and* ground-truth traffic) but structurally cannot close it. |
| **Lateral velocity / drift in the handling model** | Purely a design decision — and note PRSA is *insulated* from it in a way C is not. C's whole accuracy argument depends on the car having no lateral velocity so a straight-line projection is valid. PRSA replays actual inputs through the actual integrator, so drift costs it nothing. This is a real, under-appreciated advantage of the chosen synthesis. |
| **Re-enabling traffic lane changes in multiplayer** | Currently forced off (`TrafficDirector.cpp:39`). Re-enabling exposes the unseeded `FMath::RandBool()` at `TrafficDirector.cpp:431` and makes §3.1's straight-line extrapolation wrong. Needs a seeded stream and a replicated lane-change intent first. |

---

## 6. TEST PROCEDURE

### 6.1 PIE setup

`Play` ▸ `Advanced Settings`:
- **Net Mode:** `Play As Listen Server`, **Number of Players:** 2 (3 for the pack-racing cases in Steps 8 and 11).
- **Run Under One Process:** ON for fast iteration; **turn it OFF at least once per step** — single-process PIE shares statics and can mask a genuine desync.

### 6.2 Network emulation — exact console commands

UE 5.x CVars (typed into either PIE window's console; requires `DO_ENABLE_NET_TEST`, which is on in Development editor builds):

```
NetEmulation.PktLag 75              // one-way ms; 75 => ~150 ms RTT
NetEmulation.PktLagVariance 20      // jitter, +/- ms
NetEmulation.PktLoss 5              // percent, outgoing
NetEmulation.PktOrder 1             // reorder packets
NetEmulation.PktDup 1               // percent duplicated
NetEmulation.PktIncomingLoss 5      // asymmetric: loss on the receive side
NetEmulation.PktIncomingLagMin 60
NetEmulation.PktIncomingLagMax 100
```

Presets (defined in `BaseEngine.ini` under `[PacketSimulationProfile]`):
```
NetEmulation.PktEmulationProfile Average
NetEmulation.PktEmulationProfile Bad
NetEmulation.PktEmulationProfile Off
```

Legacy exec form also still works on the net driver: `Net PktLag=75`, `Net PktLoss=5`, `Net PktLagVariance=20`.

Measurement:
```
stat net        // in/out bytes, per-channel; this is your bandwidth acceptance gate
stat unit       // game thread cost during replay bursts
Net Status      // connection state, ping
t.MaxFPS 60     // and 144 — the frame-rate fairness cases in Step 0
```

Overlane-specific, from Step 5 on:
```
overlane.Net.DrawCorrection 1
overlane.Net.PredictLocalVehicle 0|1
overlane.Net.ExtrapolateTraffic 0|1
```

### 6.3 Per-step validation

| Step | Procedure | Pass |
|---|---|---|
| 0 | Solo. Scrape a barrier for ~30 ms at `t.MaxFPS 60`, then at `t.MaxFPS 144`. Log `CollisionEventCount`. | Count identical at both rates; route lap times within 1%. |
| 1 | 2-window. Host pauses mid-race. | Both cars stop dead; both resume on unpause. |
| 2 | `NetEmulation.PktLoss 20`. Client holds W for 3 s, releases, hands off keyboard. | Car coasts to a stop. Repeat 20×, zero runaways. Then `stat net`: outgoing RPC count is ~30/s, not ~432/s. |
| 3 | `NetEmulation.PktLag 75`. Park host and client cameras on the same traffic car passing a world marker. | Client's car is within 20 cm of the host's at the crossing. Toggle the CVar to see the ~175 cm baseline. |
| 4 | `stat net` before and after; visual A/B of the two windows. | Owner-channel bytes down ~10×; behaviour indistinguishable from Step 3. |
| 5 | `overlane.Net.PredictLocalVehicle 0`, `DrawCorrection 1`. | Ghost box sits exactly on the client's car; error readout ≈ 0. |
| 6 | Client presses R mid-race, then spams it. | One teleport to the correct lane, no snap-back, no boost refill, 5 s cooldown honoured. |
| 7 | `PktEmulationProfile Average`, real sessions, log-only. | Error histogram tight near zero; outliers only at contact and loss. Read the thresholds off it — do not derive them. |
| 8 | `PktLag 75 / PktLagVariance 20 / PktLoss 5`. 3 min clean driving, then 3 min in dense traffic. Then force a recovery snap. | 0 snaps/min clean, <3 snaps/min dense, no oscillation after the forced snap (this is the `AckedCorrectionEpoch` test — remove the echo and it should visibly rubber-band, which is how you know the echo is load-bearing). |
| 9 | Trigger a hard snap with `PktLoss 40` for 2 s. | Glide, not a jump; car body and collision box coincident during all sub-snap corrections. |
| 10 | Client races to the finish and wins. Then: kill the host's pawn reference mid-race (spawn debug) and let a client reach `RouteFinishX`. | Client shows a non-zero score and near-miss count; the race still finishes with the host pawn absent (the current build hangs). |
| 11 | 3 windows. Draft a rival at 5 m, `PktLag 75`. Rival boosts. | No visible stutter; boost flare visible on the rival. |

**Regression gate, run after every single step:** launch standalone (`Play` ▸ `Standalone Game`), race the full route against the bot, finish, restart. If solo ever breaks, the step is wrong — every gate in this plan is written so that standalone takes the `HasAuthority()` or `NM_Standalone` branch and never enters new code.