"""
Authors M_OverlaneConcrete and applies it to the route's barrier walls.

Run headless (the editor must be CLOSED, the level is saved):
    UnrealEditor-Cmd.exe E:\\Overlane\\Overlane.uproject -run=pythonscript
        -script="E:\\Overlane\\Tools\\BuildConcreteMaterial.py"

The barriers are the largest object in frame for the whole race and they are flat
untextured white, which is the single loudest "untextured prototype" cue left in the
scene now that the road carries an authored surface.

No download is required for this. The material is procedural: world-aligned noise at
two scales drives both albedo and roughness, and a horizontal band darkens the base of
the wall the way road spray actually stains concrete. That is not as good as a scanned
surface, but it removes the two things the eye objects to - a perfectly uniform colour
and a perfectly uniform roughness - and it costs nothing.

Sampling is WORLD-ALIGNED for the same reason M_OverlaneSurface is: the barriers are
box primitives scaled to thousands of units with a single 0-1 UV per face, so a
UV-sampled texture would stretch by the same three-orders-of-magnitude factor that made
the road impossible to texture conventionally.
"""

import unreal

LEVEL_PATH = "/Game/Maps/L_VehicleHandlingTest"
MATERIAL_PATH = "/Game/Environment"
MATERIAL_NAME = "M_OverlaneConcrete"

# Barriers are ~10 m tall boxes; the stain band is expressed in world cm from the road.
STAIN_HEIGHT = 260.0

BASE_TINT = (0.62, 0.63, 0.635)


def log(message):
    unreal.log_warning("[Overlane] " + message)


def make_expression(material, klass, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(material, klass, x, y)


def connect(material, from_node, from_pin, to_node, to_pin):
    unreal.MaterialEditingLibrary.connect_material_expressions(from_node, from_pin, to_node, to_pin)


def build_material():
    tools = unreal.AssetToolsHelpers.get_asset_tools()

    existing = unreal.EditorAssetLibrary.load_asset(MATERIAL_PATH + "/" + MATERIAL_NAME)
    if existing:
        log("reusing existing material")
        material = existing
        unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    else:
        material = tools.create_asset(MATERIAL_NAME, MATERIAL_PATH, unreal.Material, unreal.MaterialFactoryNew())
        log("created material")

    # --- World position, so nothing depends on the box's degenerate UVs -----------
    world_pos = make_expression(material, unreal.MaterialExpressionWorldPosition, -1500, 0)

    # Two noise octaves: a large one for panel-to-panel colour drift, a fine one for
    # the aggregate speckle that stops the surface reading as painted plastic.
    coarse = make_expression(material, unreal.MaterialExpressionNoise, -1200, -200)
    coarse.set_editor_property("scale", 0.0016)
    coarse.set_editor_property("quality", 2)
    coarse.set_editor_property("levels", 3)
    coarse.set_editor_property("output_min", 0.82)
    coarse.set_editor_property("output_max", 1.10)
    connect(material, world_pos, "", coarse, "Position")

    fine = make_expression(material, unreal.MaterialExpressionNoise, -1200, 120)
    fine.set_editor_property("scale", 0.045)
    fine.set_editor_property("quality", 1)
    fine.set_editor_property("levels", 2)
    fine.set_editor_property("output_min", 0.92)
    fine.set_editor_property("output_max", 1.06)
    connect(material, world_pos, "", fine, "Position")

    variation = make_expression(material, unreal.MaterialExpressionMultiply, -950, -40)
    connect(material, coarse, "", variation, "A")
    connect(material, fine, "", variation, "B")

    # --- Road-spray stain along the base ----------------------------------------
    # Height above the road, remapped so 0 at the bottom edge rises to 1 by
    # STAIN_HEIGHT. Concrete beside a motorway is always dirtiest at the bottom.
    mask_z = make_expression(material, unreal.MaterialExpressionComponentMask, -1200, 340)
    mask_z.set_editor_property("r", False)
    mask_z.set_editor_property("g", False)
    mask_z.set_editor_property("b", True)
    mask_z.set_editor_property("a", False)
    connect(material, world_pos, "", mask_z, "Input")

    stain = make_expression(material, unreal.MaterialExpressionDivide, -1000, 340)
    connect(material, mask_z, "", stain, "A")
    stain.set_editor_property("const_b", STAIN_HEIGHT)

    stain_clamped = make_expression(material, unreal.MaterialExpressionClamp, -850, 340)
    connect(material, stain, "", stain_clamped, "Input")

    # --- Base colour -------------------------------------------------------------
    clean = make_expression(material, unreal.MaterialExpressionConstant3Vector, -700, -180)
    clean.set_editor_property("constant", unreal.LinearColor(BASE_TINT[0], BASE_TINT[1], BASE_TINT[2], 1.0))

    dirty = make_expression(material, unreal.MaterialExpressionConstant3Vector, -700, -60)
    dirty.set_editor_property("constant", unreal.LinearColor(0.30, 0.30, 0.29, 1.0))

    tinted = make_expression(material, unreal.MaterialExpressionLinearInterpolate, -480, -120)
    connect(material, dirty, "", tinted, "A")
    connect(material, clean, "", tinted, "B")
    connect(material, stain_clamped, "", tinted, "Alpha")

    albedo = make_expression(material, unreal.MaterialExpressionMultiply, -280, -120)
    connect(material, tinted, "", albedo, "A")
    connect(material, variation, "", albedo, "B")

    # --- Roughness ---------------------------------------------------------------
    # Uniform roughness is as strong a synthetic tell as uniform colour; the same
    # noise drives it so the two agree, which is what real weathering does.
    rough = make_expression(material, unreal.MaterialExpressionLinearInterpolate, -280, 200)
    rough.set_editor_property("const_a", 0.78)
    rough.set_editor_property("const_b", 0.94)
    connect(material, variation, "", rough, "Alpha")

    unreal.MaterialEditingLibrary.connect_material_property(albedo, "", unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(MATERIAL_PATH + "/" + MATERIAL_NAME)
    log("built " + MATERIAL_NAME)
    return material


def apply_to_barriers(material):
    subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if subsystem:
        subsystem.load_level(LEVEL_PATH)

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    applied = 0

    for actor in actor_subsystem.get_all_level_actors():
        label = actor.get_actor_label()
        if "Barrier" not in label:
            continue

        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if not component:
            continue

        for index in range(max(1, component.get_num_materials())):
            component.set_material(index, material)
        applied += 1
        log("applied to " + label)

    if applied and subsystem:
        subsystem.save_current_level()

    log("applied to %d barrier(s)" % applied)


def run():
    material = build_material()
    if material:
        apply_to_barriers(material)


run()
