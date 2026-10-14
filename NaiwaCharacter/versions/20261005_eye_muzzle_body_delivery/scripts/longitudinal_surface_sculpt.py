"""Fair the full cheek/neck thickness field while locking the side projection."""
import math
import bpy
import numpy as np
from mathutils import Vector
from connection_sculpt import section

MIN_Z = 1.98
MAX_Z = 2.585
FAIR_LENGTH = 0.060

def ease(a,b,x):
    t=np.clip((x-a)/(b-a),0,1)
    return t*t*t*(10+t*(-15+6*t))

def coordinates(obj):
    data=np.empty(len(obj.data.vertices)*3,dtype=np.float32)
    obj.data.vertices.foreach_get('co',data)
    return data.reshape((-1,3))

def thickness_field(obj):
    zs=np.linspace(1.95,2.61,221)
    us=np.linspace(-0.985,0.985,81)
    sections=np.array([section(obj,float(z)) for z in zs])
    centre=(sections[:,1]+sections[:,2])/2
    depth=(sections[:,2]-sections[:,1])/2
    field=np.zeros((len(zs),len(us)))
    for i,z in enumerate(zs):
        for j,u in enumerate(us):
            y=centre[i]+depth[i]*u
            hit,p,_,_=obj.ray_cast(Vector((2,float(y),float(z))),Vector((-1,0,0)))
            assert hit, (obj.name,z,u)
            field[i,j]=p.x
    return zs,us,sections,field

def smooth_field(field,step):
    # One solve over the entire connection segment. The fourth-order penalty
    # suppresses transverse ripples without introducing discrete blend rings.
    n=len(field)
    d2=np.diff(np.eye(n),n=2,axis=0)
    lam=(FAIR_LENGTH/step)**4
    weight=np.ones(n)
    weight[:3]=weight[-3:]=300
    operator=np.diag(weight)+lam*(d2.T@d2)
    target=np.linalg.solve(operator,weight[:,None]*field)
    # Gentle angular regularity, keeping the sagittal endpoints untouched.
    target[:,1:-1]=0.25*target[:,:-2]+0.50*target[:,1:-1]+0.25*target[:,2:]
    return target

def fair_surface(obj):
    old=coordinates(obj)
    zs,us,sections,source=thickness_field(obj)
    target=smooth_field(source,zs[1]-zs[0])
    z=old[:,2].astype(np.float64)
    front=np.interp(z,zs,sections[:,1]); rear=np.interp(z,zs,sections[:,2])
    depth=(rear-front)/2; centre=(rear+front)/2
    u=(old[:,1]-centre)/depth
    zi=np.clip((z-zs[0])/(zs[1]-zs[0]),0,len(zs)-1.000001)
    ui=np.clip((u-us[0])/(us[1]-us[0]),0,len(us)-1.000001)
    i=zi.astype(int); j=ui.astype(int); f=zi-i; g=ui-j
    def cubic_weights(t):
        return ((1-t)**3/6,(3*t**3-6*t*t+4)/6,(-3*t**3+3*t*t+3*t+1)/6,t**3/6)
    # A C2 tensor B-spline avoids introducing the grid seams of bilinear
    # interpolation into the normals of an otherwise smooth longitudinal fit.
    wz=cubic_weights(f); wu=cubic_weights(g)
    desired=np.zeros(len(old))
    for a in range(4):
        for b in range(4):
            desired+=wz[a]*wu[b]*target[np.clip(i+a-1,0,len(zs)-1),np.clip(j+b-1,0,len(us)-1)]
    delta=desired-np.abs(old[:,0])
    height=ease(MIN_Z,2.14,z)*(1-ease(2.505,MAX_Z,z))
    # Preserve the anterior throat/lower-face arc and posterior side outline.
    # Protect a broad frontal patch around the eyes and mouth; fair behind it.
    angular=(1-ease(0.68,0.94,np.abs(u)))
    face=ease(2.30,2.405,z)*ease(0.18,0.58,-u)
    shoulder=ease(0.29,0.395,np.abs(old[:,0]))*ease(-0.20,-0.12,old[:,1])*(1-ease(0.19,0.27,old[:,1]))
    shoulder_feather=1-shoulder*(1-ease(2.09,2.205,z))
    amount=height*angular*(1-face)*shoulder_feather
    # The arms/shoulders and all anatomy below the root remain fixed.
    protect=(z<=MIN_Z)|((z<=2.09)&(np.abs(old[:,0])>=0.395)&(old[:,1]>=-0.12)&(old[:,1]<=0.19))
    amount[protect]=0
    delta=0.016*np.tanh(delta/0.016)*amount
    new=old.copy()
    new[:,0]=old[:,0]+np.sign(old[:,0])*delta
    # Y/Z are deliberately copied bit-for-bit: the entire side projection,
    # not just four landmark samples, is an exact invariant of this operation.
    obj.data.vertices.foreach_set('co',new.reshape(-1))
    obj.data.update()
    assert np.array_equal(old[:,1:],coordinates(obj)[:,1:])
    return {'changed_vertex_count':int(np.count_nonzero(new[:,0]!=old[:,0])),
            'max_x_change':float(np.max(np.abs(new[:,0]-old[:,0]))),
            'side_projection_exact':True,'all_heights_exact':True,
            'range_z':[MIN_Z,MAX_Z],'fair_length':FAIR_LENGTH}, (zs,us,source,target)

def sculpt_longitudinal_surface(body,shell=None):
    assert not body.get('longitudinal_surface_faired'), 'Use the saved preceding iteration.'
    report,field=fair_surface(body)
    if shell is not None:
        shell.hide_set(False)
        fair_surface(shell)
        shell.hide_set(True)
    for poly in body.data.polygons:
        poly.use_smooth=True
    body.data.update()
    body['longitudinal_surface_faired']=True
    body['head_revision']='Longitudinal cheek/neck thickness fairing; side outline locked'
    body['surface_fair_height_range']=(MIN_Z,MAX_Z)
    body['side_projection_locked']=True
    print('LONGITUDINAL_SURFACE_SCULPT_OK',report)
    return report,field
