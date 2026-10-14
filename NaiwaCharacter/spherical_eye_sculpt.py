"""Independent spherical eyes; fixed-body calibration, no per-vertex skin warp."""
import hashlib,math
import bpy
import numpy as np
from mathutils import Vector
from geometry_diagnostic_utils import mesh_record,face_point
from eye_projection_measure import visible_metrics

D0=.11724481274201151
S0=.23462580179330159
F=np.array([0.,-1.,0.])
TARGETS={'L':[-.1173129031839619,2.5052032749521373],
         'R':[.11731289860933969,2.5053275560352555]}

def install(body,x,z):
    P=face_point(body,x,z);assert P is not None
    # A small least-squares patch avoids polygon-normal stair steps.
    rows=[];values=[]
    for dx in (-.006,-.003,0,.003,.006):
        for dz in (-.006,-.003,0,.003,.006):
            p=face_point(body,x+dx,z+dz)
            if p is not None:rows.append([1,dx,dz,dx*dx,dx*dz,dz*dz]);values.append(p[1])
    fit=np.linalg.lstsq(np.asarray(rows),np.asarray(values),rcond=None)[0]
    n=np.array([fit[1],-1.,fit[2]]);n/=np.linalg.norm(n)
    u=np.array([1.,0.,0.]);u-=n*np.dot(u,n);u/=np.linalg.norm(u)
    v=np.cross(n,u)
    return P,n,u,v

class ProjectionCache:
    def __init__(self,body,target,step=.0006):
        self.step=step;x,z=target
        xs=np.arange(math.ceil((x-.145)/step)*step,x+.145,step)
        zs=np.arange(math.ceil((z-.145)/step)*step,z+.145,step)
        self.X,self.Z=np.meshgrid(xs,zs)
        values=[]
        for xx,zz in zip(self.X.ravel(),self.Z.ravel()):
            p=face_point(body,xx,zz);values.append(float(p[1]) if p is not None else math.inf)
        self.skin=np.array(values).reshape(self.X.shape)
    def metrics(self,C,R,pupil_angle=None,gaze=F):
        dx=self.X-C[0];dz=self.Z-C[2];q=R*R-dx*dx-dz*dz
        root=np.sqrt(np.maximum(q,0));Y=C[1]-root
        visible=(q>0)&(Y<self.skin-5e-7)
        assert visible.any(),'Eye completely buried'
        area=visible.sum()*self.step**2
        out={'D':math.sqrt(4*area/math.pi),'center':np.array([self.X[visible].mean(),self.Z[visible].mean()])}
        if pupil_angle is not None:
            dot=(dx*gaze[0]-root*gaze[1]+dz*gaze[2])/R
            black=visible&(dot>=math.cos(pupil_angle))
            out['pupil_ratio']=math.sqrt(black.sum()/visible.sum())
        return out

def calibrate(body,cache,side,candidate):
    factor={'A':.30,'B':.40}[candidate]
    h0=factor*D0;R0=((D0/2)**2+h0*h0)/(2*h0)
    k=h0/R0;R=R0;point=np.array(TARGETS[side],float);history=[]
    for iteration in range(35):
        P,n,u,v=install(body,*point);h=k*R;C=P-(R-h)*n
        met=cache.metrics(C,R);delta=np.array(TARGETS[side])-met['center']
        history.append({'iteration':iteration,'R':R,'h':h,'P':P.tolist(),'n':n.tolist(),
            'D':met['D'],'center':met['center'].tolist()})
        if abs(met['D']/D0-1)<.0015 and np.linalg.norm(delta)<.0002:break
        R*=1+.65*(D0/met['D']-1)
        point+=.70*delta
    P,n,u,v=install(body,*point);h=k*R;C=P-(R-h)*n
    lower,upper=.005,math.radians(85)
    for _ in range(25):
        angle=(lower+upper)/2
        if cache.metrics(C,R,angle)['pupil_ratio']<.53:lower=angle
        else:upper=angle
    angle=(lower+upper)/2
    return {'candidate':candidate,'side':side,'R':float(R),'h':float(h),'h_over_R':k,
        'sphere_center':C.tolist(),'install_P':P.tolist(),'install_normal':n.tolist(),
        'install_basis_u':u.tolist(),'install_basis_v':v.tolist(),'gaze_axis':F.tolist(),
        'pupil_cap_half_angle_radians':angle,'pupil_cap_half_angle_degrees':math.degrees(angle),
        'initial_R':R0,'initial_h':h0,'calibration_history':history,
        'calibration_method':'Body ray cache, fixed h/R per candidate; adjust radius and installation XZ only; area-equivalent visible frontal diameter and centroid.'}

def sphere_mesh(name,C,R,gaze,pupil_angle,green_color=(.105,.265,.040,1),black_color=(.008,.010,.006,1)):
    C=np.asarray(C);g=np.asarray(gaze);g=g/np.linalg.norm(g)
    # A complete sphere is invariant under a change of orthogonal basis.
    # Installation (u,v,n) places C; sampling around the independent gaze
    # axis makes the pupil boundary an exact latitude without warping Q.
    u=np.array([1.,0.,0.]);u-=g*np.dot(u,g);u/=np.linalg.norm(u);v=np.cross(g,u)
    # Keep 57 latitude rings / 96 azimuth samples and the exact pupil boundary.
    angles=[math.pi*k/58 for k in range(1,58)]
    nearest=min(range(len(angles)),key=lambda i:abs(angles[i]-pupil_angle))
    angles[nearest]=pupil_angle;angles.sort()
    vertices=[C+R*g]
    for phi in angles:
        for j in range(96):
            theta=2*math.pi*j/96
            vertices.append(C+R*(math.sin(phi)*(math.cos(theta)*u+math.sin(theta)*v)+math.cos(phi)*g))
    vertices.append(C-R*g);faces=[];black=[]
    for j in range(96):faces.append((0,1+j,1+(j+1)%96));black.append(angles[0]<=pupil_angle+1e-8)
    for k in range(len(angles)-1):
        a=1+k*96;b=a+96
        for j in range(96):
            faces.append((a+j,b+j,b+(j+1)%96,a+(j+1)%96));black.append(angles[k+1]<=pupil_angle+1e-8)
    end=len(vertices)-1;last=1+(len(angles)-1)*96
    for j in range(96):faces.append((last+j,end,last+(j+1)%96));black.append(False)
    # Explicit outward winding, independent of the chosen orthogonal basis.
    for i,face in enumerate(faces):
        a,b,c=[vertices[q] for q in face[:3]]
        if np.dot(np.cross(b-a,c-a),np.mean([vertices[q] for q in face],axis=0)-C)<0:
            faces[i]=tuple(reversed(face))
    mesh=bpy.data.meshes.new(name+' independent sphere');mesh.from_pydata(vertices,[],faces);mesh.update()
    attr=mesh.color_attributes.new(name='EyeColor',type='FLOAT_COLOR',domain='CORNER')
    colors=np.zeros((len(mesh.loops),4),np.float32)
    for p,is_black in zip(mesh.polygons,black):
        p.use_smooth=True
        for loop in p.loop_indices:colors[loop]=black_color if is_black else green_color
    attr.data.foreach_set('color',colors.ravel())
    return mesh

def apply_eye(body_obj,side,params):
    eye=bpy.data.objects[side+' concentric iris and pupil'];glint=bpy.data.objects[side+' eye glint']
    old=eye.data;materials=list(old.materials)
    palette=[tuple(c.color) for c in old.color_attributes['EyeColor'].data]
    green_color=next(c for c in palette if c[1]>.04);black_color=next(c for c in palette if c[1]<.04)
    params['source_colors_rgba']={'green':green_color,'black':black_color}
    assignments=set(tuple((eye.vertex_groups[g.group].name,float(g.weight)) for g in v.groups) for v in old.vertices)
    assert assignments=={(('Torso',1.0),)},assignments
    assert np.allclose(np.asarray(eye.matrix_world),np.eye(4),atol=1e-8),'Sphere construction uses world identity transform'
    C=np.asarray(params['sphere_center']);R=params['R']
    mesh=sphere_mesh(eye.name,C,R,params['gaze_axis'],params['pupil_cap_half_angle_radians'],green_color,black_color)
    for material in materials:mesh.materials.append(material)
    eye.data=mesh
    group=eye.vertex_groups.get('Torso') or eye.vertex_groups.new(name='Torso')
    group.add(list(range(len(mesh.vertices))),1.,'REPLACE')
    if old.users==0:bpy.data.meshes.remove(old)
    eye['complete_spherical_eye']=True;eye['complete_volume_eye']=True
    eye['spherical_eye_revision']='Independent sphere, fixed-body calibrated, world forward pupil'
    eye['sphere_center']=params['sphere_center'];eye['sphere_radius']=R
    eye['installation_normal']=params['install_normal'];eye['installation_P']=params['install_P']
    eye['exposed_cap_height']=params['h'];eye['pupil_axis']=params['gaze_axis']
    eye['pupil_cap_half_angle']=params['pupil_cap_half_angle_radians']
    eye['pupil_to_iris_diameter']=.53
    # Glint anchor stays inside the pupil and touches the actual sphere.
    x=C[0]-.09*D0;z=C[2]+.13*D0
    y=C[1]-math.sqrt(R*R-(x-C[0])**2-(z-C[2])**2)
    surface=Vector((x,y,z));normal=(surface-Vector(C)).normalized()
    glint.location=surface+.00025*normal
    glint.rotation_euler=normal.to_track_quat('Y','Z').to_euler()
    bpy.context.view_layer.update()
    metrics=visible_metrics(eye,body_obj,step=.0003)
    center=metrics['aperture']['center'];d=math.sqrt(4*metrics['aperture']['area']/math.pi)
    eye['frontal_aperture_center']=center;eye['frontal_eye_axes']=(d/2,d/2)
    eye['frontal_pupil_center']=(metrics['black']['center'][0],y,metrics['black']['center'][1])
    eye['frontal_pupil_radius']=math.sqrt(metrics['black']['area']/math.pi)
    params['actual_frontal_metrics']=metrics
    params['actual_visible_D']=d
    params['actual_pupil_ratio']=math.sqrt(metrics['black']['area']/metrics['aperture']['area'])
    params['glint_surface_anchor']=list(surface);params['glint_contact_offset']=.00025
    assert abs(d/D0-1)<=.03,(side,d)
    assert abs(center[1]-TARGETS[side][1])<=.02*D0,(side,center)
    assert abs(params['actual_pupil_ratio']-.53)<=.02,(side,params['actual_pupil_ratio'])
    return eye

def frozen_snapshot():
    from validate_lip_eye_refinement import attribute,coordinates,weight_hash
    excluded={s+t for s in ('L','R') for t in (' concentric iris and pupil',' eye glint')}
    records={}
    for obj in bpy.data.objects:
        if obj.type=='MESH' and obj.name not in excluded:
            records[obj.name]={'coords':hashlib.sha256(coordinates(obj).tobytes()).hexdigest(),
                'loops':hashlib.sha256(attribute(obj.data.loops,'vertex_index',1,np.int32).tobytes()).hexdigest(),
                'weights':weight_hash(obj,set(bpy.data.objects['Naiwa_Rig'].data.bones.keys())),'matrix':list(map(list,obj.matrix_world)),
                'colors':{a.name:hashlib.sha256(attribute(a.data,'color',4,np.float32).tobytes()).hexdigest() for a in obj.data.color_attributes}}
    rig=bpy.data.objects['Naiwa_Rig']
    records['rig']={'bones':[(b.name,b.parent.name if b.parent else None,list(b.head_local),list(b.tail_local),list(map(list,b.matrix_local))) for b in rig.data.bones],
        'actions':sorted((a.name,a.frame_range[:]) for a in bpy.data.actions),'matrix':list(map(list,rig.matrix_world))}
    return records
