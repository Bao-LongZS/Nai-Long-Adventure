"""One fixed-baseline face raise, volumetric eyes and recessed short mouth."""
import bpy,math,json
import numpy as np
from pathlib import Path
from mathutils import Vector
from longitudinal_surface_sculpt import coordinates,ease
from short_round_face_sculpt import profiles
from neck_direction_sculpt import quintic

def baseline_record():
    return json.loads((Path(__file__).resolve().parent/'eye_muzzle_baseline.json').read_text(encoding='utf8'))

def skin(body,x,z):
    hit,p,_,_=body.ray_cast(Vector((float(x),-2,float(z))),Vector((0,1,0)))
    assert hit,('Face surface missing',x,z)
    return float(p.y)

def anterior_weight(obj,old):
    zs,rows,_,_=profiles(obj)
    z=old[:,2]
    f=np.interp(z,zs,rows[:,1]); r=np.interp(z,zs,rows[:,2]); w=np.interp(z,zs,rows[:,0])
    theta=np.arctan2((old[:,1]-(f+r)/2)/((r-f)/2),old[:,0]/w)
    return ease(.05,.80,np.maximum(0,-np.sin(theta)))

def raise_face(obj,D):
    old=coordinates(obj); z=old[:,2].astype(float); x=np.abs(old[:,0])
    face=anterior_weight(obj,old)
    height=ease(2.17,2.33,z)*(1-ease(2.475,2.638,z))
    lateral=1-ease(.165,.285,x)
    new=old.copy(); new[:,2]+=.4*D*height*face*lateral
    # Above the eye attachment, slide the forehead back onto its original
    # long crown arc. This prevents the translated patch from making a brow
    # ledge while its Z displacement fades to the fixed crown.
    restore=ease(2.50,2.605,z)*face*(1-ease(.14,.22,x))
    for i in np.flatnonzero(restore>0):
        hit,p,_,_=obj.ray_cast(Vector((float(old[i,0]),-2,float(new[i,2]))),Vector((0,1,0)))
        hit0,p0,_,_=obj.ray_cast(Vector((float(old[i,0]),-2,float(old[i,2]))),Vector((0,1,0)))
        if hit and hit0:new[i,1]+=(p.y-p0.y)*restore[i]
    obj.data.vertices.foreach_set('co',new.ravel());obj.data.update()
    return {'Z_maximum':float(np.max(new[:,2]-old[:,2])),
        'vertices':int(np.count_nonzero(np.any(new!=old,axis=1))),
        'X_exact':bool(np.array_equal(new[:,0],old[:,0])),
        'crown_unchanged':bool(new[:,2].max()==old[:,2].max())}

def fair_forehead(obj):
    old=coordinates(obj)
    zs=np.linspace(2.45,2.64,96);xs=np.linspace(0,.22,45)
    field=np.full((len(zs),len(xs)),np.nan)
    for j,x in enumerate(xs):
        for i,z in enumerate(zs):
            hit,p,_,_=obj.ray_cast(Vector((float(x),-2,float(z))),Vector((0,1,0)))
            if hit:field[i,j]=p.y
    delta=np.zeros_like(field)
    for j in range(len(xs)):
        ids=np.flatnonzero(np.isfinite(field[:,j]))
        if len(ids)<10:continue
        y=field[ids,j];n=len(y);d2=np.diff(np.eye(n),n=2,axis=0)
        weights=np.ones(n);weights[:4]=weights[-4:]=400
        fair=np.linalg.solve(np.diag(weights)+(.052/.002)**4*(d2.T@d2),weights*y)
        delta[ids,j]=fair-y
    from continuous_front_face_sculpt import sample_field
    z=old[:,2];x=np.abs(old[:,0])
    change=sample_field(z,x,zs,xs,delta)
    mask=ease(2.455,2.495,z)*(1-ease(2.615,2.638,z))*(1-ease(.15,.22,x))*anterior_weight(obj,old)
    new=old.copy();new[:,1]+=change*mask
    obj.data.vertices.foreach_set('co',new.ravel());obj.data.update()
    return {'maximum_Y_fairing':float(np.max(np.abs(new[:,1]-old[:,1]))),'all_X_Z_exact':True}

def bump(x,z,cx,cz,rx,rz):
    q=((x-cx)/rx)**2+((z-cz)/rz)**2
    return np.maximum(0,1-q)**3

def short_muzzle(obj,D,mouth_z):
    old=coordinates(obj)
    front=anterior_weight(obj,old)
    throat_z=mouth_z-.9*D
    zs=np.linspace(2.165,2.575,206)
    values=np.array([skin(obj,0,float(z)) for z in zs])
    def jets(z):
        fit=np.polyfit(zs[np.abs(zs-z)<.020]-z,values[np.abs(zs-z)<.020],2)
        return float(np.interp(z,zs,values)),float(fit[1]),float(2*fit[0])
    lo=2.17;hi=2.57;f0,df0,ddf0=jets(lo);f1,df1,ddf1=jets(hi)
    tf=skin(obj,0,throat_z)+.15*D;mf=skin(obj,0,mouth_z)-.35*D
    z=old[:,2].astype(float);zz=np.clip(z,lo,hi)
    goal=np.where(zz<=throat_z,quintic(zz,lo,throat_z,f0,tf,df0,-.20,ddf0,-2),
        np.where(zz<=mouth_z,quintic(zz,throat_z,mouth_z,tf,mf,-.20,-.45,-2,60),
            quintic(zz,mouth_z,hi,mf,f1,-.45,df1,60,ddf1)))
    delta=(goal-np.interp(z,zs,values))*front*np.maximum(0,1-(old[:,0]/(1.7*D))**2)**2
    delta*=((z>lo)&(z<hi))
    mouth_before=skin(obj,0,mouth_z);throat_before=skin(obj,0,throat_z)
    new=old.copy();new[:,1]+=delta
    obj.data.vertices.foreach_set('co',new.ravel());obj.data.update()
    return {'mouth_z':mouth_z,'throat_z':throat_z,'mouth_Y_before':mouth_before,
        'mouth_Y_after':skin(obj,0,mouth_z),'throat_Y_before':throat_before,'throat_Y_after':skin(obj,0,throat_z),
        'mouth_F_advance':mouth_before-skin(obj,0,mouth_z),
        'throat_minus_F_retraction':skin(obj,0,throat_z)-throat_before,
        'X_Z_exact':bool(np.array_equal(old[:,[0,2]],new[:,[0,2]]))}

def volume_eye(eye,glint,body,spec,D,raise_z,rim_inset=.00015):
    # Keep the latitude topology and its per-corner green/black colors. The
    # explicit black/green latitude now maps to the requested pupil diameter.
    original_R=(.064**2+.0255**2)/(2*.0255)
    angles=sorted(set([math.pi*k/56 for k in range(1,56)]+[math.asin(.064*.525/original_R),math.asin(.064/original_R)]))
    assert len(eye.data.vertices)==len(angles)*96+2
    a=D/2;pr=.53*D/2;h=.20*D;R=(a*a+h*h)/(2*h)
    cx,cz=spec['properties']['frontal_aperture_center'];cz+=raise_z
    px,_,pz=spec['properties']['frontal_pupil_center'];pz+=raise_z
    verts=[]
    for phi in [0]+[p for p in angles for _ in range(96)]+[math.pi]:
        i=len(verts)
        theta=2*math.pi*((i-1)%96)/96 if i>0 and i<len(eye.data.vertices)-1 else 0
        raw=original_R*math.sin(phi)
        if raw<=.064*.525:
            rad=raw*pr/(.064*.525)
        elif raw<=.064:
            rad=float(quintic(raw,.064*.525,.064,pr,a,pr/(.064*.525),.7,0,0))
        else:
            rad=float(quintic(raw,.064,original_R,a,.69*D,.7,.8,0,0))
        mix=float(ease(pr,a,rad))
        x=px+(cx-px)*mix+rad*math.cos(theta)
        z=pz+(cz-pz)*mix+rad*math.sin(theta)
        cap=math.sqrt(max(1e-10,R*R-rad*rad))-(R-h)-rim_inset*(rad/a)**4
        buried=.12*max(0,-math.cos(phi))**2
        verts.append((x,skin(body,x,z)-cap+buried,z))
    arr=np.array(verts,dtype=np.float32)
    eye.data.vertices.foreach_set('co',arr.ravel());eye.data.update()
    eye['frontal_aperture_center']=(cx,cz);eye['frontal_eye_axes']=(a,a)
    eye['frontal_pupil_center']=(px,float(arr[0,1]),pz)
    eye['frontal_pupil_radius']=pr;eye['pupil_to_iris_diameter']=.53
    eye['exposed_cap_height']=h;eye['complete_spherical_eye']=False;eye['complete_volume_eye']=True
    eye['visible_pupil_to_aperture_area_ratio']=.53**2
    gx=px-.09*D;gz=pz+.13*D
    hit,p,n,_=eye.ray_cast(Vector((gx,-2,gz)),Vector((0,1,0)));assert hit
    glint.location=(gx,p.y-.0005,gz);glint.rotation_euler=(0,0,0)
    return {'center_X_Z':[cx,cz],'diameter':D,'pupil_diameter':2*pr,'projection_F':h,'root_buried':True}

def carve_mouth(body,D,mz):
    old=coordinates(body);u=old[:,0]/(.625*D)
    arc=mz+.012*D*u*u
    half=.03*D*np.maximum(0,1-u*u)**.65
    q=(old[:,2]-arc)/(1.6*half+.04*D)
    window=np.maximum(0,1-u*u)**.55*np.maximum(0,1-q*q)**3
    front=anterior_weight(body,old)
    new=old.copy();new[:,1]+=.07*D*window*front
    pre=skin(body,0,mz)
    body.data.vertices.foreach_set('co',new.ravel());body.data.update()
    return {'central_recess':skin(body,0,mz)-pre,'requested_carve_depth':.07*D}

def deep_seam(body,D,mz):
    mouth=bpy.data.objects['Short closed oral seam']
    weights=[]
    if len(mouth.data.vertices):
        weights=[(mouth.vertex_groups[g.group].name,g.weight) for g in mouth.data.vertices[0].groups]
    verts=[];nx=129;ny=17;rows=[]
    x=-.625*D;z=mz+.012*D;verts.append((x,skin(body,x,z)-.00035,z))
    for i in range(1,nx-1):
        u=-1+2*i/(nx-1);x=.625*D*u
        arc=mz+.012*D*u*u;half=.03*D*max(0,1-u*u)**.65
        row=[]
        for j in range(ny):
            v=-1+2*j/(ny-1);z=arc+half*v
            row.append(len(verts));verts.append((x,skin(body,x,z)-.00035,z))
        rows.append(row)
    tip=len(verts);x=.625*D;z=mz+.012*D;verts.append((x,skin(body,x,z)-.00035,z))
    count=len(verts);verts+= [(x,y+.012*D,z) for x,y,z in verts.copy()]
    faces=[]
    for j in range(ny-1):faces.append((0,rows[0][j],rows[0][j+1]))
    for i in range(len(rows)-1):
        for j in range(ny-1):faces.append((rows[i][j],rows[i+1][j],rows[i+1][j+1],rows[i][j+1]))
    for j in range(ny-1):faces.append((rows[-1][j],tip,rows[-1][j+1]))
    faces+=[tuple(v+count for v in reversed(f)) for f in faces.copy()]
    boundary=[0]+[r[0] for r in rows]+[tip]+[r[-1] for r in reversed(rows)]
    for i,a in enumerate(boundary):
        b=boundary[(i+1)%len(boundary)];faces.append((a,a+count,b+count,b))
    old_mesh=mouth.data
    mesh=bpy.data.meshes.new('Recessed dark brown tapered oral cavity');mesh.from_pydata(verts,[],faces);mesh.update()
    mat=bpy.data.materials.get('Open mouth deep brown')
    if mat is None:
        mat=bpy.data.materials.new('Recessed mouth deep brown');mat.use_nodes=True
    mat.diffuse_color=(.035,.014,.010,1)
    bs=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
    bs.inputs['Base Color'].default_value=(.035,.014,.010,1);bs.inputs['Roughness'].default_value=.9
    mesh.materials.append(mat);mouth.data=mesh
    for name,w in weights:
        group=mouth.vertex_groups.get(name) or mouth.vertex_groups.new(name=name)
        group.add(list(range(len(verts))),w,'REPLACE')
    for p in mesh.polygons:p.use_smooth=True
    mouth['recessed_cavity']=True;mouth['width']=1.25*D;mouth['central_opening']=.06*D
    return {'width':1.25*D,'central_opening':.06*D,'center_z':mz,'material':'deep brown recessed groove',
        'skin_gap':.00035,'closed_cavity_volume':True,'vertex_count':len(verts)}

def sculpt_eye_muzzle(body,shell=None,record=None,stage_directory=None):
    assert not body.get('eye_muzzle_body_revision'),'Always open the fixed preceding version; repeated deformation is forbidden.'
    rec=record or baseline_record();D=rec['D'];dz=.4*D;mz=rec['mouth_center'][2]+dz
    report={'baseline_H':rec['height'],'baseline_D':D,'F':[0,-1,0],'Z':[0,0,1]}
    report['face_raise']=raise_face(body,D)
    if shell is not None:report['shell_face_raise']=raise_face(shell,D)
    # Persist real intermediate geometry for independent stage measurements.
    # These snapshots are made from the fixed input on every run; they are
    # never used as the input to another sculpt run.
    if stage_directory is not None:
        stage_directory=Path(stage_directory)
        stage_directory.mkdir(parents=True,exist_ok=True)
        bpy.data.libraries.write(str(stage_directory/'stage_face_raise.blend'),{body},fake_user=True,compress=True)
    report['muzzle']=short_muzzle(body,D,mz)
    if shell is not None:short_muzzle(shell,D,mz)
    if stage_directory is not None:
        bpy.data.libraries.write(str(stage_directory/'stage_muzzle.blend'),{body},fake_user=True,compress=True)
        (stage_directory/'face_stage_measurements.json').write_text(json.dumps({
            'stage_paths':{'face_raise':'stage_face_raise.blend','muzzle':'stage_muzzle.blend'},
            'mouth_z':mz,'throat_z':mz-.9*D,'fixed_D':D,
            'expected_muzzle_advance':.35*D,'expected_throat_retraction':.15*D,
            'measured_during_build':report['muzzle']},indent=2),encoding='utf8')
    report['eyes']={}
    for side in ('L','R'):
        report['eyes'][side]=volume_eye(bpy.data.objects[side+' concentric iris and pupil'],
            bpy.data.objects[side+' eye glint'],body,rec['eyes'][side],D,dz)
    report['mouth_recess']=carve_mouth(body,D,mz)
    if shell is not None:carve_mouth(shell,D,mz)
    report['mouth']=deep_seam(body,D,mz)
    body['eye_muzzle_body_revision']=True;body['baseline_H']=rec['height'];body['baseline_D']=D
    body['head_revision']='Raised local face, embedded volumetric round eyes and short recessed neutral mouth'
    print('EYE_MUZZLE_SCULPT_OK',json.dumps(report))
    return report
