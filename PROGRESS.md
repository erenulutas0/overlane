# Progress

## Completed

- Audited the workspace on 2026-07-25: it was empty and was not a Git or Unreal repository.
- Detected Git 2.50.1 and Git LFS 3.7.0; initialized local Git and LFS configuration.
- Verified Unreal Engine 5.8.0 at `E:\EPIC_GAMES-games\UE_5.8` and Visual Studio 2022 C++ tooling.
- Added Phase 0 documentation, Unreal Git ignore rules, and LFS attributes.

## Current state

The `Overlane` C++ project, game/editor targets, runtime module, input configuration, and clean class skeletons exist at `E:\Overlane`. `L_VehicleHandlingTest` is now a validated 6 km Traffic Sprint with boundaries, a safe PlayerStart, and three placed lane paths. The keyboard vehicle prototype, collision/near-miss feedback, solo race loop, stable traffic pool, P soft pause, menu/settings flow, long-route collision behavior, route-progress scoring, and minimal two-player listen-server proof all passed manual play tests. During normal racing `R` is recovery; during pause or after results it restarts the full race with the countdown. The visual slice now uses a source-pivot-corrected static SportsCar player assembly, SportsCar-based passenger traffic visuals (including the SUV profile while the imported off-road mesh is repaired), a collision-free dark highway art layer, a fuel stop and rest plaza, a ConceptCar showroom landmark, and a 1.42 km city-entry interchange with ramps, signs, skyline, staged buildings, and trees. A charge-based Shift/right-shoulder turbo adds a temporary 245 km/h sprint and camera kick. The combined editor build and headless startup smoke check passed; the next manual run validates all current visuals together before the route evolves into a hand-authored city-edge map segment.

An experimental Building/ArchVis production-art staging pass was disabled after visual review exposed incompatible source pivots and proportions when generic runtime bounds scaling was used. The established road art remains active and physically isolated from gameplay; a local exposure/bloom guard keeps its templates readable, while an extra 150 m visual road overscan hides the graybox floor edge after the race finish. Any next production-art pass must use an approved asset kit and authored transforms rather than automatic mesh scaling.

## Known constraints

Chaos Vehicles is present in UE 5.8 but is experimental; activation and packaging smoke testing are a defined follow-up task. The legacy Desktop location contains an isolated `.git` metadata folder left by OneDrive/Codex after the cross-volume move; the actual project is `E:\Overlane`.

## Next objective

Run one combined solo visual-and-gameplay check: verify the player SportsCar and traffic wheels are attached and correctly oriented, the SUV traffic, fuel/rest set pieces, ConceptCar showroom, and 1.42 km interchange read clearly, Shift/right-shoulder turbo drains and recharges correctly, and road/barrier/traffic collision is unchanged. If it is clean, the next larger phase is a hand-authored city-edge map segment with licensed/generic production assets, then session discovery and real internet multiplayer.
## Current: solo practice bot first pass

- A normal vehicle pawn is now driven by a server-side bot controller along the outer lane during standalone solo races.
- It can now win a solo run and displays a distinct player-or-bot result. It is intentionally excluded from multiplayer, scoring, turbo, and lane-change rules for this validation pass.
