"""
Adds the authored post-process volume to the race level.

Run headless (the editor must be CLOSED, the level is saved):
    UnrealEditor-Cmd.exe E:\\Overlane\\Overlane.uproject -run=pythonscript
        -script="E:\\Overlane\\Tools\\ConfigurePostProcess.py"

The build previously had no colour grading at all - AutoExposureBias and
BloomIntensity from a code component, and otherwise whatever BaseEngine.ini
happens to default to. Every shipped racer has a grade; the absence of one is
itself a strong "amateur build" tell.

The two settings that matter most here:

* Film Toe 0.55 -> 0.28. The default toe crushes asphalt - a large, dark,
  low-contrast mass - into mud. Real tarmac footage has lifted, slightly
  blue-grey shadows, never pure black.
* Manual exposure. Histogram auto-exposure makes the image pump as the player
  passes bright buildings and the interchange, and that camcorder pump is one of
  the hardest amateur tells to unsee. A racer with one fixed lighting condition
  has no reason to pay for auto-exposure at all.

ExpandGamut is dropped to 0.3 because at 1.0 saturated car paint goes neon, which
is itself a toy tell. Chromatic aberration starts at 0.55 of the radius so the
centre of frame - where traffic is read - stays perfectly clean.
"""

import unreal

LEVEL_PATH = "/Game/Maps/L_VehicleHandlingTest"
VOLUME_NAME = "OverlaneGradeVolume"


def log(message):
    unreal.log("[Overlane] " + message)


def try_set(target, candidates, value, label):
    """Engine property names drift; one wrong guess must not abort the pass."""
    for name in candidates:
        try:
            target.set_editor_property(name, value)
            return True
        except Exception:
            continue

    unreal.log_warning("[Overlane] could not set " + label)
    return False


def load_level():
    subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if subsystem:
        subsystem.load_level(LEVEL_PATH)
        return subsystem
    unreal.EditorLevelLibrary.load_level(LEVEL_PATH)
    return None


def get_actor_subsystem():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def find_or_spawn_volume():
    actor_subsystem = get_actor_subsystem()
    actors = actor_subsystem.get_all_level_actors() if actor_subsystem \
        else unreal.EditorLevelLibrary.get_all_level_actors()

    for actor in actors:
        if isinstance(actor, unreal.PostProcessVolume) and actor.get_actor_label() == VOLUME_NAME:
            log("reusing existing volume")
            return actor

    volume = actor_subsystem.spawn_actor_from_class(
        unreal.PostProcessVolume, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0))
    volume.set_actor_label(VOLUME_NAME)
    log("spawned volume")
    return volume


def configure_volume(volume):
    # Unbound so it applies everywhere on a 6 km route without a bounding box,
    # and priority 1 so it outranks the code component, which was dropped to -1.
    volume.set_editor_property("unbound", True)
    volume.set_editor_property("priority", 1.0)
    volume.set_editor_property("blend_weight", 1.0)

    settings = volume.get_editor_property("settings")

    def scalar(flag, name, value):
        settings.set_editor_property(flag, True)
        settings.set_editor_property(name, value)

    def colour(flag, name, r, g, b):
        settings.set_editor_property(flag, True)
        settings.set_editor_property(name, unreal.Vector4(r, g, b, 1.0))

    # --- Exposure: actively CLEARED, not merely left unset -------------------
    #
    # This has to switch the override off explicitly. Simply removing the code
    # that set it was not enough: the volume is reused across runs, so the stored
    # Manual metering survived and the scene stayed black. "Stop writing a value"
    # and "clear a value" are different operations on a persisted asset.
    settings.set_editor_property("override_auto_exposure_method", False)
    settings.set_editor_property("override_auto_exposure_bias", False)
    settings.set_editor_property("override_auto_exposure_min_brightness", False)
    settings.set_editor_property("override_auto_exposure_max_brightness", False)

    # --- Exposure: DELIBERATELY NOT OVERRIDDEN ------------------------------
    #
    # Manual metering was tried and reverted. It hands exposure to the camera's
    # shutter/ISO/aperture defaults, which for this scene produced a nearly black
    # image. The scene was correctly exposed before anything touched it, and the
    # "exposure pumping" this was meant to solve was a theoretical concern rather
    # than an observed one. If pumping does become a real problem, the right fix
    # is to clamp the auto-exposure min and max brightness to the same value,
    # which keeps the engine's own units, not to switch metering mode.

    # --- Filmic tonemapper --------------------------------------------------
    # Toe is the important one: the default crushes asphalt into mud.
    scalar("override_film_slope", "film_slope", 0.78)
    scalar("override_film_toe", "film_toe", 0.28)
    scalar("override_film_shoulder", "film_shoulder", 0.35)
    scalar("override_film_white_clip", "film_white_clip", 0.04)

    # --- Colour grading -----------------------------------------------------
    scalar("override_color_saturation", "color_saturation", unreal.Vector4(0.92, 0.92, 0.92, 1.0))
    scalar("override_color_contrast", "color_contrast", unreal.Vector4(1.05, 1.05, 1.05, 1.0))

    # Cool shadows, warm highlights: this split is what the eye reads as outdoors.
    colour("override_color_saturation_shadows", "color_saturation_shadows", 0.85, 0.85, 0.85)
    colour("override_color_gain_shadows", "color_gain_shadows", 0.95, 0.98, 1.06)
    colour("override_color_gain_highlights", "color_gain_highlights", 1.03, 1.01, 0.97)
    scalar("override_color_correction_highlights_min", "color_correction_highlights_min", 0.6)

    # At 1.0 saturated car paint goes neon, which is itself a toy tell.
    scalar("override_expand_gamut", "expand_gamut", 0.3)
    scalar("override_blue_correction", "blue_correction", 0.6)

    scalar("override_white_temp", "white_temp", 6200.0)
    scalar("override_white_tint", "white_tint", 0.0)

    # --- Bloom and local exposure ------------------------------------------
    scalar("override_bloom_intensity", "bloom_intensity", 0.45)
    scalar("override_local_exposure_highlight_contrast_scale", "local_exposure_highlight_contrast_scale", 0.8)
    scalar("override_local_exposure_shadow_contrast_scale", "local_exposure_shadow_contrast_scale", 0.8)

    # --- Lens ---------------------------------------------------------------
    # Start offset keeps the centre of frame clean - that is where traffic is read.
    scalar("override_scene_fringe_intensity", "scene_fringe_intensity", 0.6)
    try_set(settings, ["chromatic_aberration_start_offset"], 0.55, "chromatic aberration start")
    settings.set_editor_property("override_chromatic_aberration_start_offset", True)
    scalar("override_vignette_intensity", "vignette_intensity", 0.4)

    # --- Motion blur --------------------------------------------------------
    # Per-object blur is a trap here: traffic recycles by teleport, and a teleport
    # is a one-frame velocity spike that smears that car across the whole screen.
    # Object size culling suppresses it while camera blur is kept.
    scalar("override_motion_blur_amount", "motion_blur_amount", 0.2)
    scalar("override_motion_blur_max", "motion_blur_max", 2.0)
    scalar("override_motion_blur_target_fps", "motion_blur_target_fps", 60)
    scalar("override_motion_blur_per_object_size", "motion_blur_per_object_size", 2.0)

    # --- Screen space reflections ------------------------------------------
    scalar("override_screen_space_reflection_quality", "screen_space_reflection_quality", 75.0)
    scalar("override_screen_space_reflection_intensity", "screen_space_reflection_intensity", 100.0)
    scalar("override_screen_space_reflection_max_roughness", "screen_space_reflection_max_roughness", 0.65)

    volume.set_editor_property("settings", settings)
    log("configured grade")


def run():
    load_level()
    volume = find_or_spawn_volume()
    if not volume:
        unreal.log_error("[Overlane] could not create the post process volume")
        return

    configure_volume(volume)

    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if level_subsystem:
        level_subsystem.save_current_level()
    else:
        unreal.EditorLevelLibrary.save_current_level()

    log("saved level")


run()
