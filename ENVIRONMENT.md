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

### Downloading: use the in-editor Fab browser, not the launcher

UE 5.8 ships the Fab plugin (`Engine/Plugins/Fab/Fab.uplugin`, `EnabledByDefault: true`).
Open **Overlane itself** in the editor → **Window ▸ Fab** → sign in → **My Library** →
**Add to Project**. Content lands straight in `E:\Overlane\Content\<Pack>`. This
bypasses the launcher's broken *Add to Project* dialog entirely.

Two format traps, both of which cost time here:

- **"Complete project"** listings (e.g. Vehicle Variety Pack **Volume 2**) can only
  *create* a project — they never show *Add to Project*. Only **"Asset package"**
  listings can be added to an existing project.
- Engine-version ceilings are per listing; check *İçerdiği biçimler* before trying.

### Downloaded packs are NOT in git

`/Content/VehicleVarietyPack/` is gitignored. It is 1.4 GB and `.gitattributes` routes
`*.uasset` to Git LFS, whose free quota is 1 GB storage / 1 GB month bandwidth — one
pack would exceed it. The packs are re-downloadable from the Fab library at any time.

The code degrades gracefully: `ATrafficVehicleBase`'s `FObjectFinder`s return null when
the pack is absent and every traffic profile falls back to the previous SportsCar
assembly, so a fresh clone runs without downloading anything.

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

## Where the work stands (2026-08-01)

**Free roam** — `overlane.FreeRoam=1` under `[SystemSettings]` in
`Config/DefaultEngine.ini`. No countdown, no finish, no rival; traffic and environment
still run. Use it to inspect the map.

### Next: curved route with gradients

The user wants curves and **gradients** (uphill/downhill) but explicitly **not** bumps.
That distinction matters technically: both need the pawn to follow ground height
continuously instead of tracing Z once at `BeginPlay`, but gradients are low-frequency,
so they do not fight the collision sweep or destabilise PRSA replay the way
high-frequency bumps would.

Order, and why:

1. **Route centreline spline generator** — curvature as a parametric profile, lanes
   derived from it. Do NOT hand-drag spline points.
2. **Shared station parameterisation — the load-bearing step.** Parallel lane splines
   do not have equal arc length. The difference is exactly `d × Θ` (d = lateral offset,
   Θ = total heading change) and **the radius cancels**, so gentle curves buy nothing.
   At 600 cm lane spacing and a 25.8° sweep that is **270 cm**, against a
   `MergeBufferBehind` of 150 — large enough that the rival would clear a car it is
   already touching as safe. Lanes must be indexed by a shared route station, not by
   their own distance.
3. **Geometry onto the spline** — road, shoulders, guardrail, verge.
4. **Vertical profile** — this is where the pawn's ground-follow change lands.
5. **Adapt the rival to curvature** — its pure-pursuit steering has steady-state
   cross-track lag of `e = L(v/(R·Kp·ω_max) − L/2R)`: 144 cm at R = 1000 m on boost
   against a `LaneCompleteTolerance` of 120, so **no merge would ever complete**.
   Minimum radius 2000 m (error 72 cm) plus a curvature feed-forward term that is
   exactly zero at κ = 0, so the tuned P gain is untouched.

A full 8-agent design pass found 25 blocking defects in an earlier version of this
plan; the ones above are the survivors that matter. Do not skip step 2.

### Open, unresolved

- **Rival still cannot beat the player.** Boost is no longer the constraint (duty cycle
  reaches the economy's own 30% ceiling). Cause unknown. **Do not tune it on single
  unseeded runs** — measured spread on one build is 139–184 km/h, ±14%, which swamps
  every effect chased so far. Pin `overlane.Bot.Seed` first.
- **Ground streaks** — diagnosed as grazing-angle undersampling (6.3 km plane viewed
  down its length), fixed by `r.MaxAnisotropy` 8 → 16 plus stronger mid-field fog, but
  **not visually verified**. If they persist, cut the length-wise repeat count rather
  than adding more filtering.

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
