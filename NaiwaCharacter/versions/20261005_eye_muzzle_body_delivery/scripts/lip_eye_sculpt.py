"""Reduce the broad chin pad; keep a short lip and round the visible eye rings."""
import bpy, math
import numpy as np
from mathutils import Vector
from longitudinal_surface_sculpt import coordinates, ease
from short_round_face_sculpt import profiles
from neck_direction_sculpt import quintic

MOUTH_Z = 2.359

def reshape_lower_face(obj):
    old = coordinates(obj)
    zs, rows, at, jets = profiles(obj)
    z = old[:, 2].astype(float)
    sf = np.interp(z, zs, rows[:, 1]); sr = np.interp(z, zs, rows[:, 2])
    sw = np.interp(z, zs, rows[:, 0])
    theta = np.arctan2((old[:, 1]-(sf+sr)/2)/((sr-sf)/2), old[:, 0]/sw)
    anterior = ease(.1, .8, np.maximum(0, -np.sin(theta)))
    cheeks = 1-ease(.08, .24, np.abs(old[:, 0]))
    f0,df0,ddf0=jets(2.18,1)
    f1,df1,ddf1=jets(2.435,1)
    base_mouth=at(2.36,1)+.024
    zz=np.clip(z,2.18,2.435)
    target=np.where(zz<=2.36,
        quintic(zz,2.18,2.36,f0,base_mouth,df0,-.16,ddf0,1.0),
        quintic(zz,2.36,2.435,base_mouth,f1,-.16,df1,1.0,ddf1))
    retraction=(target-sf)*cheeks*anterior
    window=ease(2.18,2.25,z)*(1-ease(2.405,2.435,z))
    lip=.004*np.exp(-(old[:,0]/.045)**4-((z-MOUTH_Z)/.015)**2)*window*anterior
    new = old.copy()
    active = (z > 2.18) & (z < 2.435) & (anterior > 0)
    new[active, 1] += (retraction-lip)[active]
    assert np.array_equal(old[:, [0, 2]], new[:, [0, 2]])
    obj.data.vertices.foreach_set('co', new.reshape(-1)); obj.data.update()
    return {'modified_vertices':int(np.count_nonzero(np.any(old!=new,axis=1))),
        'maximum_displacement':float(np.max(np.linalg.norm(new-old,axis=1))),
        'lateral_contour_and_all_heights_exact':True, 'mouth_base_retraction':.024,
        'lip_height':.004, 'lip_vertical_radius':.015, 'lower_z':2.18, 'upper_z':2.435}

def fit_mouth(body):
    mouth=bpy.data.objects['Short closed oral seam']
    old=coordinates(mouth)
    center_z=float((old[:,2].min()+old[:,2].max())/2)
    half_width=float(np.ptp(old[:,0])/2)*.90
    row_x=old.reshape((-1,2,3))[:,:,0].mean(axis=1)
    row_center=float((row_x.min()+row_x.max())/2)
    row_radius=float(np.ptp(row_x)/2)
    for i,v in enumerate(mouth.data.vertices):
        u=float((row_x[i//2]-row_center)/row_radius)
        x=half_width*u
        arc_z=center_z+.0017*u*u
        thickness=.00008+.00058*max(0,1-u*u)**.75
        z=arc_z+(-thickness if i%2==0 else thickness)
        hit,p,_,_=body.ray_cast(Vector((x,-2,z)),Vector((0,1,0)))
        assert hit
        v.co=(x,p.y-.00055,z)
    mouth.data.update()
    mouth['shallow_tapered_arc']=True
    return {'width':2*half_width,'center_z':center_z,'central_thickness':.00132,'tip_thickness':.00016}

def reshape_eye(eye, glint, body, measured):
    old=coordinates(eye).astype(float)
    center=(old[0]+old[-1])/2
    radius=float(np.linalg.norm(old[0]-center))
    axis=(old[0]-center)/radius
    u=old[1]-center; u-=axis*np.dot(axis,u); u/=np.linalg.norm(u)
    v=np.cross(axis,u)
    relative=old-center
    rx=relative@u; rz=relative@v; forward=relative@axis
    r=np.sqrt(rx*rx+rz*rz)
    colors=eye.data.color_attributes['EyeColor']
    black_vertices=set()
    for polygon in eye.data.polygons:
        if colors.data[polygon.loop_start].color[1]<.04:
            black_vertices.update(polygon.vertices)
    source_pupil=float(np.max(r[list(black_vertices)]))
    source_iris=.064
    aperture_area=measured['aperture']['area']
    pupil_radius=math.sqrt(measured['black']['area']/math.pi)
    aperture_radius=math.sqrt(aperture_area/math.pi)
    aperture_center=np.array(measured['aperture']['center'])
    old_pupil=Vector(eye['frontal_pupil_center'])
    pupil_center=np.array((old_pupil.x,old_pupil.z))
    pupil_slope=pupil_radius/source_pupil
    radial=np.where(r<=source_pupil, r*pupil_slope,
        np.where(r<=source_iris,
            quintic(r,source_pupil,source_iris,pupil_radius,aperture_radius,pupil_slope,.70,0,0),
            quintic(r,source_iris,radius,aperture_radius,.075,.70,.8,0,0)))
    mix=ease(source_pupil,source_iris,r)
    cx=pupil_center[0]+mix*(aperture_center[0]-pupil_center[0])
    cz=pupil_center[1]+mix*(aperture_center[1]-pupil_center[1])
    oval=1.04
    scale_x=1+mix*(1/math.sqrt(oval)-1)
    scale_z=1+mix*(math.sqrt(oval)-1)
    angle=np.arctan2(rz,rx)
    x=cx+radial*np.cos(angle)*scale_x
    z=cz+radial*np.sin(angle)*scale_z
    q=radial/aperture_radius
    # Cross the skin decisively outside the green edge, avoiding coplanar
    # triangles where a squared zero-height feather would shimmer.
    height=.014*(1-q*q)+.0002
    buried=.12*np.maximum(0,-forward/radius)**2
    new=np.empty_like(old)
    for i,(xx,zz) in enumerate(zip(x,z)):
        hit,p,_,_=body.ray_cast(Vector((float(xx),-2,float(zz))),Vector((0,1,0)))
        assert hit
        new[i]=(xx,p.y-height[i]+buried[i],zz)
    eye.data.vertices.foreach_set('co',new.astype(np.float32).reshape(-1)); eye.data.update()
    eye['frontal_pupil_center']=(old_pupil.x,float(new[0,1]),old_pupil.z)
    eye['frontal_aperture_center']=tuple(aperture_center)
    eye['frontal_eye_axes']=(aperture_radius/math.sqrt(oval),aperture_radius*math.sqrt(oval))
    eye['frontal_pupil_radius']=pupil_radius
    eye['visible_pupil_to_aperture_area_ratio']=measured['black']['area']/aperture_area
    eye['complete_spherical_eye']=False
    eye['complete_volume_eye']=True
    eye['exposed_cap_height']=.014
    # Refit the existing small white glint near its preserved pupil position.
    gx=old_pupil.x-.010; gz=old_pupil.z+.014
    hit,p,_,_=eye.ray_cast(Vector((gx,-2,gz)),Vector((0,1,0)))
    assert hit
    glint.location=(gx,p.y-.0007,gz)
    glint.rotation_euler=(0,0,0)
    return {'aperture_center':aperture_center.tolist(),'aperture_area_target':aperture_area,
        'black_area_target':measured['black']['area'],'ellipse_vertical_ratio':oval,
        'axes':list(eye['frontal_eye_axes']),'pupil_radius':pupil_radius,
        'pupil_x_z_exact':True,'complete_closed_eye_volume':True}

def sculpt_lip_eye(body, shell=None):
    assert not body.get('lip_eye_refined'), 'Read the preceding saved iteration.'
    from eye_projection_measure import visible_metrics
    measured={s:visible_metrics(bpy.data.objects[s+' concentric iris and pupil'],body)
              for s in ('L','R')}
    report={'body':reshape_lower_face(body)}
    if shell is not None: reshape_lower_face(shell)
    report['mouth']=fit_mouth(body)
    report['eyes']={s:reshape_eye(bpy.data.objects[s+' concentric iris and pupil'],
        bpy.data.objects[s+' eye glint'],body,measured[s]) for s in ('L','R')}
    for polygon in body.data.polygons: polygon.use_smooth=True
    body.data.update()
    body['lip_eye_refined']=True
    body['head_revision']='Compact shallow lip, early lower-face recession and round frontal eye rings'
    print('LIP_EYE_SCULPT_OK',report)
    return report
