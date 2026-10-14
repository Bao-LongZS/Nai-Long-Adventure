"""Independent dimensional, binding and mesh checks for the saved revision."""
import bpy,sys,json,math
import numpy as np
from pathlib import Path
from mathutils import Vector
OUT=Path(__file__).resolve().parent;sys.path.insert(0,str(OUT))
from validate_lip_eye_refinement import coordinates,attribute,topology,weight_hash,surface_integrity,mouth_gaps
from eye_projection_measure import visible_metrics
BASE=OUT/'iterations/before_eye_muzzle_body_revision'
TARGET=OUT/'versions/20261005_eye_muzzle_body/Naiwa_Character.blend'
if '--draft' in sys.argv:TARGET=OUT/'iterations/eye_muzzle_body_draft/Naiwa_Character.blend'
if '--target' in sys.argv:TARGET=Path(sys.argv[sys.argv.index('--target')+1]).resolve()
rec=json.loads((BASE/'baseline_dimensions.json').read_text(encoding='utf8'));H=rec['height'];D=rec['D']
names=['Naiwa unified golden body','Head and torso silhouette only','L concentric iris and pupil','R concentric iris and pupil','Short closed oral seam','Naiwa_Rig']
bpy.ops.wm.open_mainfile(filepath=str(TARGET));cur=[bpy.data.objects[n] for n in names]
action_names=set(bpy.data.actions.keys())
with bpy.data.libraries.load(str(BASE/'Naiwa_Character.blend'),link=False) as (s,t):t.objects=names
old=t.objects
collection=bpy.data.collections.new('Read-only fixed baseline verification');bpy.context.scene.collection.children.link(collection)
for o in old:collection.objects.link(o);o.hide_render=True
bpy.context.scene.frame_set(1);bpy.context.view_layer.update()
before=coordinates(old[0]);after=coordinates(cur[0]);body=cur[0]
assert abs(float(np.ptp(after[:,2]))-H)<1e-6,'Total height changed'
assert np.array_equal(before[:,0],after[:,0]),'Shoulder width/head width/limb X coordinates changed'
assert np.array_equal(before[before[:,2]<1.64],after[before[:,2]<1.64]),'Abdomen/legs/feet geometry changed'
abdomen=(before[:,2]<1.90)&(before[:,2]>.7)&((np.abs(before[:,0])<.28)|(before[:,1]<-.34))
assert np.array_equal(before[abdomen],after[abdomen]),'Belly volume changed'
assert np.array_equal(before[before[:,2]>=2.638],after[before[:,2]>=2.638]),'Crown moved'
assert np.max(after[:,2]-before[:,2])<=.4*D+1e-6,'Face raised more than specified'
for a,b in zip(old[:4],cur[:4]):
    topology(a,b)
    assert a.matrix_world==b.matrix_world,'Object transform changed'
rig,oldrig=cur[-1],old[-1];bones=set(rig.data.bones.keys())
assert bones==set(oldrig.data.bones.keys()) and len(bones)==10
for n in bones:assert rig.data.bones[n].matrix_local==oldrig.data.bones[n].matrix_local
assert {'Naiwa_Idle','Naiwa_Walk','Naiwa_Laugh','Naiwa_Jump'}<=action_names
for a,b in zip(old[:4],cur[:4]):assert weight_hash(a,bones)==weight_hash(b,bones),'Binding weights changed'
eyes={}
for side,a,b in [('L',old[2],cur[2]),('R',old[3],cur[3])]:
    color0=attribute(a.data.color_attributes['EyeColor'].data,'color',4,np.float32)
    color1=attribute(b.data.color_attributes['EyeColor'].data,'color',4,np.float32)
    assert np.array_equal(color0,color1),'Eye palette or pupil/green face assignment changed'
    cz=np.array(b['frontal_aperture_center']);oldcz=np.array(a['frontal_aperture_center'])
    assert np.max(np.abs(cz-oldcz-np.array([0,.4*D])))<1e-6,'Eye layout/raise incorrect'
    metric=visible_metrics(b,body,step=.0004)
    diameter=2*math.sqrt(metric['aperture']['area']/math.pi)
    pupil=2*math.sqrt(metric['black']['area']/math.pi)
    assert abs(diameter-D)<.015*D,('Outer eye diameter changed',side,diameter,D)
    assert abs(pupil-.53*D)<.008*D,('Pupil diameter incorrect',side,pupil)
    assert metric['aperture']['axis_ratio']<1.025 and metric['black']['axis_ratio']<1.025,'Frontal eye/pupil is not circular'
    xyz=coordinates(b);gaps=mouth_gaps(body,xyz)
    projection=float(np.max(gaps))
    assert abs(projection-.20*D)<.003*D,('Eye final projection incorrect',side,projection)
    assert gaps[-1]<-.08,'Eye root volume is not buried'
    forehead=(float(after[:,2].max())-cz[1])/D
    assert 1.3<=forehead<=1.4,('Forehead remains too long',side,forehead)
    eyes[side]={'actual_outer_diameter':diameter,'actual_pupil_diameter':pupil,'actual_maximum_F_projection':projection,
        'forehead_in_D':forehead,'center_X_Z':cz.tolist(),'visible_metrics':metric,'integrity':surface_integrity(b)}
seam=cur[4];mouth=coordinates(seam);width=float(np.ptp(mouth[:,0]));mid=mouth[np.abs(mouth[:,0])<1e-7]
opening=float(np.ptp(mid[:,2]));mouth_z=float(np.mean(mid[:,2]))
assert abs(width-1.25*D)<1e-6 and abs(opening-.06*D)<1e-6,'Mouth dimensions incorrect'
assert abs(mouth_z-(rec['mouth_center'][2]+.4*D))<1e-6,'Mouth did not rise with the eyes'
front_count=len(mouth)//2;gaps=mouth_gaps(body,mouth[:front_count])
assert gaps.min()>0 and gaps.max()<.001,'Mouth front floats or is hidden behind skin'
midface=[]
for p in list(seam.data.polygons)[:(127-1)*16+32]:
    xyz=np.mean(mouth[list(p.vertices)],axis=0)
    midface.append(xyz)
mgaps=mouth_gaps(body,np.array(midface))
assert mgaps.min()>0 and mgaps.max()<.001,'Mouth triangles cross the skin or float'
assert all(v.groups for v in seam.data.vertices),'Mouth cavity has unbound vertices'
report={'independent_validation':'passed','baseline':str(BASE),'target':str(TARGET),'H_before':H,'H_after':float(np.ptp(after[:,2])),
    'D_fixed':D,'body_X_exact':True,'crown_exact':True,'belly_volume_and_legs_feet_exact':True,
    'standing_pose_bones_weights_and_actions_preserved':True,'body_changed_vertices':int(np.count_nonzero(np.any(before!=after,axis=1))),
    'face_Z_maximum':float(np.max(after[:,2]-before[:,2])),'eyes':eyes,
    'mouth':{'width':width,'central_opening':opening,'center_z':mouth_z,'skin_gap_range':[float(gaps.min()),float(gaps.max())],
        'face_center_gap_range':[float(mgaps.min()),float(mgaps.max())],'closed_cavity_integrity':surface_integrity(seam)},
    'body_integrity':surface_integrity(body,z_range=(1.63,2.644)),
    'shell_integrity':surface_integrity(cur[1],z_range=(2.165,2.644))}
colors=attribute(body.data.color_attributes['NaiwaColor'].data,'color',4,np.float32).reshape(-1,4)
loopids=attribute(body.data.loops,'vertex_index',1,np.int32);xyz=after[loopids]
foot=(xyz[:,2]<.19)&(np.abs(xyz[:,0])>.17)&(np.abs(xyz[:,0])<.55)
assert np.max(np.abs(colors[foot,:3]-np.array([.19,.115,.065])))<1e-6,'Foot palette incomplete'
report['entire_feet_brown']=True
(TARGET.parent/'eye_muzzle_validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
print('EYE_MUZZLE_INDEPENDENT_VALIDATION_OK',json.dumps({k:v for k,v in report.items() if k!='eyes'}))
