"""
Imports the ambientCG scans and builds world-aligned materials from them.

Run headless (the editor must be CLOSED, the level is saved):
    UnrealEditor-Cmd.exe E:\\Overlane\\Overlane.uproject -run=pythonscript
        -script="E:\\Overlane\\Tools\\BuildScannedSurfaces.py"

Replaces the flat colours and the procedural concrete on the three largest surfaces in
frame. Procedural noise worked on the barrier because concrete really is a homogeneous
material, but ground is not: real variation there is blotchy, clustered and directional,
and no noise function reproduces that. These are photogrammetry scans, so they carry it
for free.

WORLD-ALIGNED, not UV. Every surface here is a box primitive scaled to thousands of
units with a single 0-1 UV per face, so UV sampling would stretch the texture by three
orders of magnitude - the same reason M_OverlaneSurface had to be world-aligned.
WorldAlignedTexture also means the tiling never swims when a box is rescaled.

Deliberately NOT doing ORM packing. Packing roughness and AO into one texture's channels
saves a sample, but it needs offline image processing this project has no tooling for,
and at three materials the saving is not worth adding a dependency. Colour, roughness
and normal are sampled separately; AO and displacement are left out entirely, since a
flat ground plane and a flat wall have nothing for either to do.

NormalDX is used rather than NormalGL: ambientCG ships both, and DX is the convention
Unreal expects, so no green-channel flip is needed.
"""

import unreal

LEVEL_PATH = "/Game/Maps/L_VehicleHandlingTest"
SOURCE_DIR = "E:/Overlane/Content/Environment/Source"
TEXTURE_PATH = "/Game/Environment/Textures"
MATERIAL_PATH = "/Game/Environment"

# Real-world tiling size in cm. The scans are ca. 2 m square, so anything near 200
# reproduces their true scale; larger values trade detail for less visible repetition
# across a 6 km route, which matters most on the surfaces that fill the most screen.
SURFACES = [
    # (asset id, material name, tiling cm, tint, applies to)
    # Raised from the first pass. Repeat count is what betrays a tiling scan, and it
    # falls linearly with tiling size: the grass plane is 20 km across, so 900 cm gave
    # over two thousand repeats and read as streaks. Larger tiles trade some detail
    # density for far less periodicity, which is the right trade on surfaces the player
    # never sees close up - and the macro variation below covers what is left.
    ("Concrete045", "M_OverlaneConcreteScan", 420.0, (1.00, 1.00, 1.00), "barrier"),
    ("Ground041", "M_OverlaneVergeScan", 1100.0, (0.92, 0.88, 0.82), "verge"),
    ("Grass003", "M_OverlaneGrassScan", 2600.0, (0.78, 0.86, 0.70), "landscape"),
]


def log(message):
    unreal.log_warning("[Overlane] " + message)


def import_texture(asset_id, map_name, is_normal):
    destination = TEXTURE_PATH + "/T_" + asset_id + "_" + map_name
    existing = unreal.EditorAssetLibrary.load_asset(destination)
    if existing:
        return existing

    task = unreal.AssetImportTask()
    task.filename = SOURCE_DIR + "/" + asset_id + "_" + map_name + ".jpg"
    task.destination_path = TEXTURE_PATH
    task.destination_name = "T_" + asset_id + "_" + map_name
    task.automated = True
    task.replace_existing = True
    task.save = True

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.EditorAssetLibrary.load_asset(destination)

    if texture:
        if is_normal:
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            texture.set_editor_property("srgb", False)
        elif map_name == "Roughness":
            # Roughness is data, not colour. Leaving sRGB on would gamma-encode it and
            # push every surface toward the wrong gloss.
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            texture.set_editor_property("srgb", False)
        unreal.EditorAssetLibrary.save_asset(destination)

    return texture


def make(material, klass, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(material, klass, x, y)


def build_material(asset_id, material_name, tiling_cm, tint):
    colour = import_texture(asset_id, "Color", False)
    rough = import_texture(asset_id, "Roughness", False)
    normal = import_texture(asset_id, "NormalDX", True)

    if not colour:
        unreal.log_error("[Overlane] missing colour map for " + asset_id)
        return None

    path = MATERIAL_PATH + "/" + material_name

    # Recreated from scratch rather than re-authored in place.
    #
    # delete_all_material_expressions on an already-loaded material trips
    # "Assertion failed: !IsRooted()" inside MaterialEditor and takes the whole
    # commandlet down - which is exactly what the second run of this tool did, after
    # the first had created the assets. Deleting the asset first sidesteps it, and the
    # tool is idempotent either way: the level reference is re-applied at the end.
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)

    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        material_name, MATERIAL_PATH, unreal.Material, unreal.MaterialFactoryNew())

    # One shared tiling scalar so colour, roughness and normal cannot drift apart.
    size = make(material, unreal.MaterialExpressionConstant, -1400, 300)
    size.set_editor_property("r", tiling_cm)

    def world_aligned(texture, y_pos, is_normal=False):
        node = make(material, unreal.MaterialExpressionMaterialFunctionCall, -1000, y_pos)
        # Verified against the engine content on disk rather than recalled: the
        # function lives under Engine_MaterialFunctions01/Texturing, not under
        # 02/WorldPositionOffset, and guessing the latter made the whole pass fail.
        function = unreal.EditorAssetLibrary.load_asset(
            "/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture.WorldAlignedTexture")
        if not function:
            return None
        node.set_material_function(function)

        sampler = make(material, unreal.MaterialExpressionTextureObject, -1300, y_pos)
        sampler.set_editor_property("texture", texture)
        if is_normal:
            sampler.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)

        unreal.MaterialEditingLibrary.connect_material_expressions(sampler, "", node, "TextureObject")
        unreal.MaterialEditingLibrary.connect_material_expressions(size, "", node, "TextureSize")
        return node

    colour_node = world_aligned(colour, -200)
    if not colour_node:
        unreal.log_error("[Overlane] WorldAlignedTexture function not found")
        return None

    tint_node = make(material, unreal.MaterialExpressionConstant3Vector, -700, -60)
    tint_node.set_editor_property("constant", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))

    tinted = make(material, unreal.MaterialExpressionMultiply, -420, -160)
    unreal.MaterialEditingLibrary.connect_material_expressions(colour_node, "XY Texture", tinted, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(tint_node, "", tinted, "B")

    # --- Macro variation, which is what makes a tiling scan survive this scale ------
    #
    # The grass plane is 20 km across and the scan is about 2 m, so it repeats over two
    # thousand times. At that count no texture holds up on its own: the eye stops
    # reading it as ground and starts reading the repeat itself, which is why the first
    # pass produced parallel streaks running to the horizon.
    #
    # Two octaves of very-low-frequency noise, an order of magnitude larger than the
    # texture, multiply into the albedo and destroy the periodicity without touching
    # the surface detail.
    #
    # CENTRED ON 1.0, deliberately. Lerping toward the raw albedo instead - which is
    # what an earlier road material did - darkens the whole surface rather than varying
    # it, and that mistake crushed the road to near-black once already.
    macro_pos = make(material, unreal.MaterialExpressionWorldPosition, -1400, 900)

    macro_a = make(material, unreal.MaterialExpressionNoise, -1150, 860)
    macro_a.set_editor_property("scale", 0.00035)
    macro_a.set_editor_property("quality", 2)
    macro_a.set_editor_property("levels", 3)
    macro_a.set_editor_property("output_min", 0.80)
    macro_a.set_editor_property("output_max", 1.20)
    unreal.MaterialEditingLibrary.connect_material_expressions(macro_pos, "", macro_a, "Position")

    macro_b = make(material, unreal.MaterialExpressionNoise, -1150, 1080)
    macro_b.set_editor_property("scale", 0.0022)
    macro_b.set_editor_property("quality", 1)
    macro_b.set_editor_property("levels", 2)
    macro_b.set_editor_property("output_min", 0.88)
    macro_b.set_editor_property("output_max", 1.12)
    unreal.MaterialEditingLibrary.connect_material_expressions(macro_pos, "", macro_b, "Position")

    macro = make(material, unreal.MaterialExpressionMultiply, -900, 960)
    unreal.MaterialEditingLibrary.connect_material_expressions(macro_a, "", macro, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(macro_b, "", macro, "B")

    varied = make(material, unreal.MaterialExpressionMultiply, -220, -120)
    unreal.MaterialEditingLibrary.connect_material_expressions(tinted, "", varied, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(macro, "", varied, "B")
    unreal.MaterialEditingLibrary.connect_material_property(varied, "", unreal.MaterialProperty.MP_BASE_COLOR)

    if rough:
        rough_node = world_aligned(rough, 200)
        if rough_node:
            unreal.MaterialEditingLibrary.connect_material_property(
                rough_node, "XY Texture", unreal.MaterialProperty.MP_ROUGHNESS)

    if normal:
        normal_node = world_aligned(normal, 600, True)
        if normal_node:
            unreal.MaterialEditingLibrary.connect_material_property(
                normal_node, "XY Texture", unreal.MaterialProperty.MP_NORMAL)

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(path)
    log("built " + material_name)
    return material


def apply_barrier(material):
    subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if subsystem:
        subsystem.load_level(LEVEL_PATH)

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    applied = 0
    for actor in actor_subsystem.get_all_level_actors():
        if "Barrier" not in actor.get_actor_label():
            continue
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if not component:
            continue
        for index in range(max(1, component.get_num_materials())):
            component.set_material(index, material)
        applied += 1

    if applied and subsystem:
        subsystem.save_current_level()
    log("barrier material applied to %d actor(s)" % applied)


def run():
    built = {}
    for asset_id, material_name, tiling, tint, target in SURFACES:
        material = build_material(asset_id, material_name, tiling, tint)
        if material:
            built[target] = material

    if "barrier" in built:
        apply_barrier(built["barrier"])

    log("done - verge and grass materials are consumed by HighwayEnvironmentDirector")


run()
