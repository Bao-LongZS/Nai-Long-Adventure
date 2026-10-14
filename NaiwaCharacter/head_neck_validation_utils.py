"""Independent saved-mesh measurements for the head/neck refinement."""
import hashlib,math
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree
from validate_lip_eye_refinement import attribute,coordinates
from geometry_diagnostic_utils import section_object,face_point

def raw_record(obj):
    points=np.asarray([tuple(obj.matrix_world@v.co) for v in obj.data.vertices],float)
    obj.data.calc_loop_triangles()
    triangles=attribute(obj.data.loop_triangles,'vertices',3,np.int32).reshape((-1,3))
    tp=points[triangles];n=np.cross(tp[:,1]-tp[:,0],tp[:,2]-tp[:,0]);lengths=np.linalg.norm(n,axis=1)
    n/=np.maximum(lengths[:,None],1e-20)
    return {'name':obj.name,'points':points,'triangles':triangles,'triangle_points':tp,'triangle_normals':n,
        'regions':np.full(len(triangles),'yellow_body',object),
        'bvh':BVHTree.FromPolygons(points.tolist(),triangles.tolist(),all_triangles=True)}

def evaluated_points(obj,depsgraph):
    """Read actual evaluated vertices through the same path for A and candidates."""
    evaluated=obj.evaluated_get(depsgraph)
    mesh=evaluated.to_mesh(preserve_all_data_layers=True,depsgraph=depsgraph)
    try:
        points=attribute(mesh.vertices,'co',3,np.float32).reshape((-1,3)).astype(float)
        matrix=np.asarray(evaluated.matrix_world,dtype=float)
        return points@matrix[:3,:3].T+matrix[:3,3]
    finally:
        evaluated.to_mesh_clear()

def fingerprint(obj):
    def digest(collection,prop,n,dtype):return hashlib.sha256(attribute(collection,prop,n,dtype).tobytes()).hexdigest()
    weight=hashlib.sha256()
    for v in obj.data.vertices:
        for name,w in sorted((obj.vertex_groups[g.group].name,g.weight) for g in v.groups):
            weight.update(f'{v.index}:{name}:{w}'.encode())
    return {'topology':{name+':'+prop:digest(getattr(obj.data,name),prop,n,np.int32)
        for name,prop,n in [('edges','vertices',2),('loops','vertex_index',1),('polygons','loop_start',1),
            ('polygons','loop_total',1),('polygons','material_index',1)]},
        'uv':{layer.name:digest(layer.data,'uv',2,np.float32) for layer in obj.data.uv_layers},
        'colors':{a.name:(a.domain,a.data_type,digest(a.data,'color',4,np.float32)) for a in obj.data.color_attributes},
        'all_vertex_group_weights':weight.hexdigest(),'vertex_group_names':list(obj.vertex_groups.keys()),
        'smooth':digest(obj.data.polygons,'use_smooth',1,np.bool_),'custom_normals':obj.data.has_custom_normals,
        'matrix_world':list(map(list,obj.matrix_world)),'vertex_count':len(obj.data.vertices)}

def fit_sphere(points):
    origin=points.mean(0);p=points-origin
    solution=np.linalg.lstsq(np.column_stack((2*p,np.ones(len(p)))),np.sum(p*p,axis=1),rcond=None)[0]
    center=origin+solution[:3];radius=math.sqrt(solution[3]+np.dot(solution[:3],solution[:3]))
    residual=float(np.max(np.abs(np.linalg.norm(points-center,axis=1)-radius))/radius)
    return {'center':center.tolist(),'radius':radius,'maximum_relative_radius_residual':residual}

def horizontal_metrics(record,z):
    front=face_point(record,0.,z)
    rear,_,_,_=record['bvh'].ray_cast(Vector((0,3,float(z))),Vector((0,-1,0)),6)
    assert front is not None and rear is not None
    full=section_object(record,2,z,select_head=False)
    assert full['loops'] and all(loop['closed'] for loop in full['loops']),'Head horizontal section is open'
    body=section_object(record,2,z);bounds=body['loops'][0]['bounds']
    return {'Z':z,'body':body,'front_Y':float(front[1]),'rear_Y':float(rear.y),
        'center_Y':float((front[1]+rear.y)/2),'depth_Y':float(rear.y-front[1]),
        'width_X':float(bounds[1][0]-bounds[0][0]),
        'all_components':[{key:loop[key] for key in ('closed','area','bounds','points3d')}
            for loop in full['loops']]}

def sections(record):
    return {'midline':{'plane_X':0.,'body':section_object(record,0,0.,select_head=True)},
        'horizontal':[horizontal_metrics(record,z) for z in (2.12,2.22,2.31,2.40,2.52,2.60)]}

def quintic(t):
    t=np.clip(t,0.,1.);return t*t*t*(10+t*(-15+6*t))

def shoulder_fixed(body):
    points=np.asarray([tuple(body.matrix_world@v.co) for v in body.data.vertices],float)
    arm=[]
    for v in body.data.vertices:
        arm.append(sum(g.weight for g in v.groups if body.vertex_groups[g.group].name.startswith(('UpperArm.','Forearm.'))))
    seeds=(np.asarray(arm)>=.5)&(points[:,2]>=1.75)
    tree=KDTree(int(seeds.sum()))
    for i,p in enumerate(points[seeds]):tree.insert(Vector(p),i)
    tree.balance()
    return tree,seeds,points

def protective_masks(points,tree,eyes,mouth_bounds):
    # These fixed domains are independently reconstructed from source geometry.
    distances=np.asarray([tree.find(Vector(p))[2] for p in points])
    shoulder=distances<=.012
    eye=np.zeros(len(points),bool)
    for fit in eyes.values():eye|=np.linalg.norm(points-np.asarray(fit['center']),axis=1)-fit['radius']<=.026
    lo,hi=np.asarray(mouth_bounds[0]),np.asarray(mouth_bounds[1])
    closest=np.maximum(lo-points,0)+np.maximum(points-hi,0)
    mouth=np.linalg.norm(closest,axis=1)<=.010
    return {'shoulder_root':shoulder,'eye_installation':eye,'mouth_skin':mouth,
        'crown_above_2_635':points[:,2]>=2.635}

def surface_integrity(record,source=None,z_range=(1.95,2.65),assert_clean=True):
    triangles=record['triangles'];points=record['points'];tp=record['triangle_points']
    assert np.isfinite(points).all(),'Nonfinite surface coordinates'
    selected=(tp[:,:,2].max(1)>=z_range[0])&(tp[:,:,2].min(1)<=z_range[1])
    ids=np.flatnonzero(selected);faces=triangles[selected]
    cross=np.cross(tp[selected,1]-tp[selected,0],tp[selected,2]-tp[selected,0]);areas=np.linalg.norm(cross,axis=1)/2
    assert len(faces),'Empty head/shoulder region'
    degenerate=ids[areas<=1e-12]
    canonical=np.sort(faces,axis=1)
    duplicate_faces=len(faces)-len(np.unique(canonical,axis=0))
    used=np.unique(faces);duplicate=len(used)-len(np.unique(np.round(points[used],7),axis=0))
    if assert_clean:
        assert len(degenerate)==0,'Degenerate face in head/shoulder region'
        assert duplicate_faces==0,'Duplicate triangles'
        assert duplicate==0,'Duplicate head/neck surface vertices'
    # Use every triangle for edge incidence: cropping at the region's Z limits
    # would incorrectly classify its cut edges as holes in the surface.
    edges=np.sort(np.concatenate((triangles[:,[0,1]],triangles[:,[1,2]],triangles[:,[2,0]])),axis=1)
    edge_keys=edges[:,0].astype(np.int64)*len(points)+edges[:,1]
    unique_edges,counts=np.unique(edge_keys,return_counts=True)
    region_edges=np.sort(np.concatenate((faces[:,[0,1]],faces[:,[1,2]],faces[:,[2,0]])),axis=1)
    region_keys=np.unique(region_edges[:,0].astype(np.int64)*len(points)+region_edges[:,1])
    region_counts=counts[np.searchsorted(unique_edges,region_keys)]
    boundary=int(np.count_nonzero(region_counts==1));nonmanifold=int(np.count_nonzero(region_counts>2))
    bvh=BVHTree.FromPolygons(points.tolist(),faces.tolist(),all_triangles=True)
    overlaps=bvh.overlap(bvh);crossings=[]
    for a,b in overlaps:
        if a>=b or set(faces[a]).intersection(faces[b]):continue
        crossings.append((int(ids[a]),int(ids[b])))
    report={'checked_triangle_count':len(faces),'minimum_triangle_area':float(areas.min()),
        'finite_coordinates':True,'degenerate_triangle_count':len(degenerate),
        'degenerate_triangle_ids':degenerate.tolist(),
        'duplicate_triangles':duplicate_faces,'duplicate_vertices_at_1e_7':duplicate,
        'boundary_edges_in_region':boundary,'nonmanifold_edges_in_region':nonmanifold,
        'nonadjacent_self_intersection_count':len(crossings),'intersection_triangle_pairs':crossings[:40]}
    if source is not None:
        dots=np.sum(record['triangle_normals'][selected]*source['triangle_normals'][selected],axis=1)
        old_tp=source['triangle_points'][selected]
        old_areas=np.linalg.norm(np.cross(old_tp[:,1]-old_tp[:,0],old_tp[:,2]-old_tp[:,0]),axis=1)/2
        valid=old_areas>1e-12
        assert not np.any((areas<=1e-12)&valid),'New degenerate head/neck face'
        report['minimum_source_target_normal_dot']=float(dots[valid].min())
        assert dots[valid].min()>0.,'Head/neck triangle normal flips relative to source'
    return report,set(crossings)
