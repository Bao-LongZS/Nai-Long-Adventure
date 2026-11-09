"""Read-only export of the approved G0 base egg SkeletalMesh to GLB."""
import hashlib
import json
import pathlib
import struct
import traceback

import unreal


ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "WebNative" / "public" / "assets" / "naiwa-egg-base.glb"
SOURCE_BLEND = ROOT / "NaiwaCharacter/deliveries/NaiwaEgg_S4-S5_v3_candidate/SK_NaiwaEgg_S4-S5.blend"
SOURCE_FBX = ROOT / "NaiwaCharacter/deliveries/NaiwaEgg_S4-S5_v3_candidate/SK_NaiwaEgg.fbx"
BLENDER_REPORT = ROOT / "NaiwaCharacter/deliveries/NaiwaEgg_S4-S5_v3_candidate/NaiwaEgg_S4-S5_export_report.json"
UE_IMPORT_REPORT = ROOT / "NaiwaCharacter/deliveries/NaiwaEgg_S4-S5_v3_candidate/NaiwaEgg_S4-S5_ue_import_report.json"
MESH_PATH = "/Game/Naiwa/P2/Art/Egg/SK_NaiwaEgg"
REPORT_PATH = ROOT / "WebNative" / "docs" / "base-egg-export.json"
REQUIRED_MORPHS = {"EyeWide", "Scream", "Squash"}
FACE_MATERIAL_COLORS_SRGB = {
    "M_NaiwaEgg_Eye": "#5E9B33",
    "M_NaiwaEgg_Pupil": "#14210E",
    "M_NaiwaEgg_Mouth": "#2A1A10",
}
EXPECTED_MATERIAL_SLOTS = [
    "M_NaiwaEgg_Body",
    "M_NaiwaEgg_Eye",
    "M_NaiwaEgg_Pupil",
    "M_NaiwaEgg_Mouth",
]


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def set_option(options, name, value):
    options.set_editor_property(name, value)


def read_glb(path):
    data = path.read_bytes()
    if len(data) < 20 or data[:4] != b"glTF":
        raise RuntimeError("UE exporter did not produce a valid GLB header")
    version, declared_length = struct.unpack_from("<II", data, 4)
    if version != 2 or declared_length != len(data):
        raise RuntimeError("UE exporter produced an unsupported or truncated GLB")

    chunks = []
    offset = 12
    while offset < len(data):
        if offset + 8 > len(data):
            raise RuntimeError("GLB ended inside a chunk header")
        chunk_length, chunk_type = struct.unpack_from("<II", data, offset)
        start = offset + 8
        end = start + chunk_length
        if end > len(data):
            raise RuntimeError("GLB ended inside a chunk payload")
        chunks.append((chunk_type, data[start:end]))
        offset = end

    json_chunks = [payload for chunk_type, payload in chunks if chunk_type == 0x4E4F534A]
    binary_chunks = [payload for chunk_type, payload in chunks if chunk_type == 0x004E4942]
    if len(json_chunks) != 1 or len(binary_chunks) != 1:
        raise RuntimeError("Expected one JSON and one binary chunk in the exported GLB")
    return json.loads(json_chunks[0].decode("utf-8").rstrip(" \t\r\n\0")), binary_chunks[0]


def write_glb(path, document, binary_chunk):
    json_chunk = json.dumps(document, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    json_chunk += b" " * ((-len(json_chunk)) % 4)
    binary_chunk += b"\0" * ((-len(binary_chunk)) % 4)
    total_length = 12 + 8 + len(json_chunk) + 8 + len(binary_chunk)
    with path.open("wb") as stream:
        stream.write(struct.pack("<4sII", b"glTF", 2, total_length))
        stream.write(struct.pack("<II", len(json_chunk), 0x4E4F534A))
        stream.write(json_chunk)
        stream.write(struct.pack("<II", len(binary_chunk), 0x004E4942))
        stream.write(binary_chunk)


def srgb_hex_to_linear(hex_value):
    value = hex_value.lstrip("#")
    channels = [int(value[index:index + 2], 16) / 255.0 for index in (0, 2, 4)]
    linear = [channel / 12.92 if channel <= 0.04045 else ((channel + 0.055) / 1.055) ** 2.4
              for channel in channels]
    return [*linear, 1.0]


def patch_glb_materials_and_validate(path, ue_report):
    document, binary_chunk = read_glb(path)
    materials = document.get("materials", [])
    slots = ue_report.get("material_slots", [])
    slot_names = [slot.get("slot_name") for slot in slots]
    if slot_names != EXPECTED_MATERIAL_SLOTS or len(materials) != len(EXPECTED_MATERIAL_SLOTS):
        raise RuntimeError(f"Unexpected UE egg material slot order: {slot_names}")

    body_color = ue_report.get("material_instance_body_color_linear", {}).get("Base")
    if not isinstance(body_color, list) or len(body_color) != 4:
        raise RuntimeError("The UE import report does not contain the approved base body color")
    colors = [body_color] + [srgb_hex_to_linear(FACE_MATERIAL_COLORS_SRGB[name])
                             for name in EXPECTED_MATERIAL_SLOTS[1:]]
    material_report = []
    for index, (material, slot_name, color) in enumerate(zip(materials, EXPECTED_MATERIAL_SLOTS, colors)):
        material["name"] = slot_name
        material["pbrMetallicRoughness"] = {
            "baseColorFactor": color,
            "metallicFactor": 0.0,
            "roughnessFactor": 0.5,
        }
        material["alphaMode"] = "OPAQUE"
        material["doubleSided"] = False
        material_report.append({
            "slot": slot_name,
            "materialIndex": index,
            "baseColorFactorLinear": color,
            "colorSource": "UE P2 import report BodyColor" if index == 0 else
                "UE5/Scripts/import_naiwa_egg_s4_s5.py SOLID_COLORS",
            "roughnessFactor": 0.5,
            "metallicFactor": 0.0,
        })

    stripped_vertex_color_attributes = 0
    meshes = document.get("meshes", [])
    if len(meshes) != 1 or len(meshes[0].get("primitives", [])) != len(EXPECTED_MATERIAL_SLOTS):
        raise RuntimeError("Unexpected primitive/material layout in the exported egg GLB")
    for primitive_index, primitive in enumerate(meshes[0]["primitives"]):
        if primitive.get("material") != primitive_index:
            raise RuntimeError("Exported egg primitive order no longer matches the UE material slots")
        color_accessor_index = primitive.get("attributes", {}).get("COLOR_0")
        if color_accessor_index is None:
            continue
        accessor = document["accessors"][color_accessor_index]
        if accessor.get("componentType") != 5121 or accessor.get("type") != "VEC4":
            raise RuntimeError("Unexpected vertex color representation in the exported egg GLB")
        view = document["bufferViews"][accessor["bufferView"]]
        start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
        stride = view.get("byteStride", 4)
        for vertex in range(accessor["count"]):
            color = binary_chunk[start + vertex * stride:start + vertex * stride + 4]
            if color != b"\0\0\0\xff":
                raise RuntimeError("Unexpected non-black vertex colors; refusing to alter UE appearance")
        del primitive["attributes"]["COLOR_0"]
        stripped_vertex_color_attributes += 1

    # UE's import report records X/Y/Z, while glTF uses X/Y/Z with Y as up.
    # The UE -> glTF exporter maps the original UE vertical Z axis to glTF Y.
    position_min = [float("inf")] * 3
    position_max = [float("-inf")] * 3
    triangle_count = 0
    vertex_count = 0
    for primitive in meshes[0]["primitives"]:
        position = document["accessors"][primitive["attributes"]["POSITION"]]
        if position.get("type") != "VEC3" or len(position.get("min", [])) != 3:
            raise RuntimeError("Exported egg primitive has no usable position bounds")
        vertex_count += position["count"]
        for axis in range(3):
            position_min[axis] = min(position_min[axis], position["min"][axis])
            position_max[axis] = max(position_max[axis], position["max"][axis])
        index_accessor = document["accessors"][primitive["indices"]]
        if index_accessor["count"] % 3:
            raise RuntimeError("Exported egg has a non-triangle primitive")
        triangle_count += index_accessor["count"] // 3

    gltf_dimensions = [position_max[i] - position_min[i] for i in range(3)]
    ue_dimensions_cm = ue_report["imported_dimensions"]["xyz_cm"]
    expected_gltf_dimensions = [ue_dimensions_cm[0] / 100.0, ue_dimensions_cm[2] / 100.0,
                                ue_dimensions_cm[1] / 100.0]
    if any(abs(actual - expected) > 0.005 for actual, expected in
           zip(gltf_dimensions, expected_gltf_dimensions)):
        raise RuntimeError(f"GLB bounds do not match UE import dimensions: {gltf_dimensions}")
    if triangle_count != int(ue_report["triangle_count"]):
        raise RuntimeError(f"GLB triangle count {triangle_count} does not match the UE import report")
    expected_vertices = int(ue_report["imported_skin_weights"]["vertex_count"])
    if vertex_count != expected_vertices:
        raise RuntimeError(f"GLB vertex count {vertex_count} does not match the UE import report")
    morph_counts = {len(primitive.get("targets", [])) for primitive in meshes[0]["primitives"]}
    if morph_counts != {len(REQUIRED_MORPHS)}:
        raise RuntimeError(f"The exported egg GLB has inconsistent morph target counts: {morph_counts}")

    mesh_nodes = [node for node in document.get("nodes", []) if "mesh" in node]
    if len(mesh_nodes) != 1 or any(key in mesh_nodes[0] for key in ("matrix", "translation", "rotation", "scale")):
        raise RuntimeError("Expected one identity-transformed UE egg mesh node in the GLB")

    write_glb(path, document, binary_chunk)
    return {
        "materials": material_report,
        "vertexColorAttributesRemoved": stripped_vertex_color_attributes,
        "vertexColorPolicy": "UE export emitted opaque black COLOR_0 values; removed them so authored UE material colors render correctly.",
        "gltfBoundsMetersXYZ": gltf_dimensions,
        "ueExpectedGltfBoundsMetersXYZ": expected_gltf_dimensions,
        "vertexCountAcrossMaterialPrimitives": vertex_count,
        "triangleCount": triangle_count,
    }


def main():
    for path in (SOURCE_BLEND, SOURCE_FBX, BLENDER_REPORT, UE_IMPORT_REPORT):
        if not path.is_file():
            raise FileNotFoundError(path)

    blender_report = json.loads(BLENDER_REPORT.read_text(encoding="utf-8"))
    ue_report = json.loads(UE_IMPORT_REPORT.read_text(encoding="utf-8"))
    if blender_report.get("status") != "PASS_S4S5_BLENDER_EXPORT_AWAITING_UE_IMPORT":
        raise RuntimeError("The frozen S4-S5 Blender export report is not PASS")
    if blender_report.get("output_blend_sha256") != sha256(SOURCE_BLEND):
        raise RuntimeError("The frozen S4-S5 Blend deliverable no longer matches its export report")
    if blender_report.get("fbx_sha256") != sha256(SOURCE_FBX):
        raise RuntimeError("The S4-S5 FBX no longer matches its Blender export report")
    if ue_report.get("status") != "PASS" or ue_report.get("source_fbx_sha256") != sha256(SOURCE_FBX):
        raise RuntimeError("The S4-S5 UE import report is not a PASS for the current FBX")

    mesh = unreal.load_asset(MESH_PATH)
    if not mesh or not isinstance(mesh, unreal.SkeletalMesh):
        raise RuntimeError(f"Expected a SkeletalMesh at {MESH_PATH}")
    morph_names = sorted(str(name) for name in mesh.get_all_morph_target_names())
    if not REQUIRED_MORPHS.issubset(set(morph_names)):
        raise RuntimeError(f"The UE SkeletalMesh is missing expected morph targets: {morph_names}")
    if int(ue_report.get("triangle_count", 0)) > 1800:
        raise RuntimeError("The approved base egg exceeds the 1800-triangle G0 budget")

    OUT.parent.mkdir(parents=True, exist_ok=True)
    resolved_out = OUT.resolve()
    if not resolved_out.is_relative_to((ROOT / "WebNative").resolve()):
        raise RuntimeError(f"GLB output escaped WebNative: {resolved_out}")

    options = unreal.GLTFExportOptions()
    for name, value in {
        "export_uniform_scale": 0.01,
        "export_vertex_skin_weights": True,
        "export_morph_targets": True,
        "export_animation_sequences": False,
        "export_preview_mesh": False,
        "export_vertex_colors": True,
        "use_mesh_quantization": True,
        "texture_image_format": unreal.GLTFTextureImageFormat.PNG,
    }.items():
        set_option(options, name, value)

    task = unreal.AssetExportTask()
    task.set_editor_properties({
        "object": mesh,
        "filename": str(OUT),
        "automated": True,
        "prompt": False,
        "options": options,
    })
    ok = unreal.Exporter.run_asset_export_task(task)
    if not ok or not OUT.is_file() or OUT.stat().st_size == 0:
        errors = list(task.get_editor_property("errors")) if task.has_editor_property("errors") else []
        raise RuntimeError(f"GLB export failed for {MESH_PATH}: {errors}")

    visual_report = patch_glb_materials_and_validate(OUT, ue_report)

    report = {
        "status": "PASS_UE_GLTF_EXPORT",
        "engine": unreal.SystemLibrary.get_engine_version(),
        "asset": MESH_PATH,
        "approvedSourceBlend": str(SOURCE_BLEND.relative_to(ROOT)).replace("\\", "/"),
        "sourceBlendSha256": sha256(SOURCE_BLEND),
        "sourceFbxSha256": sha256(SOURCE_FBX),
        "ueAssetSha256": sha256(ROOT / "UE5/NaiwaP0/Content/Naiwa/P2/Art/Egg/SK_NaiwaEgg.uasset"),
        "morphTargets": morph_names,
        "triangleCount": int(ue_report["triangle_count"]),
        "materialSlots": [slot["slot_name"] for slot in ue_report["material_slots"]],
        "browserMaterialFallback": visual_report,
        "output": str(OUT.relative_to(ROOT)).replace("\\", "/"),
        "bytes": OUT.stat().st_size,
        "sha256": sha256(OUT),
        "limitations": [
            "UE material graph baking was unavailable in the headless GLTF export; flat PBR colors use the approved UE instance and solid face-material values, while browser appearance remains unverified.",
            "The GLB preserves the mesh morph targets but does not include gameplay-specific ability animation logic.",
        ],
    }
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("WEBNATIVE_BASE_EGG_EXPORT_OK " + json.dumps(report, ensure_ascii=False))


try:
    main()
except Exception:
    error = traceback.format_exc()
    REPORT_PATH.write_text(json.dumps({"status": "FAIL_UE_GLTF_EXPORT", "error": error},
                                      ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log_error("WEBNATIVE_BASE_EGG_EXPORT_FAILED " + error)
    raise
