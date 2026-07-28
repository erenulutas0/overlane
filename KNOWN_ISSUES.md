# Known Issues

| ID | Severity | Status | Reproduction | Workaround |
|---|---|---|---|---|
| ENV-001 | blocker | resolved | UE 5.8 and Visual Studio C++ tools were previously absent. | Resolved: UE 5.8.0 and Visual Studio 2022 C++ tools are installed and verified. |
| BUILD-001 | blocker | resolved | A Live Coding session initially blocked the normal editor build. | Resolved by closing Unreal Editor before the final successful build. |
| BUILD-002 | blocker | resolved | MSVC could not process response/PCH paths containing `Masaüstü`. | Resolved by moving the project to ASCII-only path `E:\Overlane`. |
| RUNTIME-001 | blocker | resolved | The first Play session used `GameModeBase` and `DefaultPawn` because map settings were placed in `DefaultGame.ini`. | Moved `GameMapsSettings` to `DefaultEngine.ini`; the next play test must verify `OverlaneGameModeBase` and `OverlaneVehiclePawn`. |
| TRAFFIC-002 | high | in verification | Earlier lane-change interpolation at 21-car density produced overlap and deadlock. | A more conservative target-corridor reservation approach is compiled; use the long-session test to confirm it never overlaps or blocks traffic. |
| INPUT-002 | medium | in verification | Gamepad mappings compile but have not been tested on physical hardware. | Connect a controller and run the documented in-game control check. |
| SCORE-001 | high | resolved | The practice bot shared `AOverlaneVehiclePawn` with the human, so its near passes added +250 and each of its collisions cost the human -750. It also called `MarkPlayerCollision`, permanently disarming that traffic car's near-miss encounter for the human. | Resolved by a replicated `bIsAIRacer` flag on the pawn; all three scoring paths now early-return for AI racers. |
| BOT-001 | medium | resolved | The practice bot was never destroyed. A client joining a running solo race left a non-replicated controller driving an unowned pawn inside a multiplayer race. | Resolved: `PostLogin` now calls `DestroyPracticeBot()`. |
| UI-001 | high | resolved | `AOverlaneHUD::DrawHUD` early-returned when the local player had no vehicle pawn, and the menu blocks sat below that guard — no pawn meant a blank screen with no way into the game. | Resolved: menu/lobby/browser screens are drawn before the pawn requirement. |
| UI-002 | high | open | Every menu is gated on `GetAuthGameMode`, which is nullptr on clients. A joined client still has no main menu, settings or pause menu; it only sees the replicated in-race HUD. | Route menus through a client-owned UI layer plus server RPCs. Blocks Phase 7. |
| NET-001 | high | open | Remote clients do not simulate their own vehicle (`ArcadeHandlingComponent.cpp:63-67`); movement arrives only as a hard teleport from `OnRep_OwnerServerTransform`. Two transform channels replicate simultaneously and input RPCs are unreliable and edge-triggered, so a dropped release packet strands the throttle. | None. Needs the fixed-timestep prediction/reconciliation rework before multiplayer feels playable. |
| NET-002 | medium | in verification | The LAN host/join flow compiles but has never been run. `OnlineSubsystemNull` only discovers over LAN broadcast. | Test with two packaged builds on the same network before trusting it. |

No further gameplay defects are known.
