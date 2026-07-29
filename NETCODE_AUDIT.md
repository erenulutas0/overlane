# OVERLANE — Prediction Readiness: Triage, Fix List, N‑007 / N‑008 Specification

I read the netcode-relevant files in full at `E:\Overlane\Source\Overlane\` and verified every claim I use as load-bearing. Where I could not confirm something, or where I think an audit is wrong, I say so explicitly in §1.4.

---

## 1. TRIAGE

### 1.0 The one thing to understand before reading the rest

There are exactly **two** classes of defect that make prediction impossible, and both audits found them from different angles without quite naming the shared root:

**The server does not simulate one step per command.** It simulates one step per 1/60 s of wall clock, plus up to two extra steps per *frame* when the queue is deep, and it simulates steps for which no command exists at all. The client will simulate exactly one step per command. Two machines running different numbers of steps on the same input cannot be reconciled, no matter how good the replay loop is.

**The ack does not describe a single simulation state.** `FOverlaneMoveAck` omits a field that `SimulateStep` both reads and writes, and its `Sequence` does not advance monotonically with the state it labels.

Everything else in Tier 0 is downstream of those two.

---

### 1.1 Tier 0 — prediction is not merely buggy but *meaningless* until these land

| # | Defect | Location | Verified? |
|---|---|---|---|
| T0‑1 | Starvation repeats a command, simulates a full step, and never advances `LastConsumedSequence` | `ArcadeHandlingComponent.cpp:220-227` | **Confirmed** |
| T0‑2 | Catch-up runs 2 steps *per frame*, not per second, and never debits `StepAccumulator` | `ArcadeHandlingComponent.cpp:175-186` | **Confirmed** |
| T0‑3 | `FOverlaneMoveAck` omits `CollisionCutCooldown`; every ack zeroes it on the client | `OverlaneNetTypes.h:111-143`, `OverlaneVehiclePawn.cpp:438-446`, `ArcadeHandlingComponent.cpp:116` | **Confirmed** |
| T0‑4 | Client generates and transmits input with the driving gate closed | `OverlanePlayerController.cpp:194` | **Confirmed** |
| T0‑5 | `UnackedCommands` is a fixed 200 ms window, never trimmed by the ack; the 128-entry ring at `OverlaneNetTypes.h:18` is declared and unused | `OverlanePlayerController.cpp:219-222` | **Confirmed** |
| T0‑6 | Sequence 0 is simultaneously "none" and legal; accept/consume sequence state lives on two objects that reset independently | `OverlanePlayerController.h:74,77` vs `ArcadeHandlingComponent.h:140`; `OverlanePlayerController.cpp:252-256` | **Confirmed** |
| T0‑7 | **The three prediction-error getters measure the wrong quantity** | `OverlaneVehiclePawn.cpp:481-497` | **Confirmed — not reported by any audit** |

**T0‑1 and T0‑2 in detail.** `ConsumeNextCommand` returns `LastConsumedCommand` without popping when the queue is empty (`ArcadeHandlingComponent.cpp:220-227`) and the caller then runs a full `SimulateStep`. The arithmetic over one wall second: 60 accumulator steps run; 60 commands arrive; `60 − S` are popped, where `S` is the number of starved steps. The queue therefore *ratchets upward by S*, crosses the `> 8` threshold at `:176`, and the catch-up loop drains it with steps that are never debited from `StepAccumulator`. Net: the server simulates `60 + S` steps per wall second. At `MaxForwardSpeed = 5000 cm/s` one step is 83 cm. Since the game mode decides the winner purely from `PlayerPawn->GetActorLocation().X` (`OverlaneGameModeBase.cpp:166`), the jitterier connection literally wins races today. And the client will simulate exactly 60.

The catch-up rate is `2 × FrameRate` steps/s: 180/s total at a 60 fps host, 300/s at 120 fps. Audit 2's cheat framing is correct (a client that fills every batch with fresh sequences instead of the redundant window passes `ServerSendMoveBatch_Validate` at `:245-248` and every `Delta` check at `:262-268`), but **do not fix this for anti-cheat reasons** — fix it because an honest client on a jittery connection hits the same path.

**T0‑7 is mine and it is the one most likely to cost a day.** `GetPredictionErrorLongitudinalCm()` returns `dot(GetActorLocation() − ServerMoveAck.Location, Forward)`. The header at `OverlaneVehiclePawn.h:58-70` says it "must read zero while the client is a pure echo, which is the check that the measurement itself is trustworthy" — that is true *today* and false the instant prediction turns on. Under prediction that number is the client's *legitimate lead* over the server's acknowledged state: one-way latency + up to one 33 Hz net-update period (`OverlaneVehiclePawn.cpp:50`) + the jitter-buffer depth. At 100 ms RTT and 5000 cm/s that is roughly **8 metres**, and it is correct behaviour. The HUD line at `OverlaneHUD.cpp:343-353` will scream. If a dead zone is sized against that number, N‑008 will never correct anything.

The real reconciliation error is *the client's stored state for sequence N* versus *the server's state for sequence N*. Nothing in the codebase computes it, because nothing stores per-sequence state.

---

### 1.2 Tier 1 — must land before enforcement (N‑008); can ship alongside N‑007

| # | Defect | Location | Verified? |
|---|---|---|---|
| T1‑1 | `ServerMoveAck.Flags` rebuilt with `=` every Tick, so `ForceSnap` and `DrivingAllowed` can never reach the wire; `CorrectionEpoch` / `AckedCorrectionEpoch` are dead protocol | `OverlaneVehiclePawn.cpp:272-274`; grep confirms zero writers of `CorrectionEpoch` | **Confirmed** |
| T1‑2 | `SendAccumulator = 0.0f` instead of `-=`, so the send rate is frame-quantised and never actually 30 Hz | `OverlanePlayerController.cpp:224-229` | **Confirmed** |
| T1‑3 | Both accumulators zero their remainder on the 5-step cap, asymmetrically | `OverlanePlayerController.cpp:211-214`, `ArcadeHandlingComponent.cpp:166-170` | **Confirmed** |
| T1‑4 | `RecoverToStart` leaves `PendingCommands` / `LastConsumedCommand` / `bCommandDriven` intact | `OverlaneVehiclePawn.cpp:596-602` | **Confirmed** |
| T1‑5 | `RecoveryTransform` captured in `BeginPlay` before the lane shift, and it is the *start line* | `OverlaneVehiclePawn.cpp:651` vs `OverlaneGameModeBase.cpp:113-120` | **Confirmed** |
| T1‑6 | `SpeedCms` truncates toward zero; the comment claims "Exact" | `OverlaneVehiclePawn.cpp:269`, `OverlaneNetTypes.h:127` | **Confirmed** |
| T1‑7 | `BoostChargeQ` LSB (1/255) straddles the `BoostCharge > KINDA_SMALL_NUMBER` branch | `OverlaneVehiclePawn.cpp:270` vs `ArcadeHandlingComponent.cpp:250` | **Confirmed** |
| T1‑8 | `bBoostActive` written outside `SimulateStep`; `IsDrivingAllowedHere` returns **true** with no GameState | `ArcadeHandlingComponent.cpp:134`, `:87` | **Confirmed** |
| T1‑9 | `EnqueueCommand` silently drops the *oldest* queued commands on overflow | `ArcadeHandlingComponent.cpp:195-198` | **Confirmed** |
| T1‑10 | No server-side cap on commands accepted per second per connection | `OverlanePlayerController.cpp:250-279` | **Confirmed** |
| T1‑11 | **Replay will re-fire `OnComponentHit` up to 12× per correction** | `OverlaneVehiclePawn.cpp:654-668` + `ArcadeHandlingComponent.cpp:298` | **Confirmed — not reported by any audit; N‑008 creates it** |

**T1‑11.** Today a client never sweeps, so `VehicleCollision->OnComponentHit` (bound at `OverlaneVehiclePawn.cpp:618`) never fires there. The moment the client runs `SimulateStep`, the swept `AddActorWorldOffset(..., true, &Hit)` at `ArcadeHandlingComponent.cpp:298` starts dispatching blocking hits — including on every replayed step. `HandleVehicleHit → RegisterTrafficImpact` runs the colour flash and `TriggerImpactShake` *above* the `!HasAuthority()` return at `:691`, and `TriggerImpactShake` has no cooldown (`:391-401`). A single correction with a 12-deep window would fire the shake twelve times.

This has a silver lining worth stating: audit 4's "impact flash and camera shake are unreachable on a client" **self-fixes under N‑007**. Do not build the proposed `CollisionEventCount`-delta plumbing. (The *near-miss* flash does not self-fix — `NearMissTrigger->SetGenerateOverlapEvents(HasAuthority())` at `TrafficVehicleBase.cpp:396` is deliberate — that half belongs to N‑010.)

---

### 1.3 Tier 2 — confirmed, real, does **not** block prediction

Ordered by how much they will distort the N‑007 measurements.

1. **Traffic on clients sits `v × OneWayLatency` behind truth.** `TrafficVehicleBase.cpp:765-796`. I re-derived audit 3's fixed point and it holds: `OnRep_ReplicatedMovement` re-derives `ClientSmoothingOffset` from the live pose each snapshot (`:737-739`), so with bleed rate `r` and snapshot period `T`, `e_{n+1} = e_n(1−k) + vL·k` where `k = 1 − e^{−rT}`, giving `e* = vL` exactly. The extrapolation and the smoothing cancel in steady state; what actually gets removed is only the inter-snapshot sawtooth (`vT/2`, ~35 cm at 2100 cm/s). The *shape* is confirmed; the quoted 105–157 cm assumes a 75 ms one-way that has never been measured. **This will dominate the N‑007 error histogram in dense traffic and it is not a model error.** Handle it by measurement design (§5), not by fixing it first.
2. `GetClampedToMaxSize(400.0f)` **rescales** rather than thresholds, so every pool recycle places a car exactly 4 m off. `TrafficVehicleBase.cpp:739`. Two-line fix, visible bug, do it.
3. `ReplicatedLaneSpeed` is written at `:826` before `MoveAlongTrafficPath` at `:860` zeroes `CurrentSpeed` on a freeze, so a stopped car never replicates 0. Two-line fix that also deletes `ShouldHoldForLocalPlayer` (`:748-763`) and its server-disagreement problem entirely.
4. `TickClientExtrapolation` has no pause gate (`:765`) while the server's Tick early-returns at `:812-816`.
5. `ActivateOnLane` calls `ForceNetUpdate()` (`:199`) carrying the previous incarnation's `ReplicatedLaneSpeed`.
6. `PoolAnchorRacer` stores an `int32` index into `CachedRacers`, which is `Reset()` and rebuilt from `GetAllActorsOfClass` every 0.5 s (`TrafficDirector.cpp:252` vs `:165`). Active slots never re-pick (`:85` only refreshes inactive ones). Fires on any mid-race disconnect.
7. Recycling judges only the anchor racer (`TrafficDirector.cpp:279`), so cars deactivate in plain sight of everyone else.
8. Cosmetic steer/roll dead on every networked machine (`ArcadeHandlingComponent.cpp:245` never writes `SteeringInput`; consumers at `OverlaneVehiclePawn.cpp:333,345`). **The client's own car self-fixes under N‑007** if the component owns command generation (§3). The server's view of *remote* pawns needs the one-liner.
9. Camera boom impact roll goes into the yaw slot: `FRotator(Pitch, Yaw, Roll)` misused at `OverlaneVehiclePawn.cpp:369-372`. One line.
10. `TriggerImpactShake` has no cooldown (`:391-401`) — reintroduces the frame-rate dependence the `CollisionCutCooldownDuration` comment at `ArcadeHandlingComponent.h:115-125` says was removed.
11. No tick prerequisite anywhere in the module between traffic and the pawn sweep. Both default to `TG_PrePhysics`. Real for replay determinism, cheap to declare.
12. Player 4 spawns on top of player 2 (`OverlaneGameModeBase.cpp:113-114`).
13. Per-player race state: `RaceScore`/`CollisionCount`/`MaxSpeedKph` computed from `GetPlayerPawn(this, 0)` (`OverlaneGameModeBase.cpp:153-159`), and the winner scan at `:162-171` is nested inside that null check. This is the **biggest product defect in the build** and has nothing to do with prediction. N‑010.
14. Near-miss state is one flag per traffic car (`TrafficVehicleBase.h:231-233`), so with 2–4 humans one steals or cancels another's.
15. Collision cut has no `bStartPenetrating` / minimum-speed guard (`ArcadeHandlingComponent.cpp:306-314`).
16. Pitch/roll outside `FOverlaneVehicleSimState` (`OverlaneNetTypes.h:86-97`) — harmless while the model is planar, uncorrectable the day it is not.
17. Dead write at `OverlanePlayerController.cpp:273`; unreachable `break` guards at `ArcadeHandlingComponent.cpp:155,179`; `RefreshRacerCache` rescanning when the list is legitimately empty (`TrafficDirector.cpp:153`).

---

### 1.4 Reported defects I could **not** confirm, or that I downgrade

**Rejected as multiplayer defects (single-player only):**

- **"42 always-relevant traffic actors at 30 Hz, ~25 KB/s per client."** Wrong for the case that matters. `TrafficDirector.cpp:29-40` forces `VehiclesPerLane = 2` in multiplayer, and `GetSlotsPerLane() = VehiclesPerLane × RacerSupplyCapacity` (`TrafficDirector.h:63`) = 4. The **multiplayer pool is 12 cars, not 42**, with lane changes off. The 42-car figure applies only to single-player, where there is no client. Audit 3 acknowledged the override and then used 42 as the headline anyway. Scale every traffic-cost claim by 12/42.
- **"SlotPriority makes 21 of 42 cars unable to lane-change... and any multiplayer race once `bEnableLaneChanges = false` is lifted."** The first half is right, the second is wrong. `SlotPriority[i] % GetSlotsPerLane()` (`TrafficDirector.cpp:447`) is an identity only when `GetSlotsPerLane() == 14`. In multiplayer it is 4, and `{5,3,0,6,4,1,2} % 4 = {1,3,0,2,0,1,2}` covers every slot. **Single-player only.**
- **`PickAnchorRacerForSpawn` per-frame cost** and **`RacerSupplyCapacity` doubling density with one racer**: both confirmed, both single-player-only in practical terms.

**Confirmed but reframed:**

- **The catch-up "cheat."** Mechanism confirmed exactly as described — `Validate` at `:247` only bounds `Num()`, and per-command `Delta` never exceeds 1 for a fabricated stream. But the same path is hit by honest jittery clients, and *that* is why it blocks prediction. Filing it as a security item would get it deprioritised, which would be the wrong outcome.
- **`ShouldHoldForLocalPlayer` divergence.** Confirmed (server tests `TargetTransform.GetLocation()` at `:670` post-advance, client tests `GetActorLocation()` at `:759`; a client's `GetPlayerControllerIterator()` contains only itself). But fixing item 3 above (move the `ReplicatedLaneSpeed` write to after `MoveAlongTrafficPath`) makes the whole predicate deletable, so don't fix it in place.

**Independently verified as correct — do not spend time here:**

- **`SetReplicateMovement(true)` is not a competing authority channel for the owner.** I checked the engine directly: `E:\EPIC_GAMES-games\UE_5.8\Engine\Source\Runtime\Engine\Private\ActorReplication.cpp:580-581` registers `ReplicatedMovement` with `COND_SimulatedOrPhysics`. An autonomous proxy never receives it. `NETCODE_PLAN.md:33` already says this. There is no double-send to optimise.
- **The traffic client inset and the overlap disable really do reach clients.** I doubted this because `ApplyTrafficVisualState()` is only invoked from the constructor (`TrafficVehicleBase.cpp:174`) and the RepNotifies. `TrafficVehicleBase.h:200-210` confirms all four visual properties use `ReplicatedUsing = OnRep_TrafficVisualState`, and every profile colour at `TrafficDirector.cpp:315-319` differs from the CDO's `FLinearColor::White`, so the notify always fires on channel open. The claim stands.
- **The `int16` sequence-wrap idiom** at `OverlanePlayerController.cpp:262` is correct, and there is exactly one comparison site in the codebase.

**Not verified (and not load-bearing):** audit 2's `p^6` loss-probability table and its per-frame-rate send-cadence table. The conclusion those support — redundancy is generous against loss, jitter handling is the problem — is independently supported by the code, so I did not re-derive them.

---

## 2. THE FIX LIST

Ordered so each step is independently testable. Steps 1–7 are the prediction prerequisite; 8–13 are the enforcement prerequisite; 14–19 are cheap correctness that pays for itself during testing.

### Phase A — make the server simulate one step per command (prerequisite for N‑007)

**A1. `ArcadeHandlingComponent.cpp:175-186` — delete the catch-up loop entirely.**
Not rate-limited, not budgeted — deleted. Its job is taken over by A2.

**A2. `ArcadeHandlingComponent.cpp:208-233`, `ConsumeNextCommand` — starvation debt.**
New private members: `int32 StarveDebt = 0;` and `int32 ServerBufferTarget = 2;`.

- Queue non-empty *and* `StarveDebt > 0`: **pop and discard** one command without simulating — set `LastConsumedCommand`/`LastConsumedSequence` from it, `--StarveDebt`, and re-enter. The discarded command's step was already spent as a repeat; simulating it too is the free-distance ratchet.
- Queue depth `> ServerBufferTarget` (or `StarveDebt == 0` and depth `> 0`): pop and simulate as today.
- Queue empty: repeat `LastConsumedCommand`, `++StarveDebt` (clamped at 30 = 0.5 s), set `bInputStarved`. Beyond the clamp, return `false` and **do not simulate** — hold.
- Make the function's contract honest: it must now be able to return `false`, which makes the guards at `:155` and `:179` live.

After this the ack's `(Sequence, State)` is a bijection with **no new wire fields**, and total simulated steps equal wall-clock steps exactly. This single change resolves T0‑1 and T0‑2 and removes the exploit.

**A3. `ArcadeHandlingComponent.cpp:166-170` and `OverlanePlayerController.cpp:211-214` — stop discarding the accumulator remainder.**
Delete both `= 0.0f` resets; the `-=` in each loop already preserves the remainder. Raise `OverlaneMaxStepsPerFrame` (`OverlaneNetTypes.h:15`) from 5 to 8 and keep the 0.25 s input clamp at `ArcadeHandlingComponent.cpp:149`. Time is then only lost below 7.5 fps, and it is lost identically on both machines because both read the same constant.

**A4. `ArcadeHandlingComponent.cpp:195-198` — overflow drops to the target depth, not the oldest one command, and raises a correction.**
`PendingCommands.SetNum(ServerBufferTarget)` keeping the newest, then signal a `ForceSnap` + epoch bump (see B1). A silent discontinuity in the consumed sequence is exactly what the epoch exists to announce.

**A5. `OverlanePlayerController.cpp:250-279`, `ServerSendMoveBatch_Implementation` — rate limit and pawnless advance.**
- Token bucket: refill at `1/OverlaneFixedDeltaSeconds` per second, burst 4, capped at 90 accepted commands/s per connection. Reject the surplus.
- Advance `LastAcceptedSequence` even when `GetPawn()` is null (`:252-256`), so the pawnless window on join does not manufacture a `Delta > 24` resync on every client.
- Delete the dead write at `:273`.

### Phase B — make the ack a complete, unambiguous description of the state

**B1. `OverlaneNetTypes.h:111-143` — complete `FOverlaneMoveAck`, and make omission impossible.**
Add `uint8 CollisionCutCooldownQ` (quantised as `Cooldown / CollisionCutCooldownDuration × 255`). Add `ToSimState(float CooldownDuration) const` and `static FOverlaneMoveAck FromSimState(...)` as member functions on the struct, and route `OverlaneVehiclePawn.cpp:266-274` and `:438-446` through them. `FOverlaneVehicleSimState` has seven fields; the ack must carry seven. A `static_assert(sizeof(FOverlaneVehicleSimState) == ...)` is brittle; a converter pair plus a one-screen round-trip test in a `Development` build is not.

**B2. `OverlaneVehiclePawn.cpp:269-270` — fix the two quantisation defects.**
`SpeedCms` → `RoundToInt` before the cast; correct the "Exact" comment at `OverlaneNetTypes.h:127` to state 1 cm/s. For boost, raise the threshold at `ArcadeHandlingComponent.cpp:250` from `KINDA_SMALL_NUMBER` to `0.01f` so an 8-bit LSB cannot straddle a discrete branch that swings `ActiveMaxSpeed` by 1800 cm/s. (Widening `BoostChargeQ` to `uint16` also works; raising the threshold is cheaper and also survives future re-quantisation.)

**B3. `OverlaneVehiclePawn.cpp:272-274` — make `Flags` sticky and write `CorrectionEpoch`.**
New pawn members `uint8 PendingAckFlags` and `uint8 CorrectionEpoch`. `Tick` becomes `ServerMoveAck.Flags = LiveBits | PendingAckFlags;` and `ServerMoveAck.CorrectionEpoch = CorrectionEpoch;`. `PendingAckFlags` is cleared only when a batch arrives echoing the matching epoch. Set b1 (`DrivingAllowed`) from the same `GetAuthGameMode` lookup the class already uses.

**B4. `ArcadeHandlingComponent.cpp:87` — return `false`, not `true`, when there is no GameState.**
A machine that cannot see the race state must not simulate.

**B5. `ArcadeHandlingComponent.cpp:132-141` — split the gate branch by authority.**
`bBoostActive = false` must move inside `SimulateStep`. `PendingCommands.Reset()` must be authority-only. `StepAccumulator = 0.0f` stays on both — dropping paused time is correct and symmetric.

### Phase C — give the client a real command stream

**C1. Move command generation from `AOverlanePlayerController` into `UArcadeHandlingComponent`.**
This is the structural change and it is worth the churn. Today `TickLocalCommandStream` (`OverlanePlayerController.cpp:197-230`) owns the accumulator and the sequence counter, and the pawn's component owns the step loop; under prediction those two accumulators would drift apart and produce commands that were never simulated and steps that had no command. Instead:

- The controller pushes cached scalars into the component every frame (it already has `SetThrottleInput` etc.; just stop gating them on `HasAuthority()`).
- The component's client branch generates `FOverlaneInputCommand::Make(NextPredictedSequence++, ...)` once per fixed step, simulates it, stores it, and appends it to an outbox.
- The controller drains the outbox into its send ring.

One command ⇔ one step, structurally. As a free side effect this fixes the dead cosmetic steer on the client's own car (Tier 2 #8), because the component is now sampling the same `SteeringInput` the consumers read.

**C2. `ArcadeHandlingComponent.cpp:245`, inside `SimulateStep` — add `SteeringInput = StepSteering;`.**
Fixes the server's view of remote pawns' wheels and body roll. The header comment at `ArcadeHandlingComponent.h:36-37` calling this "Cosmetic only" is already wrong (`ConsumeNextCommand:231` samples it as real sim input on the host); correct it while you are there.

**C3. `OverlanePlayerController.cpp:197` — gate `TickLocalCommandStream` on the driving gate.**
With C1 the component's own `IsDrivingAllowedHere()` handles it, but the controller must also stop *sending* while gated.

**C4. Reserve sequence 0 on both sides.**
`NextCommandSequence` already starts at 1 (`OverlanePlayerController.h:74`); skip 0 on wrap. `LastConsumedSequence == 0` keeps its "nothing consumed" meaning. Without this the stream breaks silently after 18.2 minutes.

**C5. `OverlanePlayerController.cpp:219-222` — split the ring from the send window.**
A 128-entry command ring (`OverlaneMoveRingSize`) trimmed only by `ServerMoveAck.Sequence` using the same signed-delta idiom; the outgoing batch is the newest `min(12, unacked)` entries. Above ~170 ms RTT the current 200 ms window is already too small for replay.

**C6. `OverlanePlayerController.cpp:224-229` — `SendAccumulator -= (1.0f/30.0f);` with a one-interval clamp.**
Or better: send every second generated command, locking the send phase to the simulation rather than the render frame.

### Phase D — announce every discontinuity (prerequisite for N‑008 only)

**D1. Bump `CorrectionEpoch` + raise `ForceSnap` on every discontinuity.**
Call sites: `RecoverToStart` (`OverlaneVehiclePawn.cpp:596`), `StopDriving`, the `Delta > 24` resync (`OverlanePlayerController.cpp:268`), the A4 overflow trim, and **every driving-gate transition** — race start, pause, unpause, finish (`OverlaneGameModeBase.cpp:136-139`, `:446-466`, `ToggleRacePause`).

Making race start a declared correction is the right answer to the "the gate opens on different steps on the two machines" problem. Do **not** try to make the gate step-exact by stamping it into the command: a cheat client could then claim the gate is open, and the server-side `AND` that fixes that reintroduces the same boundary skew. One announced correction per race, at the start line, while everyone is stationary, is invisible. It also exercises the handshake on every single race, which is the best possible test coverage.

**D2. `OverlaneVehiclePawn.cpp:596-602`, `RecoverToStart` — clear the input pipeline.**
`ArcadeHandling->ClearPendingCommands()`, reset `LastConsumedCommand` to a neutral command (not the last one), zero `StarveDebt`, bump the epoch. Split `ResetState` into `ResetSimState()` / `ResetInputStream()` so the two concepts stop being conflated.

**D3. `OverlaneGameModeBase.cpp:113-120` — fix the recovery pose and the lane table.**
Add `AOverlaneVehiclePawn::SetRecoveryTransform()` and call it after the Y shift; or better, recover **in place** at the nearest lane centre at the current X. Recovering to `RouteStartX = -299500` from the halfway mark is a 3 km reset that also zeroes `RaceScore` (`:155-156`). Index the spawn lane from a stable per-player slot over at least four distinct Y offsets, not `GetNumPlayers() % 2`.

**D4. `OverlaneVehiclePawn.cpp:670-715` — suppress cosmetic feedback during replay.**
Add `bool IsReplaying() const` to the component and early-out `RegisterTrafficImpact` on it. Independently, gate `TriggerImpactShake` on a change in `GetCollisionEventCount()` rather than on every hit event, which also fixes Tier 2 #10.

### Phase E — cheap, do them while you are in the files

**E1.** `TrafficVehicleBase.cpp:739` — threshold instead of rescale:
`ClientSmoothingOffset = (Error.SizeSquared() <= FMath::Square(400.f)) ? Error : FVector::ZeroVector;`
Also clear it and reset `ClientExtrapolationElapsed` in `OnRep_TrafficActive`.
**E2.** `TrafficVehicleBase.cpp:826` — move the `ReplicatedLaneSpeed` write below `MoveAlongTrafficPath` at `:860`, then delete `ShouldHoldForLocalPlayer` (`:748-763`) and its call at `:789`.
**E3.** `TrafficVehicleBase.cpp:178-200` — set `ReplicatedLaneSpeed` in `ActivateOnLane` and zero it in `DeactivateForPool`, before the `ForceNetUpdate`.
**E4.** `TrafficVehicleBase.cpp:765` — gate `TickClientExtrapolation` on the replicated `AOverlaneRaceGameState`, the same source `IsDrivingAllowedHere` uses.
**E5.** `TrafficDirector.h:147` — `PoolAnchorRacer` becomes `TArray<TWeakObjectPtr<AOverlaneVehiclePawn>>`.
**E6.** `TrafficDirector.cpp:279` — recycle only when the car is behind **every** cached racer.
**E7.** `OverlaneVehiclePawn.cpp:369-372` — `FRotator(-12.f + ImpactPitch*0.45f, 0.f, ImpactRoll*0.35f)`.
**E8.** `ArcadeHandlingComponent.h:42-49` — correct the "pure function" comment. It reads the live physics scene through the sweep at `:298`; traffic is the known divergence channel. That comment is currently a load-bearing lie.
**E9.** Add `AddTickPrerequisiteActor` from the pawn's handling component to the traffic actors, or move traffic to an earlier tick group. The order must be declared, not emergent from replication-spawn order.

---

## 3. N‑007 — PREDICTION ON, RECONCILE IN LOG-ONLY MODE

**Goal:** the owning client simulates its own vehicle; the reconciliation error is measured and recorded; **nothing the player sees changes**, because `OnRep_ServerMoveAck` still hard-snaps exactly as it does today.

### 3.1 CVars — reversibility at every step

| CVar | Default | Effect |
|---|---|---|
| `overlane.Net.Predict` | `0` | `0` = today's behaviour bit for bit: `ShouldSimulateHere()` false on clients, `OnRep` hard-snaps. `1` = client simulates. |
| `overlane.Net.Reconcile` | `0` | `0` = measure and log only, still hard-snap. `1` = enforce (N‑008). |
| `overlane.Net.LogCorrection` | `0` | Per-sample CSV to `LogOverlaneNet`. |
| `overlane.Net.ServerBufferTarget` | `2` | Server jitter-buffer depth (A2). Server-side; `ECVF_Cheat`. |
| `overlane.Net.DrawCorrection` | `0` | Existing (`OverlaneVehiclePawn.cpp:7-11`). |
| `overlane.Net.ExtrapolateTraffic` | `1` | Existing (`TrafficVehicleBase.cpp:721`). Becomes a measurement lever. |

`overlane.Net.Predict 0` must restore the current build's behaviour completely, including the send path. Read it once per frame into a cached bool on the component; do not call `GetValueOnGameThread()` inside the step loop.

### 3.2 What runs on the owning client

`ArcadeHandlingComponent.cpp:90-94`:

```cpp
bool UArcadeHandlingComponent::ShouldSimulateHere() const
{
    const APawn* P = Cast<APawn>(GetOwner());
    if (!P) { return false; }
    if (P->HasAuthority() || P->GetNetMode() == NM_Standalone) { return true; }
    return bPredictEnabled && P->IsLocallyControlled()
        && P->GetLocalRole() == ROLE_AutonomousProxy;
}
```

Client branch of `TickComponent`, after the existing gate at `:132`:

```
StepAccumulator += FMath::Min(DeltaTime, 0.25f);
while (StepAccumulator >= OverlaneFixedDeltaSeconds && Steps < OverlaneMaxStepsPerFrame)
{
    FOverlaneInputCommand Cmd = FOverlaneInputCommand::Make(
        NextPredictedSequence, ThrottleInput, BrakeInput, SteeringInput, bBoostRequested);
    if (++NextPredictedSequence == 0) { NextPredictedSequence = 1; }

    SimulateStep(Cmd, OverlaneFixedDeltaSeconds, EOverlaneStepMode::Live);
    StoreMove(Cmd, GetSimState());
    PendingOutbox.Add(Cmd);

    StepAccumulator -= OverlaneFixedDeltaSeconds;
    ++Steps;
}
```

The client never touches `PendingCommands` — that array stays server-only.

### 3.3 The state ring buffer

```cpp
USTRUCT()
struct FOverlanePredictedMove
{
    GENERATED_BODY()
    UPROPERTY() FOverlaneInputCommand      Command;
    UPROPERTY() FOverlaneVehicleSimState   StateAfter;   // all 7 fields, unquantised
    UPROPERTY() uint16                     Sequence = 0; // stored, so wrap collisions are detectable
    UPROPERTY() bool                       bValid   = false;
    UPROPERTY() bool                       bBlocked = false; // the Live sweep blocked this step
};
```

`TArray<FOverlanePredictedMove> MoveRing;` sized `OverlaneMoveRingSize` (128 = 2.13 s), indexed `Sequence % 128`, with the stored `Sequence` checked on lookup. ~50 bytes/entry → ~6.5 KB per pawn. Store `bBlocked` now even though only N‑008 reads it; adding it later means re-running the whole measurement pass.

### 3.4 How the error is measured

This is the heart of N‑007 and it must be measured **at the acked sequence**, not against the current pose (T0‑7). In `OnRep_ServerMoveAck`:

```
if (!bPredictEnabled) { SetSimState(Ack.ToSimState(CooldownDuration)); return; }   // reversibility

++AckCount;
if (Ack.Sequence == LastMeasuredSequence) { ++DuplicateAckCount; return; }
if (Ack.Flags & 0x04 /*InputStarved*/)     { ++StarvedAckCount; }                  // record, still measure
if (Ack.CorrectionEpoch != LocalEpoch)     { ++EpochResetCount; HardReset(); return; }

const FOverlanePredictedMove* Mine = FindMove(Ack.Sequence);
if (!Mine) { ++RingMissCount; HardReset(); return; }

const FOverlaneVehicleSimState Srv = Ack.ToSimState(CooldownDuration);
const FVector   D     = Mine->StateAfter.Location - Srv.Location;
const FRotator  SrvR(0.f, Srv.Yaw, 0.f);
Sample.ErrLongCm   = D | SrvR.Vector();
Sample.ErrLatCm    = D | FRotationMatrix(SrvR).GetScaledAxis(EAxis::Y);
Sample.ErrYawDeg   = FMath::FindDeltaAngleDegrees(Srv.Yaw, Mine->StateAfter.Yaw);
Sample.ErrSpeedCms = Mine->StateAfter.CurrentSpeed - Srv.CurrentSpeed;
Sample.dEvents     = (int8)(Mine->StateAfter.CollisionEventCount - Srv.CollisionEventCount);
Sample.UnackedDepth= CountMovesAfter(Ack.Sequence);

RecordSample(Sample);
LastMeasuredSequence = Ack.Sequence;

// N-007 ONLY: still hard-snap, exactly as OverlaneVehiclePawn.cpp:435-446 does today.
if (!bReconcileEnabled) { SetSimState(Srv); ResetRingTo(Ack.Sequence, Srv); }
```

The error is measured in the **server's** frame (`SrvR`), not the client's, so a yaw disagreement does not contaminate the longitudinal/lateral split.

Note the `ResetRingTo` on the hard-snap path: because N‑007 still snaps, the ring's stored history is invalidated every ack. Without this the next sample compares a post-snap prediction against a pre-snap history and reports garbage. This is the subtlety that makes "log-only" harder than it sounds.

### 3.5 Rename the misleading getters

Rename `GetPredictionError{Longitudinal,Lateral,Yaw}` (`OverlaneVehiclePawn.cpp:481-497`) to `GetServerLag{...}` — they measure the client's legitimate lead and remain useful for the ghost. Add `GetReconcileError{Longitudinal,Lateral,Yaw,Speed}` returning the last recorded sample. Update the HUD (`OverlaneHUD.cpp:343-353`) to show both, labelled distinctly:

```
NET  LAG +812cm   ERR +1.4/-0.2cm  YAW +0.01  DEPTH 9  ACK 4471
```

`LAG` should be large and grow with ping. `ERR` should be near zero. If a tester sees only one number they will read the wrong one.

### 3.6 What to log

Per sample, to `LogOverlaneNet` at `Verbose`:
`t, seq, depth, errLong, errLat, errYaw, errSpeed, dEvents, starved, ping`

Running histogram with `overlane.Net.DumpCorrectionStats` printing, per channel, p50 / p90 / p99 / max, plus counts of: acks, duplicate-sequence acks, starved acks, ring misses, epoch resets. Reset with `overlane.Net.ResetCorrectionStats`.

Also log, once per second: `HostX`, `ClientX` and their difference. This is the direct test for the free-distance ratchet, and it is the one number that cannot be faked by a well-behaved histogram.

### 3.7 What the numbers must look like before enforcement

**Read the zero-latency PIE case as a logic test, not a network test.** Both worlds run in one process on one CPU with one copy of the code, so the float path is bit-identical. A non-zero error there is a *logic* bug, and PIE will systematically **under-report** what two physical machines will show.

| Scenario | Metric | Gate |
|---|---|---|
| Clean PIE, 2 players, no emulation, clear road | p99 \|errLong\|, \|errLat\| | **≤ 2 cm** |
| " | p99 \|errYaw\| | **≤ 0.02°** |
| " | p99 \|errSpeed\| | **≤ 2 cm/s** (1 cm/s is the `SpeedCms` LSB floor) |
| " | duplicate-sequence acks | **≤ 1 %** of acks |
| " | ring misses, epoch resets (excluding race start) | **0** |
| " | HostX vs ClientX over 60 s | **within 0.5 %** |
| `Average` profile (60–120 ms RTT, 1 % loss) | p50 / p90 / p99 \|errLong\| | **≤ 5 / 25 / 80 cm** |
| " | p99 unacked depth | **≤ 12** (if it exceeds `OverlaneMaxBatchCommands` the send window is undersized) |
| `Bad` profile (200–400 ms RTT, 5 % loss) | p99 \|errLong\| | **≤ 300 cm** |
| " | ring misses | **≤ 0.1 %** |
| " | HostX vs ClientX | **within 1 %** |

If clean-PIE p99 is not near zero, **stop**. The fix list is incomplete and no threshold derived from a dirty measurement is worth anything.

Separately, run the same clean-PIE pass **through dense traffic** and with `overlane.Net.ExtrapolateTraffic 0` and `1`. The delta between clear-road and dense-traffic p99 is the geometric contribution (Tier 2 #1). Expect it to be the largest single term. Whether the traffic latency fix ships before N‑008 should be decided by that number, not by argument.

---

## 4. N‑008 — ENFORCEMENT, REPLAY AND SMOOTHING

### 4.1 The three-tier dead zone

| Band | CVar | Starting value | Action |
|---|---|---|---|
| Ignore | `overlane.Net.IgnoreCm` | **MEASURE** (≈ 3 × clean p99) | Nothing. No replay, no offset. |
| Smooth | `overlane.Net.SmoothCm` | ~250 cm | Adopt server state, replay, hide the delta visually. |
| Snap | `overlane.Net.SnapCm` | **MEASURE** (> `Bad`-profile p99) | Adopt, replay, no visual hiding, bump epoch. |
| Yaw ignore | `overlane.Net.IgnoreYawDeg` | 0.25° (guess) | |
| Yaw snap | `overlane.Net.SnapYawDeg` | 8° (guess) | |
| Speed ignore | `overlane.Net.IgnoreSpeedCms` | 15 cm/s (guess) | |
| Replay depth cap | `overlane.Net.MaxReplaySteps` | **MEASURE** (≈ p99 unacked depth) | Deeper → snap instead of replay. |

**Must be measured, not guessed:** `IgnoreCm` (it must sit above the noise floor and below what a player can feel — get it wrong low and you replay on every ack; wrong high and corrections accumulate), `SnapCm` (must sit above the p99 of *legitimate* corrections or the game snaps constantly on a bad connection), `MaxReplaySteps`, and `ServerBufferTarget` (from the observed starve rate versus depth).

**Legitimately guessed and hand-tuned:** the smoothing rate, the visual offset cap, and the yaw thresholds. These are feel, and no histogram will pick them.

Any *non-zero* `dEvents` (a collision one side saw and the other did not) forces a correction regardless of the position band. It is a discrete disagreement and smoothing it is meaningless.

### 4.2 The replay loop

```cpp
void UArcadeHandlingComponent::ReconcileTo(const FOverlaneVehicleSimState& Srv, uint16 AckedSeq)
{
    const FVector PreLoc = GetOwner()->GetActorLocation();
    const float   PreYaw = GetOwner()->GetActorRotation().Yaw;

    TGuardValue<bool> ReplayGuard(bReplaying, true);   // suppresses OnComponentHit feedback (D4)

    SetSimState(Srv);                                   // the ONLY place a client overwrites its own sim state

    uint16 Seq = AckedSeq + 1;
    if (Seq == 0) { Seq = 1; }
    int32 Replayed = 0;
    while (Replayed < MaxReplaySteps)
    {
        FOverlanePredictedMove* M = FindMoveMutable(Seq);
        if (!M) { break; }                              // hole in history: stop, never invent input
        SimulateStep(M->Command, OverlaneFixedDeltaSeconds, EOverlaneStepMode::Replay);
        M->StateAfter = GetSimState();                  // REWRITE history with the corrected result
        if (++Seq == 0) { Seq = 1; }
        ++Replayed;
    }

    ClientVisualOffset    = PreLoc - GetOwner()->GetActorLocation();
    ClientVisualYawOffset = FMath::FindDeltaAngleDegrees(GetOwner()->GetActorRotation().Yaw, PreYaw);
}
```

Three details that are easy to get wrong:

- **Rewriting `M->StateAfter` is mandatory.** Skip it and the next ack compares against pre-correction history, reports a phantom error, and corrects again. That is the classic self-sustaining rubber-band and it has nothing to do with the epoch.
- **`EOverlaneStepMode::Replay` suppresses the speed cut but the sweep still blocks** (`ArcadeHandlingComponent.cpp:306`). Correct as designed. But every replayed step sweeps against traffic at time *T*, whereas the Live steps it replaces swept against traffic at *T − k·dt*. At 12 steps and 2100 cm/s that is up to 420 cm of traffic displacement, and replay can therefore slide cleanly through a car the Live pass was blocked by. Two mitigations, both cheap: (a) capping replay depth bounds it; (b) `bBlocked` in the ring — if the Live step was blocked and the replay step is not, do not extend past the Live result. Ship (a) in N‑008 and hold (b) until the measurements say it is needed.
- **`overlane.Net.ReplaySweep` (default 1).** Setting it to 0 makes replay a provably pure function of state + commands. With it off, any residual error is definitionally *not* geometric. Three lines, and it is the only way to attribute an error to traffic rather than to the model.

**Cost:** 12 swept box queries per correction. Bounded by the fact that below `IgnoreCm` no replay runs at all — in the common case the cost is zero.

### 4.3 The correction epoch handshake

The infinite rubber-band happens like this: server teleports → client applies → the client's in-flight commands, authored against the *pre*-teleport position, arrive → the server simulates them from the *post*-teleport state → divergence → another correction. The fix is that **the server must refuse pre-correction input**.

**Server** (`AOverlaneVehiclePawn` owns `CorrectionEpoch`, `AOverlanePlayerController` reads it):
1. Any discontinuity (D1) → `++CorrectionEpoch`, `PendingAckFlags |= ForceSnap`, `ClearPendingCommands()`, `StarveDebt = 0`.
2. `ServerSendMoveBatch_Implementation` (`OverlanePlayerController.cpp:250`) **discards the whole batch** when `Batch.AckedCorrectionEpoch != Pawn->GetCorrectionEpoch()`. Advance `LastAcceptedSequence` to the batch's newest sequence anyway, so the resync at `:268` does not fire once the client catches up.
3. On the first batch echoing the matching epoch, clear `ForceSnap` from `PendingAckFlags`.

**Client** (`OnRep_ServerMoveAck`):
1. `(Flags & ForceSnap) || Ack.CorrectionEpoch != LocalEpoch` → hard `SetSimState`, **clear the entire move ring**, clear the send window, `LocalEpoch = Ack.CorrectionEpoch`, and set `AckedCorrectionEpoch = LocalEpoch` on every subsequent batch.
2. No replay, no smoothing on a forced snap. A teleport must read as a teleport.

The loop terminates because step 2 on the server makes stale input incapable of dragging the state back. `uint8` wraps every 256 corrections and is compared with `!=`, which is safe because the server only ever advances by one and the client always mirrors.

Add B's 200 ms client send-suppression after a force-snap if the measurements show a burst of stale batches in flight; with the epoch discard in place it should be unnecessary, so treat it as a fallback rather than shipping it blind.

### 4.4 Smoothing — and a disagreement with the plan

`ClientVisualOffset` decays toward zero at `overlane.Net.SmoothRate` (start 18/s → ~90 % gone in 130 ms), capped at `overlane.Net.MaxVisualOffsetCm` (start 150 cm). Above the cap, do not hide the correction at all.

**`NETCODE_PLAN.md:19` says to smooth the collision root and lag the mesh only on hard snaps. I think that is wrong and I want the disagreement on the record.** Smoothing the collision root means the swept `AddActorWorldOffset` at `ArcadeHandlingComponent.cpp:298` runs at the *smoothed* position — reintroducing exactly the divergence the correction just removed, and doing it every frame. That is why UE's own CMC smooths the mesh and leaves the capsule authoritative.

The plan's rationale is nonetheless real: in this game the player threads gaps by looking at their own car, so a mesh that lags the collision box will make them clip cars that looked clear.

**The compromise:** apply the offset to *everything except* `VehicleCollision` — `VehicleMesh`, `CabinMesh`, the four wheels, both light bars, **and `CameraBoom`**. Because the camera moves with the mesh, the player's whole frame of reference shifts coherently; the car does not slide relative to the world *as the player perceives it*, and the collision box stays where the simulation says it is. Cap at 150 cm (under one car length, `VehicleCollision` extent X is 200 at `OverlaneVehiclePawn.cpp:54`) so the discrepancy can never approach a car width. Above the cap, snap the visuals too.

`CameraLagMaxDistance = 150` (`OverlaneVehiclePawn.cpp:235`) already prevents the existing camera lag from smearing across a teleport, so the two mechanisms do not fight.

**`CurrentSpeed` is never smoothed.** `NETCODE_PLAN.md:20` is right about this and the reasoning is sound: speed gates boost eligibility (`ArcadeHandlingComponent.cpp:249`), camera FOV (`OverlaneVehiclePawn.cpp:295-316`) and the near-miss floors (`TrafficVehicleBase.h:186,189`). Note the rule means *the smoothing path* must not write speed — the *replay* path necessarily does, because it adopts the server state and re-simulates. Those are not in conflict; make sure the comment says which one it means.

---

## 5. TEST PROCEDURE

### 5.1 PIE setup

Editor → Play dropdown → Advanced Settings → Multiplayer Options:
- **Net Mode: Play As Listen Server**
- **Number of Players: 2** (repeat the whole matrix at 4)
- **Run Under One Process: ON** for iteration. Run the acceptance pass with it **OFF** as well: under one process both worlds share a frame clock and a float path, so it structurally cannot reproduce frame-rate-asymmetry or cross-machine determinism bugs.

Race flow in PIE: `BeginPlay` sees `NM_ListenServer` and drops the host into the lobby (`OverlaneGameModeBase.cpp:57-64`); clients `PostLogin` and set `bMultiplayerRace = true` (`:79`); the host presses **Enter** → `StartRaceFromLobby` → `StartSoloRace` spawns the director (`:330-333`), whose `BeginPlay` then applies the multiplayer override.

**Expect 12 traffic cars, not 42, and no lane changes.** `TrafficDirector.cpp:29-40` forces `VehiclesPerLane = 2`, `GetSlotsPerLane()` is 4, three lanes. A tester expecting 42 will file a false bug.

### 5.2 Console commands — verified against `UE_5.8`

All `NetEmulation.*` commands live in `Engine\Source\Runtime\Engine\Private\Net\NetEmulationHelper.cpp:341-365` and are compiled in only when `DO_ENABLE_NET_TEST` is set — true in a Development editor build.

`ApplySimulationSettingsOnNetDrivers` (`NetEmulationHelper.cpp:26-37`) resolves the net drivers from `GEngine->GetWorldContextFromWorldChecked(World)`, and each PIE instance has its own `FWorldContext`. **The command therefore applies to whichever PIE viewport you typed it into.** Type them into the **client** window.

**Profiles** (`Engine\Config\BaseEngine.ini:3532-3568`):
```
NetEmulation.PktEmulationProfile Off        ; PktLoss/Lag all 0
NetEmulation.PktEmulationProfile Average    ; 1% loss both ways, 30-60ms each way
NetEmulation.PktEmulationProfile Bad        ; 5% loss both ways, 100-200ms each way
NetEmulation.PktEmulationProfile BufferBloat
NetEmulation.Off
```

**Individual knobs** (use the `Min`/`Max` pair, not `PktLag`; `PktLag` is documented as incompatible with `PktOrder`, and `PktLagVariance` requires it):
```
NetEmulation.PktLagMin 50
NetEmulation.PktLagMax 50
NetEmulation.PktIncomingLagMin 50
NetEmulation.PktIncomingLagMax 50
NetEmulation.PktLoss 5
NetEmulation.PktIncomingLoss 5
NetEmulation.PktJitter 20
NetEmulation.PktOrder 1
```

**Project + engine:**
```
overlane.Net.Predict 1 / 0
overlane.Net.Reconcile 1 / 0
overlane.Net.DrawCorrection 1
overlane.Net.LogCorrection 1
overlane.Net.DumpCorrectionStats
overlane.Net.ResetCorrectionStats
overlane.Net.ExtrapolateTraffic 0 / 1
overlane.Net.ReplaySweep 0 / 1
t.MaxFPS 30      (host window, to force the asymmetry)
t.MaxFPS 200     (client window)
stat net
stat unit
```

### 5.3 The pass sequence

Every step: `overlane.Net.ResetCorrectionStats` at the start, `overlane.Net.DumpCorrectionStats` at the end, 60 s of driving each.

| # | Setup | Action | **Pass** | **Fail** |
|---|---|---|---|---|
| **0** | Baseline. `Predict 0`, no emulation. | Solo `StartSoloRace`, then 2‑player. | Solo behaviour bit-identical to pre-change. Client car moves smoothly. `LAG` ≈ 0, `ERR` ≈ 0. | Any solo regression → a Phase A/B change leaked into the standalone path. |
| **1** | **Ratchet test.** `Predict 0`. Client: `PktLagMin/Max 40`, `PktJitter 20`, `PktLoss 5`. | Both hold W for 60 s from the line. | `HostX` and `ClientX` within **0.5 %**. | Client ahead by >1 % → A1/A2 did not take. This is the single most important test in the plan; it must pass **before** prediction is switched on. |
| **2** | `Predict 1`, `Reconcile 0`. No emulation. Clear road (drive the shoulder). | 60 s. | Clean-PIE gates in §3.7. `LAG` grows with ping, `ERR` p99 ≤ 2 cm, ring misses 0, duplicate acks ≤ 1 %. | Non-zero p99 → logic bug. Stop and find it. Do not proceed. |
| **3** | As 2, but through dense traffic. Then repeat with `ExtrapolateTraffic 0`. | 60 s each. | Both recorded. The delta is the traffic-geometry contribution. | Not a pass/fail — this is the number that decides whether the traffic latency fix (Tier 2 #1) ships before N‑008. |
| **4** | `Predict 1`, `Reconcile 0`, `PktEmulationProfile Average`. | 60 s. | §3.7 Average gates. p99 depth ≤ 12. | Depth > 12 → C5's ring is undersized for this RTT; raise the batch cap or the ring. |
| **5** | `Predict 1`, `Reconcile 0`, `PktEmulationProfile Bad`. | 60 s. | §3.7 Bad gates. Ring misses ≤ 0.1 %. | Ring misses > 0.1 % → 128 entries is not enough history; the RTT exceeds 2.1 s or the send window is stalling. |
| **6** | **Frame-rate asymmetry.** `Predict 1`, `Reconcile 0`. Host `t.MaxFPS 30`, client `t.MaxFPS 200`. Then swap. | 60 s each. | Error distribution within 2× of step 2. `HostX`/`ClientX` within 0.5 %. | A large asymmetry → A3 did not remove the time-discard bias. |
| **7** | **Enforcement on.** `Reconcile 1`, `Average`. | 60 s incl. deliberate traffic hits. | Steering answers within one frame. No visible snapping. Correction count per minute recorded. | Continuous snapping → `IgnoreCm` too low or history not being rewritten (§4.2). |
| **8** | **Rubber-band test.** `Reconcile 1`, `Bad`. Hold W and press **R**. | 10 recoveries. | Exactly one visible teleport each. Epoch increments by exactly 1. Car does **not** resume under pre-recovery throttle. No oscillation. | Two or more corrections per recovery → the epoch discard on the server is not rejecting stale batches. |
| **9** | **Start line.** `Reconcile 1`, `Bad`. Hold W through the whole countdown. | 5 race starts. | Car does not move during the countdown; exactly one epoch bump at GO; no visible snap. | Client launches early → C3/T0‑4. Multi-metre snap at GO → D1's start-transition bump is missing. |
| **10** | **Reversibility.** Mid-race, toggle `overlane.Net.Predict 1 → 0 → 1` and `Reconcile 1 → 0 → 1`. | 5 toggles each. | No crash, no runaway, no stuck state. `Predict 0` behaves exactly like step 0. | Anything else → a CVar is being read once at BeginPlay instead of per frame, or the ring is not being cleared on re-enable. |
| **11** | **4 players.** `Reconcile 1`, `Average`. | Full race. | All four spawn in distinct poses. No spawn interpenetration. Someone wins. | P4 on top of P2 → D3. |
| **12** | **Two physical machines, LAN.** `Reconcile 1`, no emulation. | Full race. | Error distribution comparable to step 2. | Materially worse than step 2 → cross-machine float divergence, which PIE structurally cannot show. This is the only test that can find it. |

Step 12 is not optional. Everything before it runs one copy of the code in one process; only step 12 tests the assumption the whole architecture rests on.

---

## 6. WHAT WILL STILL BE WRONG AFTERWARDS

### 6.1 Matters for a 2–4 player friends racer — fix next

**Per-player race state is the biggest remaining defect in the build, and it is not a netcode defect.** `OverlaneGameModeBase.cpp:153-159` computes `RaceScore`, `MaxSpeedKph` and the clean-drive streak from `GetPlayerPawn(this, 0)` — the host. A client's collision subtracts 750 from the *host's* score (`RegisterTrafficCollision` is called by any human pawn) and the client's own score is never computed at all. Worse, the winner scan at `:162-171` is nested inside the host-pawn null check, so a momentarily missing host pawn means nobody can win and the race hangs. Prediction will feel perfect and the scoreboard will be nonsense. **N‑010, and it should probably jump ahead of N‑009.**

**Near-miss scoring is single-occupancy.** `bNearMissEncounterActive` / `bNearMissAwarded` / `bNearMissBlocked` are one bool each per traffic car (`TrafficVehicleBase.h:231-233`) and the only guard is `IsAIRacer()`. Two humans passing the same car: one steals or cancels the other's near miss, and once anyone scores on a car nobody else can for its whole activation. In a 12-car multiplayer pool with 4 racers this loses most near misses. Near misses are the scoring mechanic. Fix with a per-pawn keyed encounter map.

**Remote racers are still 30 Hz interpolated proxies.** `SetNetUpdateFrequency(30)` (`OverlaneVehiclePawn.cpp:50`) plus `ReplicatedMovement`. Wheel-to-wheel racing with a friend — which is the entire product premise — will feel 100–150 ms stale, and the local player's swept move will block against a rival that is not there. N‑011. **This will be the loudest complaint from the first real playtest**, louder than any correction artefact.

**Traffic pose lag on clients (`v × OneWayLatency`)** if not fixed during N‑007. Traffic is systematically *closer* than it really is, which is the safe direction (you get blocked rather than driving through), but it produces recurring longitudinal corrections in exactly the dense sections that are the game.

**No adaptive jitter buffer.** A fixed `ServerBufferTarget = 2` (33 ms) will starve constantly on a 250 ms jittery connection. The `InputStarved` bit exists in the ack and nothing reads it. For LAN and good broadband it is fine; for matchmaking across a country it is not. The right fix is to put the observed depth in the ack (4 bits) and have the client servo its send phase.

### 6.2 Real, but acceptable for this product

- **Traffic recycling degrades on a mid-race disconnect** (`TrafficDirector.cpp:252`). In a 2-player friends race a disconnect usually ends the session. Fix E5 is four lines; do it, but it is not urgent.
- **Replay sees traffic at a single instant**, up to 420 cm displaced across a 12-step window. Bounded by the replay cap, and the direction is toward *more* blocking, not less, once `bBlocked` clamping ships.
- **Cheating.** After the rate limit (A5) a client can still author arbitrary throttle and steering — for its own car, which is correct and unavoidable in a client-predicted design. Nothing here reaches Steam-grade anti-cheat, and for friends-and-matchmaking that is the right trade.
- **Single-player traffic defects** (SlotPriority half-pool, `RacerSupplyCapacity` doubling density with one racer, the 42-car per-frame anchor cost). Real, single-player only, and single-player is not the goal.
- **Pitch and roll outside the sim state.** Correct while the model is planar. It becomes an uncorrectable divergence on the first ramp or banked corner — worth a comment in `OverlaneNetTypes.h:86-97` so the next reader is warned.
- **Traffic bandwidth.** 12 always-relevant cars at 30 Hz is ~7 KB/s. Fine. It becomes a question the day the multiplayer pool is raised toward the single-player 42.

### 6.3 The one thing you genuinely cannot know yet

**Cross-machine determinism.** Every operation in `SimulateStep` is IEEE‑754 basic arithmetic except `AddActorWorldRotation`, which goes through `FRotator::Quaternion()` and UE's own `FMath::SinCos` polynomial — deterministic across x86‑64 given the same binary and compile flags. I expect it to hold. But PIE runs one copy of the code in one process and **cannot** demonstrate it, and neither can any amount of packet emulation. Until test 12 in §5.3 has been run on two physical machines, treat the determinism premise as **unverified**, not as verified-by-passing-tests.

---

### Files referenced

`E:\Overlane\Source\Overlane\Private\ArcadeHandlingComponent.cpp` · `E:\Overlane\Source\Overlane\Public\ArcadeHandlingComponent.h` · `E:\Overlane\Source\Overlane\Public\OverlaneNetTypes.h` · `E:\Overlane\Source\Overlane\Private\OverlaneVehiclePawn.cpp` · `E:\Overlane\Source\Overlane\Public\OverlaneVehiclePawn.h` · `E:\Overlane\Source\Overlane\Private\OverlanePlayerController.cpp` · `E:\Overlane\Source\Overlane\Public\OverlanePlayerController.h` · `E:\Overlane\Source\Overlane\Private\OverlaneGameModeBase.cpp` · `E:\Overlane\Source\Overlane\Public\OverlaneGameModeBase.h` · `E:\Overlane\Source\Overlane\Private\OverlaneRaceGameState.cpp` · `E:\Overlane\Source\Overlane\Public\OverlaneRaceGameState.h` · `E:\Overlane\Source\Overlane\Private\TrafficVehicleBase.cpp` · `E:\Overlane\Source\Overlane\Public\TrafficVehicleBase.h` · `E:\Overlane\Source\Overlane\Private\TrafficDirector.cpp` · `E:\Overlane\Source\Overlane\Public\TrafficDirector.h` · `E:\Overlane\Source\Overlane\Private\OverlaneHUD.cpp` · `E:\Overlane\NETCODE_PLAN.md` · `E:\Overlane\TASKS.md` · `E:\EPIC_GAMES-games\UE_5.8\Engine\Source\Runtime\Engine\Private\ActorReplication.cpp` · `E:\EPIC_GAMES-games\UE_5.8\Engine\Source\Runtime\Engine\Private\Net\NetEmulationHelper.cpp` · `E:\EPIC_GAMES-games\UE_5.8\Engine\Config\BaseEngine.ini`