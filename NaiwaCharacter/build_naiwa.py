"""Build calibrated A/B spherical eyes from the pinned current head/body.

The default opens the immutable 20261005_head_side_continuity baseline.
Use --output with a new directory and --candidate A/B/both for outputs.
The separate revise_head_neck_refinement.py builds neck candidates.
"""
import bpy, math, bisect, sys, runpy
from mathutils import Vector
from pathlib import Path

OUT = Path(__file__).resolve().parent
OUT.mkdir(parents=True, exist_ok=True)
if '--from-primitives' not in sys.argv:
    runpy.run_path(str(OUT/'revise_spherical_eyes.py'),run_name='__main__')
    print('NAIWA_BUILD_OK',len([o for o in bpy.data.objects if o.type=='MESH' and
          any(m.type=='ARMATURE' for m in o.modifiers)]),len(bpy.data.actions))
    raise SystemExit(0)
bpy.ops.wm.read_factory_settings(use_empty=True)

def principled(material):
    return next(n for n in material.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')

# Palette from the supplied Big_Laugh image and the fan reference PDF.
def mat(name, color, roughness=0.58, metallic=0.0):
    m=bpy.data.materials.new(name); m.diffuse_color=(*color,1)
    m.use_nodes=True
    bs=principled(m)
    bs.inputs['Base Color'].default_value=(*color,1)
    bs.inputs['Roughness'].default_value=roughness
    bs.inputs['Metallic'].default_value=metallic
    return m
skin=mat('Warm golden yellow skin',(0.94,0.66,0.13),0.64)
skin_light=mat('Soft ochre yellow skin',(0.94,0.66,0.13),0.70)
arm_gold=mat('Golden forearms',(0.94,0.66,0.13),0.70)
belly=mat('Cream belly',(0.88,0.73,0.46),0.78)
limb_shadow=mat('Soft ochre folds',(0.62,0.36,0.11),0.72)
hand=mat('Golden palms',(0.94,0.66,0.13),0.80)
finger_mat=mat('Dark brown finger and toe tips',(0.19,0.115,0.065),0.80)
eye_rim=mat('Dark olive eye rim',(0.11,0.15,0.07),0.55)
eye_green=mat('Bright green irises',(0.24,0.62,0.14),0.30)
eye_black=mat('Black pupils',(0.009,0.012,0.009),0.40)
principled(eye_black).inputs['Specular IOR Level'].default_value=0.10
principled(eye_green).inputs['Specular IOR Level'].default_value=0.18
eye_glint=mat('Eye glints',(0.95,0.96,0.85),0.10)
mouth_dark=mat('Open mouth deep brown',(0.035,0.014,0.010),0.89)
lip=mat('Mouth rim chocolate',(0.22,0.09,0.04),0.78)
tongue=mat('Tongue muted pink',(0.60,0.19,0.17),0.60)
tooth=mat('Teeth ivory',(0.98,0.90,0.74),0.58)

character=[]

def ease(a,b,value):
    t=max(0.0,min(1.0,(value-a)/(b-a)))
    return t*t*(3-2*t)

def torso_z(z):
    # Lift the abdominal/hip mass without changing the crown or its width.
    return z+0.160*(1-ease(1.30,2.43,z))

def leg_z(z):
    # Stretch above the ankle to meet the raised pelvis; soles stay in place.
    return z+0.160*ease(0.16,0.60,z)

source_z_samples=[1.30+1.13*i/256 for i in range(257)]
lifted_z_samples=[torso_z(z) for z in source_z_samples]
def torso_source_z(z):
    if z<=1.46: return z-0.160
    if z>=2.43: return z
    i=max(1,min(256,bisect.bisect_left(lifted_z_samples,z)))
    t=(z-lifted_z_samples[i-1])/(lifted_z_samples[i]-lifted_z_samples[i-1])
    return source_z_samples[i-1]*(1-t)+source_z_samples[i]*t
def smooth(o):
    if o.type=='MESH':
        for p in o.data.polygons: p.use_smooth=True
    return o

def ell(name, loc, scale, material, bone='Torso', seg=48, rings=32):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=seg, ring_count=rings, location=loc)
    o=bpy.context.object; o.name=name; o.scale=scale
    o.data.materials.append(material); smooth(o)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    o['rig_bone']=bone; character.append(o)
    return o

def capsule(name,a,b,radius,material,bone='Torso',width=None):
    a,b=Vector(a),Vector(b); mid=(a+b)*0.5; length=(b-a).length
    o=ell(name, mid, (radius if width is None else width, radius, length*0.57+radius*0.35),material,bone)
    o.rotation_euler=(b-a).to_track_quat('Z','Y').to_euler()
    return o

def rounded_digit(name,a,b,width,depth,material,bone,root_scale=1.0):
    # A true capped tube keeps finger tips blunt.  The former ellipsoid
    # capsules tapered down to points once the palm swallowed their roots.
    a,b=Vector(a),Vector(b); axis=(b-a).normalized(); length=(b-a).length
    ux=Vector((1,0,0)); ux=(ux-axis*ux.dot(axis)).normalized()
    uy=axis.cross(ux).normalized(); cap=(width+depth)/2
    rows=[]
    for k in range(6):
        angle=-math.pi/2+(math.pi/2)*k/5
        rows.append((cap*root_scale*math.sin(angle),root_scale*max(0.015,math.cos(angle))))
    for t in (0.25,0.50,0.75,1.0):
        blend=t*t*(3.0-2.0*t)
        rows.append((length*t,root_scale*(1-blend)+blend))
    for k in range(1,6):
        angle=(math.pi/2)*k/5
        rows.append((length+cap*math.sin(angle),max(0.015,math.cos(angle))))
    vv=[]; ff=[]; count=28
    for distance,factor in rows:
        for j in range(count):
            theta=2*math.pi*j/count
            vv.append(a+axis*distance+ux*(width*factor*math.cos(theta))+uy*(depth*factor*math.sin(theta)))
    for k in range(len(rows)-1):
        for j in range(count):
            ff.append((k*count+j,k*count+(j+1)%count,(k+1)*count+(j+1)%count,(k+1)*count+j))
    ff.append(tuple(reversed(range(count)))); ff.append(tuple((len(rows)-1)*count+j for j in range(count)))
    mesh=bpy.data.meshes.new(name+' mesh'); mesh.from_pydata(vv,[],ff); mesh.update()
    obj=bpy.data.objects.new(name,mesh); bpy.context.collection.objects.link(obj)
    obj.data.materials.append(material); smooth(obj); obj['rig_bone']=bone; character.append(obj)
    return obj

# The front and rear outlines are controlled separately.  The short throat
# recess belongs to the front; the rear expands smoothly from crown to hips.
# z, half-width X, front Y, rear Y.  Negative Y points forwards.
outline_keys=[
    (0.46,0.040,-0.075,0.070),
    (0.49,0.180,-0.235,0.225),
    (0.55,0.360,-0.405,0.380),
    (0.67,0.475,-0.555,0.470),
    (0.82,0.550,-0.645,0.540),
    (0.98,0.603,-0.705,0.580),
    (1.14,0.630,-0.730,0.590),
    (1.22,0.630,-0.728,0.586),
    (1.30,0.615,-0.710,0.568),
    (1.47,0.555,-0.635,0.510),
    (1.63,0.501,-0.535,0.415),
    (1.78,0.467,-0.491,0.363),
    (1.90,0.440,-0.463,0.333),
    (2.00,0.414,-0.459,0.305),
    (2.065,0.390,-0.469,0.285),
    (2.13,0.361,-0.475,0.265),
    (2.17,0.350,-0.478,0.255),
    (2.20,0.358,-0.465,0.247),
    (2.28,0.338,-0.444,0.202),
    (2.35,0.320,-0.425,0.160),
    (2.395,0.304,-0.405,0.133),
    (2.43,0.285,-0.379,0.110),
    (2.465,0.253,-0.348,0.078),
    (2.50,0.203,-0.308,0.028),
    (2.535,0.116,-0.232,-0.040),
    (2.55,0.010,-0.140,-0.120),
]
# Narrow both cheeks and the lower head, reducing depth as well as width.
# Keep the narrow head while the new torso profile concentrates volume lower.
adjusted_outline=[]
for z,rx,front,rear in outline_keys:
    head_weight=math.exp(-((z-2.19)/0.20)**2)
    centre=(front+rear)/2
    half_depth=(rear-front)/2*(1-0.08*head_weight)
    adjusted_outline.append((torso_z(z),rx*(1-0.11*head_weight),centre-half_depth,centre+half_depth))
outline_keys=adjusted_outline
profile_keys=[(z,rx,(rear-front)/2,(rear+front)/2)
              for z,rx,front,rear in outline_keys]
def profile_value(i,k):
    return profile_keys[max(0,min(len(profile_keys)-1,i))][k]
profile=[]
for i in range(len(profile_keys)-1):
    a,b=profile_keys[i],profile_keys[i+1]
    h=b[0]-a[0]
    for step in range(4):
        t=step/4; t2=t*t; t3=t2*t
        row=[a[0]+h*t]
        for k in range(1,4):
            before=profile_keys[max(0,i-1)]; after=profile_keys[min(len(profile_keys)-1,i+2)]
            m0=(b[k]-before[k])/(b[0]-before[0])
            m1=(after[k]-a[k])/(after[0]-a[0])
            v=(2*t3-3*t2+1)*a[k]+(t3-2*t2+t)*h*m0+(-2*t3+3*t2)*b[k]+(t3-t2)*h*m1
            row.append(max(0.01,v) if k in (1,2) else v)
        profile.append(tuple(row))
profile.append(profile_keys[-1])
verts=[]; faces=[]; N=64
for z,rx,ry,cy in profile:
    for j in range(N):
        theta=2*math.pi*j/N
        x=rx*math.cos(theta); y=cy+ry*math.sin(theta)
        # A short broad lower face grows from the cheek, rather than a lip patch.
        front_weight=max(0.0,-math.sin(theta))**4
        ledge=0.026*math.exp(-((x/0.295)**2+((z-torso_z(2.223))/0.108)**2))
        # A broad, low-amplitude anterior turn joins the whole lower face to
        # the upper chest.  It never runs round the sides as a neck groove.
        throat=0.010*math.exp(-((x/0.335)**2+((z-torso_z(2.035))/0.165)**2))
        verts.append((x,y+front_weight*(throat-ledge),z))
for i in range(len(profile)-1):
    for j in range(N):
        a=i*N+j; b=i*N+(j+1)%N; c=(i+1)*N+(j+1)%N; d=(i+1)*N+j
        faces.append((a,b,c,d))
faces.append(tuple(reversed(range(N))))
faces.append(tuple((len(profile)-1)*N+j for j in range(N)))
me=bpy.data.meshes.new('pear body mesh'); me.from_pydata(verts,[],faces); me.update()
body=bpy.data.objects.new('Naiwa continuous body and head',me); bpy.context.collection.objects.link(body)
body.data.materials.append(skin_light); smooth(body)
sub=body.modifiers.new('Silhouette smoothing','SUBSURF'); sub.levels=2; sub.render_levels=2
body['rig_bone']='Torso'; character.append(body)
# Preserve the continuous head-and-torso shell for a clean silhouette check.
body_only=body.copy(); body_only.data=body.data.copy()
body_only.name='Head and torso silhouette only'
bpy.context.collection.objects.link(body_only)
body_only.hide_render=True
body_only.hide_set(True)

# Evaluate the smooth cross section, including its front-to-back depth.
def body_section(z):
    lo=profile[0]; hi=profile[-1]
    for a,b in zip(profile,profile[1:]):
        if a[0] <= z <= b[0]: lo,hi=a,b; break
    t=max(0,min(1,(z-lo[0])/(hi[0]-lo[0])))
    rx=lo[1]*(1-t)+hi[1]*t
    ry=lo[2]*(1-t)+hi[2]*t
    cy=lo[3]*(1-t)+hi[3]*t
    return rx,ry,cy

def body_front(x,z):
    rx,ry,cy=body_section(z)
    return cy-ry*math.sqrt(max(0.02,1-(x/max(rx,.001))**2))

def facial_patch(name,cx,cz,rx,rz,depth,material,tilt=0.0):
    verts=[(cx,body_front(cx,cz)-depth,cz)]; faces=[]; count=64; rings=8
    for ring in range(1,rings+1):
        rr=ring/rings
        for j in range(count):
            a=2*math.pi*j/count
            u=rx*rr*math.cos(a)*(1+0.22*math.sin(a)); v=rz*rr*math.sin(a)
            x=cx+u*math.cos(tilt)-v*math.sin(tilt)
            z=cz+u*math.sin(tilt)+v*math.cos(tilt)
            verts.append((x,body_front(x,z)-depth,z))
    for j in range(count): faces.append((0,1+j,1+(j+1)%count))
    for ring in range(1,rings):
        for j in range(count):
            a=1+(ring-1)*count+j; b=1+ring*count+j
            c=1+ring*count+(j+1)%count; d=1+(ring-1)*count+(j+1)%count
            faces.append((a,b,c,d))
    mesh=bpy.data.meshes.new(name+' mesh'); mesh.from_pydata(verts,[],faces); mesh.update()
    obj=bpy.data.objects.new(name,mesh); bpy.context.collection.objects.link(obj)
    obj.data.materials.append(material); smooth(obj)
    obj['rig_bone']='Torso'; character.append(obj)
    return obj
# Continuous, gently tapered legs.  Avoid separate thigh/shin balls, which
# made the previous version look muscular in the front and side renders.
for side,s in [('L',-1),('R',1)]:
    leg_profile=[
        (0.08,0.35,0.000,0.088,0.100),
        (0.16,0.35,0.000,0.098,0.110),
        (0.29,0.35,0.000,0.132,0.145),
        (0.44,0.34,-0.010,0.170,0.200),
        (0.60,0.30,-0.015,0.205,0.255),
        (0.77,0.28,-0.005,0.215,0.300),
        (0.92,0.27,0.000,0.215,0.325),
        (1.08,0.27,0.000,0.215,0.305),
    ]
    leg_profile=[(leg_z(z),cx,cy,rx,ry) for z,cx,cy,rx,ry in leg_profile]
    lv=[]; lf=[]; ln=32
    for z,cx,cy,rx,ry in leg_profile:
        for j in range(ln):
            a=2*math.pi*j/ln
            lv.append((s*cx+rx*math.cos(a),cy+ry*math.sin(a),z))
    for i in range(len(leg_profile)-1):
        for j in range(ln):
            a=i*ln+j; b=i*ln+(j+1)%ln; c=(i+1)*ln+(j+1)%ln; d=(i+1)*ln+j
            lf.append((a,b,c,d))
    lf.append(tuple(reversed(range(ln))))
    lf.append(tuple((len(leg_profile)-1)*ln+j for j in range(ln)))
    lm=bpy.data.meshes.new(f'{side} smooth leg mesh'); lm.from_pydata(lv,[],lf); lm.update()
    leg=bpy.data.objects.new(f'{side} smooth leg',lm); bpy.context.collection.objects.link(leg)
    leg.data.materials.append(skin_light); smooth(leg); leg['rig_bone']=f'Thigh.{side}'; character.append(leg)
    # Longitudinal foot sections put the ankle over the narrow rear third.
    # The broad forefoot extends forwards, with one continuous descending instep.
    foot_rows=[
        (0.085,0.042,0.060),
        (0.060,0.075,0.105),
        (0.025,0.095,0.163),
        (-0.025,0.105,0.182),
        (-0.095,0.115,0.158),
        (-0.170,0.129,0.124),
        (-0.235,0.140,0.093),
        (-0.280,0.131,0.070),
        (-0.305,0.098,0.053),
    ]
    fv=[]; ff=[]; fn=40; sole_z=0.008
    for yy,half_width,top_z in foot_rows:
        for j in range(fn):
            angle=2*math.pi*j/fn
            height=((1+math.sin(angle))*0.5)**1.6
            fv.append((s*0.35+half_width*math.cos(angle),yy,sole_z+(top_z-sole_z)*height))
    for i in range(len(foot_rows)-1):
        for j in range(fn):
            ff.append((i*fn+j,i*fn+(j+1)%fn,(i+1)*fn+(j+1)%fn,(i+1)*fn+j))
    ff.append(tuple(reversed(range(fn))))
    ff.append(tuple((len(foot_rows)-1)*fn+j for j in range(fn)))
    fm=bpy.data.meshes.new(f'{side} forward foot mesh'); fm.from_pydata(fv,[],ff); fm.update()
    foot=bpy.data.objects.new(f'{side} narrow heel sloping foot',fm); bpy.context.collection.objects.link(foot)
    foot.data.materials.append(skin_light); smooth(foot); foot['rig_bone']=f'Shin.{side}'; character.append(foot)
    foot_sub=foot.modifiers.new('Continuous instep','SUBSURF'); foot_sub.levels=2
    # Three blunt toe tips protrude beyond the forefoot; wide roots merge inside it.
    for k in (-1,0,1):
        xx=s*0.35+k*0.088
        end_y=-0.363+(0.013 if k else 0.0)
        rounded_digit(f'{side} integrated toe {k}',(s*0.35+k*0.069,-0.237,0.060),
                      (xx,end_y,0.041),0.044,0.031,skin_light,f'Shin.{side}',root_scale=1.32)

# A single smooth cross-section sweep joins upper arm, forearm and palm.
# This avoids the round shoulder cap and separate hanging finger beads.
for side,s in [('L',-1),('R',1)]:
    arm_rows=[
        (2.065,0.275,0.005,0.110,0.150),
        (1.975,0.365,0.010,0.112,0.143),
        (1.875,0.455,0.012,0.110,0.128),
        (1.765,0.535,0.015,0.107,0.111),
        (1.635,0.597,0.015,0.110,0.103),
        (1.49,0.650,0.015,0.122,0.105),
        (1.36,0.727,0.005,0.122,0.115),
        (1.24,0.762,-0.012,0.110,0.100),
        (1.15,0.770,-0.025,0.110,0.088),
        (1.09,0.770,-0.030,0.114,0.101),
        (1.045,0.768,-0.032,0.112,0.095),
        (1.015,0.768,-0.032,0.095,0.078),
    ]
    av=[]; af=[]; an=40
    for zz,xx,yy,rx,ry in arm_rows:
        for j in range(an):
            a=2*math.pi*j/an
            av.append((s*xx+rx*math.cos(a),yy+ry*math.sin(a),zz))
    for i in range(len(arm_rows)-1):
        for j in range(an):
            af.append((i*an+j,(i+1)*an+j,(i+1)*an+(j+1)%an,i*an+(j+1)%an))
    af.append(tuple(range(an))); af.append(tuple(reversed(tuple((len(arm_rows)-1)*an+j for j in range(an)))))
    am=bpy.data.meshes.new(f'{side} continuous arm palm mesh'); am.from_pydata(av,[],af); am.update()
    ao=bpy.data.objects.new(f'{side} relaxed arm and broad palm',am); bpy.context.collection.objects.link(ao)
    ao.data.materials.append(skin_light); smooth(ao); ao['rig_bone']=f'UpperArm.{side}'; character.append(ao)
    sub=ao.modifiers.new('Smooth arm sweep','SUBSURF'); sub.levels=2
    for k in range(3):
        x=0.697+0.075*k
        root_x=0.707+0.065*k
        low=0.923+(0.012 if k==0 else 0.000 if k==1 else 0.022)
        rounded_digit(f'{side} integrated finger {k}',(s*root_x,-0.033,1.053),
                      (s*x,-0.049,low),0.032,0.045,skin_light,f'Forearm.{side}',root_scale=1.14)
    rounded_digit(f'{side} integrated thumb',(s*0.669,-0.065,1.105),
                  (s*0.647,-0.094,1.015),0.031,0.034,skin_light,f'Forearm.{side}')

# D is the eye-rim diameter; the face starts with both gaps close to D.
EYE_DIAMETER=0.180
EYE_Z=2.55-1.05*EYE_DIAMETER
MOUTH_Z=torso_z(EYE_Z-0.78*EYE_DIAMETER)
# Eyes are made after the final skin remesh, as concentric radial surfaces.
# This avoids the depth-dependent lateral shift of the previous stacked balls.

# Fuse all golden skin parts into one sculpt-like surface to avoid toy-like seams.
skin_parts=[o for o in character if o.data.materials and o.data.materials[0] in (skin,skin_light,arm_gold,hand)]
bpy.ops.object.select_all(action='DESELECT')
for o in skin_parts: o.select_set(True)
bpy.context.view_layer.objects.active=body
bpy.ops.object.convert(target='MESH')
bpy.ops.object.join()
fused=bpy.context.object; fused.name='Naiwa unified golden body'
fused.data.materials.clear(); fused.data.materials.append(skin_light)
fused.data.remesh_voxel_size=0.012
bpy.ops.object.voxel_remesh()
smooth_mod=fused.modifiers.new('Organic surface smoothing','SMOOTH')
surface_group=fused.vertex_groups.new(name='Surface smoothing')
for v in fused.data.vertices:
    p=v.co
    detail=(abs(p.x)>0.63 and p.z<1.13) or p.z<0.14
    surface_group.add([v.index],0.16 if detail else 1.0,'REPLACE')
smooth_mod.vertex_group=surface_group.name
smooth_mod.factor=0.95; smooth_mod.iterations=20
bpy.ops.object.modifier_apply(modifier=smooth_mod.name)
# Smooth a broad head/cheek region after the voxel union; no narrow sculpt
# masks cross the cheek, so lighting reads one continuous rounded surface.
cheek_group=fused.vertex_groups.new(name='Broad cheek smoothing')
for vertex in fused.data.vertices:
    point=vertex.co; zz=point.z
    t=max(0.0,min(1.0,(zz-1.78)/0.24))
    front_gate=max(0.0,min(1.0,(-point.y-0.10)/0.25))
    preserve_oral=math.exp(-((point.x/0.295)**2+((zz-torso_z(2.223))/0.150)**2))*front_gate
    cheek_group.add([vertex.index],t*t*(3-2*t)*(1-0.50*preserve_oral),'REPLACE')
cheek_smooth=fused.modifiers.new('Smooth broad face and nape','SMOOTH')
cheek_smooth.vertex_group=cheek_group.name; cheek_smooth.factor=0.55; cheek_smooth.iterations=90
bpy.ops.object.modifier_apply(modifier=cheek_smooth.name)
# Fill only the anterior palm trough left by the finger-root union.  The
# desired surface gently descends into the fingers instead of forming a bar.
for vertex in fused.data.vertices:
    p=vertex.co; ax=abs(p.x)
    if 0.64<ax<0.91 and 0.97<p.z<1.10 and p.y<-0.050:
        target_y=-0.123+0.040*((p.z-1.045)/0.095)**2+0.022*((ax-0.770)/0.125)**2
        fill_weight=math.exp(-((p.z-1.050)/0.070)**2)
        if p.y>target_y: p.y-=(p.y-target_y)*fill_weight
        # Two narrow longitudinal clefts start near the finger roots.
        cleft=sum(math.exp(-((ax-gap)/0.008)**2) for gap in (0.7345,0.8095))
        p.y+=0.012*cleft*math.exp(-((p.z-0.997)/0.055)**2)
    if p.z<0.15 and 0.19<ax<0.51:
        # Local shallow grooves follow the two spaces between the toe roots.
        local_x=ax-0.35
        gaps=sum(math.exp(-((local_x-gap)/0.010)**2) for gap in (-0.044,0.044))
        top_weight=max(0.0,min(1.0,(p.z-0.025)/0.030))
        p.z-=0.010*gaps*math.exp(-((p.y+0.318)/0.052)**2)*top_weight
        # A small planar contact zone joins all three toes to the level sole.
        if p.z<0.023:
            t=max(0.0,min(1.0,(p.z-0.012)/0.011))
            blend=t*t*(3.0-2.0*t)
            p.z=0.008+(p.z-0.008)*blend
fused.data.update()
smooth(fused)
# Keep one smooth subdivision on the final skin, including the short rounded
# digit tips.  It is evaluated for rendering and baked into the FBX export.
finish=fused.modifiers.new('Smooth finished skin','SUBSURF')
finish.levels=1; finish.render_levels=1
# Paint a broad cream belly directly on the continuous skin.  The oval wraps
# partway round the sides, while the depth gate keeps thighs and arms yellow.
def smoothstep(a,b,v):
    t=max(0.0,min(1.0,(v-a)/(b-a)))
    return t*t*(3.0-2.0*t)
colors=fused.data.color_attributes.new(name='NaiwaColor',type='FLOAT_COLOR',domain='CORNER')
skin_rgb=(0.94,0.66,0.13); cream_rgb=(0.70,0.54,0.33)
tip_rgb=(0.19,0.115,0.065)
for poly in fused.data.polygons:
    for li in poly.loop_indices:
        p=fused.data.vertices[fused.data.loops[li].vertex_index].co
        rx,ry,cy=body_section(p.z)
        frontness=(cy-p.y)/max(ry,0.01)
        belly_height=(torso_source_z(p.z)-0.89)/0.81
        cream_angle=1.52-0.86*max(0.0,min(1.0,belly_height))
        around_front=math.atan2(p.x/max(rx,0.01),frontness)
        oval=abs(around_front/cream_angle)**2.4+abs(2*belly_height-1)**2.6
        oval_mask=1.0-smoothstep(0.94,1.07,oval)
        depth_mask=1.0-smoothstep(0.05,0.32,p.y-body_front(p.x,p.z))
        amount=oval_mask*depth_mask
        rgb=tuple(skin_rgb[i]*(1-amount)+cream_rgb[i]*amount for i in range(3))
        # Brown follows the actual integrated finger/toe surfaces.  A narrow
        # transition leaves readable phalanges without detached colored beads.
        finger_mask=0.0
        hand_min_x=0.61 if p.z<1.11 else 0.64
        if abs(p.x)>hand_min_x and 0.80<p.z<1.18 and abs(p.y+0.04)<0.15:
            palm_curve=0.018*(1.0-min(1.0,((abs(p.x)-0.77)/0.11)**2))+0.22*(p.y+0.032)
            finger_mask=1.0-smoothstep(1.050+palm_curve,1.115+palm_curve,p.z)
        toe_mask=0.0
        if p.z<0.180 and 0.19<abs(p.x)<0.53:
            toe_mask=1.0-smoothstep(-0.255,-0.192,p.y)
        tip_amount=max(finger_mask,toe_mask)
        colors.data[li].color=tuple(rgb[i]*(1-tip_amount)+tip_rgb[i]*tip_amount for i in range(3))+(1.0,)
nodes=skin_light.node_tree.nodes
attribute=nodes.new('ShaderNodeAttribute'); attribute.attribute_name='NaiwaColor'
skin_light.node_tree.links.new(attribute.outputs['Color'],principled(skin_light).inputs['Base Color'])
for o in skin_parts:
    if o is not fused and o in character: character.remove(o)
fused['rig_bone']='AUTO'
if fused not in character: character.append(fused)

# Each eye is a complete globe partially buried in the face, with one sphere
# and one material.  Pupil and green aperture are painted on that shared
# curved surface; no yellow mounting disk or dark raised outline is used.
# Bake skin refinement before head shaping and the shared spherical eyes.
bpy.context.view_layer.objects.active=fused
bpy.ops.object.modifier_apply(modifier=finish.name)

# Local head sculpt on the finished body.  All vertices at or below the
# existing upper-chest boundary remain untouched, preserving shoulders,
# abdominal volume, arms, legs and feet from the preceding version.
# One analytic round head replaces piecewise cheek rings.  A quintic blend
# matches position, slope and curvature at the protected chest and head ends.
head_cutoff=torso_z(2.065)
head_sculpt_rows=[(head_cutoff,)]

def old_head_coordinate(z,k):
    rx,ry,cy=body_section(z)
    return rx if k==0 else cy-ry if k==1 else cy+ry

def head_ellipse(z,k):
    if k==0:
        centre_z=2.330; vertical_radius=0.330 if z<2.330 else 0.220; radius=0.260
        u=(z-centre_z)/vertical_radius
        q=math.sqrt(max(0.0008,1-u*u))
        return radius*q, -radius*u/(vertical_radius*q), -radius/(vertical_radius**2*q**3)
    centre_z=2.300; vertical_radius=0.250; radius=0.295
    u=(z-centre_z)/vertical_radius
    q=math.sqrt(max(0.0008,1-u*u))
    sign=-1 if k==1 else 1
    return -0.125+sign*radius*q, -sign*radius*u/(vertical_radius*q), -sign*radius/(vertical_radius**2*q**3)

def quintic_bridge(z,z0,z1,v0,d0,s0,v1,d1,s1):
    h=z1-z0; t=max(0.0,min(1.0,(z-z0)/h))
    a0=v0; a1=h*d0; a2=0.5*h*h*s0
    dv=v1-a0-a1-a2; dd=h*d1-a1-2*a2; ds=h*h*s1-2*a2
    a3=10*dv-4*dd+0.5*ds; a4=-15*dv+7*dd-ds; a5=6*dv-3*dd+0.5*ds
    return a0+a1*t+a2*t*t+a3*t**3+a4*t**4+a5*t**5

head_bridge_specs=[]
for k in range(3):
    end_z=2.255 if k in (0,1) else 2.330
    v0=old_head_coordinate(head_cutoff,k)
    eps=0.018
    left=old_head_coordinate(head_cutoff-eps,k); right=old_head_coordinate(head_cutoff+eps,k)
    d0=(right-left)/(2*eps)
    s0=(right-2*v0+left)/(eps*eps)
    v1,d1,s1=head_ellipse(end_z,k)
    head_bridge_specs.append((end_z,v0,d0,s0,v1,d1,s1))

def head_sculpt_values(z):
    if z<=head_cutoff: return None
    values=[]
    for k,spec in enumerate(head_bridge_specs):
        end_z,v0,d0,s0,v1,d1,s1=spec
        value=head_ellipse(z,k)[0] if z>=end_z else quintic_bridge(z,head_cutoff,end_z,v0,d0,s0,v1,d1,s1)
        values.append(value)
    return values

def sculpt_head_point(point):
    values=head_sculpt_values(point.z)
    if values is None: return
    rx,ry,cy=body_section(point.z)
    blend=ease(head_sculpt_rows[0][0],head_sculpt_rows[0][0]+0.075,point.z)
    width,front,rear=values
    nx=point.x/max(rx,0.001); ny=(point.y-cy)/max(ry,0.001)
    theta=math.atan2(ny,nx)
    cap_weight=ease(2.495,2.530,point.z)
    cap_radius=min(1.0,math.hypot(nx,ny))
    radial=1-cap_weight*(1-cap_radius)
    xx=width*math.cos(theta)*radial
    yy=(front+rear)/2+(rear-front)/2*math.sin(theta)*radial
    face_weight=max(0.0,-math.sin(theta))**4
    # Refit the whole cheek surface to clean curved cross sections.  Merely
    # squeezing X preserves and amplifies the old cheek's horizontal ripples.
    oral=0.036*math.exp(-((xx/0.255)**2+((point.z-torso_z(2.245))/0.112)**2))
    throat=0.007*math.exp(-((xx/0.310)**2+((point.z-torso_z(2.080))/0.155)**2))
    yy+=face_weight*(throat-oral)
    point.x=point.x*(1-blend)+xx*blend
    point.y=point.y*(1-blend)+yy*blend
    # Pole-interior vertices fill the rounded dome; projecting them all onto
    # one perimeter ring would leave the old voxel cap as a flat top.
    point.z+=(2.550-point.z)*cap_weight*(1-cap_radius*cap_radius)

for vertex in fused.data.vertices: sculpt_head_point(vertex.co)
fused.data.update()
# Smooth the deformed cheek surface itself, with zero weight below the
# protected boundary.  Lighting then follows geometry with no collar crease.
head_finish_group=fused.vertex_groups.new(name='Compact head finish')
for vertex in fused.data.vertices:
    weight=ease(head_sculpt_rows[0][0],head_sculpt_rows[0][0]+0.100,vertex.co.z)
    if weight>0: head_finish_group.add([vertex.index],weight,'REPLACE')
head_finish=fused.modifiers.new('Round compact cheeks and nape','SMOOTH')
head_finish.vertex_group=head_finish_group.name
head_finish.factor=0.32; head_finish.iterations=18
bpy.ops.object.modifier_apply(modifier=head_finish.name)
for vertex in body_only.data.vertices: sculpt_head_point(vertex.co)
body_only.data.update()
fused['head_revision']='Compact rounded cheeks and short forward face'
fused['preserved_body_below_z']=head_sculpt_rows[0][0]

# Preserve the current crown and rebuild the lower head as a continuous chest/back bridge.
import sys
sys.path.insert(0, str(OUT))
from connection_sculpt import sculpt_connection
sculpt_connection(fused, body_only)
def face_surface_y(x,z):
    hit,loc,normal,index=fused.ray_cast(Vector((x,-2,z)),Vector((0,1,0)))
    if not hit: raise RuntimeError('Eye surface missed the face')
    return loc.y

eye_specs=[]
for side,s in [('L',-1),('R',1)]:
    cx=s*0.103; cz=EYE_Z
    iris_radius=0.064; pupil_ratio=0.525
    pupil_radius=iris_radius*pupil_ratio
    cap_height=0.0255
    sphere_radius=(iris_radius**2+cap_height**2)/(2*cap_height)
    surface=Vector((cx,face_surface_y(cx,cz),cz))
    eps=0.008
    slope_x=(face_surface_y(cx+eps,cz)-face_surface_y(cx-eps,cz))/(2*eps)
    slope_z=(face_surface_y(cx,cz+eps)-face_surface_y(cx,cz-eps))/(2*eps)
    normal=Vector((slope_x,-1,slope_z)).normalized()
    tangent_u=Vector((1,slope_x,0)).normalized()
    tangent_v=normal.cross(tangent_u).normalized()
    # Bury the root slightly below the tangent plane rather than lifting a
    # yellow mounting ring up to it.  The exposed globe remains low and round.
    centre=surface-normal*(sphere_radius-cap_height+0.0068)
    eye_specs.append((side,cx,cz,surface,normal,tangent_u,tangent_v,centre,sphere_radius,iris_radius,pupil_radius))

# The globe intersects the unraised, refined facial skin directly.
# Keeping the surrounding skin intact avoids a yellow annular pedestal.
eye_green_rgb=(0.105,0.265,0.040)
eye_black_rgb=(0.008,0.010,0.006)
for side,cx,cz,surface,normal,u,v,centre,radius,iris_r,pupil_r in eye_specs:
    # Centre the pupil in the *frontal projection* of the green aperture.
    # Using the cheek tangent's polar axis would shift both pupils outwards
    # because its cap centre stands farther forward than its edge.
    aperture_centre=surface-normal*0.0068
    look_x=aperture_centre.x; look_z=aperture_centre.z
    look_y=centre.y-math.sqrt(radius**2-(look_x-centre.x)**2-(look_z-centre.z)**2)
    color_normal=(Vector((look_x,look_y,look_z))-centre).normalized()
    color_u=Vector((1,0,0))
    color_u=(color_u-color_normal*color_u.dot(color_normal)).normalized()
    color_v=color_normal.cross(color_u).normalized()
    pupil_angle=math.asin(pupil_r/radius)
    iris_angle=math.asin(iris_r/radius)
    # Explicit latitude boundaries keep pupil size exact on the same sphere.
    angles=sorted(set([math.pi*k/56 for k in range(1,56)]+[pupil_angle,iris_angle]))
    count=96
    ev=[tuple(centre+color_normal*radius)]; ef=[]; black_faces=[]
    for phi in angles:
        rr=radius*math.sin(phi)
        for j in range(count):
            theta=2*math.pi*j/count
            point=centre+color_normal*(radius*math.cos(phi))+color_u*(rr*math.cos(theta))+color_v*(rr*math.sin(theta))
            ev.append(tuple(point))
    ev.append(tuple(centre-color_normal*radius)); south=len(ev)-1
    for j in range(count):
        ef.append((0,1+j,1+(j+1)%count)); black_faces.append(True)
    for k in range(len(angles)-1):
        for j in range(count):
            ef.append((1+k*count+j,1+(k+1)*count+j,1+(k+1)*count+(j+1)%count,1+k*count+(j+1)%count))
            black_faces.append(angles[k+1]<=pupil_angle+1e-8)
    start=1+(len(angles)-1)*count
    for j in range(count):
        ef.append((south,start+(j+1)%count,start+j)); black_faces.append(False)
    em=bpy.data.meshes.new(f'{side} complete shallow eye globe')
    em.from_pydata(ev,[],ef); em.update()
    eo=bpy.data.objects.new(f'{side} concentric iris and pupil',em)
    bpy.context.collection.objects.link(eo)
    eye_material=mat(f'{side} single eye surface',eye_green_rgb,0.43)
    principled(eye_material).inputs['Specular IOR Level'].default_value=0.23
    eye_colors=em.color_attributes.new(name='EyeColor',type='FLOAT_COLOR',domain='CORNER')
    for poly,is_black in zip(em.polygons,black_faces):
        poly.use_smooth=True
        color=eye_black_rgb if is_black else eye_green_rgb
        for li in poly.loop_indices: eye_colors.data[li].color=(*color,1.0)
    eye_attribute=eye_material.node_tree.nodes.new('ShaderNodeAttribute')
    eye_attribute.attribute_name='EyeColor'
    eye_material.node_tree.links.new(eye_attribute.outputs['Color'],principled(eye_material).inputs['Base Color'])
    eo.data.materials.append(eye_material)
    eo['rig_bone']='Torso'; eo['pupil_to_iris_diameter']=0.525
    eo['exposed_cap_height']=0.0187; eo['complete_spherical_eye']=True
    eo['frontal_pupil_center']=(look_x,look_y,look_z)
    character.append(eo)
    # One small highlight follows the globe, inside the centred pupil.
    gu=-0.012; gv=0.016
    glint_point=centre+color_normal*math.sqrt(radius**2-gu**2-gv**2)+color_u*gu+color_v*gv
    glint=ell(f'{side} eye glint',glint_point+color_normal*0.0006,
              (0.0034,0.0012,0.0034),eye_glint,'Torso',24,16)
    glint.rotation_euler=color_normal.to_track_quat('Y','Z').to_euler()

# The oral seam is placed on the finished face by surface ray casts.  Its
# root is the same continuous mesh as the cheek and shallow throat curve.
mouth_verts=[]; mouth_faces=[]; mouth_steps=32
for j in range(mouth_steps+1):
    x=-0.068+0.136*j/mouth_steps
    z=MOUTH_Z+0.0012*(x/0.068)**2
    for dz in (-0.002,0.002):
        hit,loc,normal,index=fused.ray_cast(Vector((x,-2,z+dz)),Vector((0,1,0)))
        if not hit: raise RuntimeError('Oral seam missed the face surface')
        mouth_verts.append((x,loc.y-0.0015,z+dz))
    if j<mouth_steps: mouth_faces.append((2*j,2*j+2,2*j+3,2*j+1))
mouth_mesh=bpy.data.meshes.new('Surface oral seam'); mouth_mesh.from_pydata(mouth_verts,[],mouth_faces); mouth_mesh.update()
mouth_obj=bpy.data.objects.new('Short closed oral seam',mouth_mesh); bpy.context.collection.objects.link(mouth_obj)
mouth_obj.data.materials.append(lip); mouth_obj['rig_bone']='Torso'; character.append(mouth_obj)

# Reduce the head and refit the complete eye globes and oral seam.
from head_volume_sculpt import sculpt_head_volume
sculpt_head_volume(fused, body_only)

# Raise the preserved head and construct the longer neck above fixed shoulders.
from neck_extension_sculpt import extend_neck
extend_neck(fused, body_only)

from neck_blend_sculpt import blend_neck_seams
blend_neck_seams(fused, body_only)

from longitudinal_surface_sculpt import sculpt_longitudinal_surface
# This iteration follows the actual open scene, which preceded the optional
# forward-head version. Its side projection is locked during surface fairing.
sculpt_longitudinal_surface(fused, body_only)
from short_round_face_sculpt import sculpt_short_round_face
sculpt_short_round_face(fused, body_only)
from broad_face_arc_sculpt import sculpt_broad_face_arc
sculpt_broad_face_arc(fused, body_only)
from lip_eye_sculpt import sculpt_lip_eye
sculpt_lip_eye(fused, body_only)
from continuous_front_face_sculpt import sculpt_continuous_front_face
sculpt_continuous_front_face(fused, body_only)
from eye_muzzle_sculpt import sculpt_eye_muzzle, baseline_record
from body_finish_sculpt import finish_body
eye_muzzle_record=baseline_record()
sculpt_eye_muzzle(fused, body_only, eye_muzzle_record)
finish_body(fused, body_only, eye_muzzle_record['height'], eye_muzzle_record['D'], eye_muzzle_record)

# Armature, with separated limbs so it can be used for a walk or laugh cycle.
bpy.ops.object.armature_add(enter_editmode=True, location=(0,0,0))
rig=bpy.context.object; rig.name='Naiwa_Rig'
arm=rig.data; arm.name='Naiwa_Skeleton'
arm.edit_bones.remove(arm.edit_bones[0])
def bone(name,head,tail,parent=None,connected=False):
    b=arm.edit_bones.new(name); b.head=head; b.tail=tail
    if parent: b.parent=arm.edit_bones[parent]; b.use_connect=connected
    return b
bone('Root',(0,0,0),(0,0,0.40))
bone('Torso',(0,0,torso_z(0.62)),(0,0,2.38),'Root')
for side,s in [('L',-1),('R',1)]:
    bone(f'UpperArm.{side}',(s*0.455,0.012,1.875),(s*0.727,0.005,1.36),'Torso')
    bone(f'Forearm.{side}',(s*0.727,0.005,1.36),(s*0.77,-0.032,1.015),f'UpperArm.{side}')
    bone(f'Thigh.{side}',(s*0.32,0.015,leg_z(0.78)),(s*0.34,0.005,leg_z(0.44)),'Root')
    bone(f'Shin.{side}',(s*0.34,0.005,leg_z(0.44)),(s*0.35,0.000,0.09),f'Thigh.{side}')
bpy.ops.object.mode_set(mode='OBJECT')
rig.show_in_front=True
for o in character:
    if o.type!='MESH': continue
    if o['rig_bone']=='AUTO': continue
    vg=o.vertex_groups.new(name=o['rig_bone'])
    vg.add(list(range(len(o.data.vertices))),1.0,'REPLACE')
    mod=o.modifiers.new('Naiwa armature','ARMATURE'); mod.object=rig
    o.parent=rig
bpy.ops.object.select_all(action='DESELECT')
fused.select_set(True); rig.select_set(True); bpy.context.view_layer.objects.active=rig
bpy.ops.object.parent_set(type='ARMATURE_AUTO')

# Lightweight in-place animation clips: motion is driven by the UE Character capsule.
for p in rig.pose.bones: p.rotation_mode='XYZ'
rig.animation_data_create()
def pose_key(frame,vals):
    for pb in rig.pose.bones:
        pb.rotation_euler=vals.get(pb.name,(0,0,0))
        pb.keyframe_insert(data_path='rotation_euler',frame=frame,group=pb.name)

def action(name,frames):
    for pb in rig.pose.bones: pb.rotation_euler=(0,0,0)
    act=bpy.data.actions.new(name); rig.animation_data.action=act
    for f,vals in frames: pose_key(f,vals)
    act.use_fake_user=True
    return act
idle=action('Naiwa_Idle',[(1,{'Torso':(0,0,0)}),(16,{'Torso':(0.025,0,0)}),(31,{'Torso':(0,0,0)})])
walk_frames=[]
for f,sgn in [(1,1),(9,-1),(17,1),(25,-1),(33,1)]:
    walk_frames.append((f,{'Thigh.L':(0.33*sgn,0,0),'Thigh.R':(-0.33*sgn,0,0),
        'Shin.L':(-0.12*sgn,0,0),'Shin.R':(0.12*sgn,0,0),
        'UpperArm.L':(-0.18*sgn,0,0),'UpperArm.R':(0.18*sgn,0,0),
        'Torso':(0.025,0,0)}))
walk=action('Naiwa_Walk',walk_frames)
laugh=action('Naiwa_Laugh',[(1,{'Torso':(0.04,0,0)}),(7,{'Torso':(-0.14,0,0),'UpperArm.L':(-0.14,0,0),'UpperArm.R':(-0.14,0,0)}),(13,{'Torso':(0.10,0,0)}),(19,{'Torso':(-0.14,0,0),'UpperArm.L':(-0.14,0,0),'UpperArm.R':(-0.14,0,0)}),(25,{'Torso':(0.04,0,0)})])
jump=action('Naiwa_Jump',[(1,{'Torso':(0,0,0)}),(5,{'Torso':(-0.13,0,0),'Thigh.L':(0.20,0,0),'Thigh.R':(0.20,0,0)}),(12,{'Torso':(0.05,0,0),'Thigh.L':(-0.12,0,0),'Thigh.R':(-0.12,0,0)}),(20,{'Torso':(0,0,0)})])
rig.animation_data.action=idle
bpy.context.scene.frame_set(1)

# World and render setup. Only rig and character are selected for FBX export.
world=bpy.data.worlds.new('Warm studio'); bpy.context.scene.world=world; world.use_nodes=True
next(n for n in world.node_tree.nodes if n.type == 'BACKGROUND').inputs['Color'].default_value=(0.13,0.15,0.17,1)
next(n for n in world.node_tree.nodes if n.type == 'BACKGROUND').inputs['Strength'].default_value=0.7
bg=mat('Backdrop',(0.81,0.79,0.72),0.9)
bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-0.005)); plane=bpy.context.object; plane.name='Preview floor'; plane.data.materials.append(bg)

def light(name,loc,power,size):
    dat=bpy.data.lights.new(name,'AREA'); dat.energy=power; dat.shape='DISK'; dat.size=size
    o=bpy.data.objects.new(name,dat); bpy.context.collection.objects.link(o); o.location=loc
    o.rotation_euler=(Vector((0,0,1.15))-o.location).to_track_quat('-Z','Y').to_euler()
light('Large warm key',(4,-5,6),520,5)
light('Soft fill',(-4,-3,4),310,4)
light('Broad lower fill',(0,-5,1.3),230,6)
light('Rim',(0,3,5),620,3)
cam_data=bpy.data.cameras.new('Preview camera'); cam=bpy.data.objects.new('Preview camera',cam_data)
bpy.context.collection.objects.link(cam); bpy.context.scene.camera=cam
cam_data.type='ORTHO'; cam_data.ortho_scale=3.45
scene=bpy.context.scene
scene.render.engine='BLENDER_EEVEE_NEXT' if 'BLENDER_EEVEE_NEXT' in bpy.app.build_options.__dir__() else 'BLENDER_EEVEE'
scene.render.resolution_x=900; scene.render.resolution_y=1000; scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'; scene.render.film_transparent=False
scene.view_settings.view_transform='Standard'
scene.view_settings.exposure=-0.80
from render_surface_diagnostics import configure_soft_lighting
configure_soft_lighting(scene)

def render(name,where):
    cam.location=where; cam.rotation_euler=(Vector((0,0,1.31))-cam.location).to_track_quat('-Z','Y').to_euler()
    scene.render.filepath=str(OUT/name)
    bpy.ops.render.render(write_still=True)
render('naiwa_front.png',(0,-7,1.31))
render('naiwa_three_quarter.png',(7*math.sin(math.radians(35)),-7*math.cos(math.radians(35)),1.31))
render('naiwa_side.png',(7,0,1.31))
# A single-color render separates actual surface ripples from color gradients.
diagnostic=mat('Plain yellow diagnostic',(0.94,0.66,0.13),0.72)
view_layer=bpy.context.view_layer
view_layer.material_override=diagnostic
render('naiwa_surface_check_side.png',(7,0,1.31))
view_layer.material_override=None
# Flat black profile checks make the head/shoulder and belly contours easier
# to judge before inspecting material, eyes, hands and feet.
silhouette=mat('Silhouette preview black',(0,0,0),1.0)
old_background=tuple(next(n for n in world.node_tree.nodes if n.type == 'BACKGROUND').inputs['Color'].default_value)
old_strength=next(n for n in world.node_tree.nodes if n.type == 'BACKGROUND').inputs['Strength'].default_value
view_layer.material_override=silhouette
plane.hide_render=True
next(n for n in world.node_tree.nodes if n.type == 'BACKGROUND').inputs['Color'].default_value=(1,1,1,1)
next(n for n in world.node_tree.nodes if n.type == 'BACKGROUND').inputs['Strength'].default_value=1.0
render('naiwa_silhouette_front.png',(0,-7,1.31))
render('naiwa_silhouette_side.png',(7,0,1.31))
# Isolate the head-torso shell for the decisive rear-neck silhouette check.
old_hidden=[(o,o.hide_render) for o in character if o.type=='MESH']
for o,_ in old_hidden: o.hide_render=True
body_only.hide_render=False
render('naiwa_torso_silhouette_front.png',(0,-7,1.31))
render('naiwa_torso_silhouette_side.png',(7,0,1.31))
body_only.hide_render=True
for o,was_hidden in old_hidden: o.hide_render=was_hidden
view_layer.material_override=None
plane.hide_render=False
next(n for n in world.node_tree.nodes if n.type == 'BACKGROUND').inputs['Color'].default_value=old_background
next(n for n in world.node_tree.nodes if n.type == 'BACKGROUND').inputs['Strength'].default_value=old_strength
rig.animation_data.action=idle; scene.frame_set(1)

# Keep a ready-to-edit native scene and a UE-importable skinned FBX.
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Naiwa_Character.blend'))
bpy.ops.object.select_all(action='DESELECT')
for o in character: o.select_set(True)
rig.select_set(True); bpy.context.view_layer.objects.active=rig
bpy.ops.export_scene.fbx(filepath=str(OUT/'Naiwa_Character.fbx'),use_selection=True,
    object_types={'ARMATURE','MESH'},add_leaf_bones=False,apply_unit_scale=True,
    bake_anim=True,bake_anim_use_all_actions=True,bake_anim_use_nla_strips=False,
    path_mode='AUTO')
print('NAIWA_BUILD_OK',len(character),len(bpy.data.actions))



