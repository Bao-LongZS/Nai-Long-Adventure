"""Read saved viewport and pupil shader settings without saving the project."""
import json
from pathlib import Path
import bpy

ROOT=Path(__file__).resolve().parent
TARGET=ROOT/'versions/20261005_head_side_continuity/Naiwa_Character.blend'
bpy.ops.wm.open_mainfile(filepath=str(TARGET))
eyes={}
for side in ('L','R'):
    obj=bpy.data.objects[side+' concentric iris and pupil']
    ca=obj.data.color_attributes.get('EyeColor')
    black=sum(1 for c in ca.data if c.color[1]<0.04)
    slots=[]
    for mat in obj.data.materials:
        slots.append({'name':mat.name,'diffuse_color':list(mat.diffuse_color),'use_nodes':mat.use_nodes,
                      'nodes':[{'type':n.bl_idname,'name':n.name,'attribute_name':getattr(n,'attribute_name',None)}
                               for n in mat.node_tree.nodes] if mat.node_tree else [],
                      'links':[(l.from_node.name,l.from_socket.name,l.to_node.name,l.to_socket.name)
                               for l in mat.node_tree.links] if mat.node_tree else []})
    eyes[side]={'hide_render':obj.hide_render,'hide_viewport':obj.hide_viewport,
                'EyeColor_domain':ca.domain,'black_color_corner_count':black,'materials':slots}
screens=[]
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type=='VIEW_3D':
            space=area.spaces.active
            screens.append({'screen':screen.name,'shading_type':space.shading.type,
                            'color_type':space.shading.color_type,'show_overlays':space.overlay.show_overlays})
report={'target':str(TARGET),'render_engine':bpy.context.scene.render.engine,
        'view_layer_overrides':{layer.name:(layer.material_override.name if layer.material_override else None)
                                for layer in bpy.context.scene.view_layers},
        'eyes':eyes,'viewports':screens}
output=ROOT/'pupil_display_diagnosis.json'
output.write_text(json.dumps(report,indent=2),encoding='utf-8')
print('PUPIL_DISPLAY_INSPECTION_OK',json.dumps(report))
