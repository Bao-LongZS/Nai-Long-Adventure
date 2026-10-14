"""Redistribute face/neck volume with continuous sectional curves and fixed heights."""
import bpy, math
import numpy as np
from mathutils import Vector
from connection_sculpt import section
from longitudinal_surface_sculpt import coordinates,ease
from neck_direction_sculpt import quintic
from head_volume_sculpt import face_surface

ROOT_Z=1.98
WIDTH_JOIN_Z=2.425
FRONT_JOIN_Z=2.48
REAR_JOIN_Z=2.54
THROAT_Z=2.185
MOUTH_Z=2.359
FACE_FORWARD=0.037

def profiles(obj):
    zs=np.linspace(1.94,2.60,331)
    data=np.array([section(obj,float(z)) for z in zs])
    def at(z,k): return float(np.interp(z,zs,data[:,k]))
    def jets(z,k):
        mask=np.abs(zs-z)<0.024
        aa,bb,cc=np.polyfit(zs[mask]-z,data[mask,k],2)
        return at(z,k),float(bb),float(2*aa)
    return zs,data,at,jets

def reshape(obj):
    old=coordinates(obj); zs,source,at,jets=profiles(obj)
    w0,dw0,ddw0=jets(ROOT_Z,0)
    w1,dw1,ddw1=jets(WIDTH_JOIN_Z,0)
    turn_z=2.275; turn_w=0.279
    def width(z):
        if z<=ROOT_Z or z>=WIDTH_JOIN_Z: return at(z,0)
        if z<=turn_z: return quintic(z,ROOT_Z,turn_z,w0,turn_w,dw0,-0.13,ddw0,0)
        return quintic(z,turn_z,WIDTH_JOIN_Z,turn_w,w1,-0.13,dw1,0,ddw1)
    f0,df0,ddf0=jets(ROOT_Z,1)
    f1,df1,ddf1=jets(FRONT_JOIN_Z,1)
    throat=at(THROAT_Z,1)+0.004
    mouth=at(MOUTH_Z,1)-FACE_FORWARD
    def front(z):
        if z<=ROOT_Z or z>=FRONT_JOIN_Z: return at(z,1)
        if z<=THROAT_Z: return quintic(z,ROOT_Z,THROAT_Z,f0,throat,df0,0,ddf0,-3.8)
        if z<=MOUTH_Z: return quintic(z,THROAT_Z,MOUTH_Z,throat,mouth,0,0,-3.8,7.0)
        return quintic(z,MOUTH_Z,FRONT_JOIN_Z,mouth,f1,0,df1,7.0,ddf1)
    r0,dr0,ddr0=jets(ROOT_Z,2)
    r1,dr1,ddr1=jets(REAR_JOIN_Z,2)
    def rear(z):
        if z<=ROOT_Z or z>=REAR_JOIN_Z: return at(z,2)
        return quintic(z,ROOT_Z,REAR_JOIN_Z,r0,r1,dr0,dr1,ddr0,ddr1)
    sample=np.linspace(ROOT_Z,2.56,581)
    goals=np.array([[width(z),front(z),rear(z)] for z in sample])
    assert np.all(np.diff(goals[:,0])<0.0001),'Width goal reverses into a neck waist'
    assert np.all(np.diff(goals[:,2])<0.0001),'Nape goal contains a bulge'
    z=old[:,2].astype(np.float64)
    sw=np.interp(z,zs,source[:,0]); sf=np.interp(z,zs,source[:,1]); sr=np.interp(z,zs,source[:,2])
    tw=np.interp(z,sample,goals[:,0]); tf=np.interp(z,sample,goals[:,1]); tr=np.interp(z,sample,goals[:,2])
    centre=(sf+sr)/2; depth=(sr-sf)/2
    theta=np.arctan2((old[:,1]-centre)/depth,old[:,0]/sw)
    s=np.sin(theta)
    anterior=np.maximum(0,-s)**2
    posterior=np.maximum(0,s)**2
    shoulder=ease(0.29,0.395,np.abs(old[:,0]))*ease(-0.20,-0.12,old[:,1])*(1-ease(0.19,0.27,old[:,1]))
    feather=1-shoulder*(1-ease(2.09,2.205,z))
    new=old.copy()
    wx=(z>ROOT_Z)&(z<WIDTH_JOIN_Z)
    fy=(z>ROOT_Z)&(z<FRONT_JOIN_Z)
    ry=(z>ROOT_Z)&(z<REAR_JOIN_Z)
    new[wx,0]=old[wx,0]*(1+(tw[wx]/sw[wx]-1)*feather[wx])
    delta=(tf-sf)*anterior*fy+(tr-sr)*posterior*ry
    new[:,1]=old[:,1]+delta*feather
    protected=(z<=ROOT_Z)|((z<=2.09)&(np.abs(old[:,0])>=0.395)&(old[:,1]>=-0.12)&(old[:,1]<=0.19))
    new[protected]=old[protected]
    assert np.array_equal(new[:,2],old[:,2])
    obj.data.vertices.foreach_set('co',new.reshape(-1)); obj.data.update()
    return {'modified_vertices':int(np.count_nonzero(np.any(new!=old,axis=1))),
            'maximum_displacement':float(np.max(np.linalg.norm(new-old,axis=1))),
            'all_heights_exact':True,'root_z':ROOT_Z,'width_join_z':WIDTH_JOIN_Z,
            'front_join_z':FRONT_JOIN_Z,'rear_join_z':REAR_JOIN_Z,
            'front_face_extension':FACE_FORWARD,'throat_z':THROAT_Z},sample,goals

def refit_features(body, seam_height_scale=0.55):
    # Rigidly fit each sphere to its new tangent plane. Keep the existing
    # projected pupil X/Z layout while preserving globe and iris dimensions.
    eye_report={}
    for side in ('L','R'):
        eye=bpy.data.objects[side+' concentric iris and pupil']
        anchor=Vector(eye['head_reduction_anchor'])
        p,normal=face_surface(body,anchor.x,anchor.z)
        old_center=(eye.data.vertices[0].co+eye.data.vertices[-1].co)/2
        radius=(eye.data.vertices[0].co-old_center).length
        old_pupil=Vector(eye['frontal_pupil_center'])
        old_axis=(old_pupil-old_center).normalized()
        center=p-normal*(radius-float(eye.get('exposed_cap_height',0.0187)))
        pupil_x,pupil_z=old_pupil.x,old_pupil.z
        pupil_y=center.y-math.sqrt(radius**2-(pupil_x-center.x)**2-(pupil_z-center.z)**2)
        pupil=Vector((pupil_x,pupil_y,pupil_z))
        rotation=old_axis.rotation_difference((pupil-center).normalized())
        for v in eye.data.vertices: v.co=center+rotation@(v.co-old_center)
        eye.data.update()
        glint=bpy.data.objects[side+' eye glint']
        glint.location=center+rotation@(glint.location-old_center)
        glint.rotation_euler=(rotation@glint.rotation_euler.to_quaternion()).to_euler()
        anchor.y=p.y; eye['head_reduction_anchor']=tuple(anchor)
        eye['frontal_pupil_center']=tuple(pupil)
        eye_report[side]={'old_center':list(old_center),'new_center':list(center),'radius':radius,
                          'pupil_x_z_preserved':True,'rigid_rotation':list(rotation)}
    mouth=bpy.data.objects['Short closed oral seam']
    mouth_z=sum(v.co.z for v in mouth.data.vertices)/len(mouth.data.vertices)
    for v in mouth.data.vertices:
        v.co.z=mouth_z+(v.co.z-mouth_z)*seam_height_scale
        hit,p,_,_=body.ray_cast(Vector((v.co.x,-2,v.co.z)),Vector((0,1,0)))
        assert hit; v.co.y=p.y-0.0007
    mouth.data.update()
    return eye_report

def sculpt_short_round_face(body,shell=None,fit_features=True):
    assert not body.get('short_round_face_reshaped'),'Start from the saved preceding iteration.'
    report,zs,goals=reshape(body)
    if shell is not None: reshape(shell)
    if fit_features: report['eyes']=refit_features(body)
    for p in body.data.polygons: p.use_smooth=True
    body.data.update()
    body['short_round_face_reshaped']=True
    body['head_revision']='Short rounded face, receding throat and continuous nape with curved chest flare'
    body['side_projection_locked']=False
    body['short_face_extension']=FACE_FORWARD
    body['neck_surface_root_z']=ROOT_Z
    print('SHORT_ROUND_FACE_SCULPT_OK',report)
    return report
