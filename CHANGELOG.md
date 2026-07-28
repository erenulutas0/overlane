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

# 2026-07-28 - Session host/join flow and AI racer identity (P5-002)

## Practice bot: finished the interrupted work

- Replaced the unread `PracticeBot` actor tag with a replicated `bIsAIRacer` flag on
  `AOverlaneVehiclePawn`. Tags are not replicated, so clients could never have read it.
- Fixed a live scoring corruption: the bot shares the human pawn class, so its near
  passes were adding +250 to the human score and each bot collision was costing the
  human 750 points. All three scoring paths now early-return for AI racers.
- The bot no longer calls `MarkPlayerCollision`, which had been permanently disarming
  traffic cars so they could never award the human a near miss.
- `PostLogin` now destroys the practice bot when a client joins, instead of leaving a
  non-replicated controller driving an unowned pawn inside a multiplayer race.
- Corrected the result string to `ANTRENMAN BOTU KAZANDI`.

## Session layer

- Enabled `OnlineSubsystem`, `OnlineSubsystemUtils` and `OnlineSubsystemNull`; added the
  first two to `Overlane.Build.cs` and configured `DefaultPlatformService=NULL`.
- Added `UOverlaneSessionSubsystem` (GameInstance subsystem) with host, find, join and
  leave built on the classic `IOnlineSubsystem` interface, so Phase 7 can swap in
  OnlineSubsystemSteam via config plus a `bUseLanMatch` flip rather than a rewrite.
- Hosting creates a listen session and server-travels to the race map; joining resolves
  the connect string and client-travels. No IP address is ever typed.
- Stale sessions are destroyed before a new one is created.

## UI

- Fixed a blocking HUD defect: `DrawHUD` early-returned without a vehicle pawn and the
  menu blocks sat below that guard, so a player with no pawn saw a blank screen.
  Menu, lobby and browser screens are now drawn before the pawn is required.
- Main menu is now a four-row vertical list: SOLO YARIS / ONLINE - OYUN KUR /
  ONLINE - OYUN BUL / AYARLAR.
- Added a session browser screen (results plus a TEKRAR ARA row) and a host lobby screen
  showing the connected player count, where Enter starts the race.
- W/S navigate the browser; Esc and the pause key back out of it.

`OverlaneEditor Win64 Development` compiles clean. **No part of the session flow has been
run yet** - `OnlineSubsystemNull` only discovers over LAN broadcast, so it needs two
packaged builds on one network to validate.

# 2026-07-28 - Practice bot becomes a competitive rival (P5-005)

## The reported bug, and what it actually was

The user reported: "the bot never leaves the right lane; it drives directly behind the
traffic car ahead of it without hitting it and just travels along with it."

It was not polite car-following. The bot had a hardcoded `SetBrakeInput(0.0f)` next to a
constant `SetThrottleInput(0.82f)` and no perception at all, so it drove *into* the traffic
car every frame. The swept move was blocked by that car's collision and the horizontal
impact penalty cut its speed to 45%, after which the throttle re-closed the gap - a
multi-hertz limit cycle pinned to the lead car's speed. From the chase camera that reads as
a car keeping station. It also had no lane-change logic whatsoever, and never boosted, so
its ceiling was 180 km/h against the player's 244.8 km/h; the outcome was decided in advance.

## Bot driver rewrite

- Forward sensing along the lane spline, following the traffic system's own car-following
  curve (now shared as `ATrafficDirector::ComputeFollowSpeedLimit`) so the rival and the
  traffic around it behave identically. Following distances scale with real closing speed,
  because the bot closes ~3.8x faster than traffic closes on traffic.
- Real throttle/brake control. These are mutually exclusive in the handling component, so
  the controller never sets both.
- Overtaking: when held below cruise for 0.6 s it scores both neighbouring lanes and merges
  into the better one, steering the pawn physically rather than teleporting it. Asymmetric,
  speed-scaled safety windows; aborts and steers back if the target lane closes up.
- Fixed the `StartingDistance` lower clamp, which froze the bot's perceived progress forever
  if anything knocked it backwards. Distance is now tracked continuously with a per-frame
  step limit. Added spin recovery: brake, turn, resume.
- Steering gained a damping term, a speed-scaled look-ahead and a slew limit, so a lane
  change reads as a merge rather than a swerve at 180 km/h.
- Difficulty setting (`RAKIP ZORLUGU: KOLAY / NORMAL / ZOR`) driving speed scale, turbo use
  and rubber-band strength, persisted in the local settings save. Applies on the next race.
- Turbo use with hysteresis, never while blocked, merging or in contact. Sustainable duty is
  ~30%, giving roughly 199 km/h average - beats a human who never boosts, loses to one who
  boosts well.
- Light rubber-banding (max +-6% at 120 m) that freezes inside the last 300 m so the finish
  is honest. Per-race random seed so two runs at the same difficulty differ.

## Correctness fixes found along the way

- **Near-miss theft (live bug).** `ATrafficVehicleBase::HandleNearMissBegin/End` did not
  check `IsAIRacer`, and the end handler cleared the shared encounter flag unconditionally.
  A bot passing a traffic car silently cancelled the human's in-flight near miss. The
  pawn-side guards added earlier did not cover this path.
- **Traffic could not see the bot.** Spawn safety, the spawn window anchor and lane-change
  safety all iterated player controllers, which the bot does not have - so traffic spawned
  inside it and merged through it. All three now use a cached racer list. The spawn window
  uses the lead racer and recycling uses the trailing racer, which were previously the same
  function returning the wrong answer for one of them.
- `ShouldHoldForPlayer` is deliberately left human-only, now with a comment saying why:
  it hard-stops traffic as an anti-tunnelling hack, and including the bot would deadlock
  both cars at a standstill.
- Bot finish detection was nested inside the human-pawn null check, so a momentarily missing
  player pawn made the race unfinishable. Hoisted out.
- Settings values are clamped on load; `GetSettingsLine` indexes fixed arrays with them.

## Other

- Racer-on-racer contact shows `RAKIP TEMASI` in amber and deliberately does not charge the
  human the 750-point traffic-collision penalty. The bot lifts off for a beat after contact.
- The rival is crimson, unused by the player or any traffic profile.
- Solo HUD shows `RAKIP: n M ONDE / GERIDE`.
- One shared `ATrafficLanePath::CollectSortedLanes` replaces three hand-rolled lane lists
  that used different filters and could disagree on what lane index N meant.

`OverlaneEditor Win64 Development` compiles clean. **Runtime validation is pending** - none
of the new driving behaviour has been observed in play yet.

# 2026-07-28 - Rival control loop, corrected after adversarial review

An independent frame-accurate review of the previous commit refuted two of its
three steering fixes and found that one of them made the reported wall strike
roughly twice as bad. Corrections:

- **The wall strike was a bug introduced by the lane-change work, not derivative
  kick.** On the frame a merge completed, `TrackedLaneDistance` was set to -1 to
  "re-seed against the new spline" - but `UpdateTrackedLaneDistance` had already
  run that tick and the steering block consumes the value later in the SAME tick.
  The aim point therefore landed about 1.5 km behind the car, `bSpunAround` fired,
  and full lock was applied toward the side the car was already overshooting.
  It now re-projects onto the new lane in place, keeping the step clamp.
- **Reverted the shorter merge look-ahead.** Halving it halves the pursuit time
  constant: measured damping ratio fell from 0.42 to 0.19 and lane-width overshoot
  rose from 24% to 55%. It made the symptom it was meant to fix worse.
- **Reverted the sign-crossing completion test.** It was unreachable: crossing the
  tolerance band inside one frame needs a lateral rate ~2.9x the car's speed.
  Replaced with the correct version - a merge completes only when the lateral
  offset AND the heading offset are both small, so the car cannot be declared
  merged while still carrying outward yaw with nothing to shed it.
- **The steering damping term was anti-damping.** `-Kd * d(HeadingError)/dt`
  contains `+Kd * yaw_rate`, i.e. positive yaw-rate feedback. Measured damping
  ratio is 0.489 at Kd = 0 versus 0.415 at Kd = 0.28. Default is now 0, with a
  note that real damping must come from measured yaw rate.
- **Spin recovery no longer bypasses the slew limiter.** It assigned the steering
  command directly, putting a 2.0-wide step into yaw rate that took ~290 ms to
  unwind. All paths now feed one limiter.

The "changes lane very slowly" half of the report had a separate, structural cause:

- **Lane-change clearance was absolute-speed based.** It demanded
  `max(3200, BotSpeed * 1.6)` ahead - 80 m at cruise - while the traffic director
  packs each lane into platoons spaced at its own 32 m following distance. The
  window was structurally never available. Clearance is now derived from RELATIVE
  speed via the same braking-distance law used for following, so a merge is legal
  exactly when it is physically safe.
- **The overtake benefit test compared raw gaps.** Both the current and candidate
  gap are pinned to the same platoon spacing, so the "candidate must beat mine by
  15 m" test almost never passed. It now compares the speed each lane would
  actually allow, which is the quantity the decision is really about.
- `ComfortDeceleration` lowered from 2600 to 1600 cm/s^2. The brake command has a
  0.20 floor, making the achievable deceleration set {550 coast} union [1440, 7200],
  so 1440 is a hard lower bound. At 1600 the blocked flag trips 36 m back = 0.72 s
  of warning at 180 km/h, comfortably above the 0.6 s the overtake needs to arm;
  at 2600 it tripped at 0.53 s and the overtake could never arm at cruising speed.

`OverlaneEditor Win64 Development` compiles clean. Runtime validation pending.

# 2026-07-28 - Rival validated in play

Manual solo run confirms the corrected control loop. The rival accelerates on open
road, slows and holds a gap behind slower traffic rather than grinding its bumper,
changes lane when it is held up, and returns to speed once the road clears. The
tester's summary: it drives "just like us". The HUD rival gap reads correctly and
flips colour on overtake, and the settings screen can now be backed out of with Q.

P5-005 is closed. Remaining rival work is polish, tracked as BOT-005 (difficulty is
read once at spawn, so pause-menu changes apply next race) and SAVE-001 (the
persistent best time is a legacy record from the retired 300 m route and is
unbeatable on the current 6 km one).

# 2026-07-28 - Netcode N-001 and N-002: pause gate and input batches

## N-001 - the pause gate was a no-op on clients

`UArcadeHandlingComponent` gated driving on `GetAuthGameMode`, which is null on a
client, so the check silently did nothing there. Only the authority test two lines
below hid it. It now falls back to `AOverlaneRaceGameState::IsRaceActive/IsRacePaused`,
which every machine can see - required before the client simulates its own vehicle.

## N-002 - sequenced, redundant input batches

The old path sent one `Unreliable` RPC per input event, with no resend, no timeout and
no heartbeat. Releasing the throttle produced exactly one packet, so **losing it left
the server holding the throttle down forever.** `ETriggerEvent::Triggered` also fired
every frame on every axis, producing roughly 432 RPCs per second.

- The `Handle*` functions now only update the input cache.
- The owning client samples that cache once per fixed step into a command with a
  sequence number, and sends `ServerSendMoveBatch` at 30 Hz carrying every command it
  has not seen acknowledged, capped at 12 - 200 ms of redundancy, so a dropped packet
  is recovered by the next one without paying for reliable delivery.
- The server merges batches into a per-pawn jitter queue keyed on sequence, ignores
  anything already consumed (compared as a signed delta so the wrap at 65535 does not
  stall the stream), and resynchronises rather than banking input that is implausibly
  far ahead.
- The handling component consumes exactly one command per fixed step. If the queue
  runs dry it repeats the client's last known intent and flags starvation, instead of
  inheriting whatever scalars happened to be cached locally. Two catch-up steps per
  frame drain a deep queue after a latency spike, in order.
- Queued input is discarded while driving is not allowed, so a client that keeps
  sending through a pause cannot cash the backlog the moment it lifts.

Standalone and the listen-server host driving their own car bypass the queue entirely
and sample the cache directly, so single-player is unchanged.

`OverlaneEditor Win64 Development` compiles clean. N-002 needs the two-client packet
loss test before it can be called done.

# 2026-07-28 - Netcode N-003: client-side traffic extrapolation

A client's traffic sat wherever the last packet put it, roughly one-way latency behind
the server - about 175 cm at 75 ms for a fast car. That error landed directly on the
player's collision and near-miss geometry, so the client could see a gap the server did
not agree existed.

- `ATrafficVehicleBase` replicates `int16 ReplicatedLaneSpeed`, and clients extrapolate
  along it between snapshots. The speed is replicated rather than inferred from two
  position samples because it already encodes both the `ShouldHoldForPlayer` freeze and
  the `FInterpTo` acceleration, neither of which position differencing can see. The
  remaining error is the acceleration term only - centimetres.
- Clients never re-run the lane logic. `ShouldHoldForPlayer` iterates player controllers,
  and a client sees only itself, so identical code would give a permanently different
  answer.
- One deliberate exception: the server stops a traffic car dead inside a 540 x 310 uu box
  around a player, and the near-miss window is 300 uu wide - so that freeze happens during
  essentially every near miss. Naive extrapolation would slide the car forward and show
  the client a gap that is not there, at exactly the scoring moment. The hold test is
  reproduced locally for the local player only, which is the one case a client can
  evaluate the same way the server does.
- Mispredictions are biased toward false negatives: the client-side collision box is inset
  12 uu laterally, so a marginal graze becomes a late server collision rather than a ghost
  collision the client felt and the server never agreed to.
- `NearMissTrigger` overlap events are disabled on clients; scoring is server-only.
- A new snapshot no longer pops the car backwards - the extrapolated error becomes a
  decaying visual offset. Extrapolation stops after 250 ms without a snapshot, past which
  guessing is worse than standing still.
- Toggle with `overlane.Net.ExtrapolateTraffic`.

`OverlaneEditor Win64 Development` compiles clean. Single-player is untouched: the
authority path is unchanged.

# 2026-07-28 - Netcode N-004: owner wire format

Replaces the raw `OwnerServerTransform` channel with `FOverlaneMoveAck`.

Under LWC the old channel sent an unquantised `FTransform` - ten doubles, about 80 bytes -
at 60 Hz, roughly 4.8 KB/s per owner. The ack is about 15 bytes at 30 Hz, roughly 450 B/s,
and carries strictly more: the last input sequence the server consumed, quantised position
and yaw, exact speed, boost charge, the collision event count and the driving-allowed and
input-starved flags. That is the channel reconciliation needs, and it is an order of
magnitude cheaper on the one channel that most needs headroom.

`SetReplicateMovement` is deliberately left on: it is `COND_SimulatedOrPhysics` in the
engine, so the owner never paid for it, and it is how other players' cars move. There is
no double-send to remove.

This step is behaviour-neutral on purpose - `OnRep_ServerMoveAck` still hard-snaps exactly
as the old handler did - so a replication break can be told apart from a feel change.
Replay and smoothing arrive in N-008.

`OverlaneEditor Win64 Development` compiles clean. Single-player is untouched.

# 2026-07-28 - Netcode N-005 and N-006: measurement first, then authority hygiene

## N-005 - the correction overlay ships BEFORE the correction

`overlane.Net.DrawCorrection 1` draws the server's last acknowledged pose as a wireframe
ghost box and prints the longitudinal, lateral and yaw error plus the acked input
sequence. Deliberately not gated on the game mode, which is null on a client - precisely
where the number needs to be readable.

Every remaining step in this rework is tuning against this error, and without a ghost
there is no way to tell a genuine divergence from a projection artefact. Prediction is
still off, so the ghost must currently sit exactly on the car: that is the check that the
measurement itself is trustworthy before anything is enforced.

## N-006 - authority hygiene, including a live single-player exploit

1. **`R` was a free full turbo bar.** `ResetState` refilled `BoostCharge` to 1.0, and
   `RecoverToStart` called it with no cooldown - on the host, server-side, with nothing to
   stop it. Recovery now preserves the boost charge.
2. **Recovery is now the server's decision.** It ran client-locally with no authority check
   and no cooldown, so a client could reposition itself at will and the next correction
   would drag it back - a rubber-band loop that also read as a bug. It is now a reliable
   server RPC with a 5 s cooldown, gated on driving being allowed.
3. **`RegisterTrafficImpact` is split by authority.** Local impact feedback runs anywhere,
   because being a frame early on a cosmetic flash beats being a round trip late. But
   `MarkPlayerCollision`, the contact set and the game-mode collision counter are now
   server-only. `MarkPlayerCollision` permanently disarms that traffic car's near-miss
   encounter, so a client running it silently denied itself points for a collision the
   server may never have agreed happened.

`OverlaneEditor Win64 Development` compiles clean.

# 2026-07-29 - Traffic is now maintained per racer, not per leader

A measured solo run exposed a structural defect in the traffic director. Spawning was
anchored to the FURTHEST racer (`TrafficDirector.cpp:223`) while recycling was judged
against the NEAREST one (`:238`). With the player at 130 km/h, the rival at 58 km/h and
traffic itself at ~54 km/h, the rival moved at almost exactly traffic speed - so no car
ever fell the required 150 m behind it, nothing recycled, and therefore nothing respawned.

The consequences were larger than a tuning problem:

- The player cleared all 21 cars inside the first minute and then drove **five kilometres
  of completely empty road**. The dense-traffic pillar simply stopped existing for most of
  the race.
- The rival was permanently trapped in the pack it could not clear, and by being there it
  also pinned the whole pool in place. No amount of AI tuning could have fixed this: the
  leader structurally sees sparse traffic and the follower structurally sees dense traffic.

Measured before the fix: player 130 km/h average over 6 km, rival 58 km/h, rival 3331 m
behind at the finish. The rival figure matches the 61 km/h the design analysis predicted
for the dense case, confirming that model.

Fix: every pool slot is now anchored to whichever racer currently has the least traffic
ahead of it, and recycling is judged against that same racer, so density is maintained
around each racer independently rather than around whoever happens to be in front. The
pool is sized by a new `RacerSupplyCapacity` (default 2) because traffic is now shared -
a race with a rival needs twice the pool of a time trial to feel the same density.

Two other places mapped a pool index to a lane and a slot using `VehiclesPerLane`. The
per-lane block grew, so both were silently reading the wrong index: traffic profile
selection and traffic lane-change selection. Both now use `GetSlotsPerLane()`.

`OverlaneEditor Win64 Development` compiles clean. **The 42-car pool needs a frame-rate
check**; if it costs too much, drop `VehiclesPerLane` from 7 to 5 for a 30-car pool.

# 2026-07-29 - Stop painting over the cars PBR materials

A rendering diagnosis found that the single largest contributor to the build's "plastic
toy" look was not the models. It was this code.

`ATrafficVehicleBase::ApplyTrafficVisualState` overwrote **every** material slot on the
SportsCar mesh with a dynamic instance of `/Engine/BasicShapes/BasicShapeMaterial` - a
material with no normal map, no roughness map, no metallic and no AO. Epic's authored
`MI_SportsCarBody` ships with proper metallic/roughness/AO car paint, and the code threw
all of it away to set one colour. `AOverlaneVehiclePawn::SetBodyColor` did the same.

The proof was already running in the build: the player's car and the traffic cars use the
bit-identical `SM_SportsCar` mesh, but `SetBodyColor` is only ever called for the AI rival,
so the human's car kept its authored PBR while traffic was repainted flat. Same geometry,
same lighting, side by side - and the player's car visibly read as more real. Geometry was
held constant and appearance still differed, so appearance was never driven by geometry.

Both paths now tint the mesh's **authored** materials instead of replacing them:

- The material is asked whether it exposes a tint parameter, rather than assuming a name -
  seven candidates are tried, because Epic's templates and third-party packs disagree.
- Slots with no tint parameter (glass, tyres, trim) keep their authored material instead of
  being painted body colour.
- If nothing on the mesh is tintable, the old flat material is used as a fallback. Traffic
  colour is gameplay-readable state that near-miss and collision feedback match against, so
  it can never be silently lost in exchange for nicer shading.
- Dynamic instances are always created from the static mesh asset, never from the
  component, so a pooled car changing variant cannot nest instances inside each other.

`OverlaneEditor Win64 Development` compiles clean.

# 2026-07-29 - Rendering settings and the motion pass

## The renderer was switched off

`Config/DefaultEngine.ini` had no `[/Script/Engine.RendererSettings]` section at all - it
went from `GameMapsSettings` straight to `OnlineSubsystem`. Because those config values
zero-initialise, that absence meant Lumen global illumination, reflections and virtual
shadow maps were not turned down but **off**. Every GI, reflection, shadow and
anti-aliasing choice in the game was whatever `BaseEngine.ini` happened to default to.
The "plastic toy" look was the faithful, correct output of an engine with its rendering
features disabled.

The section now enables Lumen GI, screen-space reflections (deliberately not Lumen
Reflections - a road seen from a chase camera is SSR's best case and it saves 1.5-2 ms),
virtual shadow maps, mesh distance fields, TSR and local exposure.

**Enabling mesh distance fields triggers a full derived-data rebuild of every static
mesh - expect 10-30 minutes of unresponsive editor on first open.**

## The motion pass

Nothing on screen moved except position, which is why 245 km/h read like 80.

- **Wheels now turn.** They were set once in the constructor and never touched again. At
  245 km/h a 34 cm wheel should turn ~32 times a second. Front wheels also steer.
- **Cosmetic body attitude.** The car could not dive, squat or lean, so every control
  input produced zero visible body response - the largest control-feedback gap in the
  build. Roll follows steering scaled by speed, pitch follows longitudinal acceleration.
  Applied to the mesh only, never to the collision root and never to
  `FOverlaneVehicleSimState`: rolling the root would change the box orientation and break
  the tested near-miss and traffic-collision behaviour.
- **Camera lag.** The boom was rigidly welded to the collision box, so a lane change made
  the world snap sideways as one block. Now it trails, overshoots and settles.
- **The camera curve is now asymmetric and acceleration-aware.** A sense of speed comes
  from change, not from an absolute value, so acceleration feeds FOV directly and the
  throttle visibly punches it out. FOV expands at rate 12 and returns at 3, instead of 4
  in both directions, which made the boost kick arrive mushy.
- `MaxCameraDistance` 1050 to 820: pulling the arm back at speed shrank the car and
  reduced apparent road motion, which is the toy-car-on-a-track look. FOV does that job.
- `MaxCameraFov` 104 to 100, with the boost kick raised to +8 so the extreme is reserved
  for boost rather than being the cruising state.

The full free-only art plan is saved as `ART_PLAN.md`.

`OverlaneEditor Win64 Development` compiles clean.
