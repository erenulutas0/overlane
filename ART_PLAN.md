# OVERLANE — Free-Only Realism Plan

*Every claim below was re-verified against the files on disk at `E:\Overlane` and the engine at `E:\EPIC_GAMES-games\UE_5.8` before writing. Line references are current.*

---

## 1. THE HONEST HEADLINE

With zero budget you can get Overlane to **"credible mid-budget highway racer in motion"** — the look of a competent Steam release that reviews as "surprisingly good-looking for a solo dev", not as a toy. That is genuinely achievable, and the reason it is achievable is that the build currently has **no rendering decisions in it at all**: `E:\Overlane\Config\DefaultEngine.ini` is 66 lines and contains zero occurrences of `RendererSettings`, which means global illumination, reflections *and* virtual shadow maps are all switched **off** — not turned down, off — because the config UPROPERTYs zero-initialise to `EDynamicGlobalIlluminationMethod::None` / `EReflectionMethod::None` / VSM disabled. Add to that a single `/Engine/BasicShapes/BasicShapeMaterial` instance serving asphalt, steel, glass and tree bark alike, and code at `TrafficVehicleBase.cpp:325-327` that actively paints over Epic's authored PBR car paint with that flat material, and the plastic look is the *correct output* of the current settings. Fixing it costs hours, not money.

**The specific ceiling free cannot pass:** *uniqueness*. Every CC0 asphalt tile you can download is also in a hundred other games, and a tiled surface repeated 750 times over 6 km will always be a tiled surface. Free gets you correct materials, correct light, correct motion and correct silhouettes — it does not get you **bespoke geometry and bespoke texture variation authored specifically for your road**. Concretely, the things that require money are: (a) unique non-tiling surface art and hero decals — an artist's time, or Megascans surfaces at ~$0.99 each; (b) brand-accurate real cars — never legally free, and mostly not available to a solo dev at any price; (c) modular highway kits with correct W-beam guardrail, gantries and localised signage — $20-60 on Fab; (d) vehicle damage/deformation; (e) a cockpit/interior view. Note also that steps 1-3 below total **under 2.5 hours** and produce the single biggest jump available, so this whole exercise need not displace the online-racing work that is your actual critical path.

---

## 2. THE ORDERED PLAN — ranked by visual gain per hour

**Step 0 (10 min, do this first).** Park the car at a fixed transform on the highway, note the exact coordinates, screenshot. Re-shoot from that same transform after every numbered step. With this many interacting changes your memory will lie to you about which change bought what.

---

### 1. Write the missing `[/Script/Engine.RendererSettings]` block — **0.5 h + unattended rebuild**

**What to do.** Append to `E:\Overlane\Config\DefaultEngine.ini`:

```ini
[/Script/Engine.RendererSettings]
r.DynamicGlobalIlluminationMethod=1
r.ReflectionMethod=2
r.Shadow.Virtual.Enable=1
r.GenerateMeshDistanceFields=True
r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange=True
r.DefaultFeature.LocalExposure.HighlightContrastScale=0.8
r.DefaultFeature.LocalExposure.ShadowContrastScale=0.8
r.AntiAliasingMethod=4
r.Lumen.HardwareRayTracing=0
r.SSR.Quality=3
r.MaxAnisotropy=8
r.SkinCache.CompileShaders=True
```

`ReflectionMethod=2` is Screen Space, deliberately — not Lumen Reflections. Your scene is a horizontal surface viewed from a chase camera, which is SSR's *best* case (everything a road reflects is already on screen), and it saves 1.5-2 ms versus Lumen Reflections. `RayTracingMode=Disabled` already in `WindowsTargetSettings` is correct and should stay: 20-60 moving vehicles means constant BLAS/TLAS rebuilds.

**Download:** nothing. **Licence:** engine settings, UE EULA, zero exposure.

**What the player notices:** the shadowed side of every car and the underside of the interchange stop being flat black (Lumen sky bounce). The road reflects *something* for the first time. Shadows line up with the objects casting them. This is the largest single-file change available to you.

**Caution:** enabling `r.GenerateMeshDistanceFields` triggers a full DDC rebuild of every static mesh — 10-30 min of unresponsive editor. Do it before a break. Measure VSM cost with `stat GPU` at 60 cars; if it's over ~4 ms, set `r.Shadow.Virtual.Enable=0` and fall back to tuned CSM (step 3).

---

### 2. Fix the sun and add contact shadows — **0.75 h**

**What to do.** In `L_VehicleHandlingTest.umap`, on `DirectionalLight_0`:

| Property | Value |
|---|---|
| Rotation Pitch | **-15°** (currently near-overhead) |
| Rotation Yaw | set so the sun is **~40° off the highway axis** — never straight down it |
| Source Angle | **1.2** (default 0.5357) |
| Use Temperature / Temperature | **on / 5800 K** |
| Atmosphere Sun Light | **on** |
| **Contact Shadow Length** | **0.03**, or tick *In World Space Units* and use **20** (cm) |
| Dynamic Shadow Distance MovableLight | **40000** (currently 200 m — this is why the 6 km straight reads as a tabletop) |
| Num Dynamic Shadow Cascades | **4** |
| Distribution Exponent | **3.0** |

World-space contact shadow length matters specifically for you, because your camera arm varies 750→1050 and screen-space length would make the tyre-contact gap breathe as the camera moves.

**Download:** nothing. **Licence:** engine, zero exposure.

**What the player notices:** cars stop hovering. Right now the shadow-map texel footprint at highway distance is far larger than the tyre-to-road gap, so the shadow starts several centimetres away from the tyre and every car reads as pasted on. Contact shadows fill exactly that gap for ~0.1-0.3 ms. Separately, a 15° sun rakes across the tarmac and produces the specular sheen band toward the horizon — the strongest "this is a real road" cue that exists in any highway photograph.

**Also:** turn `Cast Contact Shadow` **off** on the `LaneDashes` and `DistantHills` ISMs — you will never see those traces.

---

### 3. Stop painting over Epic's car materials — **1 h**

**What to do.** You are already running a controlled experiment: the player's car and the traffic cars use the *bit-identical* `/Game/Vehicles/SportsCar/SM_SportsCar`. The player's keeps Epic's authored `MI_SportsCarBody` (which ships `T_SportsCar_Body_D`, `_MRA`, `_BotN` — a real metallic/roughness/AO car paint). The traffic's gets overwritten flat by this loop at `TrafficVehicleBase.cpp:325-327`:

```cpp
if (bUsingTemplateSportsCarVisual)
{
    for (int32 MaterialIndex = 0; MaterialIndex < VehicleMesh->GetNumMaterials(); ++MaterialIndex)
    {
        VehicleMesh->SetMaterial(MaterialIndex, VehicleMaterial);   // <- flat BasicShapeMaterial
    }
```

and identically at `OverlaneVehiclePawn.cpp:417-420` in `SetBodyColor` (which only fires on the AI-racer path, hence the difference).

Replace both with MIDs created **from the authored material already on the slot**:

```cpp
for (int32 Index = 0; Index < VehicleMesh->GetNumMaterials(); ++Index)
{
    UMaterialInterface* Authored = VehicleMesh->GetMaterial(Index);
    if (!Authored) continue;
    UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Authored);
    if (!MID) { MID = UMaterialInstanceDynamic::Create(Authored, this); VehicleMesh->SetMaterial(Index, MID); }
    MID->SetVectorParameterValue(TEXT("TeamTint"), TrafficColor);
}
```

Then duplicate `M_SportsCarBase`, add a `TeamTint` vector parameter multiplied into base colour (default white = no change), and re-parent `MI_SportsCarBody` to it. 10 minutes in the material editor. Your `Offroad` variant at `TrafficVehicleBase.cpp:333` already does the right thing and keeps its authored paint — use it as the reference.

**Download:** nothing, assets are already at `E:\Overlane\Content\Vehicles\SportsCar\`. **Licence:** Epic template content under the UE EULA, already in your project, safe in a paid Steam game.

**What the player notices:** 21 of the objects on screen get metallic paint with panel-gap normals, proper roughness on tyres and trim, and — combined with steps 1 and 2 — something for the sky and sun to actually reflect in. Look at any existing screenshot and ask whether your own car looks better than the traffic. It does. Geometry is held constant; the difference is 100% shading.

---

### 4. One real post-process volume, and disarm the existing component — **2.5 h**

**Sequencing gotcha first.** `HighwayEnvironmentDirector.cpp:50-57` creates `VisualPostProcess` with `bUnbound = true` and `Priority = 1000.0f`, overriding `AutoExposureBias = -1.25` and `BloomIntensity = 0.18`. That outranks any volume you place. **Drop its Priority to `-1`, or delete the two overrides**, before doing anything else here — otherwise you will lose an afternoon.

Add an unbound `PostProcessVolume` to `L_VehicleHandlingTest`:

| Group | Setting | Value | (engine default) |
|---|---|---|---|
| Exposure | Metering Mode | **Manual** | Histogram |
| Exposure | Exposure Compensation | **0.0** (the `-1.25` was compensating for legacy units; `ExtendDefaultLuminanceRange=True` from step 1 changes what it means) | |
| Film | Slope | **0.78** | 0.88 |
| Film | **Toe** | **0.28** | 0.55 |
| Film | Shoulder | **0.35** | 0.26 |
| Film | White Clip | 0.04 | 0.0 |
| Grading | Global Saturation | 0.92 | 1.0 |
| Grading | Global Contrast | 1.05 | 1.0 |
| Grading | Shadows Saturation | 0.85 | 1.0 |
| Grading | Shadows Gain | **(0.95, 0.98, 1.06)** cool | 1,1,1 |
| Grading | Highlights Gain | **(1.03, 1.01, 0.97)** warm | 1,1,1 |
| Grading | Highlights Min | 0.6 | 1.0 |
| Grading | Expand Gamut | **0.3** | 1.0 |
| Grading | Blue Correction | 0.6 | 0.6 |
| WB | Temp / Tint | 6200 / 0 | 6500 / 0 |
| Bloom | Method / Intensity | Standard / **0.45** | 0.675 |
| Local Exposure | Highlight & Shadow Contrast Scale | **0.8 / 0.8** | 1.0 |
| Lens | Chromatic Aberration | **0.6**, Start Offset **0.55** | 0.0 |
| Lens | Vignette | 0.4 (keep) | 0.4 |
| SSR | Quality / Intensity / Max Roughness | **75 / 100 / 0.65** | 50 / 100 / 0.6 |

**Download:** nothing. **Licence:** native ACES-family tonemapper parameters, no LUTs, no third-party colour science. Zero exposure.

**What the player notices:** the single biggest lever here is **Film Toe 0.55 → 0.28**. The default toe crushes asphalt — a large dark low-contrast mass — into mud. Real tarmac footage has lifted, slightly blue-grey shadows, never pure black. The cool-shadow/warm-highlight split is what the eye reads as "outdoors". `ExpandGamut` 1.0 → 0.3 stops saturated car paint going neon, which is itself a strong toy tell. **Manual exposure kills the brightness pumping** as you pass bright buildings and the interchange — that camcorder pump is one of the hardest amateur tells there is, and a racer with one fixed lighting condition has no reason to pay for auto-exposure. Chromatic Aberration Start Offset at 0.55 keeps the centre of frame — where you read traffic — perfectly clean.

---

### 5. Exponential height fog + movable SkyLight — **1 h**

On `ExponentialHeightFog_0`: Fog Density **0.012**, Height Falloff **0.15**, **Volumetric Fog on**, Scattering Distribution **0.6**, **Directional Inscattering on** with a warm inscattering colour. If it costs too much, set `r.VolumetricFog.GridPixelSize=16` (default 8) to roughly halve it — barely visible over a long road.

Set `SkyLight_0` Mobility = **Movable**, Real Time Capture = **true**.

**Download:** nothing (optional CC0 HDRI in step 8). **Licence:** engine, zero exposure.

**What the player notices:** aerial perspective. Right now the far end of a 6 km straight has the same contrast and saturation as the near end, which makes 6 km read as 200 m. Fog restores the depth cue and gives the road a horizon to vanish into instead of ending abruptly. The movable SkyLight supplies blue skylight fill in shadow while the sun stays warm — that warm/cool split is most of "this is outdoors".

---

### 6. World-aligned road master material + CC0 asphalt — **7 h (largest absolute gain on the list)**

**Why nothing else will do.** `RoadSurface` is `/Engine/BasicShapes/Cube` at `SetRelativeScale3D(FVector(VisualRouteLength / CubeSize, RoadWidth / CubeSize, 0.07f))` — **scale (6300, 20, 0.07)**. The engine cube has one 0-1 UV set per face, so any UV-sampled texture stretches 6.3 km by 20 m: an anisotropy ratio around **315:1**. An 8K Megascans asphalt on that mesh looks *worse* than the current flat black. The road has never had a texture because the geometry physically cannot carry one. **Do not download a single texture before this material exists.**

**What to do.** Author `M_OverlaneSurface` sampling by world position, not UV:

- UVs: `AbsoluteWorldPosition.XY / TilingSize` (or the built-in `WorldAlignedTexture` function). Start `TilingSize = 400` (4 m) for the road.
- Parameters: `BaseColorTex`, `ARMTex`, `NormalTex`, `TilingSize`, `RoughnessScale`, `Tint`, `MacroVariationAmount`, `WetnessAmount`, `WearLaneStrength`.
- **Macro variation (non-negotiable):** a second sample of the same texture at a different scale and rotation, blended by a very-low-frequency noise at roughly **one cycle per 200 m**, also modulating roughness and base-colour brightness. 6 km at an 8 m tile is 750 repeats; at 68 m/s you traverse a tile every ~0.12 s, so any recognisable feature becomes a strobe. One extra sample + one noise lookup.
- **Wheel-polish wear lanes:** drive roughness from lateral world Y — **~0.35** in the two wheel tracks per lane, **~0.7** at the coarse centre and edges. This is generated analytically, no texture needed, and it is *the* photographic highway signature. Combined with the 15° sun from step 2, the sheen band splits into two bright polished stripes running down the road.

**Download — Poly Haven first (fastest path):** `https://polyhaven.com/a/asphalt_track` and `https://polyhaven.com/a/worn_asphalt`, plus `gravel_road` for the shoulder. Take **4K, and take `Diffuse` + `nor_dx` + `arm`**. `nor_dx` is the DirectX-convention normal UE expects (no green-channel flip) and `arm` is pre-packed AO/Rough/Metallic — three samples, zero prep. Do **not** take 8K: `asphalt_track`'s source scan is ~2000px upscaled, so 8K buys literally nothing and quadruples memory.

**Download — ambientCG for the gaps:** `Concrete045`, `Concrete046` (barrier/pier), `Metal032`-class painted/galvanised steel (guardrail), `Ground050` (puddle/wear mask). Pattern: `https://ambientcg.com/view?id=Concrete045`. ambientCG ships **loose maps only**, so budget ~2 h to channel-pack AO/Rough/Metallic into a single ORM.

**Licence:** **CC0 1.0 Universal on both** — public-domain dedication, commercial use explicit, no attribution, no revenue cap, no engine restriction. `https://docs.ambientcg.com/license/` lists no excluded assets. This is the cleanest legal position available to you, cleaner than Fab.

**What the player notices:** the road becomes a surface instead of a slab, and — this is the part that answers "we'd feel the driving better" — **a textured road is what generates optic flow**. Speed is perceived from angular velocity of detail in the periphery. Your road currently contributes literally zero flow; your only cues are lane dashes at 7.2 m and edge reflectors. High-frequency surface detail streaming underneath you is most of the sensation of speed.

**Memory budget:** ~40 MB per 4K set (BC7 albedo, BC5 normal, BC7 ORM). Road at 4K, shoulders/barriers/terrain at 2K. Four sets ≈ 160 MB — comfortable at 1080p.

---

### 7. Motion and feel pass — **3 h** *(full parameter values in section 4)*

Wheel rotation, camera lag, cosmetic body roll/pitch, asymmetric FOV response, speed-scaled camera shake. Zero assets, ~80 lines of code. This is the change players will describe as *"the handling got better"* even though you touch no handling code.

---

### 8. Sky: keep SkyAtmosphere for the dome, CC0 HDRI for lighting only — **1.5 h**

**Download:** Poly Haven `_puresky` HDRIs (`https://polyhaven.com/hdris` — the puresky set is sky-only with no baked ground, so no fake horizon fights your real one), or ambientCG `DaySkyHDRI065A` / `DayEnvironmentHDRI107`. **4K EXR, not 16K.**

**Licence:** CC0 both sites. Safe, no attribution.

**Critical usage note:** do **not** make the HDRI your visible sky. Use UE 5.8 `SkyAtmosphere` for the dome — it gives real sun position and real aerial perspective over 6 km — and use the HDRI purely as the SkyLight's source cubemap for ambient and reflections. That hybrid is the standard racing-game setup. A static HDRI as the visible sky locks time of day and its reflections won't track a moving sun.

**Volumetric Clouds: skip them.** They are the most expensive item on this entire list and the least load-bearing for a game where the player is looking at the road.

---

### 9. Lane markings: from instanced cubes to material layer or decals — **3 h**

`AddLaneMarkings()` (`HighwayEnvironmentDirector.cpp:297-317`) adds `LaneDashes` instances as `/Engine/BasicShapes/Cube` at scale `(3.6, 0.11, 0.025)` — 3.6 m × 11 cm × **2.5 cm raised boxes** — with `CreateColorMaterial(FLinearColor(0.92f, 0.92f, 0.86f))`. Painted markings are not raised boxes. They are thin, worn, chipped, tyre-polished, and partially transparent to the asphalt beneath.

Bake them into the road material as a masked layer driven by world position (best — mipmapping and `r.MaxAnisotropy=8` then do the anti-shimmer work properly, which no AA method can do for sub-pixel *geometry*), or use deferred decals capped at ~50 in a rolling window around the player.

**Download:** ambientCG `RoadLines020C` (faded/worn) and `RoadLines029C` (cracked/damaged) for the alpha. `https://ambientcg.com/view?id=RoadLines020C`. **Licence:** CC0.

**Do not** use ambientCG's `Road0XX` tiles for the road surface itself — they have lane lines **baked in**, which will produce visible ladders at your tiling interval. Clean asphalt for the surface, markings as a separate controllable layer.

---

### 10. Segmented guardrail and denser roadside — **6 h**

`LeftGuardRail` / `RightGuardRail` are cubes at scale `(6300, 0.12, 0.09)` — an unbroken 6.3 km ribbon **12 cm deep and 9 cm tall**. Real W-beam is ~31 cm tall on posts every ~2 m, and no texture can fix a wrong silhouette. Model a ~20-triangle W-beam profile plus a post in **UE Modeling Mode** (free, already installed), replace the two `UStaticMeshComponent`s with `GuardRailBeams` + `GuardRailPosts` ISMs, and instance them along the route.

While you are in `AddRoadsideFurniture()`: `LampSpacing = 8000.0f` (80 m) and the loop starts at `X = 100000.0f` — **the first kilometre has no lamps at all**. Change to `LampSpacing = 3500.0f` and start at `X = 2000.0f`.

**Download:** nothing required. **Licence:** your own geometry, zero exposure.

**What the player notices:** the repeating post rhythm is a major speed cue — the eye counts it. Highest silhouette-per-polygon win available on a highway.

---

### 11. Traffic silhouette variety — **1 day**

**Download:** **City Sample Vehicles** (standalone pack, *not* the 100 GB full project) — `https://www.fab.com/listings/2909157b-ddfa-4cef-a925-69dc2467021f`. 13 realistic vehicles: sedans, hatchback, pickup, taxi, bus, delivery van, semi and garbage trucks. Plus **Vehicle Variety Pack** — `https://www.fab.com/listings/dc1ada50-2523-44b1-b0e2-a72d14076fb4` (5 more; your current SportsCar is from this family). 18 distinct bodies for 21 traffic slots.

**Licence:** *UE-Only Content* / Fab Standard, free at both Personal and Professional tiers. It sounds alarming but explicitly permits commercial use — it only forbids taking the assets to Unity/Godot. Overlane is a UE 5.8 product, so this is satisfied. **Verify the licence text at download and archive it.**

**Why the effort is lower than it looks:** Epic ships **body-without-wheels meshes with wheels as separate static meshes**, precisely because Nanite cannot handle rotating wheels, and the wheel origins are hub-centred with the correct spin axis because Chaos Vehicles mathematically requires it. That is exactly the topology your kinematic four-separate-wheel rig at `TrafficVehicleBase.cpp:518-570` already expects. Random CC0 car packs almost never have this and cost hours of per-wheel re-origining.

**Risk:** the bodies are Nanite. Test with Nanite on, but be ready to disable it and use the fallback mesh — Nanite has per-instance overhead that suits static scenery better than 60 small moving objects.

**Do not use** Kenney or Quaternius car kits here. Both are flawless CC0 and both are *explicitly toy-styled* (Kenney's pack is literally called "Toy Car Kit"). They would make the exact complaint measurably worse. Mixed fidelity reads as broken, not as a style.

---

### 12. Wet-road variant — **3 h, highest ceiling of anything free**

Add `WetnessAmount` to `M_OverlaneSurface`: lerp roughness **0.7 → 0.15** in the wheel-track bands, darken base colour **~35-40%**, mask puddles with a CC0 noise/ground texture. SSR from step 1 then does all the work.

**Download:** nothing new (`Ground050` from step 6). **Licence:** your own material.

**What the player notices:** a dry road is nearly Lambertian and returns almost no information about speed or light direction. A wet road turns every tail light, headlight and patch of sky into a moving streak on the surface. This is the look every good-looking highway racer screenshot uses, and it makes the *existing* free car meshes look far more expensive because they suddenly have something to reflect in.

---

### 13. TSR tuning + velocity audit — **2 h**

`r.AntiAliasingMethod` already defaults to 4 (TSR) — correct, keep it. But `BaseScalability.ini` leaves two critical passes **off** at quality tiers 0 and 1:

```ini
r.TSR.ShadingRejection.Flickering=1     ; anti-moire — converging lane dashes are a textbook moire generator
r.TSR.ReprojectionField=1               ; Jacobian reprojection — holds thin lamp posts sharp in motion
r.TSR.Velocity.WeightClampingSampleCount=2.0   ; (def 4.0) — kills blur trails on moving traffic
r.TSR.History.ScreenPercentage=200      ; High preset only
```

Force the first two at your **Medium** preset. Also expose `r.ScreenPercentage` as Quality/Balanced/Performance = 100/75/66 — at 1080p, 75% with TSR often looks better than 100% with FXAA and buys back 2-4 ms.

**Velocity audit (do not skip):** TSR quality depends entirely on correct motion vectors. Component `Mobility` defaults to `Movable` in C++ (`SceneComponent.cpp:124`), so the vehicles are fine — but `TrafficVehicleBase.cpp:658, 691, 709` move traffic with `ETeleportType::TeleportPhysics`, which can suppress velocity-buffer writes. Verify with the Velocity buffer / `r.VisualizeMotionVectors` view. Any recycle-teleport on the 6 km loop needs the actor hidden for one frame.

---

### 14. Lumen at Medium, not High — **1 h (this funds the rest of the budget)**

Ship `sg.GlobalIlluminationQuality=1` as your default. Verified in `E:\EPIC_GAMES-games\UE_5.8\Engine\Config\BaseScalability.ini`: `@1` sets `r.Lumen.FinalGatherMethod=0` (irradiance-field gather + world-space radiance cache — "Lumen Lite"), `@2` sets `=1` (full screen probe gather). Roughly **2× cheaper**, and Epic targets 60 fps on Switch 2 and medium PC with the `@1` path.

Your scene is close to the worst value-for-money case for full Lumen: one dominant light, open sky, almost no meaningful indirect bounce, and 20-60 kinematic cars constantly invalidating the surface cache. The quality difference at 245 km/h is close to invisible.

**Frame budget at 1080p / 60 fps (16.6 ms), class-typical for RTX 3060 / RX 6600 — verify with `stat GPU` at 60 cars, these are not measurements from your project:**

| System | Budget |
|---|---|
| Lumen GI @ Medium | 2-3 ms |
| Virtual Shadow Maps | 2-4 ms |
| SSR quality 3, full-res scene colour | 1.5-2 ms |
| Volumetric fog | 0.5-1 ms |
| Contact shadows | 0.1-0.3 ms |
| TSR | 1-2 ms |
| **Remaining for base pass + everything else** | **4-7 ms** |

Lumen High + Lumen Reflections would be 6-9 ms on its own. That is the trap.

**One extra free win:** `r.SSR.HalfResSceneColor=1` is pinned at tiers 0, 1 **and** 2 and only cleared at tier 3. Half-res scene colour is exactly where reflected tail lights become unreadable blobs. Clear it on your High preset — ~0.3-0.5 ms.

---

## 3. WHAT CHANGES IN CODE

The generator does not need rewriting. It needs **four surgical changes** so downloaded assets can be wired in without touching placement logic.

### 3.1 Replace the single `BaseMaterial` with a surface-material set

Currently `HighwayEnvironmentDirector.h:170` holds one `TObjectPtr<UMaterialInterface> BaseMaterial`, loaded at `.cpp:129` from `BasicShapeMaterial`, and `CreateColorMaterial()` (`.cpp:204-214`) creates 18+ MIDs that differ only in a `Color` vector.

Add alongside it:

```cpp
// HighwayEnvironmentDirector.h — EditDefaultsOnly so a BP subclass can swap assets with no recompile
UPROPERTY(EditDefaultsOnly, Category = "Highway|Surfaces")
TObjectPtr<UMaterialInterface> RoadSurfaceMaterial;      // M_OverlaneSurface + asphalt_track
UPROPERTY(EditDefaultsOnly, Category = "Highway|Surfaces")
TObjectPtr<UMaterialInterface> ShoulderSurfaceMaterial;  // gravel_road
UPROPERTY(EditDefaultsOnly, Category = "Highway|Surfaces")
TObjectPtr<UMaterialInterface> TerrainSurfaceMaterial;
UPROPERTY(EditDefaultsOnly, Category = "Highway|Surfaces")
TObjectPtr<UMaterialInterface> MetalSurfaceMaterial;     // guardrail / poles / signs
UPROPERTY(EditDefaultsOnly, Category = "Highway|Surfaces")
TObjectPtr<UMaterialInterface> ConcreteSurfaceMaterial;  // interchange decks / piers
UPROPERTY(EditDefaultsOnly, Category = "Highway|Surfaces")
TObjectPtr<UMaterialInterface> LaneMarkingMaterial;      // worn paint, alpha-masked
UPROPERTY(EditDefaultsOnly, Category = "Highway|Surfaces")
TObjectPtr<UMaterialInterface> EmissiveMaterial;         // lamp heads, sign retroreflectors

struct FSurfaceParams { float TilingSize = 400.f; float RoughnessScale = 1.f; float Wetness = 0.f; float MacroVariation = 1.f; FLinearColor Tint = FLinearColor::White; };
UMaterialInstanceDynamic* CreateSurfaceMaterial(UMaterialInterface* Parent, const FSurfaceParams& Params) const;
```

`CreateColorMaterial` stays for the props that genuinely only need a colour. In `BuildVisualRoute()` (`.cpp:227-231`), the four `CreateColorMaterial` calls for `TerrainMaterial` / `AsphaltMaterial` / `ShoulderMaterial` / `RailMaterial` become `CreateSurfaceMaterial` calls with per-surface `TilingSize` — road 400, shoulder 250, terrain 1200, guardrail 120.

**No geometry change is required for the road.** Because `M_OverlaneSurface` samples by `AbsoluteWorldPosition`, the 315:1 UV anisotropy on the (6300, 20, 0.07) cube stops mattering entirely.

### 3.2 Convert the hard-coded mesh finders to editable defaults

`.cpp:121-129` hard-codes eight `ConstructorHelpers::FObjectFinder` paths. Every one is a recompile to change. Convert each to:

```cpp
UPROPERTY(EditDefaultsOnly, Category = "Highway|Meshes")
TObjectPtr<UStaticMesh> GuardRailBeamMesh;   // W-beam profile
UPROPERTY(EditDefaultsOnly, Category = "Highway|Meshes")
TObjectPtr<UStaticMesh> GuardRailPostMesh;
UPROPERTY(EditDefaultsOnly, Category = "Highway|Meshes")
TObjectPtr<UStaticMesh> LampPostMesh;        // single combined pole+arm+head
UPROPERTY(EditDefaultsOnly, Category = "Highway|Meshes")
TObjectPtr<UStaticMesh> SignGantryMesh;
UPROPERTY(EditDefaultsOnly, Category = "Highway|Meshes")
TObjectPtr<UStaticMesh> BuildingMeshes[4];   // stop reusing one mesh 12 times
```

Keep the `FObjectFinder`s as constructor fallbacks so nothing breaks. Then create `BP_HighwayEnvironmentDirector` as a Blueprint subclass and spawn *that*. **Downloading a City Sample guardrail becomes: set a pin in a Blueprint. No C++ change, no recompile, no placement-logic rewrite.** This is the single most important architectural change for asset-swapping.

### 3.3 Guardrail: two mesh components → two ISMs

Replace `LeftGuardRail` / `RightGuardRail` (`UStaticMeshComponent`, `.h:74-79`, placed at `.cpp:252-257`) with:

```cpp
UPROPERTY(VisibleAnywhere, Category = "Highway") TObjectPtr<UInstancedStaticMeshComponent> GuardRailBeams;
UPROPERTY(VisibleAnywhere, Category = "Highway") TObjectPtr<UInstancedStaticMeshComponent> GuardRailPosts;
```

and a loop mirroring the existing pattern in `AddRoadsideFurniture()`: post every 200 cm, beam segment every 400 cm, both sides at Y = ±1180. Same structure as the lamp loop you already have.

### 3.4 Four small but load-bearing fixes

**(a) Post-process priority.** `.cpp:52-53`: set `VisualPostProcess->Priority = -1.0f` (or delete the component) so your new `PostProcessVolume` wins. Do this *before* authoring the grade.

**(b) Flip environment mobility to Static after building.** All components default to `Movable` (`SceneComponent.cpp:124`), which makes VSM pay invalidation cost on ~6 km of scenery that never moves. At the end of `BuildVisualRoute()`, after every `AddInstance` call has completed:

```cpp
TInlineComponentArray<USceneComponent*> Components;
GetComponents(Components);
for (USceneComponent* C : Components)
{
    if (C != SceneRoot && C != VisualPostProcess) { C->SetMobility(EComponentMobility::Static); }
}
```

Verify no "component is static but was moved" warnings appear in the log; if any do, exclude that component.

**(c) Lamp heads must emit.** `.cpp:263` gives `LampHeads` a pale-blue *diffuse* `CreateColorMaterial` — ~150 spheres that emit nothing. Give them `EmissiveMaterial` with `EmissiveColor` around `(1.0, 0.85, 0.6) × 25`. Do **not** add 150 PointLights; if you go for the night variant, enable **MegaLights** (production-ready in 5.8) and spawn a rolling window of ~12 lights around the player.

**(d) Stop uniform-scaling buildings.** `CalculateUniformScaleForHeight` (`.cpp:16-25`) is called from `.cpp:405, 472, 492, 597` and scales `SM_Building` uniformly from 720 cm to 1550 cm — which scales the *windows and doors* too. Humans read architectural scale off floor height. Either use 3-4 distinct meshes at their authored scale, or scale only Z and accept the stretch, but not both dimensions uniformly.

### 3.5 Traffic vehicle table (for step 11)

`TrafficVehicleBase.cpp:98-138` hard-codes `SM_SportsCar` / `SM_SportsCar_Wheel` paths with a two-branch `bUsingTemplateSportsCarVisual` / `bUsingTemplateOffroadVisual` split. Replace with a `UDataTable` row struct:

```cpp
USTRUCT() struct FTrafficBodyDefinition
{
    TSoftObjectPtr<UStaticMesh> BodyMesh, GlassMesh, WheelMesh;
    FVector CollisionExtent;
    float FrontWheelX, RearWheelX, WheelY, WheelZ, WheelRadius;
    bool bKeepAuthoredMaterials = true;   // default true — see step 3
};
```

Adding a City Sample bus is then a data-table row, not a code branch.

---

## 4. THE DRIVING-FEEL ITEMS

All free, all pure code, ~80 lines total. **Keep every one of these strictly client-cosmetic on `VehicleMesh` — never on `VehicleCollision` (the root) and never in `FOverlaneVehicleSimState`** (`OverlaneNetTypes.h:82-108`, which carries Location + Yaw only). Putting body roll on the root would change the collision box orientation and break your tested near-miss and traffic-collision behaviour.

### 4.1 Wheel rotation — 10 lines, biggest single "cartoon" fix

Wheels are set once in the constructors (`OverlaneVehiclePawn.cpp:154-164`, `TrafficVehicleBase.cpp:518-570`) and **never touched again**. At 245 km/h a ~34 cm wheel should spin ~32 rev/s. A viewer clocks frozen wheels on a moving car in under a second.

```cpp
// AOverlaneVehiclePawn::Tick — ArcadeHandling->GetForwardSpeed() is already cm/s
const float R = FrontLeftWheel->GetStaticMesh()->GetBounds().BoxExtent.Z * FrontLeftWheel->GetRelativeScale3D().Z;
WheelSpinDeg = FMath::Fmod(WheelSpinDeg + (ArcadeHandling->GetForwardSpeed() * DeltaSeconds) / (2.f * PI * FMath::Max(R, 1.f)) * 360.f, 360.f);
const float Steer = ArcadeHandling->GetSteeringInput() * 14.f;   // add a getter, see below
FrontLeftWheel ->SetRelativeRotation(FRotator(WheelSpinDeg, Steer, 0.f));
FrontRightWheel->SetRelativeRotation(FRotator(WheelSpinDeg, Steer, 0.f));
RearLeftWheel  ->SetRelativeRotation(FRotator(WheelSpinDeg, 0.f,   0.f));
RearRightWheel ->SetRelativeRotation(FRotator(WheelSpinDeg, 0.f,   0.f));
```

`ArcadeHandlingComponent` has `SteeringInput` as a private field (`.h:143`) — add `float GetSteeringInput() const { return SteeringInput; }` next to the existing `GetForwardSpeed()` at `.h:34`. Note the template wheels use `FRotator::ZeroRotator` (`OverlaneVehiclePawn.cpp:163`) while the placeholder cylinders use `FRotator(0,0,90)` (`TrafficVehicleBase.cpp:78`) — check the spin axis per path and mirror the right-hand side if the mesh is not symmetric.

### 4.2 Camera lag — 3 lines, largest feel-per-character-typed on the list

`CameraBoom` (`OverlaneVehiclePawn.cpp:220-231`) never sets lag, so the camera is rigidly welded to the collision box. Nothing trails, nothing overshoots, nothing settles — the world appears to snap sideways as one block on a lane change, the exact perceptual signature of moving a diorama.

```cpp
CameraBoom->bEnableCameraLag         = true;
CameraBoom->CameraLagSpeed           = 10.0f;
CameraBoom->bEnableCameraRotationLag = true;
CameraBoom->CameraRotationLagSpeed   = 8.0f;
CameraBoom->CameraLagMaxDistance     = 150.0f;
```

### 4.3 Rework the speed→camera curve

Current code (`OverlaneVehiclePawn.cpp:285-291`) is linear in speed with `CameraResponseSpeed = 4.0f` on both arm length and FOV — a ~250 ms time constant, so the boost kick arrives mushy, and 200 vs 245 km/h is nearly indistinguishable. Worse, the arm pulls **750 → 1050** at speed, which *shrinks the car and reduces apparent road motion* — the classic toy-car-on-a-track look.

| Field | Current | New | Why |
|---|---|---|---|
| `MaxCameraDistance` | 1050 | **820** | stop shrinking the car; let FOV do the work |
| `MaxCameraFov` | 104 | **100** | 104° has heavy edge distortion |
| boost FOV kick | +7 | **+8** (→108, boost only) | reserve the extreme for boost |
| `CameraResponseSpeed` | 4.0 (both directions) | **12.0 out / 3.0 back** | punch out fast, ease back slow — the asymmetry *is* the feel |
| FOV curve | `Lerp(base, max, SpeedRatio)` | `Lerp(base, max, SpeedRatio)` **+ `AccelTerm`** | sense of speed comes from *change*, not absolute value |

```cpp
const float Accel = (ArcadeHandling->GetForwardSpeed() - PrevForwardSpeed) / FMath::Max(DeltaSeconds, KINDA_SMALL_NUMBER);
const float AccelTerm = FMath::Clamp(Accel / 2800.f, -1.f, 1.f) * 4.0f;   // 2800 = Acceleration in ArcadeHandlingComponent.h:80
const float TargetFov = FMath::Lerp(BaseCameraFov + CameraFovOffset, MaxCameraFov + CameraFovOffset + TurboKick, SpeedRatio) + AccelTerm;
const float Rate = (TargetFov > ChaseCamera->FieldOfView) ? 12.0f : 3.0f;
ChaseCamera->SetFieldOfView(FMath::FInterpTo(ChaseCamera->FieldOfView, TargetFov, DeltaSeconds, Rate));
PrevForwardSpeed = ArcadeHandling->GetForwardSpeed();
```

Throttle now visibly punches the FOV out; lifting pulls it back.

### 4.4 Cosmetic body attitude — ~20 lines

The car structurally cannot dive, squat or lean, so **every control input produces zero visible body response**. This is the largest control-feedback gap in the build. On `VehicleMesh` only:

```cpp
const float TargetRoll  = -ArcadeHandling->GetSteeringInput() * SpeedRatio * 3.5f;      // degrees
const float TargetPitch = FMath::Clamp(-Accel / 2800.f, -1.f, 1.f) * 2.2f;              // dive/squat
CosmeticRoll  = FMath::FInterpTo(CosmeticRoll,  TargetRoll,  DeltaSeconds, 6.0f);
CosmeticPitch = FMath::FInterpTo(CosmeticPitch, TargetPitch, DeltaSeconds, 8.0f);
VehicleMesh->SetRelativeRotation(FRotator(CosmeticPitch, 0.f, CosmeticRoll));
CabinMesh  ->SetRelativeRotation(FRotator(CosmeticPitch, 0.f, CosmeticRoll));
```

### 4.5 Speed-scaled camera shake

Zero `CameraShake` classes exist in the project. A perfectly still camera at 68 m/s tells the brain the surface is frictionless and the car is weightless. Create a `ULegacyCameraShake`:

- **Location** Y/Z: amplitude **0.6-1.2 cm**, frequency **8-14 Hz**
- **Rotation** Pitch/Yaw: amplitude **0.08-0.15°**, frequency **5-9 Hz**
- **Roll: 0** — roll shake makes traffic hard to track
- Blend weight: **`SpeedRatio²`**, so it is invisible at cruising speed and present at 245

### 4.6 Radial vignette blur — best perceived-speed gain per readability risk

Forward motion produces almost no camera motion blur near the vanishing point, which is why 245 km/h currently feels like 80. A post-process material in the PPV's **Blendables** array (Blendable Location = *After Tonemapping*), blurring only the outer **~25%** of screen using the same radius mask as the vignette so they reinforce rather than fight:

```
Strength = saturate((SpeedRatio - 0.55) / 0.45) * 0.6 * UserSlider
```

~0.2-0.4 ms. **Expose it as a settings slider (0-100%)** — radial blur induces motion sickness in some players, and you already have `OverlaneSettingsSaveGame` and the `CameraFovOffset` accessibility hook, so the pattern exists.

**Speed lines: skip them.** They are the most arcade-toy effect available and you are trying to escape exactly that read.

### 4.7 Motion blur config — 15 min, removes a bug class

| Setting | Default | Set to |
|---|---|---|
| `MotionBlurAmount` | 0.5 | **0.2** |
| `MotionBlurMax` | 5.0 (% of screen width — enormous) | **2.0** |
| `MotionBlurTargetFPS` | 30 | **60** |
| `MotionBlurPerObjectSize` | 0.0 (**no size culling at all**) | **2.0** |

Per-object blur is a trap here, not a tuning question: traffic is recycled by teleport on the 6 km loop, and a teleport is a one-frame velocity spike that will smear that car across the whole screen on **every recycle**. Object blur also scales with *relative* velocity, so the fastest-approaching cars — the ones you most need to read to judge a gap — get the heaviest blur. Camera-rotation blur is fine and worth keeping. Disabling motion blur does **not** disable velocity output, so TSR is unaffected either way.

### 4.8 Optic-flow furniture density

At 68 m/s: lane dashes at 720 cm pass every **0.106 s** (~6.4 frames at 60 fps — that reads as strobing, not speed); lamp posts at 8000 cm pass every **1.18 s**; signs at 30000 cm every **4.4 s**. Your roadside is essentially static at racing speed. Set `LampSpacing = 3500.0f` starting at `X = 2000.0f`, `SignSpacing = 18000.0f`, and add a shoulder rumble strip as an ISM at 150 cm spacing. Combined with a textured road (step 6), this changes perceived speed more than any car model ever could.

---

## 5. LICENCE LEDGER

Record each of these as a row in `E:\Overlane\ASSET_REQUIREMENTS.md` with the licence text **as it read on the day you downloaded it**, plus a PDF/screenshot of the licence page saved alongside the source files. Add a `DECISIONS.md` entry (in your D-001…D-010 style) recording the choice to build environment art on CC0 + free Epic first-party content and to explicitly exclude Textures.com, the Quixel Mixer offline bundle, and Sketchfab/CGTrader vehicles on licence grounds. That record is what protects you if the question is ever raised at release.

| Asset / Source | Licence | Ships in a paid Steam game? |
|---|---|---|
| UE 5.8 engine features (Lumen, VSM, SSR, TSR, SkyAtmosphere, fog, post-process, MegaLights, Modeling Mode) | Unreal Engine EULA (5% royalty above $1M lifetime, unrelated to assets) | **Yes** |
| Your own C++, materials, W-beam mesh, camera shake, post-process materials | First-party | **Yes** |
| `/Game/Vehicles/SportsCar`, `/Game/Vehicles/OffroadCar`, `/Game/ConceptCar`, `/Game/Building`, `/Game/ArchVis` (already in `Content/`) | UE EULA, Epic template content | **Yes** |
| **ambientCG** — Asphalt031/033, Road007/012A, RoadLines019-033 series, Concrete045-047A, Metal, Ground050, DaySkyHDRI series (`ambientcg.com`, licence at `docs.ambientcg.com/license/`) | **CC0 1.0 Universal** | **Yes** — no attribution, no revenue cap, no engine restriction, no NoAI clause |
| **Poly Haven** — `asphalt_track`, `worn_asphalt`, `clean_asphalt`, `road_damaged`, `gravel_road`, `*_puresky` HDRIs (`polyhaven.com`) | **CC0** | **Yes** — only prohibitions are claiming authorship and re-licensing |
| **cgbookcase.com**, **3dtextures.me** (gap-fillers: kerb stone, guardrail metal, grime decals) | **CC0 1.0** | **Yes** |
| **City Sample Vehicles** (Fab, standalone pack) | Fab Standard / *UE-Only Content*, free at both tiers | **Yes** — permits commercial use, forbids only non-UE engines and standalone resale. **Verify the EULA text at download and archive it.** |
| **Vehicle Variety Pack** (Fab) | Epic "Free For Life", Fab Standard | **Yes** — same conditions |
| **Automotive Substrate Materials** (Fab, 280+, UE 5.7.3+) | Epic first-party, Fab Standard | **Yes** — **but player car only.** Epic's own docs state Substrate regresses runtime performance vs. legacy GBuffer even for simple materials; layered car paint on 60 vehicles is self-sabotage. Also **verify it opens cleanly in 5.8** and that enabling Substrate project-wide does not regress your existing materials. |
| **City Sample (full project)** — freeway deck, road decals, guardrails, signage | *UE-Only Content* | **Probably yes — VERIFY BEFORE SHIPPING.** The UE-only condition is satisfied by Overlane, but I could find no explicit Epic staff confirmation on paid commercial redistribution; the affirmative forum answers are community interpretation. Widely relied on in shipped titles. Read the EULA at download and record the exact wording. Also: 93-100 GB, Nanite-heavy, built for a demo that did not target 60 fps at 1080p — cherry-pick, never migrate wholesale. |
| **Fab monthly/limited-time free drops** | Fab Standard — often **third-party**, not Epic first-party | **Yes — VERIFY PER LISTING.** The Personal (<$100k) / Professional (>$100k) tier split is a threshold on *your* revenue, not a cap on your game's price, and both tiers grant the same scope. **Unresolved:** what happens if a third-party asset is claimed under Personal and Overlane later crosses $100k — no official Epic answer exists, only a community reading that eligibility fixes at transaction time. Keep dated screenshots of every claim. Check for "Personal Use Only" / "Engine-restricted" flags, which some free listings carry and which would **not** be shippable. |
| **Megascans on Fab** (post-2025) | **NOT FREE** — ~$0.99/surface, $4.99/kit, $24.99/pack since Jan 2025. Free only if personally claimed **and downloaded** before 31 Dec 2024. | Not on the critical path. If you have pre-2025 downloads on disk, they are yours. Library-only entitlements that were never downloaded are genuinely ambiguous — do not plan around them. |
| **ShareTextures** | States CC0, but runs a patron tier and aggregates from other sources | **VERIFY PER ASSET.** Mixed free/paywall catalogues are where per-asset licence drift creeps in. Do not treat a site-wide CC0 banner as sufficient evidence for a paid release. |
| **Kenney.nl**, **Quaternius** | CC0 — licence is flawless | Legally yes, but **do not use**. Explicitly toy-styled (Kenney's is literally "Toy Car Kit"). Mixing flat-shaded low-poly with photoscanned surfaces reads as broken, not as a style. |
| ❌ **Quixel Mixer offline bundle** (the 800+ bundled assets) | Quixel Megascans Free Assets EULA — **explicit non-commercial restriction**, and Quixel reserved the right to **watermark free assets specifically to detect commercial use** and terminate access | **NO.** Shipping these on Steam for money is exactly the case that EULA was written to catch. This is the most dangerous advice currently circulating. |
| ❌ **Textures.com** | Licence written around incorporation rather than distribution; community disagreement over whether packaged game content violates the redistribution clause; licence page returns 403 to verification attempts | **NO.** "I cannot verify this licence" is by itself sufficient reason to decline on a paid title. Everything you'd want is on ambientCG or Poly Haven under CC0. |
| ❌ **Sketchfab / CGTrader "free CC0" cars** | Nominally CC0 | **NO.** Meshes ripped from Forza, GTA and Assetto Corsa are routinely re-uploaded mislabelled as CC0. **A mislabelled licence gives you zero protection** — if the uploader had no right to grant it, the grant is void and *you* are the one shipping it on a paid storefront. The photoreal free cars are precisely the ones most likely to be stolen, because nobody models one for free. A DMCA on a paid Steam title is unrecoverable for a solo dev. |

**Trade dress note:** US courts have repeatedly protected expressive use of real vehicles in games under *Rogers v. Grimaldi* — AM General lost against Activision over Humvees. But a solo dev cannot afford to *win* a lawsuit, let alone lose one. Epic's vehicles are deliberately generic and lawyer-vetted precisely so licensees can ship them. That is a real and underrated argument for using them over any lookalike.

---

## 6. WHAT STILL LOOKS FAKE AFTERWARDS

| What remains fake | Why free cannot fix it | Cheapest paid fix |
|---|---|---|
| **Texture repetition.** 6 km at an 8 m tile is 750 repeats; at 68 m/s you cross one every 0.12 s. Macro variation hides it, it does not remove it. A trained eye will see the ladder. | Uniqueness costs authoring time. Every CC0 asphalt is also in a hundred other games. | Megascans surfaces on Fab, **~$0.99 each** — 3-4 surfaces is the cheapest quality purchase in this entire domain. Or a texture artist for one day of unique decal work, ~$200-400. |
| **Buildings read at the wrong scale.** Even with 4 meshes instead of 1, you have no architectural variety and no interior parallax on windows. | You cannot author a believable modular city kit for free in reasonable time. | **City Sample Buildings is free** (24 modular kits, 44 buildings) but is a huge download and needs 8K textures downsized. A focused Fab modular architecture kit is **$20-40** and far less work. |
| **Distant hills are literally cones** (`.cpp:149, 266, 709`) and the skyline is 8 scaled cubes (`.cpp:570-584`). | Believable terrain silhouettes need sculpted geometry or a Landscape, both of which are real work, and a Landscape conflicts with your runtime-procedural architecture. | A CC0/low-cost mountain-mesh pack, **$0-20**, or 4-6 hours in UE Modeling Mode sculpting three ridge meshes (free, but time). |
| **Flat road: zero camber, zero crown, zero curvature, zero elevation over 6 km.** No real motorway is. | Free to code, but it is a week's work to build a spline road director, **and** your playable collision corridor is separate flat `StaticMeshActor` slabs in the level — change the visuals without the collision and cars visibly float or sink. | A spline road tool on Fab, **$30-100**, still requires the collision-route rework. Honest alternative: keep the road flat under the racing lanes and add camber only to the shoulders. Free, and 90% of the read. |
| **Brand-accurate real cars.** Epic's vehicles are deliberately generic — recognisably "a sedan", never "a 2019 Golf". | Manufacturer licensing is not available to solo devs at any indie price, and lookalike models carry trade-dress risk you cannot afford to litigate. | **No amount of money fixes this for a solo dev.** Accept generic. It is also the *safe* answer. |
| **No cockpit / interior view.** | No free interior exists at a quality that would survive a first-person camera on these meshes. | Vehicle Variety Pack vehicles *do* ship interiors — free — but they are template-quality. A proper interior model is **$50-200** per car, and it is not worth it for a chase-cam game. Just don't ship an interior camera. |
| **Signage is generic blue/green boards with no legible typography or destinations.** | Free sign packs are generic; localised, legible motorway signage is bespoke decal work. | A font licence + 4 hours of decal authoring, **$0-50**. Genuinely cheap and disproportionately convincing. |
| **No environmental storytelling** — no roadworks, cones, breakdowns, overhead gantries, skid marks at the right places. | These are dozens of small unique props plus hand-placement logic. | Fab highway/roadworks prop kits, **$20-60**. Or take them from City Sample for free at 100 GB of download and several days of migration. |
| **No vehicle damage or deformation.** Cars bounce off each other and stay pristine. | There is no free runtime-deformation solution compatible with kinematic static meshes. | A damage-material system (dirt/scratch decal overlays driven by a damage scalar) is **free and 4 hours** and gets you 60% of the read. Real deformation is a major engineering project, not a purchase. |
| **Shimmer on thin geometry at 1080p.** TSR reduces it; nothing removes sub-pixel geometry aliasing. | Physics. No AA method saves geometry smaller than a pixel. | Free mitigation only — bake lane markings into the material (step 9), `r.MaxAnisotropy=8`, `r.TSR.History.ScreenPercentage=200`. The real fix is authoring at higher render resolution, which costs frames, not money. |
| **Audio.** Not asked, but "feel the driving better" is 40% engine note, tyre roar, wind, and doppler on passing traffic — and Overlane's silence will undercut every visual win above. | Free CC0 engine loops exist but are thin and generic. | Freesound.org CC0 packs (free) get you started; a decent engine-sound pack on Fab is **$15-40** and is arguably the highest feel-per-dollar purchase available to you once the visuals land. |

**The bottom line on budget:** roughly 85% of the gap costs nothing. Your constraint has never really been money — it has been that no deliberate rendering decision has yet been made in this project, and the plastic look you keep seeing is the faithful, correct output of that. Steps 1-3 total under 2.5 hours and will produce the most visible jump of anything on this list. Do them before you spend another hour hunting for assets.