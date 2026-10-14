"""Preserve head/height, advance it, and redistribute the neck shape over its length."""
import math
import bpy
import numpy as np
from mathutils import Vector
from connection_sculpt import section

FORWARD_Y=-0.055
BASE_Z=1.98
FULL_FORWARD_Z=2.325
FRONT_HEAD_Z=2.335
REAR_HEAD_Z=2.405
SIDE_HEAD_Z=(FRONT_HEAD_Z+REAR_HEAD_Z)/2
WIDTH_BASE_Z=2.08
WIDTH_TURN_Z=2.22
WIDTH_TURN=0.285
FRONT_PIVOT_Z=2.19

def ease(a,b,value):
    t=max(0.0,min(1.0,(value-a)/(b-a)))
    return t*t*t*(10-15*t+6*t*t)

def head_boundary(angle):
    return FRONT_HEAD_Z+(REAR_HEAD_Z-FRONT_HEAD_Z)*(1+math.sin(angle))/2

def quintic(z,a,b,va,vb,da,db,dda,ddb):
    t=(z-a)/(b-a); h=b-a
    c0=va; c1=da*h; c2=dda*h*h/2
    A=vb-c0-c1-c2; B=db*h-c1-2*c2; C=ddb*h*h-2*c2
    return c0+c1*t+c2*t*t+(10*A-4*B+C/2)*t**3+(-15*A+7*B-C)*t**4+(6*A-3*B+C/2)*t**5

def source_profiles(obj):
    zs=np.linspace(1.94,2.445,265)
    rows=np.array([section(obj,float(z)) for z in zs])
    def at(z):
        return tuple(float(np.interp(z,zs,rows[:,k])) for k in range(3))
    def jets(z,k,shift=False):
        data=rows[:,k].copy()
        if shift:
            data+=np.array([FORWARD_Y*ease(BASE_Z,FULL_FORWARD_Z,float(v)) for v in zs])
        mask=np.abs(zs-z)<=0.024
        aa,bb,cc=np.polyfit(zs[mask]-z,data[mask],2)
        value=at(z)[k]+(FORWARD_Y*ease(BASE_Z,FULL_FORWARD_Z,z) if shift else 0)
        return value,float(bb),float(2*aa)
    return at,jets

def point_angle(p,profile):
    width,front,rear=profile(p.z)
    return math.atan2((p.y-(front+rear)/2)/((rear-front)/2),p.x/width)

def rigid_head_indices(obj):
    profile,_=source_profiles(obj)
    return [v.index for v in obj.data.vertices
            if v.co.z>=REAR_HEAD_Z or
            (v.co.z>FRONT_HEAD_Z and v.co.z>=head_boundary(point_angle(v.co,profile)))]

def reshape_neck_direction(body,shell=None,move_features=True):
    assert 'head_forward_neck_reshaped' not in body, 'Start from the preceding saved iteration.'
    def deform(obj):
        profile,jets=source_profiles(obj)
        original=[v.co.copy() for v in obj.data.vertices]
        width0,wd0,wdd0=jets(WIDTH_BASE_Z,0)
        width1,wd1,wdd1=jets(SIDE_HEAD_Z,0)
        wd0=max(-0.85,min(-0.30,wd0)); wdd0=max(-4.0,min(4.0,wdd0))
        wd1=max(-0.48,min(-0.20,wd1)); wdd1=max(-2.0,min(2.0,wdd1))
        def width_target(z):
            if z<=WIDTH_TURN_Z:
                return quintic(z,WIDTH_BASE_Z,WIDTH_TURN_Z,width0,WIDTH_TURN,wd0,-0.12,wdd0,0)
            return quintic(z,WIDTH_TURN_Z,SIDE_HEAD_Z,WIDTH_TURN,width1,-0.12,wd1,0,wdd1)
        width_samples=[width_target(z) for z in np.linspace(WIDTH_BASE_Z,SIDE_HEAD_Z,401)]
        assert all(b<=a+1e-7 for a,b in zip(width_samples,width_samples[1:])), 'Width goal reverses into a collar'
        fa,fd0,fdd0=jets(2.0,1,True)
        fb,fd1,fdd1=jets(FRONT_HEAD_Z,1,True)
        throat=profile(FRONT_PIVOT_Z)[1]+FORWARD_Y*ease(BASE_Z,FULL_FORWARD_Z,FRONT_PIVOT_Z)+0.014
        def front_target(z):
            if z<=FRONT_PIVOT_Z:
                return quintic(z,2.0,FRONT_PIVOT_Z,fa,throat,fd0,0,fdd0,-4)
            return quintic(z,FRONT_PIVOT_Z,FRONT_HEAD_Z,throat,fb,0,fd1,-4,fdd1)
        ra,rd0,rdd0=jets(BASE_Z,2,True)
        rb,rd1,rdd1=jets(REAR_HEAD_Z,2,True)
        def rear_target(z):
            return quintic(z,BASE_Z,REAR_HEAD_Z,ra,rb,rd0,rd1,rdd0,rdd1)
        rear_samples=[rear_target(z) for z in np.linspace(BASE_Z,REAR_HEAD_Z,401)]
        assert all(b<=a+1e-7 for a,b in zip(rear_samples,rear_samples[1:])), 'Rear goal is not a continuous sloping arc'
        head_ids=[]; neck_ids=[]; fixed_ids=[]; finish_weights=[]
        for index,p0 in enumerate(original):
            p=obj.data.vertices[index].co; z=p0.z
            if z<=BASE_Z or (z<=2.09 and abs(p0.x)>=0.395 and -0.12<=p0.y<=0.19):
                fixed_ids.append(index); finish_weights.append(0.0); continue
            if z>=REAR_HEAD_Z:
                p.y=p0.y+FORWARD_Y
                head_ids.append(index); finish_weights.append(0.0); continue
            angle=point_angle(p0,profile)
            cut=head_boundary(angle)
            if z>=cut:
                p.y=p0.y+FORWARD_Y
                head_ids.append(index); finish_weights.append(0.0); continue
            width,front,rear=profile(z)
            lower_carry=math.sin(angle)**4+(1-math.sin(angle)**4)*ease(2.025,2.11,z)
            translation=FORWARD_Y*ease(BASE_Z,FULL_FORWARD_Z,z)*lower_carry
            p.y=p0.y+translation
            upper_feather=1-ease(cut-0.060,cut,z)
            if WIDTH_BASE_Z<z<SIDE_HEAD_Z:
                w=width_target(z)
                mask=ease(WIDTH_BASE_Z,WIDTH_BASE_Z+0.040,z)*upper_feather
                p.x=p0.x+(p0.x*w/width-p0.x)*mask
            if 2.0<z<FRONT_HEAD_Z:
                delta=front_target(z)-(front+FORWARD_Y*ease(BASE_Z,FULL_FORWARD_Z,z))
                p.y+=delta*max(0.0,-math.sin(angle))**4*ease(2.0,2.045,z)*upper_feather
            if BASE_Z<z<REAR_HEAD_Z:
                delta=rear_target(z)-(rear+FORWARD_Y*ease(BASE_Z,FULL_FORWARD_Z,z))
                p.y+=delta*ease(0.0,1.0,max(0.0,math.sin(angle)))*ease(BASE_Z,BASE_Z+0.05,z)*upper_feather
            weight=ease(BASE_Z,BASE_Z+0.075,z)*upper_feather*lower_carry
            finish_weights.append(weight)
            neck_ids.append(index)
        obj.data.update()
        # Only XY may relax: the head height and the added neck length are fixed.
        bpy.context.view_layer.objects.active=obj
        obj.hide_set(False)
        group=obj.vertex_groups.new(name='Forward neck XY surface finish')
        for i,w in enumerate(finish_weights):
            if w>0:
                group.add([i],w,'REPLACE')
        smooth=obj.modifiers.new('Forward neck XY surface finish','SMOOTH')
        smooth.vertex_group=group.name
        smooth.factor=0.40
        smooth.iterations=256
        smooth.use_x=True; smooth.use_y=True; smooth.use_z=False
        while obj.modifiers.find(smooth.name)>0:
            bpy.ops.object.modifier_move_up(modifier=smooth.name)
        bpy.ops.object.modifier_apply(modifier=smooth.name)
        obj.data.update()
        assert all(v.co.z==original[v.index].z for v in obj.data.vertices)
        assert all(obj.data.vertices[i].co==original[i] for i in fixed_ids)
        assert all((obj.data.vertices[i].co-original[i]-Vector((0,FORWARD_Y,0))).length<1e-6 for i in head_ids)
        return {'rigid_head_indices':head_ids,'neck_indices':neck_ids,'fixed_indices':fixed_ids,
                'width_turn_goal':WIDTH_TURN,'front_throat_goal':throat}
    result=deform(body)
    if shell is not None:
        deform(shell)
        shell.hide_set(True)
    if move_features:
        for name in ('L concentric iris and pupil','R concentric iris and pupil','Short closed oral seam'):
            feature=bpy.data.objects[name]
            for v in feature.data.vertices:
                v.co.y+=FORWARD_Y
            feature.data.update()
            for key in ('frontal_pupil_center','head_reduction_anchor'):
                if key in feature:
                    p=Vector(feature[key]); p.y+=FORWARD_Y
                    feature[key]=tuple(p)
        for name in ('L eye glint','R eye glint'):
            bpy.data.objects[name].location.y+=FORWARD_Y
    body['head_forward_neck_reshaped']=True
    body['head_forward_offset_y']=FORWARD_Y
    body['neck_direction_height_range']=(BASE_Z,REAR_HEAD_Z)
    body['preserved_body_below_z']=BASE_Z
    body['preserved_crown_above_z']=REAR_HEAD_Z
    body['head_revision']='Rigid forward head; fixed height; narrower upper neck and distributed sloping nape'
    print('FORWARD_NECK_SCULPT_OK','head=',len(result['rigid_head_indices']),'neck=',len(result['neck_indices']))
    return result


