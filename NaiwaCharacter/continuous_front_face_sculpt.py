"""A broad continuous foreface replaces the narrow lip and central upper bump."""
import bpy
import numpy as np
from mathutils import Vector
from longitudinal_surface_sculpt import coordinates,ease
from short_round_face_sculpt import profiles
from neck_direction_sculpt import quintic

LOWER_Z=2.20
UPPER_Z=2.46
MOUTH_Z=2.365

def lateral_field(obj):
    zs=np.linspace(2.16,2.48,161)
    xs=np.linspace(0,.19,39)
    field=np.empty((len(zs),len(xs)))
    for i,z in enumerate(zs):
        for j,x in enumerate(xs):
            ys=[]
            for xx in ((float(x),) if x==0 else (-float(x),float(x))):
                hit,p,_,_=obj.ray_cast(Vector((xx,-2,float(z))),Vector((0,1,0)))
                assert hit
                ys.append(p.y)
            field[i,j]=sum(ys)/len(ys)
    offset=field-field[:,0,None]
    d2=np.diff(np.eye(len(zs)),n=2,axis=0)
    weights=np.ones(len(zs)); weights[:3]=weights[-3:]=300
    fair=np.linalg.solve(np.diag(weights)+(.055/(zs[1]-zs[0]))**4*(d2.T@d2),weights[:,None]*offset)
    return zs,xs,fair-offset

def sample_field(z,x,zs,xs,field):
    a=np.clip((z-zs[0])/(zs[1]-zs[0]),0,len(zs)-1.000001)
    b=np.clip((x-xs[0])/(xs[1]-xs[0]),0,len(xs)-1.000001)
    i=a.astype(int); j=b.astype(int)
    def weights(t):
        return ((1-t)**3/6,(3*t**3-6*t*t+4)/6,(-3*t**3+3*t*t+3*t+1)/6,t**3/6)
    wz=weights(a-i); wx=weights(b-j)
    result=np.zeros(len(z))
    for k in range(4):
        for l in range(4):
            result+=wz[k]*wx[l]*field[np.clip(i+k-1,0,len(zs)-1),np.clip(j+l-1,0,len(xs)-1)]
    return result

def reshape(obj):
    old=coordinates(obj)
    zs,rows,at,jets=profiles(obj)
    f0,df0,ddf0=jets(LOWER_Z,1)
    f1,df1,ddf1=jets(UPPER_Z,1)
    middle=2.32
    fm=at(middle,1)+.001
    mouth=at(2.35949,1)+.0055
    def goal(z):
        return np.where(z<=middle,
            quintic(z,LOWER_Z,middle,f0,fm,df0,-.19,ddf0,0),
            np.where(z<=MOUTH_Z,
                quintic(z,middle,MOUTH_Z,fm,mouth,-.19,0,0,2.5),
                quintic(z,MOUTH_Z,UPPER_Z,mouth,f1,0,df1,2.5,ddf1)))
    samples=np.linspace(LOWER_Z,UPPER_Z,501)
    targets=goal(samples)
    assert np.min(np.diff(targets[samples>=MOUTH_Z]))>-.00002,'Upper-face goal forms another nose peak'
    assert np.max(np.diff(targets[samples<=MOUTH_Z]))<.00002,'Lower-face goal forms a separate lip'
    z=old[:,2].astype(float); x=np.abs(old[:,0]).astype(float)
    sf=np.interp(z,zs,rows[:,1]); sr=np.interp(z,zs,rows[:,2]); sw=np.interp(z,zs,rows[:,0])
    theta=np.arctan2((old[:,1]-(sf+sr)/2)/((sr-sf)/2),old[:,0]/sw)
    anterior=ease(.10,.80,np.maximum(0,-np.sin(theta)))
    cheeks=1-ease(.065,.19,x)
    height=ease(LOWER_Z,LOWER_Z+.035,z)*(1-ease(UPPER_Z-.025,UPPER_Z,z))
    fz,fx,delta_field=lateral_field(obj)
    lateral_delta=sample_field(z,x,fz,fx,delta_field)*ease(0,.02,x)
    delta=goal(np.clip(z,LOWER_Z,UPPER_Z))-sf+lateral_delta
    new=old.copy()
    active=(z>LOWER_Z)&(z<UPPER_Z)&(anterior>0)&(cheeks>0)
    new[active,1]+=(delta*anterior*cheeks*height)[active]
    assert np.array_equal(old[:,[0,2]],new[:,[0,2]])
    obj.data.vertices.foreach_set('co',new.reshape(-1)); obj.data.update()
    return {'modified_vertices':int(np.count_nonzero(np.any(old!=new,axis=1))),
        'maximum_displacement':float(np.max(np.linalg.norm(new-old,axis=1))),
        'lower_z':LOWER_Z,'upper_z':UPPER_Z,'mouth_peak_z':MOUTH_Z,
        'local_lip_bump_removed':True,'all_x_z_exact':True}

def skin_depths(body,coords):
    result=np.empty(len(coords))
    for i,v in enumerate(coords):
        hit,p,_,_=body.ray_cast(Vector((float(v[0]),-2,float(v[2]))),Vector((0,1,0)))
        assert hit,'Eye attachment must remain on the head surface'
        result[i]=p.y
    return result

def reattach_eye(eye,body,old,old_skin):
    new_skin=skin_depths(body,old)
    new=old.copy()
    new[:,1]+=new_skin-old_skin
    assert np.array_equal(old[:,[0,2]],new[:,[0,2]])
    eye.data.vertices.foreach_set('co',new.reshape(-1)); eye.data.update()
    pupil=eye['frontal_pupil_center']
    eye['frontal_pupil_center']=(pupil[0],float(new[0,1]),pupil[2])
    return {'all_x_z_exact':True,'maximum_y_reattachment':float(np.max(np.abs(new[:,1]-old[:,1]))),
        'round_frontal_outline_preserved':True,'color_and_topology_preserved':True}

def refit_seam(body):
    mouth=bpy.data.objects['Short closed oral seam']
    for v in mouth.data.vertices:
        hit,p,_,_=body.ray_cast(Vector((v.co.x,-2,v.co.z)),Vector((0,1,0)))
        assert hit
        v.co.y=p.y-.00045
    mouth.data.update()

def sculpt_continuous_front_face(body,shell=None):
    assert not body.get('continuous_front_face_refined'),'Read the saved preceding iteration.'
    attachments={}
    for side in ('L','R'):
        eye=bpy.data.objects[side+' concentric iris and pupil']
        old=coordinates(eye)
        attachments[side]=(old,skin_depths(body,old))
    report=reshape(body)
    if shell is not None: reshape(shell)
    report['eyes']={}
    for side,(old,skin) in attachments.items():
        eye=bpy.data.objects[side+' concentric iris and pupil']
        glint=bpy.data.objects[side+' eye glint']
        gx,gz=glint.location.x,glint.location.z
        hit,old_glint_surface,_,_=eye.ray_cast(Vector((gx,-2,gz)),Vector((0,1,0)))
        assert hit
        report['eyes'][side]=reattach_eye(eye,body,old,skin)
        hit,new_glint_surface,_,_=eye.ray_cast(Vector((gx,-2,gz)),Vector((0,1,0)))
        assert hit
        glint.location.y+=new_glint_surface.y-old_glint_surface.y
    refit_seam(body)
    body['continuous_front_face_refined']=True
    body['head_revision']='Continuous shallow foreface without rolled lips or a central upper bump'
    print('CONTINUOUS_FRONT_FACE_SCULPT_OK',report)
    return report
