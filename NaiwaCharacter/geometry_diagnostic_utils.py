"""World-space evaluated triangle meshes and exact planar intersections."""
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

def array(items,key,width,dtype):
    data=np.empty(len(items)*width,dtype=dtype);items.foreach_get(key,data)
    return data.reshape((-1,width))

def mesh_record(obj,depsgraph,eye=False):
    evaluated=obj.evaluated_get(depsgraph)
    mesh=evaluated.to_mesh(preserve_all_data_layers=True,depsgraph=depsgraph)
    mesh.calc_loop_triangles()
    points=array(mesh.vertices,'co',3,np.float32).astype(np.float64)
    matrix=np.asarray(evaluated.matrix_world,dtype=float)
    points=points@matrix[:3,:3].T+matrix[:3,3]
    triangles=array(mesh.loop_triangles,'vertices',3,np.int32)
    labels=np.full(len(triangles),'yellow_body',dtype=object)
    if eye:
        loops=array(mesh.loop_triangles,'loops',3,np.int32)
        colors=array(mesh.color_attributes['EyeColor'].data,'color',4,np.float32)
        labels=np.where(colors[loops[:,0],1]<.04,'black','green')
    tp=points[triangles]
    normals=np.cross(tp[:,1]-tp[:,0],tp[:,2]-tp[:,0])
    lengths=np.linalg.norm(normals,axis=1)
    normals/=np.maximum(lengths[:,None],1e-20)
    return {'name':obj.name,'points':points,'triangles':triangles,'triangle_points':tp,
            'regions':labels,'triangle_normals':normals,
            'bvh':BVHTree.FromPolygons(points.tolist(),triangles.tolist(),all_triangles=True)}

def face_point(record,x,z):
    p,n,idx,d=record['bvh'].ray_cast(Vector((float(x),-3,float(z))),Vector((0,1,0)),6)
    return np.asarray(p,dtype=float) if p is not None else None

def intersections(record,axis,value):
    tp=record['triangle_points'];distance=tp[:,:,axis]-value
    selected=np.flatnonzero((distance.min(1)<=1e-10)&(distance.max(1)>=-1e-10))
    result=[];seen=set()
    for idx in selected:
        triangle=tp[idx];delta=distance[idx];points=[]
        for i,j in ((0,1),(1,2),(2,0)):
            if abs(delta[i])<1e-10:points.append(triangle[i].copy())
            if delta[i]*delta[j]<0:
                points.append(triangle[i]+(triangle[j]-triangle[i])*delta[i]/(delta[i]-delta[j]))
        unique={tuple(np.round(p,10)):p for p in points}
        points=list(unique.values())
        if len(points)<2:continue
        # Coplanar triangle edges are handled separately and deduplicated.
        pairs=[(points[0],points[1])] if len(points)==2 else [(triangle[i],triangle[j]) for i,j in ((0,1),(1,2),(2,0))]
        for a,b in pairs:
            a=a.copy();b=b.copy();a[axis]=b[axis]=value
            ka,kb=tuple(np.round(a,7)),tuple(np.round(b,7))
            if ka==kb:continue
            key=tuple(sorted((ka,kb)))
            if key in seen:continue
            seen.add(key)
            result.append({'points':[a.tolist(),b.tolist()],'region':str(record['regions'][idx]),'triangle_index':int(idx)})
    return result

def components(segments,axes):
    links={};points={}
    for i,seg in enumerate(segments):
        for p in seg['points']:
            key=tuple(np.round(p,7));points[key]=p;links.setdefault(key,[]).append(i)
    unseen=set(range(len(segments)));found=[]
    while unseen:
        first=next(iter(unseen));todo=[first];ids=set();nodes=set()
        while todo:
            i=todo.pop()
            if i in ids:continue
            ids.add(i);unseen.discard(i)
            for p in segments[i]['points']:
                key=tuple(np.round(p,7));nodes.add(key)
                todo.extend(j for j in links[key] if j not in ids)
        p=np.asarray([points[k] for k in nodes],float)
        lo=p.min(0);hi=p.max(0)
        start=next(iter(nodes));ordered=[start];current=start;used=set()
        while True:
            available=[i for i in links[current] if i in ids and i not in used]
            if not available:break
            i=available[0];used.add(i)
            pair=[tuple(np.round(q,7)) for q in segments[i]['points']]
            current=pair[1] if pair[0]==current else pair[0]
            ordered.append(current)
            if current==start:break
        poly=np.asarray([points[k] for k in ordered],float)
        closed=bool(len(used)==len(ids) and ordered[-1]==start)
        area=0.;centroid=None
        if closed and len(poly)>3:
            q=poly[:,axes];cross=q[:-1,0]*q[1:,1]-q[1:,0]*q[:-1,1]
            signed=.5*cross.sum();area=abs(float(signed))
            if abs(signed)>1e-15:
                c=((q[:-1]+q[1:])*cross[:,None]).sum(0)/(6*signed)
                centroid=c.tolist()
        found.append({'ids':sorted(ids),'points3d':poly.tolist(),'closed':closed,'area':area,
                      'bounds':[lo.tolist(),hi.tolist()],'bbox_center':((lo+hi)/2).tolist(),
                      'area_centroid':centroid})
    return found

def section_object(record,axis,value,select_head=True):
    segs=intersections(record,axis,value);axes=[i for i in range(3) if i!=axis]
    comps=components(segs,axes)
    if select_head:
        chosen=max(comps,key=lambda c:c['area'] if axis==2 else c['bounds'][1][2])
        ids=set(chosen['ids']);segs=[s for i,s in enumerate(segs) if i in ids];comps=[chosen]
    loops=[]
    for i,c in enumerate(comps):
        loop={k:v for k,v in c.items() if k!='ids'};loop.update(component_id=i,selected=True)
        loops.append(loop)
    return {'name':record['name'],'component_selection':'largest head/neck loop' if select_head else 'all eye regions',
            'segments':segs,'loops':loops}
