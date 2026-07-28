# Tasks

Status values: `blocked`, `pending`, `in progress`, `done`.

## Phase 0

| ID | Status | Task | Depends on | Acceptance criteria |
|---|---|---|---|---|
| P0-001 | done | Audit workspace and tools | — | Empty workspace, Git/LFS state, UE availability, and build viability recorded. |
| P0-002 | done | Establish source-control rules | P0-001 | Git initialized; LFS attributes and Unreal ignore rules committed for review. |
| P0-003 | done | Create project documentation | P0-001 | Required documents reflect actual empty-project state and planned architecture. |

## Phase 1 — prioritized

| ID | Status | Task | Depends on | Acceptance criteria |
|---|---|---|---|---|
| P1-001 | done | Install/select stable UE5 and compatible Visual Studio C++ tools | — | UE 5.8.0 and Visual Studio 2022 C++ tools verified locally; selected version recorded in DECISIONS.md. |
| P1-002 | done | Create `Overlane` C++ UE project and baseline-build it | P1-001 | `OverlaneEditor Win64 Development` built successfully with UE 5.8.0 in 105.1 seconds from `E:\Overlane`. |
| P1-003 | done | Verify enabled built-in plugins | P1-002 | Enhanced Input is explicitly enabled and non-beta. Chaos Vehicles is present but experimental/disabled; activation awaits vehicle-phase smoke testing. |
| P1-004 | done | Add clean gameplay class skeletons | P1-002 | `AOverlaneGameModeBase`, `AOverlanePlayerController`, `AOverlaneVehiclePawn`, and `AOverlaneHUD` compiled successfully in 21.13 seconds. |
| P1-005 | in progress | Create vehicle handling test map | P1-003, P1-004 | `L_VehicleHandlingTest` has floor, two barriers, and a safe PlayerStart. Runtime route/debug markers remain. |
| P1-006 | in progress | Add Enhanced Input mappings | P1-003, P1-004 | Keyboard controls and expanded gamepad mappings compile: triggers/left stick drive, Left Shift/right shoulder activates turbo, Start pauses, A starts Solo, D-pad controls settings, left-stick click recovers. Physical controller verification remains pending. |
| P1-007 | done | Integrate/tune initial playable vehicle | P1-005, P1-006 | Keyboard prototype passed spawn, acceleration, brake/reverse, steering, barrier collision, and three repeat road runs without a stuck/flip issue. |
| P1-008 | done | Add chase camera and speed HUD | P1-007 | 180 km/h normal HUD cap and speed-reactive chase camera/FOV were visually verified in play; the new turbo path extends the target to roughly 245 km/h and needs one combined manual check. |
| P1-009 | done | Add reset/recovery and Phase 1 test pass | P1-007, P1-008 | `R` returned the vehicle to start at `000 KM/H`; three repeat runs had no persistent stuck or flip issue. |

The immediate task is a play test of the compiled placeholder vehicle before any handling claims are made.

## Phase 2 — initial traffic foundation

| ID | Status | Task | Depends on | Acceptance criteria |
|---|---|---|---|---|
| P2-001 | done | Add lane-path and traffic-vehicle foundations | P1 keyboard prototype | `ATrafficLanePath` and `ATrafficVehicleBase` compile successfully; the default lane is a 100 m straight spline and no runtime traffic is spawned yet. |
| P2-002 | done | Author three test-road lanes and spawn fixtures | P2-001 | `CenterLane`, `LeftLane`, and `RightLane` cover the handling road at Y=0, -600, and 600. |
| P2-003 | done | Add pooled local traffic spawning | P2-001, P2-002 | Three local placeholder vehicles stayed on their lanes, recycled at the route end, and blocked the player on impact in a play test. |
| P2-004 | done | Add traffic visual distinction and impact feedback | P2-003 | Blue/orange/yellow traffic placeholders and the short `TRAFIK CARPISMASI` HUD message were confirmed in play. |
| P2-005 | done | Increase local traffic density with safe respawns | P2-003, P2-004 | Nine placeholders stayed in their lanes with spacing, recycled repeatedly from safe fixed route points, and did not appear immediately beside or in front of the player. |
| P2-006 | done | Match impact feedback to the struck traffic vehicle | P2-004 | The `TRAFIK CARPISMASI` HUD message matched blue, orange, and yellow struck-vehicle colors in play. |
| P3-001 | done | Add close-pass detection and scoring | P2 traffic prototype | A 90+ km/h non-collision close overtake awarded one color-matched `YAKIN GECIS +1` point and incremented the HUD counter in play. |
| P3-002 | done | Add race countdown and input gate | P3-001 | The 3-2-1-GO HUD countdown blocked player driving and traffic movement until GO in play. |
| P3-003 | done | Add route progress and solo finish | P3-002 | The `KALAN` HUD counted down; at zero the vehicle stopped and displayed the green elapsed-time finish result in play. |
| P3-004 | done | Add results restart | P3-003 | The result showed `R - YENIDEN BASLA`; R restarted a clean countdown with reset near-miss score and traffic in play. |
| P3-005 | done | Add live timer and result statistics | P3-004 | Live elapsed time updated during the run and the finish result showed both time and near-miss total in play. |

## Phase 4 — traffic quality

| ID | Status | Task | Depends on | Acceptance criteria |
|---|---|---|---|---|
| P4-001 | done | Add readable traffic speed profiles | P3 solo sprint | Orange, blue, and yellow traffic used distinct stable slow/medium/fast speeds without same-lane overlaps in play. |
| P4-002 | done | Extend the traffic test road to 300 m | P4-001 | Floor, barriers, start, and three default lane paths cover -15000 to +15000 cm; 21 pooled vehicles were evaluated before recycling in play. |
| P4-003 | in progress | Rebuild lane changes with full target-gap reservation | P4-002, P4-004 | Automatic lane changes are re-enabled only for cars slowed by a leader. Both source and target lanes are reserved during the transition; a 28 m usable target clearance and player exclusion are compiled, awaiting long-play stress validation. |
| P4-004 | done | Stabilize same-lane traffic flow | P4-002 | With lane changes disabled, 21 vehicles flowed without overlap or ground stalls, and traffic held before a stopped player in play. |
| P4-005 | done | Add in-race pause control | P3-005 | In play, `P` froze player, traffic, and timer with `DURAKLATILDI`; `P` resumed correctly. While paused, `R` performed a full restart with the 3-second countdown, while active-race `R` remained vehicle recovery. |
| P4-006 | in progress | Add traffic variants and safer recycling | P4-004 | Compact, commuter, sport, and truck placeholder profiles now have different dimensions, colors, and speeds. Respawns require safe player and traffic spacing; long-play validation is pending. |
| P4-007 | in progress | Add local menu and basic settings | P3-005 | Canvas-based main menu, settings screen, actionable pause menu, local settings save, graphics/VSync/frame-cap/FOV/debug controls compile. Runtime UI validation is pending. |
| P4-008 | in progress | Add extended solo race statistics and records | P3-005 | Collision count, max speed, longest clean-drive streak, best time, best score, progression score, near-pass bonus, and collision penalties compile; the revised results need one full runtime validation. |
| P4-009 | done | Build a 2-minute local sprint route | P4-006, P4-008 | The 6 km route was completed in a 124.8-second manual run. Long-floor collision seams are removed while barrier and traffic impacts remain blocking. |
| P4-010 | in progress | Tune near-pass fairness and scoring | P4-008, P4-009 | Near passes require a 90+ km/h close, forward, 25+ km/h relative-speed overtake. Each vehicle can award only once per active encounter, but different cars can score consecutively. Progress is the main score; near passes are bonuses and collisions are penalties. Full-run validation is pending. |

## Phase 5 — minimal multiplayer prototype

| ID | Status | Task | Depends on | Acceptance criteria |
|---|---|---|---|---|
| P5-001 | done | Replicate two-player low-traffic driving | P4-009 | Two PIE players received separate lanes; each could drive its own vehicle and see the other vehicle move through the listen server. |
| P5-002 | in verification | Add minimal session host/join flow | P5-001 | `UOverlaneSessionSubsystem` implements host/find/join/leave over `OnlineSubsystemNull` (LAN broadcast). The main menu gained ONLINE - OYUN KUR and ONLINE - OYUN BUL, plus a session browser and a host lobby. Compiles clean; **has not been run**. Needs two packaged builds on one network to confirm a client joins without typing an address. |
| P5-003 | done | Validate two-player race authority | P5-001 | Two clients see each other drive, input remains responsive enough for the prototype, first finish ends the shared run, both clients receive the replicated winner result, and neither client can locally author race state. |
| P5-004 | done | Add first server-authoritative shared traffic | P5-003 | A six-car, no-lane-change server pool is visible and moves identically for two players; cars avoid spawning beside either player and block either player consistently. |
| P5-005 | in progress | Promote the practice bot into a standalone rival | P5-004 | Identity and scoring isolation are done: a replicated `bIsAIRacer` flag keeps the bot out of the human's score and near-miss state, and the bot is destroyed when a client joins. Still outstanding before it is a genuine rival: forward sensing and braking (brake is hardcoded to 0), overtaking, the `StartingDistance` lower-clamp stall trap, steering damping and speed-scaled lookahead, a difficulty parameter and boost use (without boost it is capped at 180 km/h against the player's 245), player-vs-bot contact feedback, a distinct body colour, and a rival gap readout on the HUD. |
| P5-006 | pending | Promote practice bot into a competitive race participant | P5-005 | Human and bot racers use a shared authoritative participant list; the bot can finish, place, and be reported correctly to all clients without corrupting player/session handling. |

## Phase 6 — visual vertical slice

| ID | Status | Task | Depends on | Acceptance criteria |
|---|---|---|---|---|
| P6-001 | in progress | Add collision-free highway art layer and stylized vehicle silhouettes | P4-009, P5-004 | A dark asphalt road, markings, shoulders, guardrail accents, reflectors, lamps, signs, distant terrain, and readable cabin/wheel/light vehicle visuals render in solo and two-player PIE without changing road, barrier, or traffic collision. The first kilometre now uses imported trees, lamps, buildings, a fuel stop, rest plaza, and ConceptCar showroom; the 1.42 km landmark adds a visual-only city-entry interchange. Combined runtime validation is pending. |
| P6-002 | in progress | Migrate the installed unbranded Unreal template sports-car and road assets | P6-001 | Official SportsCar, OffroadCar, Building, ArchVis, and ConceptCar content with dependencies are copied into `/Game`. Player and passenger-traffic profiles use the source-pivot-aligned SportsCar body/glass/wheel assembly without changing collision roots; the imported Offroad mesh is deferred until its authored wheel pivots can be fitted safely. Runtime fit/orientation validation is pending. |
| P6-003 | in progress | Art-direction and performance pass | P6-002 | Traffic colors remain readable, route decoration is performant, and the visual language is approved before creating a richer hand-authored map. The hero district, fuel/rest areas, ConceptCar showroom, and city-entry interchange are the first combined composition pass. |
| P6-004 | pending | Build the first authored production-art city/highway vertical slice | P6-003 | Select an approved licensed art kit or build an authored local layout, then place its road-side modules using mesh-specific transforms. The generic Building/ArchVis runtime staging is deliberately disabled after visual QA revealed incompatible pivots and proportions. All production art must remain local, non-replicated, and `NoCollision`. |
