# Known Issues

| ID | Severity | Status | Reproduction | Workaround |
|---|---|---|---|---|
| ENV-001 | blocker | resolved | UE 5.8 and Visual Studio C++ tools were previously absent. | Resolved: UE 5.8.0 and Visual Studio 2022 C++ tools are installed and verified. |
| BUILD-001 | blocker | resolved | A Live Coding session initially blocked the normal editor build. | Resolved by closing Unreal Editor before the final successful build. |
| BUILD-002 | blocker | resolved | MSVC could not process response/PCH paths containing `Masaüstü`. | Resolved by moving the project to ASCII-only path `E:\Overlane`. |
| RUNTIME-001 | blocker | resolved | The first Play session used `GameModeBase` and `DefaultPawn` because map settings were placed in `DefaultGame.ini`. | Moved `GameMapsSettings` to `DefaultEngine.ini`; the next play test must verify `OverlaneGameModeBase` and `OverlaneVehiclePawn`. |
| TRAFFIC-002 | high | in verification | Earlier lane-change interpolation at 21-car density produced overlap and deadlock. | A more conservative target-corridor reservation approach is compiled; use the long-session test to confirm it never overlaps or blocks traffic. |
| INPUT-002 | medium | in verification | Gamepad mappings compile but have not been tested on physical hardware. | Connect a controller and run the documented in-game control check. |

No gameplay defects are known because no game implementation exists.
