"""Local shoulder fairing and baseline-relative belly/foot vertex painting.

The production entry point is finish_body(body, shell, H, D, baseline_record).
No remeshing, global rescaling, or limb pose operations are performed here.
"""
import json
import math
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector

SKIN_RGB = np.array((0.94, 0.66, 0.13), dtype=float)
CREAM_RGB = np.array((0.70, 0.54, 0.33), dtype=float)
FOOT_RGB = np.array((0.19, 0.115, 0.065), dtype=float)
BASELINE = Path(__file__).resolve().parent / 'iterations' / 'before_eye_muzzle_body_revision' / 'Naiwa_Character.blend'


def _ease(a, b, value):
    t = np.clip((value - a) / (b - a), 0.0, 1.0)
    return t*t*t*(10.0 + t*(-15.0 + 6.0*t))


def _coords(obj):
    result = np.empty(len(obj.data.vertices)*3, dtype=np.float32)
    obj.data.vertices.foreach_get('co', result)
    return result.reshape((-1, 3))


def _colors(obj):
    attr = obj.data.color_attributes.get('NaiwaColor')
    assert attr is not None and attr.domain == 'CORNER', 'NaiwaColor corner colors are required'
    result = np.empty(len(attr.data)*4, dtype=np.float32)
    attr.data.foreach_get('color', result)
    return attr, result.reshape((-1, 4))


def _cream_amount(rgb):
    direction = CREAM_RGB - SKIN_RGB
    return np.clip(np.sum((rgb-SKIN_RGB)*direction, axis=-1) / np.dot(direction, direction), 0.0, 1.0)


def _vertex_colors(obj, colors):
    indices = np.empty(len(obj.data.loops), dtype=np.int32)
    obj.data.loops.foreach_get('vertex_index', indices)
    result = np.zeros((len(obj.data.vertices), 4), dtype=float)
    np.add.at(result, indices, colors)
    count = np.bincount(indices, minlength=len(result))
    result /= np.maximum(1, count)[:, None]
    return indices, result


def _color_bounds(coords, colors, threshold=.5):
    amount = _cream_amount(colors[:, :3])
    active = (amount >= threshold) & (coords[:, 2] > .70) & (coords[:, 2] < 2.05) & (coords[:, 1] < -.15) & (np.abs(coords[:, 0]) < .68)
    points = coords[active]
    assert len(points), 'No baseline cream belly found'
    low, high = points.min(axis=0), points.max(axis=0)
    return {'threshold': float(threshold), 'bounds_xyz': [low.tolist(), high.tolist()],
            'width': float(high[0]-low[0]), 'lower_z': float(low[2]),
            'upper_z': float(high[2]), 'center_x': float((high[0]+low[0])*.5),
            'sample_count': int(len(points))}


def _fixed_baseline():
    """Read the immutable preceding body without opening or saving a scene."""
    assert BASELINE.is_file(), 'Fixed body-finish baseline is missing'
    name = 'Naiwa unified golden body'
    existing_materials=set(bpy.data.materials.keys())
    if Path(bpy.data.filepath).resolve() == BASELINE.resolve():
        obj=bpy.data.objects[name].copy()
        obj.data=obj.data.copy()
    else:
        with bpy.data.libraries.load(str(BASELINE), link=False) as (source, target):
            assert 'pear body mesh' in source.meshes
            target.meshes = ['pear body mesh']
        obj = bpy.data.objects.new('Body finish fixed baseline sampler',target.meshes[0])
    obj['_body_finish_imported_materials']=json.dumps(list(set(bpy.data.materials.keys())-existing_materials))
    obj.modifiers.clear()
    bpy.context.scene.collection.objects.link(obj)
    obj.hide_render=True
    bpy.context.view_layer.update()
    coords = _coords(obj)
    _, corners = _colors(obj)
    indices, vertex_colors = _vertex_colors(obj, corners)
    return obj, coords, indices, corners, vertex_colors


def _release_baseline(obj):
    mesh=obj.data
    imported_materials=json.loads(obj.get('_body_finish_imported_materials','[]'))
    bpy.data.objects.remove(obj, do_unlink=True)
    if mesh.users == 0:
        bpy.data.meshes.remove(mesh)
    for name in imported_materials:
        material=bpy.data.materials.get(name)
        if material is not None and material.users == 0:
            bpy.data.materials.remove(material)


def _front_color(obj, colors, x, z):
    hit,point,_,face=obj.ray_cast(Vector((float(x),-2,float(z))),Vector((0,1,0)))
    if not hit:
        return 0.0, 0.0
    polygon=obj.data.polygons[face]
    ids=list(polygon.vertices)
    # The fixed body is a smooth quad mesh; interpolate its original per-corner
    # paint across the polygon rather than regenerating a pre-subdivision mask.
    rgb=None
    for k in range(1,len(ids)-1):
        tri=(ids[0],ids[k],ids[k+1])
        a,b,c=(obj.data.vertices[i].co for i in tri)
        u,v,w=b-a,c-a,point-a
        uu,uv,vv,wu,wv=u.dot(u),u.dot(v),v.dot(v),w.dot(u),w.dot(v)
        det=uu*vv-uv*uv
        if abs(det)<1e-20:
            continue
        t=(vv*wu-uv*wv)/det; s=(uu*wv-uv*wu)/det
        if t>=-.0005 and s>=-.0005 and t+s<=1.0005:
            rgb=colors[tri[0],:3]*(1-t-s)+colors[tri[1],:3]*t+colors[tri[2],:3]*s
            break
    if rgb is None:
        rgb=np.mean(colors[ids,:3],axis=0)
    return float(_cream_amount(rgb)),float(point.y)


def _belly_field(obj, colors):
    xs=np.arange(0,.661,.003)
    zs=np.arange(.96,1.852,.003)
    amount=np.zeros((len(zs),len(xs))); front=np.zeros_like(amount)
    for i,z in enumerate(zs):
        for j,x in enumerate(xs):
            # Symmetric source sampling centers the revised color region.
            left,ly=_front_color(obj,colors,-x,z)
            right,ry=_front_color(obj,colors,x,z)
            amount[i,j]=(left+right)*.5; front[i,j]=(ly+ry)*.5
    return xs,zs,amount,front


def _original_profile():
    """Load only the original numeric belly formula, never execute the build."""
    import ast
    import bisect
    source = BASELINE.with_name('build_naiwa.py').read_text(encoding='utf-8')
    tree = ast.parse(source)
    namespace = {'math': math, 'bisect': bisect}
    # This is the original profile-construction range, all numeric operations.
    selected = []
    capture = False
    functions = {'ease', 'torso_z', 'torso_source_z', 'smoothstep'}
    assignments = {'source_z_samples', 'lifted_z_samples'}
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in functions:
            selected.append(node)
        if isinstance(node, ast.Assign):
            names = {t.id for t in node.targets if isinstance(t, ast.Name)}
            if names & assignments:
                selected.append(node)
            if 'outline_keys' in names and not capture and 'profile' not in namespace:
                capture = True
            if 'verts' in names:
                capture = False
        if capture:
            selected.append(node)
    # Stop profile capture after its final append; avoid any Blender operators.
    safe=[]
    for node in selected:
        text = ast.unparse(node)
        if 'bpy' not in text and 'verts.append' not in text and 'faces.append' not in text:
            safe.append(node)
    exec(compile(ast.Module(body=safe, type_ignores=[]), '<baseline numeric profile>', 'exec'), namespace)
    return np.asarray(namespace['profile'], dtype=float), namespace['torso_source_z']


def _profile_values(profile, z):
    return tuple(np.interp(z, profile[:, 0], profile[:, i]) for i in range(1, 4))


def _smoothstep(a, b, value):
    t = np.clip((value-a)/(b-a), 0, 1)
    return t*t*(3-2*t)


def _original_belly(coords, profile, source_z):
    x, y, z = coords.T
    rx, ry, cy = _profile_values(profile, z)
    height = (np.asarray([source_z(float(value)) for value in z])-.89)/.81
    angle = 1.52-.86*np.clip(height, 0, 1)
    frontness = (cy-y)/np.maximum(ry, .01)
    around = np.arctan2(x/np.maximum(rx, .01), frontness)
    oval = np.abs(around/angle)**2.4 + np.abs(2*height-1)**2.6
    front = cy-ry*np.sqrt(np.maximum(.02, 1-(x/np.maximum(rx, .001))**2))
    return (1-_smoothstep(.94, 1.07, oval))*(1-_smoothstep(.05, .32, y-front))


def _bilinear(xs, zs, field, x, z):
    a=np.clip((z-zs[0])/(zs[1]-zs[0]), 0, len(zs)-1.0000001)
    b=np.clip((x-xs[0])/(xs[1]-xs[0]), 0, len(xs)-1.0000001)
    i=a.astype(int); j=b.astype(int); u=a-i; v=b-j
    return ((1-u)*(1-v)*field[i,j] + u*(1-v)*field[i+1,j]
            + (1-u)*v*field[i,j+1] + u*v*field[i+1,j+1])


def _spline_field(xs, zs, field, x, z):
    """Cubic B-spline sampling avoids derivative lines from the ray grid."""
    a=np.clip((z-zs[0])/(zs[1]-zs[0]),0,len(zs)-1.0000001)
    b=np.clip((x-xs[0])/(xs[1]-xs[0]),0,len(xs)-1.0000001)
    i=a.astype(int); j=b.astype(int)
    def weights(t):
        return ((1-t)**3/6,(3*t**3-6*t*t+4)/6,(-3*t**3+3*t*t+3*t+1)/6,t**3/6)
    wz=weights(a-i); wx=weights(b-j)
    result=np.zeros_like(a)
    for k in range(4):
        for l in range(4):
            result+=wz[k]*wx[l]*field[np.clip(i+k-1,0,len(zs)-1),np.clip(j+l-1,0,len(xs)-1)]
    return result


def _fair_shoulder_field(source, step):
    fair=np.zeros_like(source); total=np.zeros_like(source)
    sigma=.035/step
    radius=int(math.ceil(2.1*sigma))
    for di in range(-radius,radius+1):
        for dj in range(-radius,radius+1):
            weight=math.exp(-.5*((di/sigma)**2+(dj/sigma)**2))
            shifted=np.roll(source,(di,dj),(0,1))
            valid=np.isfinite(shifted)
            if di>0: valid[:di,:]=False
            if di<0: valid[di:,:]=False
            if dj>0: valid[:,:dj]=False
            if dj<0: valid[:,dj:]=False
            fair[valid]+=weight*shifted[valid]; total[valid]+=weight
    fair/=np.maximum(total,1e-12)
    source=np.where(np.isfinite(source),source,fair)
    return source,fair


def _shoulder_finish(body):
    old = _coords(body)
    # The same broad fairing neighbourhood covers anterior and posterior
    # shoulder roots.  Only their concave departures move outward, retaining
    # the arm's broad neighbouring volume and its exact frontal silhouette.
    xs=np.arange(.285, .721, .004)
    zs=np.arange(1.58, 2.061, .004)
    sources=[np.full((len(zs),len(xs)),np.nan) for _ in range(2)]
    for i,z in enumerate(zs):
        for j,x in enumerate(xs):
            for surface,origin,direction in ((0,-2,1),(1,2,-1)):
                hits=[]
                for sign in (-1,1):
                    hit,p,_,_=body.ray_cast(Vector((sign*float(x),origin,float(z))), Vector((0,direction,0)))
                    if hit:
                        hits.append(p.y)
                if hits:
                    sources[surface][i,j]=sum(hits)/len(hits)
    fronts,front_fair=_fair_shoulder_field(sources[0],xs[1]-xs[0])
    rears,rear_fair=_fair_shoulder_field(sources[1],xs[1]-xs[0])
    # A C2 zero gate and smooth displacement cap avoid a crease where an
    # outward-only fairing field crosses zero or reaches its safety bound.
    front_fill=np.maximum(0,fronts-front_fair)
    rear_fill=np.maximum(0,rear_fair-rears)
    front_change=-.017*np.tanh(front_fill/.017)*_ease(0,.0025,front_fill)
    rear_change=.021*np.tanh(rear_fill/.021)*_ease(0,.0025,rear_fill)
    x=np.abs(old[:,0]).astype(float); z=old[:,2].astype(float)
    center=.62-.47*(z-1.50)
    lateral=1-_ease(.026,.135,np.abs(x-center))
    height=_ease(1.64,1.73,z)*(1-_ease(1.925,2.015,z))
    front_y=_spline_field(xs,zs,fronts,x,z)
    rear_y=_spline_field(xs,zs,rears,x,z)
    # Continuous thickness-based blending reaches the side wall as well as
    # the two poles; it does not leave a narrow surface-distance mask edge.
    thickness=np.maximum(.035,rear_y-front_y)
    depth_position=np.clip(.5+(old[:,1]-(front_y+rear_y)*.5)/thickness,0,1)
    posterior=_ease(.10,.90,depth_position)
    delta=(_spline_field(xs,zs,front_change,x,z)*(1-posterior)
           +_spline_field(xs,zs,rear_change,x,z)*posterior)*lateral*height
    new=old.copy(); new[:,1]+=delta
    body.data.vertices.foreach_set('co',new.reshape(-1)); body.data.update()
    changed=np.any(new!=old,axis=1)
    assert np.array_equal(new[:,[0,2]],old[:,[0,2]])
    assert np.array_equal(new[height==0],old[height==0])
    return {'changed_vertices':int(changed.sum()), 'maximum_displacement':float(np.max(np.abs(new[:,1]-old[:,1]))),
            'local_bounds_xyz': [new[changed].min(axis=0).tolist(),new[changed].max(axis=0).tolist()] if changed.any() else None,
            'mask_range_z':[1.64,2.015], 'all_x_z_exact':True,
            'shoulder_width_and_arm_pose_preserved':True,'armpit_below_1_64_exact':True,
            'anterior_changed_vertices':int(np.count_nonzero(changed&(delta<0))),
            'posterior_changed_vertices':int(np.count_nonzero(changed&(delta>0))),
            'fair_length':.035,'cubic_field_sampling':True,
            'method':'Anterior and posterior concave Y departures locally filled with a 0.035 neighbourhood, broad C2 mask and continuous side-wall blending'}, old, new


def finish_body(body, shell, H, D, baseline_record=None):
    """Called once on the current head/face revision; report values are JSON-safe."""
    assert not body.get('body_finish_refined'), 'Body finishing must start from the fixed preceding revision'
    assert H>0 and D>0
    if baseline_record:
        assert abs(float(baseline_record.get('height',H))-H)<1e-7
    baseline,base_coords,base_indices,base_corners,base_vertex_colors=_fixed_baseline()
    try:
        original=_color_bounds(base_coords,base_vertex_colors)
        xs,zs,source_amount,front=_belly_field(baseline,base_vertex_colors)
    finally:
        _release_baseline(baseline)
    # XZ and topology are preserved by the shoulder pass, so the same fixed
    # color field remains aligned with the unchanged abdominal surface.
    shoulder,old,new=_shoulder_finish(body)
    attr,colors=_colors(body)
    old_colors=colors.copy()
    indices,vertex_colors=_vertex_colors(body,colors)
    x=np.abs(new[:,0]).astype(float); z=new[:,2].astype(float)
    half=original['width']*.9*.5
    lower=original['lower_z']-.025*H
    upper=original['upper_z']
    original_half=original['width']*.5
    base_cream=_cream_amount(base_vertex_colors[:,:3])
    widest=(base_cream>=.5)&(base_coords[:,1]<-.15)&(np.abs(base_coords[:,0])>=original_half-.00045)&(base_coords[:,2]>.7)&(base_coords[:,2]<2.0)
    peak=float(np.mean(base_coords[widest,2]))
    amount=_bilinear(xs,zs,source_amount,x/.9,z)
    # The lower half uses one continuous ellipse, giving a rounded bottom
    # and gathering its sides inward instead of extending a flat lower band.
    ellipse=(x/half)**2+((z-peak)/(peak-lower))**2
    lower_amount=1-_smoothstep(.94,1.07,1.005*ellipse)
    blend=1-_ease(peak-.026,peak+.026,z)
    amount=amount*(1-blend)+lower_amount*blend
    source_front=_bilinear(xs,zs,front,x,z)
    depth=1-_ease(.008,.055,np.abs(new[:,1]-source_front))
    area=(z>.92)&(z<1.85)&(x<.64)&(new[:,1]<-.15)
    amount=np.where(area,amount*depth,0)
    old_amount=_cream_amount(vertex_colors[:,:3])
    old_belly=area&(old_amount>1e-6)
    paint=(amount>1e-6)|old_belly
    desired=SKIN_RGB[None,:]*(1-amount[:,None])+CREAM_RGB[None,:]*amount[:,None]
    color_vertices=vertex_colors.copy()
    color_vertices[paint,:3]=desired[paint]
    # All soles, heels, insteps, and toes carry the same existing deep brown.
    # Only a short height fade near the narrow ankle joins to the golden shin.
    foot_area=(z<.235)&(x>.17)&(x<.55)
    foot_amount=np.where(foot_area,1-_ease(.190,.235,z),0)
    foot_rgb=color_vertices[:,:3]*(1-foot_amount[:,None])+FOOT_RGB[None,:]*foot_amount[:,None]
    foot=foot_amount>0
    color_vertices[foot,:3]=foot_rgb[foot]
    changed_vertices=paint|foot
    changed_loops=changed_vertices[indices]
    colors[changed_loops,:3]=color_vertices[indices[changed_loops],:3]
    attr.data.foreach_set('color',colors.reshape(-1)); body.data.update()
    assert np.array_equal(colors[~changed_loops],old_colors[~changed_loops])
    assert np.array_equal(colors[:,3],old_colors[:,3])
    assert np.array_equal(old[:,[0,2]],_coords(body)[:,[0,2]])
    assert np.array_equal(old[old[:,2]<1.64],_coords(body)[old[:,2]<1.64])
    measured=_color_bounds(new,color_vertices)
    target={'width':float(2*half),'lower_z':float(lower),'upper_z':float(upper),
            'center_x':0.0,'width_ratio':.9,'lowering':float(.025*H),
            'lower_round_arc_center_z':peak,'lower_arc_vertical_radius':float(peak-lower)}
    report={'fixed_baseline':str(BASELINE),'H':float(H),'D':float(D),
            'shoulders':shoulder,'shell':'Unchanged diagnostic shell: head/torso only, no arms',
            'belly':{'original':original,'target':target,'measured_after':measured,
                'upper_original_paint_resampled':True,'lower_boundary_continuous_ellipse':True,
                'body_geometry_exact':True,'original_palette_preserved':True},
            'feet':{'rgb':FOOT_RGB.tolist(),'fully_brown_below_z':.190,'ankle_transition_z':[.190,.235],
                'painted_vertices':int(foot.sum()),'legs_and_feet_geometry_exact':True},
            'color_validation':{'changed_loops':int(changed_loops.sum()),'outside_paint_loops_exact':True,'alpha_exact':True}}
    body['body_finish_refined']=True
    body['body_finish_fixed_baseline']=str(BASELINE)
    body['body_finish_report']=json.dumps(report)
    print('BODY_FINISH_SCULPT_OK',json.dumps(report))
    return report
