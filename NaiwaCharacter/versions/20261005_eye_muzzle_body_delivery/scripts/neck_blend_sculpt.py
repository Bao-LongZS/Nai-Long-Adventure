"""Blend both sides of the raised-neck seams while holding the narrower middle."""
import math
import bpy
from mathutils import Vector
from connection_sculpt import section

BLEND_MIN_Z=1.94
CORE_LOW_Z=2.18
CORE_HIGH_Z=2.255
FRONT_TOP_Z=2.354
REAR_TOP_Z=2.44

def ease(a,b,value):
    t=max(0.0,min(1.0,(value-a)/(b-a)))
    return t*t*t*(10-15*t+6*t*t)

def blend_neck_seams(body,shell=None):
    assert 'neck_seams_blended' not in body, 'Start from the saved previous iteration.'
    def deform(obj):
        profile_obj=shell if obj is body and shell is not None else obj
        count=180
        profiles=[section(profile_obj,BLEND_MIN_Z+(REAR_TOP_Z-BLEND_MIN_Z)*i/count) for i in range(count+1)]
        original=[v.co.copy() for v in obj.data.vertices]
        weights=[]
        for p in original:
            z=p.z
            if not BLEND_MIN_Z<z<REAR_TOP_Z:
                weights.append(0.0)
                continue
            index=(z-BLEND_MIN_Z)/(REAR_TOP_Z-BLEND_MIN_Z)*count
            lo=min(count-1,int(index)); q=index-lo
            width,front,rear=[profiles[lo][k]*(1-q)+profiles[lo+1][k]*q for k in range(3)]
            ux=p.x/width; uy=(p.y-(front+rear)/2)/((rear-front)/2)
            angle=math.atan2(uy,ux)
            anterior=max(0.0,-math.sin(angle))**4
            top=REAR_TOP_Z-(REAR_TOP_Z-FRONT_TOP_Z)*anterior
            lower=ease(BLEND_MIN_Z,BLEND_MIN_Z+0.070,z)*(1-ease(CORE_LOW_Z-0.070,CORE_LOW_Z,z))
            upper=ease(CORE_HIGH_Z,CORE_HIGH_Z+0.045,z)*(1-ease(top-0.060,top,z))
            # Chest/back enter the lower blend; lateral shoulders stay fixed.
            lower*=math.sin(angle)**4+(1-math.sin(angle)**4)*ease(2.025,2.11,z)
            radius=math.hypot(ux,uy)
            outside=1-ease(1.06,1.23,radius)
            weight=max(lower,upper)*outside
            if z<=2.09 and abs(p.x)>=0.395 and -0.12<=p.y<=0.19:
                weight=0.0
            weights.append(weight)
        bpy.context.view_layer.objects.active=obj
        obj.hide_set(False)
        group=obj.vertex_groups.new(name='Broad lower-face neck and upper-chest blend')
        for index,weight in enumerate(weights):
            if weight>0:
                group.add([index],weight,'REPLACE')
        smooth=obj.modifiers.new('Broad seam geometry blend','SMOOTH')
        smooth.vertex_group=group.name
        smooth.factor=0.60
        smooth.iterations=400
        while obj.modifiers.find(smooth.name)>0:
            bpy.ops.object.modifier_move_up(modifier=smooth.name)
        bpy.ops.object.modifier_apply(modifier=smooth.name)
        obj.data.update()
        for poly in obj.data.polygons:
            poly.use_smooth=True
        obj.data.update()
        # Blender regenerates normals from the changed coordinates.
        assert not obj.data.has_custom_normals
        fixed=[i for i,w in enumerate(weights) if w==0.0]
        assert all(obj.data.vertices[i].co==original[i] for i in fixed)
        return {'changed_vertex_count':sum(w>0 for w in weights),
                'fixed_vertex_count':len(fixed),
                'max_displacement':max((v.co-original[v.index]).length for v in obj.data.vertices)}
    report=deform(body)
    if shell is not None:
        deform(shell)
        shell.hide_set(True)
    body['neck_seams_blended']=True
    body['preserved_body_below_z']=BLEND_MIN_Z
    body['preserved_crown_above_z']=REAR_TOP_Z
    body['head_revision']='Broad lower-face and upper-chest seam blending with narrow neck middle retained'
    body['neck_core_preserved_range']=(CORE_LOW_Z,CORE_HIGH_Z)
    body['neck_blend_height_range']=(BLEND_MIN_Z,REAR_TOP_Z)
    print('BROAD_NECK_BLEND_OK',report)
    return report
