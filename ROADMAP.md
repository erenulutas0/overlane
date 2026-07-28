# Roadmap

| Phase | Outcome | Exit condition |
|---|---|---|
| 0 | Audit and foundation | Project state documented; source-control rules and Phase 1 plan exist. |
| 1 | Local vehicle prototype | One vehicle completes a test road with stable keyboard/gamepad driving, camera, reset, and speed HUD. |
| 2 | Basic traffic prototype | 20+ pooled lane-following vehicles operate without unsafe visible spawns. |
| 3 | Solo Traffic Sprint | Countdown, route progress, finish, results, restart, and near misses work in one complete local race. |
| 4 | Traffic quality | Dense, fair, profiled traffic with readable lane changes and stress testing. |
| 5 | Minimal multiplayer | Two clients complete synchronized empty-road race using listen-server authority. |
| 6 | Multiplayer Traffic Sprint | Two to four players complete authoritative traffic races with rematch. |
| 7 | Steam integration | Private lobby, invitation, join, and packaged Steam test build work. |
| 8 | Content and presentation | Licensed/original art, sound, UI, settings, and optimization reach presentation quality. |
| 9 | Demo and release | Stable packaged build, audit, playtesting, store readiness, and release checklist complete. |

### Immediate dependencies

Phase 1 cannot start until Unreal Engine 5 and compatible C++ build tools are installed, then a new C++ project exists in this repository. Multiplayer explicitly depends on validated local driving, and Steam integration depends on working session flow.
