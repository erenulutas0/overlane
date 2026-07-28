# Decisions

## D-001 — Use Unreal Engine 5.8.0 for the project baseline

- **Status:** accepted
- **Decision:** `Overlane` is a C++ Unreal Engine **5.8.0** project, associated with the locally installed `E:\EPIC_GAMES-games\UE_5.8` release build.
- **Reason:** The local engine install reports `5.8.0`, promoted build changelist `55116800`, branch `++UE5+Release-5.8`; Visual Studio 2022 with C++ tools is also installed.
- **Alternatives:** Create placeholder source files without a project; use a Blueprint-only project; silently change engine versions.
- **Consequences:** The project must stay on UE 5.8.0 unless a future approved decision explicitly changes it.

## D-004 — Defer Chaos Vehicles activation until its prototype validation

- **Status:** accepted
- **Decision:** Enhanced Input may be used when input work begins. Chaos Vehicles will not be enabled in the empty baseline; it will be enabled and validated as part of the vehicle prototype.
- **Reason:** UE 5.8 ships Enhanced Input as non-beta/default-enabled, while the built-in Chaos Vehicles plugin is marked experimental and disabled by default.
- **Alternatives:** Enable Chaos Vehicles immediately; replace it with an unproven third-party vehicle plugin.
- **Consequences:** P1-003 must record an editor/package smoke-test result before vehicle-dependent work is treated as stable.

## D-005 — Use a temporary C++ arcade mover before Chaos vehicle integration

- **Status:** accepted
- **Decision:** The first drivable placeholder uses `UArcadeHandlingComponent`, a collision box, a primitive cube body, and a chase camera. It receives Enhanced Input and uses swept movement with speed-sensitive steering.
- **Reason:** No wheel-rigged playable vehicle asset exists, while UE 5.8 marks Chaos Vehicles experimental. This permits local handling validation without claiming final vehicle physics.
- **Alternatives:** Block all driving until licensed/created vehicle art exists; enable and depend on experimental Chaos immediately.
- **Consequences:** The component is intentionally simple and local-only. A future Chaos migration needs its own editor and packaged-build validation once a compliant vehicle rig is available.

## D-002 — Git LFS for Unreal binary assets

- **Status:** accepted
- **Decision:** Track `.uasset`, `.umap`, and selected large source asset formats with Git LFS.
- **Reason:** Unreal binary assets are not merge-friendly and can grow rapidly.
- **Alternatives:** Store all binaries in ordinary Git; add assets before configuring LFS.
- **Consequences:** Contributors must have Git LFS installed before adding assets. Exact asset licenses remain recorded separately.

## D-003 — Local-first traffic and networking architecture

- **Status:** accepted
- **Decision:** Validate local vehicle and traffic gameplay before multiplayer; use host authority for future online races.
- **Reason:** This minimizes coupled debugging and prevents premature replication design from driving vehicle feel.
- **Alternatives:** Build Steam/multiplayer first; fully simulate all traffic actors on every client.
- **Consequences:** Network traffic synchronization is deferred until Phase 5/6.

## D-010 - Driving netcode: predictive reconciliation with a sequenced ack (2026-07-28)

**Decision.** The owning client simulates its own vehicle on a fixed 60 Hz timestep. The
server simulates the same input commands authoritatively and acknowledges the last one it
consumed together with the resulting state. When the client's stored state for that
sequence differs by more than a dead zone, it resets to the server state and replays every
unacknowledged command. Traffic is never re-simulated by a client; it is extrapolated from
a newly replicated lane speed.

**Alternatives considered.** Three architectures were designed independently and judged on
correctness, solo-developer feasibility and gameplay feel. Each was disqualified by one
judge. Owner-authoritative movement was rejected structurally: the client would author the
0.45x collision cut, which is the quantity that decides a dense-traffic race, and the
proposed positional backstop was arithmetically wrong in the direction that steals wins.
Continuous blending without replay was rejected because it assigns a speed that is one-way
latency stale, injecting a ~540 cm/s sawtooth every packet at 150 ms RTT. Full rollback's
traffic-pose staging was dropped as too expensive and too hard to debug for one developer.

**Consequences.** The handling model must be deterministic, which is why the integrator was
split into an accumulator and a pure step function first. Replay suppresses the collision
speed cut - the sweep still blocks, but the discrete 55% branch cannot flip between the
prediction and the replay, which is what makes replay stable by construction. Rival contact
must stay cosmetic; if ramming or drafting ever affects speed, rivals have to enter the
replayed simulation and that is a separate project.

The full plan is in `NETCODE_PLAN.md`.
