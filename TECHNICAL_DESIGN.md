# Technical Design

## Baseline

The repository does not yet contain an Unreal project. When Unreal Engine 5 is installed, create a C++ project named `Overlane` without changing engines thereafter unless explicitly approved. Use C++ for reusable gameplay, authority boundaries, and performance-sensitive systems; use Blueprints for content assembly and tuning.

## Proposed module and gameplay boundaries

Start with a single runtime module, `Overlane`, and split only when profiling or ownership makes it useful. Initial core classes:

```text
AOverlaneGameModeBase / ATrafficSprintGameMode  authoritative race phases
AOverlaneGameState / AOverlanePlayerState       replicated session and race data
AOverlanePlayerController                        input, HUD ownership, pause
AOverlaneVehiclePawn                             player-facing vehicle composition
UArcadeHandlingComponent                         tuned handling layer over Chaos Vehicles
URaceProgressComponent                           checkpoint and route progress
UNearMissComponent                               unique pass evaluation
UTrafficDirectorSubsystem                        spawn, pooling, fairness, simulation LOD
ATrafficVehicleBase                              pooled lane-following traffic actor
```

Use Enhanced Input for input; it is explicitly enabled in the project and is non-beta in UE 5.8. Chaos Vehicles remains the preferred vehicle foundation, but UE 5.8 marks its built-in plugin experimental and disabled by default. Enable it only for the vehicle prototype, then record an editor and packaged-build smoke test before treating it as a production dependency. The initial vertical slice should keep traffic lane-based rather than full vehicle physics. Avoid Level Blueprint gameplay logic.

### Current local vehicle placeholder

`AOverlaneVehiclePawn` currently composes a collision box, cube placeholder mesh, spring-arm chase camera, ground snap, and `UArcadeHandlingComponent`. The controller creates an Enhanced Input mapping context at runtime: W/right trigger accelerates; S/left trigger brakes or reverses; A/D/left stick steer. This is a deliberately temporary art- and asset-independent foundation, not a substitute for Chaos validation.

## Traffic approach

Traffic is host-authoritative in multiplayer, but it is local-only until Phase 5. Vehicles use pooled actors and three configurable simulation tiers: full nearby collision/behavior, simplified mid-range lane following, and low-frequency background movement. Spawn safety is assessed per relevant player using lane occupancy, forward visibility, and time-to-collision. Development builds expose state, lane, desired speed, LOD, and pool diagnostics.

## Multiplayer path

First validate two players on an empty road. The listen server owns race state, recovery, finish ordering, and critical traffic events. Do not rely on deterministic physics. Nearby traffic receives controlled transform/state replication with client interpolation; distant traffic uses lower-cost state replication or representation. Steam-compatible sessions are deferred until local driving and empty-road networking work.

## Performance and configuration

Target 60 FPS at 1080p on a mid-range PC; treat 16.6 ms as the total frame budget. Keep tunables in data assets/configuration, pool traffic, avoid per-actor Blueprint Tick, and profile CPU, GPU, memory, and network independently. Supported hardware is determined only after packaged-build profiling.

## Save and UI

Later `UOverlaneSaveGame` stores local settings, selected vehicle, and personal records. UMG (or Common UI-compatible conventions) owns presentation only; gameplay state remains in controller/state/subsystems.
