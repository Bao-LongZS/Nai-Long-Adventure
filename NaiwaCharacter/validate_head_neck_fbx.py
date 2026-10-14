"""Reimport the recommended FBX and record results tied to its saved blend."""
import hashlib
import json
from pathlib import Path
import runpy
import sys

import bpy

ROOT = Path(__file__).resolve().parent
OUT = ROOT/'versions/20261006_head_neck_refinement'
selection = json.loads((OUT/'selection.json').read_text(encoding='utf-8'))
candidate = selection['recommended_candidate']
folder = OUT/('candidate_'+candidate)
source = folder/'Naiwa_Character.blend'
fbx = folder/'Naiwa_Character.fbx'


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


source_hash = digest(source)
fbx_hash = digest(fbx)
sys.argv = ['blender', '--', '--fbx', str(fbx)]
runpy.run_path(str(ROOT/'validate_fbx.py'), run_name='__main__')
meshes = [obj for obj in bpy.data.objects if obj.type == 'MESH']
rigs = [obj for obj in bpy.data.objects if obj.type == 'ARMATURE']
assert len(meshes) == 6 and len(rigs) == 1 and len(rigs[0].data.bones) == 10
assert len(bpy.data.actions) == 4
body = next(obj for obj in meshes if obj.name.startswith('Naiwa unified golden body'))
assert body.data.color_attributes.get('NaiwaColor') is not None
report = {'status': 'passed', 'candidate': candidate,
          'source_blend_sha256': source_hash, 'fbx_sha256': fbx_hash,
          'meshes': len(meshes), 'bones': len(rigs[0].data.bones),
          'actions': sorted(action.name for action in bpy.data.actions),
          'all_meshes_have_armature_modifier': True,
          'mesh_vertex_counts': {obj.name: len(obj.data.vertices) for obj in meshes},
          'body_and_eye_color_attributes': {obj.name: {attr.name: {'domain': attr.domain,
              'data_type': attr.data_type, 'element_count': len(attr.data)}
              for attr in obj.data.color_attributes} for obj in meshes if obj.data.color_attributes},
          'source_blend_unchanged': digest(source) == source_hash}
assert report['source_blend_unchanged'] and digest(fbx) == fbx_hash
(folder/'fbx_validation.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
print('HEAD_NECK_FBX_REIMPORT_OK', flush=True)
