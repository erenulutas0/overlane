# OVERLANE

OVERLANE is a PC-first, high-speed arcade traffic racing game planned for Steam. Players weave through dense but fair highway traffic in short, replayable Traffic Sprint races.

## Current state

Phase 0 is complete and the project has entered Phase 1. The UE 5.8 C++ project resides at `E:\Overlane`; the former Desktop path contains no project files. A clean `OverlaneEditor Win64 Development` baseline build completed successfully in 105.1 seconds. There is no playable map, vehicle behavior, input mapping, or production asset yet.

## Prerequisites

- Windows 10/11 development machine
- A stable Unreal Engine 5 installation, selected deliberately and kept fixed for the project
- Visual Studio 2022 with the Game development with C++ workload and Windows SDK compatible with the selected engine
- Git and Git LFS (both detected in this environment)

## Getting started after the engine is installed

1. Open `E:\Overlane\Overlane.uproject` using Unreal Engine 5.8.0.
2. Keep generated folders out of source control.
3. Build the `OverlaneEditor` target after C++ changes.
4. Follow the Phase 1 task order in [TASKS.md](TASKS.md).

See [SESSION_HANDOFF.md](SESSION_HANDOFF.md) for the exact next action.
