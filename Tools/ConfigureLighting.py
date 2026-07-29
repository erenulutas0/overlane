"""
Sets up the level's directional light for a 6 km highway.

Run headless (the editor must be CLOSED, the level is saved):
    UnrealEditor-Cmd.exe E:\\Overlane\\Overlane.uproject -run=pythonscript
        -script="E:\\Overlane\\Tools\\ConfigureLighting.py"

The defaults were the engine template's, and two of them are why the route read
as a tabletop model rather than a road:

* DynamicShadowDistanceMovableLight was 200 m. On a 6 km straight, everything
  past ~200 m was completely unshadowed and flat-lit - the classic diorama tell.
* The sun was near-overhead, which produces no grazing-angle specular. A low sun
  raking across the tarmac is the single strongest "this is a real road" cue in
  any highway photograph, and it is what makes the wheel-polish wear lanes in
  M_OverlaneSurface visible at all.

Contact shadows are added because the shadow-map texel footprint at highway
distance is far larger than the gap between a tyre and the road, so shadows start
several centimetres away from the car and every vehicle reads as pasted on.
"""

import unreal

LEVEL_PATH = "/Game/Maps/L_VehicleHandlingTest"

# Roughly 40 degrees off the highway axis: never straight down the road, or the
# specular band collapses onto the vanishing point instead of raking across.
SUN_PITCH = -15.0
SUN_YAW = 40.0


def log(message):
    unreal.log("[Overlane] " + message)


def load_level():
    subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if subsystem:
        subsystem.load_level(LEVEL_PATH)
        return subsystem
    unreal.EditorLevelLibrary.load_level(LEVEL_PATH)
    return None


def get_all_actors():
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if actor_subsystem:
        return actor_subsystem.get_all_level_actors()
    return unreal.EditorLevelLibrary.get_all_level_actors()


def try_set(component, candidates, value, label):
    """
    Set the first property name that exists.

    Engine property names drift between versions and the Python bindings do not
    always match the C++ or the editor label, so a single wrong guess must not
    abort the whole pass and leave the level half-configured.
    """
    for name in candidates:
        try:
            component.set_editor_property(name, value)
            return True
        except Exception:
            continue

    unreal.log_warning("[Overlane] could not set " + label + " (tried: " + ", ".join(candidates) + ")")
    return False


def configure_directional_light(actor):
    actor.set_actor_rotation(unreal.Rotator(0.0, SUN_PITCH, SUN_YAW), False)

    component = actor.get_component_by_class(unreal.DirectionalLightComponent)
    if not component:
        unreal.log_error("[Overlane] directional light has no light component")
        return False

    # A 6 km straight needs shadows far past the 200 m default.
    try_set(component, ["dynamic_shadow_distance_movable_light"], 40000.0, "shadow distance")
    try_set(component, ["dynamic_shadow_cascades"], 4, "cascades")
    try_set(component, ["cascade_distribution_exponent"], 3.0, "cascade exponent")

    # Softens the shadow edge the way a real sun does; 0.5357 is a point source.
    try_set(component, ["light_source_angle"], 1.2, "source angle")

    # World-space, not screen-space: the camera arm varies with speed, and a
    # screen-space length would make the tyre contact gap breathe as it moves.
    if try_set(component,
               ["contact_shadow_length_in_ws", "contact_shadow_length_in_world_space_units"],
               True, "contact shadow world space"):
        try_set(component, ["contact_shadow_length"], 20.0, "contact shadow length")
    else:
        # Screen-space fallback: 0.03 is the plan's equivalent.
        try_set(component, ["contact_shadow_length"], 0.03, "contact shadow length")

    try_set(component, ["use_temperature"], True, "use temperature")
    try_set(component, ["temperature"], 5800.0, "temperature")
    try_set(component, ["atmosphere_sun_light"], True, "atmosphere sun")

    log("configured directional light")
    return True


def configure_sky_light(actor):
    component = actor.get_component_by_class(unreal.SkyLightComponent)
    if not component:
        return False

    # Movable so Lumen keeps it in sync as the scene changes.
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    try_set(component, ["real_time_capture"], True, "real time capture")
    try_set(component, ["intensity"], 1.0, "sky light intensity")
    log("configured sky light")
    return True


def run():
    load_level()

    touched = False
    for actor in get_all_actors():
        if isinstance(actor, unreal.DirectionalLight):
            touched |= configure_directional_light(actor)
        elif isinstance(actor, unreal.SkyLight):
            touched |= configure_sky_light(actor)

    if not touched:
        unreal.log_error("[Overlane] no lights found to configure")
        return

    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if level_subsystem:
        level_subsystem.save_current_level()
    else:
        unreal.EditorLevelLibrary.save_current_level()

    log("saved level")


run()
