# Changelog

## Unreleased — 2026-07-25

### 2026-07-27 — visual vertical slice

- Added a local, non-colliding `HighwayEnvironmentDirector` that creates a dark asphalt route, lane dashes, shoulders, guardrail accents, reflectors, lamps, signs, and distant terrain without adding map actors or changing the tested floor/barrier collision.
- Reworked player and traffic placeholders into readable stylized vehicle silhouettes with separate body, cabin, wheel, and light components while preserving their collision roots and existing replicated traffic state.
- Successfully rebuilt `OverlaneEditor Win64 Development`; first runtime art verification is pending.
- Confirmed the Phase 5 shared-finish and six-car server-authoritative traffic prototype in two-player PIE.
- Copied the dependency-complete, installed Unreal template packages into the project: SportsCar (13.13 MiB), OffroadCar (26.53 MiB), Building (63.64 MiB), and ArchVis (39.03 MiB). Their original `/Game` root paths are preserved.
- Replaced the player and commuter/coupe/sport traffic silhouettes with the official unbranded SportsCar static body, glass, and wheel meshes. The taller truck profile deliberately remains stylized until a dedicated truck body is selected.
- Added a collision-free first-900-metre hero district using imported street lights, trees, and distant building instances; the remaining route keeps lightweight procedural scenery for prototype performance.
- Successfully rebuilt `OverlaneEditor Win64 Development` after the imported-asset integration; the first runtime visual/collision check is pending.
- Fixed the imported-traffic runtime crash: asset pointers are now resolved only in the traffic actor constructor and reused during live traffic-variant changes, rather than calling `ConstructorHelpers` after play has begun.
- Normalized the Windows renderer target to desktop SM5 + SM6 only, removed the accidental D3D12 mobile shader target, and disabled unused hardware ray tracing for a lighter first visual test.
- Moved the four raw SportsCar FBX source files out of `Content` into `SourceArt/Vehicles/SportsCar`; the runtime `.uasset` packages remain in `Content`, so Unreal no longer mistakes those source files for pending automatic reimports.
- Replaced the short-lived skeletal player attempt with the verified static SportsCar body, glass, and wheel assembly. The skeletal rig needs animation infrastructure this arcade pawn does not own; the static assembly keeps the player visible while preserving the collision and handling root.
- Reset this project's local PIE default to Standalone with one player, preventing ordinary solo visual tests from reopening the previous two-client listen-server configuration.
- Corrected network restart travel: a listen server now server-travels connected clients through `R` restart, while standalone races still reload locally.
- Added an explicit replicated Offroad SUV traffic profile with native body/tire visuals, deterministic server/client selection, and unchanged traffic collision/following rules.
- Added two collision-free roadside set pieces in the first kilometre: a right-side fuel stop and a left-side rest plaza, each with canopies, signs, pumps, pads, buildings, trees, and lighting.
- Added a charge-based turbo system: hold `Left Shift` or gamepad right shoulder after gaining speed to boost toward 245 km/h; HUD charge, network owner replication, and a temporary chase-camera kick are included.
- Successfully rebuilt `OverlaneEditor Win64 Development` and passed a headless map startup smoke check after the combined visual/turbo package.
- Corrected the visible SportsCar wheel assembly on both the player and SportsCar traffic: the official source pivots and native orientation are now used instead of generic collision-box offsets and an incorrect 90-degree roll. Offroad wheels now use body-relative placement as well; placeholder truck wheels retain their original cylinder rotation.
- Added a collision-free city-entry landmark at roughly 1.42 km: an elevated interchange, ramp silhouettes, guide signs, piers, skyline blocks, staged buildings, and tree framing are all HISM-based and leave the proven driving corridor untouched.
- Copied the dependency-complete official ConceptCar template package into `/Game/ConceptCar` (nine `.uasset` files, no raw FBX) and added its hero car as a non-interactive fuel-stop showroom landmark.
- Successfully rebuilt `OverlaneEditor Win64 Development` after the city-entry, ConceptCar, and wheel-alignment package.
- Prototyped a production-art upgrade using already installed Building and ArchVis meshes, then disabled the generic runtime staging after visual review exposed incompatible source pivots and proportions. The proven clean highway composition remains active until an approved art kit receives authored placement.
- Routed the SUV traffic profile through the proven SportsCar body/glass/wheel assembly while the imported Offroad mesh's incompatible wheel pivots are repaired; traffic collision, movement, and networking remain unchanged.
- Added a deferred bot-driver roadmap: the first milestone will be one server-authoritative practice bot that possesses a normal replicated vehicle and follows a lane without scoring or ending a human race.
- Successfully rebuilt `OverlaneEditor Win64 Development` after disabling incompatible production-art staging and switching the SUV visual fallback; a headless map startup check confirmed the disabled staging has no runtime references.
- Added a local exposure/bloom guard to prevent the template sky from washing out the road and traffic, and extended the visual asphalt, shoulders, rails, and lane markings 150 m past the finish so the results camera no longer reveals the graybox floor edge. The physical race finish is unchanged.

- Added a local Canvas main menu, settings menu, and actionable pause menu; local graphics, VSync, frame-rate cap, camera FOV, and traffic-debug preferences save to a `SaveGame` slot.
- Added cautious traffic lane changes with source/target lane reservation, speed-gated attempts, player exclusion, and shared-lane following logic.
- Added compact, commuter, sport, and truck placeholder traffic profiles with varied geometry, colors, and speed targets; recycling now checks player and traffic spacing.
- Expanded gamepad mappings: triggers/left stick drive, Start pauses, A starts Solo, D-pad navigates settings, and left-stick click recovers.
- Successfully rebuilt `OverlaneEditor Win64 Development` after all local-play additions; runtime long-session and physical-controller validation are pending.
- Added local solo-race records: collision count, maximum speed, longest clean-drive streak, persistent best time, and a new-personal-best result indicator.
- Prepared the 6 km / roughly two-minute local sprint code path: route limits and default lanes were extended, and traffic now recycles after falling behind the player before safely respawning ahead. Existing map actors still require the matching editor transform update.

- Initialized the OVERLANE Git repository and local Git LFS configuration.
- Added Unreal-oriented ignore and LFS attribute rules.
- Completed the Phase 0 empty-workspace audit.
- Added initial game, technical, production, test, asset, release, and handoff documentation.
- Recorded the absence of Unreal Engine 5 and the resulting baseline-build blocker.
- Added the UE 5.8 `Overlane` C++ project descriptor, runtime module, editor/game targets, and base project configuration.
- Explicitly enabled Enhanced Input after engine-plugin inspection.
- Recorded the active Live Coding session blocking the initial editor build.
- Moved the project to `E:\Overlane` because the former Unicode Desktop path is incompatible with MSVC response/PCH files.
- Successfully built `OverlaneEditor Win64 Development` with UE 5.8.0 in 105.1 seconds.
- Added and compiled the initial GameMode, PlayerController, VehiclePawn, and HUD C++ architecture skeletons.
- Added the graybox `L_VehicleHandlingTest` map with floor, boundaries, and PlayerStart.
- Added runtime Enhanced Input mappings and a temporary C++ arcade vehicle placeholder with a chase camera.
- Corrected map and Game Mode declarations to use `DefaultEngine.ini`; removed an unused generated Android file-server token/configuration block.
- Added a Canvas km/h HUD readout and tunable speed-reactive chase-camera distance/FOV response.
- Added `R`/gamepad recovery input that resets velocity and returns the local placeholder vehicle to its start transform.
- Validated keyboard driving, collision, speed HUD, speed-reactive camera, recovery, and repeat-run stability in the handling test map.
- Added and successfully compiled the Phase 2 `ATrafficLanePath` and `ATrafficVehicleBase` foundations, including a 100 m default straight lane spline.
- Added `CenterLane`, `LeftLane`, and `RightLane` to the handling map, plus a compiled local `ATrafficDirector` that discovers them and recycles one placeholder traffic vehicle per lane.
- Validated the local three-vehicle traffic pool in play: lane following, end-of-road recycling, and player-impact blocking all behaved as expected.
- Added compiled traffic-placeholder color variation and a short HUD impact message for player-to-traffic collisions; runtime confirmation is pending.
- Confirmed traffic color variation and collision HUD feedback in play.
- Added a compiled nine-vehicle local pool with three spaced placeholders per lane and a 30 m player-distance gate for recycled spawns; runtime confirmation is pending.
- Validated the nine-vehicle pool's lane stability, spacing, repeated recycling, and safe visible respawn behavior in play.
- Updated the traffic impact HUD message to inherit the struck vehicle's color; runtime confirmation is pending.
- Validated color-matched traffic-impact HUD feedback in play.
- Added a compiled first near-miss system: high-speed close non-collision overtakes trigger a color-matched `YAKIN GECIS +1` message and a persistent counter; runtime validation is pending.
- Validated the high-speed close-pass score and color-matched `YAKIN GECIS +1` feedback in play.
- Added a compiled 3-2-1-GO race-start countdown that gates driving input and traffic movement; runtime confirmation is pending.
- Validated the 3-2-1-GO countdown, player-input lock, and traffic-start gate in play.
- Added compiled remaining-distance HUD and a local solo finish at X=4500 that stops the vehicle and displays the elapsed time; runtime confirmation is pending.
- Validated the remaining-distance HUD, solo finish stop, and elapsed-time result in play.
- Added compiled result-state `R - YENIDEN BASLA` restart handling that reloads the local test race; runtime confirmation is pending.
- Validated the result-state R restart, including fresh countdown, reset near-miss score, and reset traffic in play.
- Added compiled live elapsed-time HUD and result near-miss statistic; runtime confirmation is pending.
- Validated live elapsed time and near-miss result statistics in the complete local solo-race loop.
- Added compiled traffic speed profiles: orange is slow, blue medium, and yellow fast, with stable same-lane initial spacing; runtime confirmation is pending.
- Extended compiled traffic defaults to 300 m lane paths and a 21-vehicle pool (seven per lane); the editor map must be scaled to match before runtime validation.
- Validated the extended 300 m map, 21-vehicle pool, stable profile spacing, and color-speed readability in play.
- Added compiled guarded traffic lane changes with a clear-target check, player-distance gate, and two-second interpolation; runtime confirmation is pending.
- Added compiled same-lane traffic following/speed limiting to prevent fast profiles intersecting slower traffic; also shortened lane-change attempts to 3.5 seconds. Runtime confirmation is pending.
- Added swept traffic movement so traffic cannot pass through a stopped player, and prioritized faster profile lane-change attempts every 1.5 seconds; runtime confirmation is pending.
- Disabled automatic traffic lane changes by default after the 21-vehicle test exposed overlap and traffic-lock regressions; retained following and swept movement as the stable fallback pending validation.
- Replaced swept traffic movement after it exposed floor stalls: traffic now uses logical following, ignores physical traffic-to-traffic blocking, and uses an explicit stopped-player hold check. Runtime confirmation is pending.
- Validated the stable 21-vehicle fallback: no traffic overlap or ground stalls, and traffic held before a stopped player.
# 2026-07-26 - In-race soft pause implementation

- Added a `P` Enhanced Input action for a local race pause toggle.
- Paused state freezes the vehicle handling tick, traffic vehicles, traffic director, race timer, and finish detection without globally pausing the engine.
- HUD now shows `DURAKLATILDI` and `P - DEVAM` during the pause; elapsed time excludes all paused time.
- `OverlaneEditor Win64 Development` compiled successfully after the change.
- While paused, `R` now performs a full map restart (including the 3-second countdown) instead of only recovering the player vehicle.

# 2026-07-26 - Long-route traffic polish

- Increased the traffic pool's behind-player recycle distance from 50 m to 150 m, so safely passed vehicles remain visible behind the player for longer before reuse.
- Filtered swept-movement floor contacts from the player's collision speed penalty; only predominantly horizontal impacts (traffic or barriers) now reduce speed.
- `OverlaneEditor Win64 Development` compiled successfully after the changes.
- Replaced the player's generic swept movement with an explicit obstacle sweep that only treats traffic vehicles and actors named/tagged as barriers as blockers. The road mesh is no longer able to create an invisible movement block on the long route.
- Traffic impacts detected by the explicit sweep now use the same one-hit-per-contact feedback and result-stat accounting as physical collision events.
- Reverted the explicit obstacle sweep after it failed to recognize editor-labelled barriers at runtime and produced broad traffic blocking. Standard swept movement is restored.
- At pawn startup, the detected long, thin road static mesh now ignores the Pawn channel; tall/narrow barriers and traffic retain their normal collision responses.
- `OverlaneEditor Win64 Development` compiled successfully after the collision correction.

# 2026-07-27 - Progression-led Traffic Sprint scoring

- Added a route-progress-led race score, `+250` selective near-pass bonuses, and `-750` collision penalties.
- Tightened near-pass qualification to require a close lateral pass, meaningful forward progress, 90+ km/h player speed, and at least 25 km/h relative passing speed.
- Removed the player-wide near-pass cooldown after it incorrectly suppressed legitimate consecutive passes. Each traffic actor already owns the correct one-award-per-active-encounter guard.
- Results and HUD now display the live score, near-pass bonus, collision penalty, and persistent best solo score.
- `OverlaneEditor Win64 Development` compiled successfully; runtime scoring validation is pending.

# 2026-07-27 - Listen-server driving foundation

- Made the player vehicle server-authoritative and movement-replicated, with replicated speed for remote HUD/camera feedback.
- Added unreliable server input RPCs for throttle, brake, and steering, preserving local host control while allowing a connected client to drive its own vehicle through the server.
- Added `AOverlaneRaceGameState` to replicate countdown, active/finished flow, timer, and route bounds to clients.
- Joining players are assigned alternating side lanes; the current local-only traffic pool is removed/withheld for the initial low-traffic networking test.
- The server now ends the shared test race when any connected player reaches the finish.
- `OverlaneEditor Win64 Development` compiled successfully; two-client runtime validation is pending.
- Added owner-only server transform replication for the locally controlled client pawn after the first PIE test confirmed that the host saw client movement while the client lacked its own visual correction.
- `OverlaneEditor Win64 Development` compiled successfully after the owner-transform correction.

# 2026-07-27 - Shared listen-server finish result

- Confirmed the two-player PIE driving proof: both players can drive their own vehicle and see the other player move.
- Added a replicated winner `PlayerState` id to `AOverlaneRaceGameState`; the first player past the finish now ends the shared run server-side.
- Both host and client now receive a common result message: `SEN KAZANDIN!` for the winner or `DIGER OYUNCU KAZANDI` for the other player, plus the shared finish time.
- Multiplayer finishes do not overwrite solo personal records; only solo runs update the local progression save.
- `OverlaneEditor Win64 Development` compiled successfully; the first-finish/two-result runtime test is pending.

# 2026-07-27 - First replicated traffic pass

- Removed the duplicate solo HUD result/stat blocks from the host during a multiplayer race; host and client now use the same compact replicated race/result UI.
- The traffic director now uses a six-car, wide-gap, lane-change-disabled pool when more than one player is connected.
- Traffic vehicles are server-authoritative, movement-replicated, always relevant for this small prototype pool, and replicate their active/hidden state, colors, and placeholder dimensions.
- Spawn safety, traffic holding, and lane-distance calculations now consider every connected player instead of only the host pawn.
- `OverlaneEditor Win64 Development` compiled successfully; two-window shared-traffic runtime validation is pending.
## Practice bot (in progress)

- Added a server-only `AOverlaneBotDriverController` that possesses a normal `AOverlaneVehiclePawn` and follows an outer traffic lane using look-ahead steering.
- The bot is spawned only for standalone solo races, starts one lane over and 90 m ahead, does not boost, score, or lane-change.
- The bot now acts as a real solo rival: if it reaches the finish first, the run ends and the result screen reports `PRAKTIK BOT KAZANDI`; player personal records are never overwritten by a bot win.
- Listen-server multiplayer remains unchanged while this first driving pass is validated.
