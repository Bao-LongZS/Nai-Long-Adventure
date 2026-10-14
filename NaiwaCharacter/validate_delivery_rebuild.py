"""Compare separately loaded saved builds, independently of construction reports."""
import argparse,hashlib,json,sys
from pathlib import Path
import bpy
import numpy as np

root=Path(__file__).resolve().parent
sys.path.insert(0,str(root))
from validate_lip_eye_refinement import coordinates,attribute,weight_hash
from saved_delivery_metadata import preserved_links,differences,scalar
parser=argparse.ArgumentParser()
parser.add_argument('--target',type=Path,required=True)
parser.add_argument('--rebuild',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:])
args.target=args.target.resolve();args.rebuild=args.rebuild.resolve();args.output=args.output.resolve()

def digest(array):return hashlib.sha256(array.tobytes()).hexdigest()

def saved_snapshot(path):
    bpy.ops.wm.open_mainfile(filepath=str(path))
    rig=bpy.data.objects['Naiwa_Rig']
    rig.animation_data.action=bpy.data.actions['Naiwa_Idle'];rig.animation_data.use_nla=False
    bpy.context.scene.frame_set(1);bpy.context.view_layer.update()
    dg=bpy.context.evaluated_depsgraph_get();bone_names=set(rig.data.bones.keys())
    result={'metadata':preserved_links(),'objects':{}}
    for obj in bpy.data.objects:
        if obj.type not in ('MESH','ARMATURE'):continue
        rec={'matrix_world':list(map(list,obj.matrix_world)),
            'custom_properties':{k:scalar(obj[k]) for k in obj.keys()}}
        if obj.type=='MESH':
            rec['coords']=coordinates(obj).copy()
            rec['topology']={name+':'+prop:digest(attribute(getattr(obj.data,name),prop,size,np.int32))
                for name,prop,size in [('edges','vertices',2),('loops','vertex_index',1),
                    ('polygons','loop_start',1),('polygons','loop_total',1),('polygons','material_index',1)]}
            rec['weights']=weight_hash(obj,bone_names)
            rec['all_groups']={str(v.index):sorted((obj.vertex_groups[g.group].name,float(g.weight)) for g in v.groups)
                for v in obj.data.vertices}
            rec['colors']={c.name:{'domain':c.domain,'type':c.data_type,
                'hash':digest(attribute(c.data,'color',4,np.float32))} for c in obj.data.color_attributes}
            rec['UV_layers']={uv.name:digest(attribute(uv.data,'uv',2,np.float32))
                for uv in obj.data.uv_layers}
            rec['shading']={'smooth':[p.use_smooth for p in obj.data.polygons],
                'custom_normals':obj.data.has_custom_normals,'sharp_edges':[e.use_edge_sharp for e in obj.data.edges]}
            evaluated=obj.evaluated_get(dg);mesh=evaluated.to_mesh(preserve_all_data_layers=True,depsgraph=dg)
            rec['evaluated_coords']=attribute(mesh.vertices,'co',3,np.float32).reshape((-1,3)).copy()
            evaluated.to_mesh_clear()
        else:
            rec['bones']={b.name:{'matrix_local':list(map(list,b.matrix_local)),
                'head':list(b.head_local),'tail':list(b.tail_local),'parent':b.parent.name if b.parent else None}
                for b in obj.data.bones}
            rec['pose']={p.name:{'matrix':list(map(list,p.matrix)),'location':list(p.location),
                'rotation_euler':list(p.rotation_euler),'rotation_quaternion':list(p.rotation_quaternion),
                'scale':list(p.scale)} for p in obj.pose.bones}
        result['objects'][obj.name]=rec
    return result

target=saved_snapshot(args.target);rebuilt=saved_snapshot(args.rebuild)
metadata_differences=differences(target['metadata'],rebuilt['metadata'])
assert not metadata_differences,('Saved relationships/materials/actions differ',metadata_differences[:8])
assert target['objects'].keys()==rebuilt['objects'].keys(),'Saved mesh/rig object set differs'
report={'independent_validation':'passed','target':str(args.target),'rebuild':str(args.rebuild),
    'target_sha256':hashlib.sha256(args.target.read_bytes()).hexdigest(),
    'rebuild_sha256':hashlib.sha256(args.rebuild.read_bytes()).hexdigest(),
    'separate_saved_file_loads':True,'pose':'Naiwa_Idle frame 1',
    'parent_links_material_settings_modifier_settings_animation_curves_exact':True,'objects':{}}
for name,a in target['objects'].items():
    b=rebuilt['objects'][name]
    coords=a.pop('coords',None);other=b.pop('coords',None)
    evaluated=a.pop('evaluated_coords',None);other_evaluated=b.pop('evaluated_coords',None)
    object_differences=differences(a,b)
    assert not object_differences,('Saved settings/topology/weights/colors differ',name,object_differences[:8])
    if coords is not None:
        assert coords.shape==other.shape and np.array_equal(coords,other),('Geometry is not exactly reproduced',name)
        assert evaluated.shape==other_evaluated.shape and np.array_equal(evaluated,other_evaluated),('Idle evaluated geometry differs',name)
        report['objects'][name]={'vertex_count':len(coords),'coordinate_maximum_error':0.,
            'coordinates_exact':True,'Idle_frame1_evaluated_coordinates_exact':True,
            'all_vertex_group_weights_exact':True,'weights_exact':True,
            'UV_layers_exact':True,
            'object_transform_exact':True,'topology_and_material_assignments_exact':True,
            'colors':{n:'exact' for n in a['colors']},'shading_and_custom_properties_exact':True}
    else:
        report['objects'][name]={'bones_exact':True,'bone_count':len(a['bones']),
            'Idle_frame1_pose_exact':True,'object_transform_exact':True}
report['frontal_body_and_head_geometry_exact']=True
args.output.write_text(json.dumps(report,indent=2),encoding='utf8')
print('DELIVERY_REBUILD_INDEPENDENT_MATCH_OK')
