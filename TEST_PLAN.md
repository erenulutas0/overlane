# Test Plan

## Baseline and automated testing

When the project exists, build `OverlaneEditor` before gameplay changes. Add Unreal Automation tests for race state transitions, checkpoint ordering, near-miss duplicate prevention, spawn fairness calculations, and save serialization where they can run without rendering. Tests must assert meaningful behavior, not merely instantiate classes.

## Manual test maps

- **Vehicle Handling:** acceleration, braking, steering at low/high speed, reverse, reset, camera comfort, keyboard, and gamepad.
- **Traffic Stress:** pool reuse, 20–30 prototype actors, lane spacing, spawn visibility/TTC, despawn, and frame timing.
- **Race Flow:** countdown, checkpoint sequence, finish, results, restart, and wrong-way/missed-checkpoint handling.
- **Multiplayer:** later—two/four clients, join/leave, authority, latency behavior, finish agreement, rematch, and host disconnect.

## Performance checks

Profile CPU, GPU, memory, hitches, and (when online) bandwidth independently. Record hardware, build type, map, traffic count, and capture method. The early target is 60 FPS / 16.6 ms at 1080p; this is a budget, not a released system requirement.

## Current result

Baseline editor build succeeded on 2026-07-25: UE 5.8.0 built `OverlaneEditor Win64 Development` from `E:\Overlane` in 105.1 seconds. The initial Unicode Desktop path is unsuitable for MSVC response/PCH files; all future builds use the ASCII-only project path.

## Current play-test procedure

1. Open `E:\Overlane\Overlane.uproject`; confirm `L_VehicleHandlingTest` is the opened map.
2. Click Play in Selected Viewport.
3. Verify a cube placeholder vehicle spawns near the start, rests on the road, and has a third-person chase camera.
4. Hold W to accelerate; use A/D to steer; hold S to brake, then reverse after stopping. If available, confirm left stick steering and right/left triggers accelerate/brake.
5. Drive into each barrier and record whether the vehicle remains on the road and loses speed rather than passing through.
6. Stop the session, close Unreal Editor, and report results with screenshots or exact symptoms.

### Result — initial keyboard pass

The first editor play test confirmed that the cube placeholder spawned, followed correctly by the chase camera, responded to W/A/S/D, braked/reversed, and stopped at both barriers. Gamepad and speed-HUD validation remain pending.

### Result — HUD/camera pass

The second play test confirmed the upper-left km/h HUD, a 180 km/h top speed, and subtle speed-driven chase-camera distance/FOV response. Recovery remained pending at that point.

### Result — recovery and repeat-run pass

The recovery test confirmed that R returned the vehicle to the start at 000 km/h. Three further road runs had no persistent stuck or flip failure. Keyboard validation is complete; hardware gamepad validation remains open.
