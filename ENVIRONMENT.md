# ENVIRONMENT — where everything lives

Read this first in a new session. It records machine-level facts that are **not**
derivable from the source tree, and the traps that have already cost time.

Last verified: 2026-07-30.

---

## Paths

| What | Where | Note |
|---|---|---|
| **Real project** | `E:\Overlane` | The only real repo. `E:\Overlane\.git` |
| Symlink shadow | `C:\Users\pc\OneDrive\Masaüstü\traffic-game` | **Symlinks only.** Its `.git` is a broken leftover — never commit from here |
| Unreal Engine | `E:\EPIC_GAMES-games\UE_5.8` | **5.8 is the only version installed** |
| Build script | `E:\EPIC_GAMES-games\UE_5.8\Engine\Build\BatchFiles\Build.bat` | |
| Shared DDC | `E:\UnrealCache\DerivedDataCache` | Moved off C: on 2026-07-30. Env var `UE-LocalDataCachePath` (User scope) points here |
| Project DDC | `E:\Overlane\DerivedDataCache` | |
| Fab vault cache | *not yet created* | **Set the Launcher download location to E: before downloading anything large** |

### Disk

- `C:` — 465 GB, **only ~20 GB free (96 % full)**. Do not let caches or downloads land here.
- `E:` — 1.9 TB, ~960 GB free. Everything heavy goes here.

`E:\Overlane\Intermediate` is ~2.7 GB and is build output — safe to delete, it rebuilds.

---

## Build

```bash
cd /e/Overlane && "/e/EPIC_GAMES-games/UE_5.8/Engine/Build/BatchFiles/Build.bat" \
  OverlaneEditor Win64 Development -Project="E:\Overlane\Overlane.uproject" -WaitMutex
```

Headless Unreal Python (**the editor must be closed**):

```
UnrealEditor-Cmd.exe E:\Overlane\Overlane.uproject -run=pythonscript -script=<path>
```

`unreal.log` is **not** captured from a headless commandlet — write reports to a file
instead. `E:\Overlane\Tools\InspectLighting.py` does this and exists because of it.

---

## Reproducible measurement

The rival used to be unmeasurable between runs: `ConfigureDifficulty` jitters
`DifficultySpeedScale` by ±1.5 % and `BoostEngageCharge` by ±0.10 from an unseeded
stream, and traffic drew lane-change directions from the global RNG. Two runs
therefore differed **before a frame was simulated**.

To pin a race, edit **`Config/DefaultEngine.ini`**:

```ini
[SystemSettings]
overlane.Bot.Seed=1234    ; 0 = random per race (shipping)
```

This survives a restart; the console equivalent (`overlane.Bot.Seed 1234`) does not.
The seed used is logged at `BeginPlay` either way.

**Always pin the seed before claiming a telemetry change means anything.**

---

## Fab / free assets

Licence position, verified on the listings:

| Pack | Engine versions | Status |
|---|---|---|
| [Vehicle Variety Pack](https://www.fab.com/listings/dc1ada50-2523-44b1-b0e2-a72d14076fb4) | 4.21–4.27, **5.0–5.8** | Free For Life. 5 vehicles, ≤4 material slots each |
| [City Sample Vehicles](https://www.fab.com/listings/2909157b-ddfa-4cef-a925-69dc2467021f) | **5.0–5.3 only** | Free, *UE-Only Content* licence. 13 vehicles |
| Automotive Materials (Epic) | — | Free |
| Quixel Megascans | — | **No longer free** — the free-for-all period ended 2024-12-31 |

*UE-Only Content* means free for use in Unreal-based products, which Overlane is —
commercial release included. All these packs are flagged **"allow AI use: No"**, which
does not restrict shipping them in the game but does rule them out as training input
for the `B-004` AI-vehicle backlog item.

### The "Uyumlu kullanıcı projesi bulunamadı" trap

Fab's *Add to Project* lists **nothing at all**, even for packs that support 5.8. Two
independent causes were ruled in:

1. City Sample Vehicles genuinely caps at 5.3, so it can never appear.
2. The launcher does not know `E:\Overlane` exists. `Documents\Unreal Projects` has
   never been created on this machine, and the project was made outside the launcher.
   The `RecentlyOpenedProjectFiles` entry found in
   `%LOCALAPPDATA%\UnrealEngine\5.8\Saved\Config\WindowsEditor\EditorSettings.ini`
   is the **editor's** list, not the launcher's.

Working route: use **Create Project** (not *Add to Project*) to drop the pack into a
throwaway project on `E:`, then copy its `Content\<Pack>` folder into
`E:\Overlane\Content\`. UE 5.8 loads older uassets fine; the reverse is not true.

---

## Constraints that are permanent

- **No Chaos Vehicles.** Movement is kinematic: one scalar `CurrentSpeed`, then
  `AddActorWorldRotation` (yaw) and a swept `AddActorWorldOffset`. Decisions D-004 /
  D-005. This is why the road may only ever have *gentle* curves.
- **The route must stay dead flat.** The pawn traces Z once at `BeginPlay` and never
  re-traces, so elevation change would leave cars floating or sunk.
- Fixed `1/60` timestep with a PRSA netcode design (predict, reconcile, sequenced ack).
  Anything with per-frame randomness or non-recomputable state breaks replay.
- No ripped assets, no real manufacturer logos, fictional brands only, no Steam
  credentials in source control.

---

## Related documents

`TASKS.md` · `DECISIONS.md` · `KNOWN_ISSUES.md` · `CHANGELOG.md` ·
`NETCODE_PLAN.md` · `ART_PLAN.md` · `SESSION_HANDOFF.md`
