"""
Builds the world-aligned road surface material for OVERLANE.

Run headless:
    UnrealEditor-Cmd.exe E:\\Overlane\\Overlane.uproject -run=pythonscript
        -script="E:\\Overlane\\Tools\\BuildRoadMaterial.py"

Why this exists as a script rather than being hand-authored: the road is an
engine cube scaled to (6300, 20, 0.07) and that cube carries one 0-1 UV set per
face, so any UV-sampled texture stretches 6.3 km by 20 m -- about 315:1. The
material below samples by world position instead, which is the only way this
geometry can carry a surface at all.

Three things it does that a naive asphalt material would not:

1. Macro variation. 6 km at a 4 m tile is 1500 repeats, and at 68 m/s a tile
   passes every ~0.06 s. Any recognisable feature becomes a strobe. A second
   sample at a much larger scale modulates brightness and roughness so the
   repeat stops reading as a repeat.
2. Wheel-polish wear lanes. Roughness is driven from lateral world position:
   polished in the two wheel tracks of each lane, coarse at the centre and
   edges. This is the photographic signature of a real motorway and it costs no
   texture at all.
3. Green-channel flip. Poly Haven ships OpenGL-convention normals; Unreal
   expects DirectX.
"""

import os
import unreal

SOURCE_DIR = r"E:\Overlane\SourceArt\Textures\Asphalt"
TEXTURE_PACKAGE = "/Game/Environment/Surfaces/Textures"
MATERIAL_PACKAGE = "/Game/Environment/Surfaces"

MATERIAL_NAME = "M_OverlaneSurface"
INSTANCE_NAME = "MI_OverlaneAsphalt"

TEXTURES = [
    ("asphalt_track_diff_4k.jpg", "T_Asphalt_D", True, unreal.TextureCompressionSettings.TC_DEFAULT),
    ("asphalt_track_nor_gl_4k.exr", "T_Asphalt_N", False, unreal.TextureCompressionSettings.TC_NORMALMAP),
    ("asphalt_track_rough_4k.exr", "T_Asphalt_R", False, unreal.TextureCompressionSettings.TC_MASKS),
]


def log(message):
    unreal.log("[Overlane] " + message)


def import_textures():
    """Import the source maps with the right colour space and compression."""
    tasks = []
    for file_name, asset_name, srgb, _compression in TEXTURES:
        source = os.path.join(SOURCE_DIR, file_name)
        if not os.path.exists(source):
            unreal.log_error("[Overlane] missing source texture: " + source)
            continue

        task = unreal.AssetImportTask()
        task.filename = source
        task.destination_path = TEXTURE_PACKAGE
        task.destination_name = asset_name
        task.automated = True
        task.replace_existing = True
        task.save = True
        tasks.append(task)

    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    imported = {}
    for _file_name, asset_name, srgb, compression in TEXTURES:
        path = TEXTURE_PACKAGE + "/" + asset_name
        texture = unreal.EditorAssetLibrary.load_asset(path)
        if not texture:
            unreal.log_error("[Overlane] texture did not import: " + path)
            continue

        # sRGB must be off for normal and roughness or the values are gamma
        # decoded and every lighting response is subtly wrong.
        texture.set_editor_property("srgb", srgb)
        texture.set_editor_property("compression_settings", compression)
        unreal.EditorAssetLibrary.save_asset(path)
        imported[asset_name] = texture
        log("imported " + asset_name)

    return imported


def make(material, expression_class, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(material, expression_class, x, y)


def link(from_expression, from_pin, to_expression, to_pin):
    unreal.MaterialEditingLibrary.connect_material_expressions(
        from_expression, from_pin, to_expression, to_pin)


def mask(material, source, x, y, r=False, g=False, b=False):
    node = make(material, unreal.MaterialExpressionComponentMask, x, y)
    node.set_editor_property("r", r)
    node.set_editor_property("g", g)
    node.set_editor_property("b", b)
    node.set_editor_property("a", False)
    link(source, "", node, "")
    return node


def scalar(material, name, value, x, y, group="Surface"):
    node = make(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    node.set_editor_property("group", group)
    return node


def constant(material, value, x, y):
    node = make(material, unreal.MaterialExpressionConstant, x, y)
    node.set_editor_property("r", value)
    return node


def texture_param(material, name, texture, x, y, sampler_type):
    node = make(material, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("texture", texture)
    node.set_editor_property("sampler_type", sampler_type)
    node.set_editor_property("group", "Surface")
    return node


def build_material(textures):
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

    existing = MATERIAL_PACKAGE + "/" + MATERIAL_NAME
    if unreal.EditorAssetLibrary.does_asset_exist(existing):
        unreal.EditorAssetLibrary.delete_asset(existing)

    material = asset_tools.create_asset(
        MATERIAL_NAME, MATERIAL_PACKAGE, unreal.Material, unreal.MaterialFactoryNew())

    # ---- World-aligned UVs -------------------------------------------------
    world_position = make(material, unreal.MaterialExpressionWorldPosition, -2400, 0)
    world_xy = mask(material, world_position, -2150, 0, r=True, g=True)

    tiling = scalar(material, "TilingSize", 400.0, -2400, 200)
    detail_uv = make(material, unreal.MaterialExpressionDivide, -1900, 0)
    link(world_xy, "", detail_uv, "A")
    link(tiling, "", detail_uv, "B")

    # A much larger tile, used only to break up the repeat.
    macro_tiling_scale = scalar(material, "MacroTileScale", 11.0, -2400, 320)
    macro_tiling = make(material, unreal.MaterialExpressionMultiply, -2150, 260)
    link(tiling, "", macro_tiling, "A")
    link(macro_tiling_scale, "", macro_tiling, "B")

    macro_uv = make(material, unreal.MaterialExpressionDivide, -1900, 260)
    link(world_xy, "", macro_uv, "A")
    link(macro_tiling, "", macro_uv, "B")

    # ---- Samples -----------------------------------------------------------
    base_sample = texture_param(
        material, "BaseColorTex", textures.get("T_Asphalt_D"), -1600, -300,
        unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    link(detail_uv, "", base_sample, "UVs")

    macro_sample = make(material, unreal.MaterialExpressionTextureSample, -1600, 100)
    macro_sample.set_editor_property("texture", textures.get("T_Asphalt_D"))
    macro_sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    link(macro_uv, "", macro_sample, "UVs")

    normal_sample = texture_param(
        material, "NormalTex", textures.get("T_Asphalt_N"), -1600, 460,
        unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    link(detail_uv, "", normal_sample, "UVs")

    rough_sample = texture_param(
        material, "RoughTex", textures.get("T_Asphalt_R"), -1600, 820,
        unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    link(detail_uv, "", rough_sample, "UVs")

    # ---- Macro variation ---------------------------------------------------
    macro_luma = make(material, unreal.MaterialExpressionDesaturation, -1250, 100)
    link(macro_sample, "RGB", macro_luma, "")

    # Asphalt albedo sits around 0.06, so the raw luminance is nowhere near the
    # 0.5 midpoint a variation term needs. Gain it up and saturate first, or the
    # variation is not a variation at all - it is a near-black multiplier that
    # crushes the surface, which is exactly what the first version did.
    macro_gain = scalar(material, "MacroGain", 9.0, -1600, 200)
    macro_gained = make(material, unreal.MaterialExpressionMultiply, -1100, 100)
    link(macro_luma, "", macro_gained, "A")
    link(macro_gain, "", macro_gained, "B")

    macro_norm = make(material, unreal.MaterialExpressionSaturate, -950, 100)
    link(macro_gained, "", macro_norm, "")

    # Centre the variation on 1.0: [1 - amount/2, 1 + amount/2].
    macro_amount = scalar(material, "MacroVariationAmount", 0.35, -1600, 300)
    macro_half = make(material, unreal.MaterialExpressionMultiply, -1350, 300)
    half_const = constant(material, 0.5, -1600, 380)
    link(macro_amount, "", macro_half, "A")
    link(half_const, "", macro_half, "B")

    one_const = constant(material, 1.0, -1350, 220)
    macro_low = make(material, unreal.MaterialExpressionSubtract, -1100, 260)
    link(one_const, "", macro_low, "A")
    link(macro_half, "", macro_low, "B")

    macro_high = make(material, unreal.MaterialExpressionAdd, -1100, 340)
    link(one_const, "", macro_high, "A")
    link(macro_half, "", macro_high, "B")

    macro_range = make(material, unreal.MaterialExpressionLinearInterpolate, -850, 260)
    link(macro_low, "", macro_range, "A")
    link(macro_high, "", macro_range, "B")
    link(macro_norm, "", macro_range, "Alpha")

    tinted = make(material, unreal.MaterialExpressionMultiply, -700, -260)
    link(base_sample, "RGB", tinted, "A")
    link(macro_range, "", tinted, "B")

    tint = make(material, unreal.MaterialExpressionVectorParameter, -1000, -420)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    tint.set_editor_property("group", "Surface")

    base_color = make(material, unreal.MaterialExpressionMultiply, -420, -300)
    link(tinted, "", base_color, "A")
    link(tint, "RGB", base_color, "B")

    # ---- Wheel-polish wear lanes ------------------------------------------
    # Lateral world position folded into one lane, then distance to the wheel
    # track. Polished where tyres run, coarse at the lane centre and the edges.
    world_y = mask(material, world_position, -2150, 1100, g=True)
    lane_width = scalar(material, "LaneWidth", 666.0, -2400, 1180)

    lane_uv = make(material, unreal.MaterialExpressionDivide, -1900, 1100)
    link(world_y, "", lane_uv, "A")
    link(lane_width, "", lane_uv, "B")

    half = constant(material, 0.5, -1900, 1260)
    lane_shift = make(material, unreal.MaterialExpressionAdd, -1650, 1100)
    link(lane_uv, "", lane_shift, "A")
    link(half, "", lane_shift, "B")

    lane_frac = make(material, unreal.MaterialExpressionFrac, -1450, 1100)
    link(lane_shift, "", lane_frac, "")

    lane_centred = make(material, unreal.MaterialExpressionSubtract, -1250, 1100)
    link(lane_frac, "", lane_centred, "A")
    link(half, "", lane_centred, "B")

    lane_abs = make(material, unreal.MaterialExpressionAbs, -1050, 1100)
    link(lane_centred, "", lane_abs, "")

    track_offset = scalar(material, "WheelTrackOffset", 0.27, -1250, 1280)
    track_delta = make(material, unreal.MaterialExpressionSubtract, -850, 1100)
    link(lane_abs, "", track_delta, "A")
    link(track_offset, "", track_delta, "B")

    track_dist = make(material, unreal.MaterialExpressionAbs, -650, 1100)
    link(track_delta, "", track_dist, "")

    track_width = scalar(material, "WheelTrackWidth", 0.11, -850, 1280)
    track_norm = make(material, unreal.MaterialExpressionDivide, -450, 1100)
    link(track_dist, "", track_norm, "A")
    link(track_width, "", track_norm, "B")

    track_sat = make(material, unreal.MaterialExpressionSaturate, -280, 1100)
    link(track_norm, "", track_sat, "")

    wear = make(material, unreal.MaterialExpressionOneMinus, -120, 1100)
    link(track_sat, "", wear, "")

    wear_strength = scalar(material, "WearLaneStrength", 1.0, -280, 1280)
    wear_scaled = make(material, unreal.MaterialExpressionMultiply, 40, 1100)
    link(wear, "", wear_scaled, "A")
    link(wear_strength, "", wear_scaled, "B")

    # Roughness: coarse base, polished in the tracks, plus macro variation.
    rough_coarse = scalar(material, "RoughnessCoarse", 0.78, -450, 820)
    rough_polished = scalar(material, "RoughnessPolished", 0.36, -450, 900)

    rough_target = make(material, unreal.MaterialExpressionLinearInterpolate, 240, 860)
    link(rough_coarse, "", rough_target, "A")
    link(rough_polished, "", rough_target, "B")
    link(wear_scaled, "", rough_target, "Alpha")

    # The roughness map is used as a VARIATION around the target, not as a
    # multiplier. Multiplying by a 0.3-0.8 map would have dragged the whole
    # surface far shinier than any asphalt and washed out the wear lanes.
    rough_var_low = constant(material, 0.85, 240, 1000)
    rough_var_high = constant(material, 1.15, 240, 1060)
    rough_variation = make(material, unreal.MaterialExpressionLinearInterpolate, 420, 1000)
    link(rough_var_low, "", rough_variation, "A")
    link(rough_var_high, "", rough_variation, "B")
    link(rough_sample, "R", rough_variation, "Alpha")

    rough_textured = make(material, unreal.MaterialExpressionMultiply, 620, 860)
    link(rough_target, "", rough_textured, "A")
    link(rough_variation, "", rough_textured, "B")

    rough_final = make(material, unreal.MaterialExpressionMultiply, 800, 860)
    link(rough_textured, "", rough_final, "A")
    link(macro_range, "", rough_final, "B")

    # ---- Normal: OpenGL to DirectX ----------------------------------------
    normal_r = mask(material, normal_sample, -1250, 400, r=True)
    normal_g = mask(material, normal_sample, -1250, 480, g=True)
    normal_b = mask(material, normal_sample, -1250, 560, b=True)

    minus_one = constant(material, -1.0, -1250, 640)
    normal_g_flipped = make(material, unreal.MaterialExpressionMultiply, -1000, 480)
    link(normal_g, "", normal_g_flipped, "A")
    link(minus_one, "", normal_g_flipped, "B")

    normal_rg = make(material, unreal.MaterialExpressionAppendVector, -750, 440)
    link(normal_r, "", normal_rg, "A")
    link(normal_g_flipped, "", normal_rg, "B")

    normal_rgb = make(material, unreal.MaterialExpressionAppendVector, -520, 460)
    link(normal_rg, "", normal_rgb, "A")
    link(normal_b, "", normal_rgb, "B")

    # ---- Outputs -----------------------------------------------------------
    unreal.MaterialEditingLibrary.connect_material_property(
        base_color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(
        rough_final, "", unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.connect_material_property(
        normal_rgb, "", unreal.MaterialProperty.MP_NORMAL)

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(MATERIAL_PACKAGE + "/" + MATERIAL_NAME)
    log("built " + MATERIAL_NAME)
    return material


def build_instance(material):
    instance_path = MATERIAL_PACKAGE + "/" + INSTANCE_NAME
    if unreal.EditorAssetLibrary.does_asset_exist(instance_path):
        unreal.EditorAssetLibrary.delete_asset(instance_path)

    instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        INSTANCE_NAME, MATERIAL_PACKAGE, unreal.MaterialInstanceConstant,
        unreal.MaterialInstanceConstantFactoryNew())
    unreal.MaterialEditingLibrary.set_material_instance_parent(instance, material)
    unreal.EditorAssetLibrary.save_asset(instance_path)
    log("built " + INSTANCE_NAME)
    return instance


CAR_PAINT_NAME = "M_OverlaneCarPaint"


def build_car_paint():
    """
    A tintable metallic car paint.

    Traffic still renders through /Engine/BasicShapes/BasicShapeMaterial - flat,
    metallic 0, one roughness, no fresnel - because Epic's MI_SportsCarBody
    exposes no tint parameter and the pawn code correctly falls back rather than
    silently losing the colour coding that near-miss feedback matches against.
    This gives the fallback a real shading model instead.
    """
    path = MATERIAL_PACKAGE + "/" + CAR_PAINT_NAME
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)

    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        CAR_PAINT_NAME, MATERIAL_PACKAGE, unreal.Material, unreal.MaterialFactoryNew())

    tint = make(material, unreal.MaterialExpressionVectorParameter, -800, -200)
    tint.set_editor_property("parameter_name", "Color")
    tint.set_editor_property("default_value", unreal.LinearColor(0.6, 0.1, 0.05, 1.0))
    tint.set_editor_property("group", "Paint")

    # Real car paint is pigment under clear lacquer: LOW metallic with low
    # roughness and high specular. A high metallic value turns a saturated colour
    # into coloured chrome, because on a metal the base colour IS the reflection
    # tint - which is exactly how the first version read.
    metallic = scalar(material, "Metallic", 0.12, -800, 60, "Paint")
    roughness = scalar(material, "Roughness", 0.18, -800, 140, "Paint")

    # Edge sheen goes to SPECULAR, not base colour. Adding it to base colour
    # washed the pigment out and lifted the whole car toward white.
    fresnel = make(material, unreal.MaterialExpressionFresnel, -800, 280)
    fresnel.set_editor_property("exponent", 4.0)
    fresnel.set_editor_property("base_reflect_fraction", 0.04)

    sheen_strength = scalar(material, "EdgeSheen", 0.6, -800, 400, "Paint")
    sheen = make(material, unreal.MaterialExpressionMultiply, -520, 320)
    link(fresnel, "", sheen, "A")
    link(sheen_strength, "", sheen, "B")

    specular_base = constant(material, 0.5, -800, 480)
    specular = make(material, unreal.MaterialExpressionAdd, -280, 380)
    link(specular_base, "", specular, "A")
    link(sheen, "", specular, "B")

    unreal.MaterialEditingLibrary.connect_material_property(
        tint, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(
        specular, "", unreal.MaterialProperty.MP_SPECULAR)
    unreal.MaterialEditingLibrary.connect_material_property(
        metallic, "", unreal.MaterialProperty.MP_METALLIC)
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(path)
    log("built " + CAR_PAINT_NAME)
    return material


def run():
    build_car_paint()
    textures = import_textures()
    if "T_Asphalt_D" not in textures:
        unreal.log_error("[Overlane] aborting: base colour texture missing")
        return

    material = build_material(textures)
    build_instance(material)
    log("done")


run()
