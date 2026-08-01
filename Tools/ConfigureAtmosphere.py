"""
Gives the route atmospheric depth by configuring the level's height fog.

Run headless (the editor must be CLOSED, the level is saved):
    UnrealEditor-Cmd.exe E:\\Overlane\\Overlane.uproject -run=pythonscript
        -script="E:\\Overlane\\Tools\\ConfigureAtmosphere.py"

The scene's remaining "flat" cue is that distance costs nothing. Hills 6 km away
render at the same contrast and saturation as a tree 30 m away, so the eye has no
depth information at all beyond perspective, and the whole route reads as a painted
backdrop at arm's length.

Real aerial perspective does three things, and all three matter here:
  * contrast falls with distance - far objects lose their blacks first
  * colour shifts toward the sky - which is why distant hills read blue
  * the sun scatters forward, so haze brightens toward it and stays dim away from it

The last one is what stops fog looking like grey soup. DirectionalInscattering ties
the haze to the same 5800K sun ConfigureLighting.py sets at -35 degrees, so driving
toward and away from it look different.

Density is deliberately low. This is a clear day on a motorway, not weather: the goal
is that 6 km reads as 6 km, not that the player squints. StartDistance keeps the
traffic the player is actually reading perfectly crisp - fog that touches the cars in
the next lane would cost readability for no visual gain.
"""

import unreal

LEVEL_PATH = "/Game/Maps/L_VehicleHandlingTest"

# Low enough to be invisible up close, strong enough to separate the 6 km horizon.
FOG_DENSITY = 0.017

# Nearly flat falloff: the route is 6 km long and level, so a fog layer that thins
# with height would sit above the road rather than along it.
FOG_HEIGHT_FALLOFF = 0.045

# Pulled in from 180 m to 140 m. The grazing-angle streaks live in the mid-field, and
# fog there costs nothing in readability: traffic the player is reacting to is inside
# 100 m, so this still never touches a car being read.
FOG_START_DISTANCE = 14000.0

# Cool and desaturated: the colour distance actually shifts toward on a clear day.
FOG_COLOUR = (0.42, 0.50, 0.62)

# Warm forward scatter, tied to the sun ConfigureLighting.py places.
SUN_SCATTER_COLOUR = (1.00, 0.86, 0.66)
SUN_SCATTER_EXPONENT = 12.0
SUN_SCATTER_START_DISTANCE = 22000.0


def log(message):
    unreal.log_warning("[Overlane] " + message)


def try_set(target, candidates, value, label):
    """Engine property names drift between versions; one wrong guess must not abort."""
    for name in candidates:
        try:
            target.set_editor_property(name, value)
            return True
        except Exception:
            continue
    unreal.log_warning("[Overlane] could not set " + label)
    return False


def configure_fog(actor):
    component = actor.get_component_by_class(unreal.ExponentialHeightFogComponent)
    if not component:
        unreal.log_error("[Overlane] fog actor has no fog component")
        return False

    try_set(component, ["fog_density"], FOG_DENSITY, "fog density")
    try_set(component, ["fog_height_falloff"], FOG_HEIGHT_FALLOFF, "height falloff")
    try_set(component, ["start_distance"], FOG_START_DISTANCE, "start distance")

    try_set(component, ["fog_inscattering_luminance", "fog_inscattering_color"],
            unreal.LinearColor(FOG_COLOUR[0], FOG_COLOUR[1], FOG_COLOUR[2], 1.0),
            "fog colour")

    # Forward scatter around the sun. Without this the haze is a flat grey wash and
    # reads as a rendering artefact rather than as air.
    try_set(component, ["directional_inscattering_luminance", "directional_inscattering_color"],
            unreal.LinearColor(SUN_SCATTER_COLOUR[0], SUN_SCATTER_COLOUR[1], SUN_SCATTER_COLOUR[2], 1.0),
            "sun scatter colour")
    try_set(component, ["directional_inscattering_exponent"], SUN_SCATTER_EXPONENT, "scatter exponent")
    try_set(component, ["directional_inscattering_start_distance"], SUN_SCATTER_START_DISTANCE, "scatter start")

    # A second, denser layer hugging the ground. It is what puts haze in the gap
    # between the road and the distant hills, which is the band the eye actually
    # uses to judge how far away the horizon is.
    try_set(component, ["second_fog_data"], unreal.ExponentialHeightFogData(
        fog_density=0.010,
        fog_height_falloff=0.35,
        fog_height_offset=-2000.0), "second fog layer")

    # Volumetric fog is deliberately OFF. At 245 km/h its froxel grid produces
    # visible temporal swimming along the road, and it costs frame time this scene
    # has no reason to spend - there are no light shafts here to justify it.
    # Measured: "volumetric_fog" is not the binding's name in 5.8, so this warned and
    # did nothing on the first run. Harmless - the property defaults to off - but the
    # alternatives are listed so the intent survives if it is ever enabled by hand.
    try_set(component, ["b_enable_volumetric_fog", "enable_volumetric_fog", "volumetric_fog"],
            False, "volumetric off")

    log("configured height fog")
    return True


def run():
    subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if subsystem:
        subsystem.load_level(LEVEL_PATH)

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    touched = False

    for actor in actor_subsystem.get_all_level_actors():
        if isinstance(actor, unreal.ExponentialHeightFog):
            touched |= configure_fog(actor)

    if not touched:
        unreal.log_error("[Overlane] no ExponentialHeightFog found in the level")
        return

    if subsystem:
        subsystem.save_current_level()
    log("saved level")


run()
