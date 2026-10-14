"""Extend only the original connection strip; translate the complete head rigidly."""
import bpy
import math
from mathutils import Vector
from connection_sculpt import section

NECK_BASE_Z=2.08
HEAD_BASE_Z=2.185
HEAD_MAX_BASE_Z=HEAD_BASE_Z
HEAD_RAISE=0.14
NECK_TOP_Z=HEAD_BASE_Z+HEAD_RAISE
REAR_ATTACHMENT_RISE=0.0

def ease(a,b,value):
    t=max(0.0,min(1.0,(value-a)/(b-a)))
    return t*t*t*(10-15*t+6*t*t)

def neck_length_fraction(t,angle):
    # A broader derivative plateau distributes the added length smoothly.
    u=2*t-1
    fraction=(u-0.4*u**5+u**9/9+32/45)/(64/45)
    # Offset front/side/back curvature bands without changing either boundary.
    return fraction+0.08*math.sin(angle)*64*t**3*(1-t)**3

def attachment_height(angle):
    return HEAD_BASE_Z

def rigid_head_indices(obj):
    return [v.index for v in obj.data.vertices if v.co.z>=HEAD_BASE_Z]

def extend_neck(body,shell=None,move_features=True):
    assert 'head_rigid_raise' not in body, 'Always start from the saved previous iteration.'
    def deform(obj):
        count=160
        profiles=[section(obj,NECK_BASE_Z+(HEAD_BASE_Z-NECK_BASE_Z)*i/count) for i in range(count+1)]
        head_ids=[]; neck_ids=[]
        for v in obj.data.vertices:
            p=v.co; old_z=p.z
            if old_z<=NECK_BASE_Z:
                continue
            if old_z>=HEAD_BASE_Z:
                p.z+=HEAD_RAISE
                head_ids.append(v.index)
                continue
            t=(old_z-NECK_BASE_Z)/(HEAD_BASE_Z-NECK_BASE_Z)
            index=t*count; lo=min(count-1,int(index)); q=index-lo
            width,front,rear=[profiles[lo][k]*(1-q)+profiles[lo+1][k]*q for k in range(3)]
            angle=math.atan2((p.y-(front+rear)/2)/((rear-front)/2),p.x/width)
            bump=64*t**3*(1-t)**3
            # Broad front-only setback has zero value, slope and curvature at
            # both ends; it cannot form an annular groove.
            p.y+=0.030*bump*max(0.0,-math.sin(angle))**6
            side_taper=(t/0.75)**9*((1-t)/0.25)**3
            p.x*=1-0.015*side_taper*math.cos(angle)**2
            p.z+=HEAD_RAISE*neck_length_fraction(t,angle)
            neck_ids.append(v.index)
        obj.data.update()
        return head_ids,neck_ids
    head_ids,neck_ids=deform(body)
    if shell is not None:
        deform(shell)
    if move_features:
        for name in ('L concentric iris and pupil','R concentric iris and pupil','Short closed oral seam'):
            feature=bpy.data.objects[name]
            for v in feature.data.vertices:
                v.co.z+=HEAD_RAISE
            feature.data.update()
            for key in ('frontal_pupil_center','head_reduction_anchor'):
                if key in feature:
                    p=Vector(feature[key]); p.z+=HEAD_RAISE
                    feature[key]=tuple(p)
        for name in ('L eye glint','R eye glint'):
            bpy.data.objects[name].location.z+=HEAD_RAISE
    body['head_revision']='Rigid raised complete head with smoothly extended connection above fixed shoulders'
    body['preserved_body_below_z']=NECK_BASE_Z
    body['rigid_head_original_front_base_z']=HEAD_BASE_Z
    body['rigid_head_original_rear_base_z']=HEAD_BASE_Z
    body['head_rigid_raise']=HEAD_RAISE
    body['neck_section_length']=NECK_TOP_Z-NECK_BASE_Z
    print('NECK_EXTENSION_SCULPT_OK','head_vertices=',len(head_ids),'neck_vertices=',len(neck_ids),'raise=',HEAD_RAISE)
    return {'rigid_head_indices':head_ids,'neck_indices':neck_ids}


