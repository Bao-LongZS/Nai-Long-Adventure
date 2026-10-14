"""Decode Blender FBX sRGB vertex colors in gameplay materials; retain E2 assets."""
import json
import pathlib
import traceback
import unreal

ROOT = pathlib.Path(__file__).resolve().parents[2]
report = {'phase': 'gameplay_color_decode', 'engine': unreal.SystemLibrary.get_engine_version(), 'materials': []}
CODE = '''return float3(
ColorSRGB.r <= 0.04045 ? ColorSRGB.r / 12.92 : pow((ColorSRGB.r + 0.055) / 1.055, 2.4),
ColorSRGB.g <= 0.04045 ? ColorSRGB.g / 12.92 : pow((ColorSRGB.g + 0.055) / 1.055, 2.4),
ColorSRGB.b <= 0.04045 ? ColorSRGB.b / 12.92 : pow((ColorSRGB.b + 0.055) / 1.055, 2.4));'''

try:
    assets = unreal.AssetToolsHelpers.get_asset_tools()
    folder = '/Game/Naiwa/Gameplay/Materials'
    materials = {}
    for key, roughness in (('Body', 0.65), ('Eye', 0.4)):
        name = 'M_Naiwa' + key + '_Gameplay'
        mat = unreal.load_asset(folder + '/' + name) or assets.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
        unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
        vertex = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -600, 0)
        decode = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionCustom, -300, 0)
        color_input = unreal.CustomInput()
        color_input.set_editor_property('input_name', 'ColorSRGB')
        decode.set_editor_properties({'code': CODE, 'output_type': unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                                     'description': 'Blender FBX sRGB vertex colors to linear BaseColor',
                                     'inputs': [color_input]})
        assert unreal.MaterialEditingLibrary.connect_material_expressions(vertex, '', decode, 'ColorSRGB')
        assert unreal.MaterialEditingLibrary.connect_material_property(decode, '', unreal.MaterialProperty.MP_BASE_COLOR)
        for value, prop in ((roughness, unreal.MaterialProperty.MP_ROUGHNESS), (0.2, unreal.MaterialProperty.MP_SPECULAR)):
            scalar = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 200)
            scalar.set_editor_property('r', value)
            assert unreal.MaterialEditingLibrary.connect_material_property(scalar, '', prop)
        unreal.MaterialEditingLibrary.set_base_material_usage(mat, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH, True)
        # The game mesh has belly morphs. Persist their shader usage so the first
        # animated frame does not substitute the default material or recompile.
        unreal.MaterialEditingLibrary.set_base_material_usage(mat, unreal.MaterialUsage.MATUSAGE_MORPH_TARGETS, True)
        errors = unreal.MaterialEditingLibrary.recompile_material(mat)
        assert not errors, str(errors)
        materials[key] = mat
        report['materials'].append({'path': mat.get_path_name(), 'shader': CODE, 'roughness': roughness,
                                    'skeletal_mesh_usage': True, 'morph_targets_usage': True})
    mesh = unreal.load_asset('/Game/Naiwa/Gameplay/SK_Naiwa_Elastic')
    slots = mesh.get_editor_property('materials')
    for index, slot in enumerate(slots):
        name = str(slot.get_editor_property('material_slot_name')).casefold()
        if 'eye surface' in name:
            slot.set_editor_property('material_interface', materials['Eye'])
        elif 'glint' not in name and 'mouth' not in name:
            slot.set_editor_property('material_interface', materials['Body'])
        slots[index] = slot
    mesh.set_editor_property('materials', slots)
    assert unreal.EditorAssetLibrary.save_directory('/Game/Naiwa/Gameplay', only_if_is_dirty=False, recursive=True)
    report['material_slots'] = [{'slot': str(slot.get_editor_property('material_slot_name')),
                                'material': slot.get_editor_property('material_interface').get_path_name()} for slot in slots]
    report['color_evidence'] = 'run_export_diagnostics.md: Blender FBX exports sRGB; UE imports normalized bytes and VertexColor passes them to a linear material input.'
    report['status'] = 'saved_pending_rendered_visual_review'
    unreal.log('NAIWA_GAMEPLAY_COLOR_CONFIGURED')
except Exception:
    report['status'] = 'failed'
    report['error'] = traceback.format_exc()
    unreal.log_error(report['error'])
finally:
    (ROOT / 'UE5/Records/gameplay_visuals.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
if report['status'] == 'failed':
    raise RuntimeError(report['error'])
