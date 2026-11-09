"""Read-only Blender export helper for the G0 GLB asset probe.

Run with:
  blender --background --python tools/export-glb.py -- --input model.blend --output model.glb
"""

import argparse
import json
import pathlib
import sys
import traceback

import bpy


def parse_args():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    return parser.parse_args(args)


def main():
    args = parse_args()
    source = args.input.resolve()
    output = args.output.resolve()
    if not source.is_file():
        raise FileNotFoundError(source)
    output.parent.mkdir(parents=True, exist_ok=True)

    bpy.ops.wm.open_mainfile(filepath=str(source))
    bpy.ops.object.select_all(action="DESELECT")
    exportable = [obj for obj in bpy.context.scene.objects if obj.type in {"MESH", "ARMATURE"}]
    if not exportable:
        raise RuntimeError("No mesh or armature objects found in the source scene")
    for obj in exportable:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = next(
        (obj for obj in exportable if obj.type == "ARMATURE"), exportable[0]
    )

    # Only the destination is written. The opened .blend file is never saved.
    bpy.ops.export_scene.gltf(
        filepath=str(output),
        export_format="GLB",
        use_selection=True,
    )
    if not output.is_file() or output.stat().st_size == 0:
        raise RuntimeError("Blender did not produce a non-empty GLB")

    print(json.dumps({
        "status": "PASS_GLTF_EXPORT",
        "source": str(source),
        "output": str(output),
        "output_bytes": output.stat().st_size,
        "blender_version": bpy.app.version_string,
        "exported_objects": [obj.name for obj in exportable],
        "animations": [action.name for action in bpy.data.actions],
    }, ensure_ascii=False))


try:
    main()
except Exception:
    print(json.dumps({"status": "FAIL_GLTF_EXPORT", "error": traceback.format_exc()}, ensure_ascii=False))
    raise
