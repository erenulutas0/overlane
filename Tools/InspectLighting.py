"""Read-only: dump the level's actual lighting state so it can be diagnosed."""

import unreal

LEVEL_PATH = "/Game/Maps/L_VehicleHandlingTest"


REPORT_PATH = r"E:\Overlane\Saved\LightingReport.txt"
_lines = []


def show(label, value):
    # Written to a file rather than the log: unreal.log output is not reliably
    # captured when this runs as a headless commandlet.
    _lines.append("%s = %s" % (label, value))
    unreal.log_warning("[Overlane] %s = %s" % (label, value))


def get(component, name):
    try:
        return component.get_editor_property(name)
    except Exception:
        return "<no such property>"


def run():
    subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if subsystem:
        subsystem.load_level(LEVEL_PATH)

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in actor_subsystem.get_all_level_actors():
        if isinstance(actor, unreal.DirectionalLight):
            component = actor.get_component_by_class(unreal.DirectionalLightComponent)
            show("SUN rotation", actor.get_actor_rotation())
            show("SUN intensity", get(component, "intensity"))
            show("SUN units", get(component, "intensity_units"))
            show("SUN temperature", get(component, "temperature"))
            show("SUN use_temperature", get(component, "use_temperature"))
            show("SUN shadow_distance", get(component, "dynamic_shadow_distance_movable_light"))
            show("SUN source_angle", get(component, "light_source_angle"))
            show("SUN atmosphere_sun_light", get(component, "atmosphere_sun_light"))
            show("SUN mobility", component.get_editor_property("mobility"))

        elif isinstance(actor, unreal.SkyLight):
            component = actor.get_component_by_class(unreal.SkyLightComponent)
            show("SKY intensity", get(component, "intensity"))
            show("SKY real_time_capture", get(component, "real_time_capture"))
            show("SKY source_type", get(component, "source_type"))
            show("SKY mobility", component.get_editor_property("mobility"))

        elif isinstance(actor, unreal.PostProcessVolume):
            settings = actor.get_editor_property("settings")
            show("PPV label", actor.get_actor_label())
            show("PPV priority", actor.get_editor_property("priority"))
            show("PPV override_auto_exposure_method", get(settings, "override_auto_exposure_method"))
            show("PPV auto_exposure_method", get(settings, "auto_exposure_method"))
            show("PPV override_auto_exposure_bias", get(settings, "override_auto_exposure_bias"))
            show("PPV auto_exposure_bias", get(settings, "auto_exposure_bias"))
            show("PPV film_toe", get(settings, "film_toe"))
            show("PPV film_slope", get(settings, "film_slope"))


run()

with open(REPORT_PATH, "w") as report:
    report.write("\n".join(_lines) + "\n")
