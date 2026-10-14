"""Independent saved-mesh checks, including stage geometry and paint contours."""
import bpy,sys,json,runpy,argparse,math
from pathlib import Path
import numpy as np
from mathutils import Vector

ROOT=Path(__file__).resolve().parent
parser=argparse.ArgumentParser()
parser.add_argument('--target',type=Path,default=ROOT/'versions/20261005_eye_muzzle_body_delivery/Naiwa_Character.blend')
parser.add_argument('--output',type=Path)
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
target=args.target.resolve();out=args.output or target.parent/'delivery_validation.json'
saved_argv=sys.argv[:]
sys.argv=[str(ROOT/'validate_eye_muzzle_revision.py'),'--','--target',str(target)]
try:
    checked=runpy.run_path(str(ROOT/'validate_eye_muzzle_revision.py'),run_name='__main__')
finally:
    sys.argv=saved_argv
body=checked['body'];previous=checked['old'][0];D=checked['D'];H=checked['H'];rec=checked['rec']
report=checked['report']
collection=bpy.data.collections.new('Independent stage and boundary measurements')
bpy.context.scene.collection.children.link(collection)

def stage(name):
    path=target.parent/(name+'.blend')
    with bpy.data.libraries.load(str(path),link=False) as (source,loaded):
        loaded.objects=['Naiwa unified golden body']
    obj=loaded.objects[0];collection.objects.link(obj);obj.hide_render=True
    return obj

def depth(obj,x,z,rear=False):
    hit,p,_,_=obj.ray_cast(Vector((float(x),2 if rear else -2,float(z))),Vector((0,-1 if rear else 1,0)))
    return float(p.y) if hit else None

raised=stage('stage_face_raise');muzzle=stage('stage_muzzle')
bpy.context.scene.frame_set(1);bpy.context.view_layer.update()
mz=rec['mouth_center'][2]+.4*D;tz=mz-.9*D
advance=depth(raised,0,mz)-depth(muzzle,0,mz)
retraction=depth(muzzle,0,tz)-depth(raised,0,tz)
assert abs(advance-.35*D)<.005*D,('Short muzzle advance differs',advance,.35*D)
assert abs(retraction-.15*D)<.005*D,('Throat retraction differs',retraction,.15*D)
final_recess=depth(body,0,mz)-depth(muzzle,0,mz)
assert final_recess>.025*D,('Mouth has no real geometric recess',final_recess)
report['stage_geometry']={'muzzle_F_advance':advance,'throat_minus_F_retraction':retraction,
    'mouth_z':mz,'throat_z':tz,'final_mouth_geometric_recess':final_recess,
    'source':'Independent rays on saved stage_face_raise.blend, stage_muzzle.blend and final body'}

skin=np.array((.94,.66,.13));cream=np.array((.70,.54,.33));direction=cream-skin
def vertex_colors(obj):
    attr=obj.data.color_attributes['NaiwaColor']
    corners=np.empty(len(attr.data)*4,dtype=np.float32);attr.data.foreach_get('color',corners)
    corners=corners.reshape(-1,4)
    ids=np.empty(len(obj.data.loops),dtype=np.int32);obj.data.loops.foreach_get('vertex_index',ids)
    colors=np.zeros((len(obj.data.vertices),4));np.add.at(colors,ids,corners)
    colors/=np.maximum(1,np.bincount(ids,minlength=len(colors)))[:,None]
    return colors,corners,ids

def amount(obj,colors,x,z):
    hit,p,_,face=obj.ray_cast(Vector((float(x),-2,float(z))),Vector((0,1,0)))
    if not hit:return 0.
    ids=list(obj.data.polygons[face].vertices)
    for k in range(1,len(ids)-1):
        tri=(ids[0],ids[k],ids[k+1]);a,b,c=(obj.data.vertices[i].co for i in tri)
        u,v,w=b-a,c-a,p-a
        uu,uv,vv,wu,wv=u.dot(u),u.dot(v),v.dot(v),w.dot(u),w.dot(v)
        det=uu*vv-uv*uv
        if abs(det)<1e-20:continue
        t=(vv*wu-uv*wv)/det;s=(uu*wv-uv*wu)/det
        if t>=-.001 and s>=-.001 and t+s<=1.001:
            rgb=(1-t-s)*colors[tri[0],:3]+t*colors[tri[1],:3]+s*colors[tri[2],:3]
            return float(np.clip(np.dot(rgb-skin,direction)/np.dot(direction,direction),0,1))
    return float(np.clip(np.dot(colors[ids,:3].mean(axis=0)-skin,direction)/np.dot(direction,direction),0,1))

def contour(obj,colors):
    widths=[]
    for z in np.arange(.93,1.90,.003):
        if amount(obj,colors,0,z)<.5:continue
        edges=[]
        for sign in (-1,1):
            lo,hi=0.,.68
            for _ in range(17):
                mid=(lo+hi)/2
                if amount(obj,colors,sign*mid,z)>=.5:lo=mid
                else:hi=mid
            edges.append(sign*(lo+hi)/2)
        widths.append((edges[1]-edges[0],z,(edges[1]+edges[0])/2))
    assert widths,'Belly color outline missing'
    width,peak,center=max(widths)
    def crossing(lo,hi,lower):
        for _ in range(23):
            mid=(lo+hi)/2;inside=amount(obj,colors,0,mid)>=.5
            if inside==lower:hi=mid
            else:lo=mid
        return (lo+hi)/2
    return {'threshold':.5,'width':width,'lower_z':crossing(.75,peak,True),
            'upper_z':crossing(peak,2.02,False),'center_x':center,'peak_width_z':peak}

oldcolors,oldcorners,oldids=vertex_colors(previous)
newcolors,newcorners,newids=vertex_colors(body)
original=contour(previous,oldcolors);painted=contour(body,newcolors)
ratio=painted['width']/original['width'];extension=original['lower_z']-painted['lower_z']
assert abs(ratio-.90)<.003,('Belly color width ratio differs',ratio)
assert abs(extension-.025*H)<.003,('Belly lower boundary differs',extension,.025*H)
assert abs(painted['upper_z']-original['upper_z'])<.003,'Belly upper boundary moved'
assert abs(painted['center_x'])<.001,'Belly color is no longer centered'
coords=checked['after'][newids];x=np.abs(coords[:,0]);z=coords[:,2]
paint_domain=((z>.92)&(z<1.85)&(x<.64)&(coords[:,1]<-.15))|((z<.235)&(x>.17)&(x<.55))
assert np.array_equal(oldcorners[~paint_domain],newcorners[~paint_domain]),'Colors changed outside belly/feet'
assert np.array_equal(oldcorners[:,3],newcorners[:,3]),'Vertex color alpha changed'
report['belly_color_contour']={'before':original,'after':painted,'actual_width_ratio':ratio,
    'actual_lower_extension':extension,'requested_lower_extension':.025*H,
    'color_outside_paint_domain_exact':True,'alpha_exact':True,'body_volume_exact':True,
    'source':'Independent front-visible mesh color interpolation and 50% boundary bisection'}

def shoulder_grooves(obj):
    result=[]
    for rear in (False,True):
        rows=[]
        for z in np.arange(1.74,1.926,.012):
            center=.62-.47*(z-1.50)
            for x in np.arange(center-.07,center+.071,.012):
                for sign in (-1,1):
                    c=depth(obj,sign*x,z,rear)
                    neighbours=[depth(obj,sign*(x+dx),z+dz,rear) for dx,dz in
                                [(-.035,0),(.035,0),(0,-.035),(0,.035)]]
                    if c is None or any(v is None for v in neighbours):continue
                    departure=(np.mean(neighbours)-c) if rear else (c-np.mean(neighbours))
                    rows.append(max(0,float(departure)))
        result.append(np.array(rows))
    return result

oldgrooves=shoulder_grooves(previous);newgrooves=shoulder_grooves(body)
shoulders={}
for label,a,b in zip(('anterior','posterior'),oldgrooves,newgrooves):
    assert len(a)==len(b) and len(a)
    selected=a>.001
    before_mean=float(a[selected].mean());after_mean=float(b[selected].mean())
    assert after_mean<before_mean,('Shoulder groove depth has not improved',label,before_mean,after_mean)
    shoulders[label]={'original_groove_mean':before_mean,'modified_groove_mean':after_mean,
                     'improvement_percent':100*(1-after_mean/before_mean),'samples':int(selected.sum())}
report['shoulder_chord_measurements']=shoulders
report['independent_validation']='passed'
out.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
print('DELIVERY_INDEPENDENT_VALIDATION_OK',json.dumps({
    'stage_geometry':report['stage_geometry'],'belly_color_contour':report['belly_color_contour'],
    'shoulders':shoulders},ensure_ascii=False))
