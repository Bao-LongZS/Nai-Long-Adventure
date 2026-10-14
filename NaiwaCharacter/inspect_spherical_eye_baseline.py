import bpy,json,hashlib,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent
SOURCE=ROOT/'versions/20261005_head_side_continuity/Naiwa_Character.blend'
OUT=ROOT/'versions/20261006_spherical_eyes'
OUT.mkdir(parents=True,exist_ok=True)
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest()=='a95a5c7efb25cb6f379897dd6ec4af98a201bc2619dc8caa2bb5b5de8ea3dfd5'
bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
rig=bpy.data.objects['Naiwa_Rig'];rig.animation_data.action=bpy.data.actions['Naiwa_Idle'];rig.animation_data.use_nla=False
bpy.context.scene.frame_set(1);bpy.context.view_layer.update()
records={}
for side in ('L','R'):
 obj=bpy.data.objects[side+' concentric iris and pupil']
 weights=[[(obj.vertex_groups[g.group].name,g.weight) for g in v.groups] for v in obj.data.vertices]
 unique=sorted(set(tuple(r) for r in weights))
 records[side]={'matrix_world':[list(r) for r in obj.matrix_world],'matrix_local':[list(r) for r in obj.matrix_local],
  'scale':list(obj.scale),'parent':obj.parent.name,'parent_type':obj.parent_type,
  'vertex_count':len(obj.data.vertices),'unique_weight_assignments':unique,
  'modifiers':[{'name':m.name,'type':m.type,'show_viewport':m.show_viewport,'show_render':m.show_render,
       'object':getattr(m,'object',None).name if getattr(m,'object',None) else None} for m in obj.modifiers],
  'color_attributes':[(a.name,a.domain,a.data_type) for a in obj.data.color_attributes],
  'materials':[m.name for m in obj.data.materials]}
(OUT/'source_eye_structure.json').write_text(json.dumps(records,indent=2),encoding='utf-8')
print('SPHERICAL_EYE_BASELINE_OK',json.dumps(records))
