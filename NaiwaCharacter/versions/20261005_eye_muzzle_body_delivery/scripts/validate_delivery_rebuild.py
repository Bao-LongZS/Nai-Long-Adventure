"""Compare two saved delivery builds, independently of construction reports."""
import bpy,sys,argparse,json,hashlib
from pathlib import Path
import numpy as np

root=Path(__file__).resolve().parent
sys.path.insert(0,str(root))
from validate_lip_eye_refinement import coordinates,attribute,weight_hash,topology
parser=argparse.ArgumentParser()
parser.add_argument('--target',type=Path,required=True)
parser.add_argument('--rebuild',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:])
bpy.ops.wm.open_mainfile(filepath=str(args.target))
names=['Naiwa unified golden body','Head and torso silhouette only','L concentric iris and pupil',
       'R concentric iris and pupil','L eye glint','R eye glint','Short closed oral seam','Naiwa_Rig']
target={name:bpy.data.objects[name] for name in names}
with bpy.data.libraries.load(str(args.rebuild),link=False) as (source,loaded):
    assert all(name in source.objects for name in names)
    loaded.objects=list(names)
rebuilt=dict(zip(names,loaded.objects))
collection=bpy.data.collections.new('Independent rebuilt delivery comparison')
bpy.context.scene.collection.children.link(collection)
for obj in rebuilt.values():
    collection.objects.link(obj)
    obj.hide_render=True
bpy.context.scene.frame_set(1)
bpy.context.view_layer.update()
report={'independent_validation':'passed','target':str(args.target),'rebuild':str(args.rebuild),'objects':{}}
bones=set(target['Naiwa_Rig'].data.bones.keys())
for name in names:
    a,b=target[name],rebuilt[name]
    assert a.matrix_world==b.matrix_world,('Object transform differs',name)
    if a.type=='MESH':
        topology(a,b)
        ac,bc=coordinates(a),coordinates(b)
        error=float(np.max(np.abs(ac-bc)))
        assert np.array_equal(ac,bc),('Geometry is not exactly reproduced',name,error)
        assert weight_hash(a,bones)==weight_hash(b,bones),('Skin weights differ',name)
        colors={}
        for ca in a.data.color_attributes:
            cb=b.data.color_attributes.get(ca.name)
            assert cb is not None and ca.domain==cb.domain and ca.data_type==cb.data_type
            x=attribute(ca.data,'color',4,np.float32);y=attribute(cb.data,'color',4,np.float32)
            assert np.array_equal(x,y),('Vertex colors differ',name,ca.name)
            colors[ca.name]='exact'
        report['objects'][name]={'vertex_count':len(ac),'coordinate_maximum_error':error,
                                 'coordinates_exact':True,'weights_exact':True,'colors':colors}
    else:
        assert bones==set(b.data.bones.keys())
        for bone in bones:assert a.data.bones[bone].matrix_local==b.data.bones[bone].matrix_local
        report['objects'][name]={'bones_exact':True,'bone_count':len(bones)}
report['frontal_body_and_head_geometry_exact']=True
args.output.write_text(json.dumps(report,indent=2),encoding='utf8')
print('DELIVERY_REBUILD_INDEPENDENT_MATCH_OK')
