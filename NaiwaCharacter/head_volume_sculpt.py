"""Reduce the existing head while retaining a continuous chest and sloping back."""
import math
import bpy
from mathutils import Vector
from connection_sculpt import section, hermite, refit_mouth

LOWER_Z = 2.00
WIDTH_REDUCTION = 0.13
FRONT_REDUCTION = 0.10
REAR_REDUCTION = 0.28
HEIGHT_REDUCTION = 0.045
PIVOT_Y = -0.18


def ease(a, b, value):
    t = max(0.0, min(1.0, (value-a)/(b-a)))
    return t*t*t*(10.0-15.0*t+6.0*t*t)


def head_weight(z):
    return ease(LOWER_Z, 2.35, z)


def new_z(z):
    return z - HEIGHT_REDUCTION*ease(2.02, 2.53, z)


def shrink_depth(y, z):
    rear_amount = ease(-0.08, 0.04, y-PIVOT_Y)
    rate = FRONT_REDUCTION + (REAR_REDUCTION-FRONT_REDUCTION)*rear_amount
    return y - rate*head_weight(z)*(y-PIVOT_Y)


def face_surface(body, x, z):
    hit, point, _, _ = body.ray_cast(Vector((x, -2, z)), Vector((0, 1, 0)))
    assert hit, ('Face surface missed', x, z)
    eps = 0.006
    left = body.ray_cast(Vector((x-eps, -2, z)), Vector((0, 1, 0)))[1].y
    right = body.ray_cast(Vector((x+eps, -2, z)), Vector((0, 1, 0)))[1].y
    below = body.ray_cast(Vector((x, -2, z-eps)), Vector((0, 1, 0)))[1].y
    above = body.ray_cast(Vector((x, -2, z+eps)), Vector((0, 1, 0)))[1].y
    normal = Vector(((right-left)/(2*eps), -1, (above-below)/(2*eps))).normalized()
    return point, normal


def sculpt_head_volume(body, shell=None, fit_features=True):
    bpy.context.scene.frame_set(1)
    eye_info = []
    if fit_features:
        for side in ('L', 'R'):
            eye = bpy.data.objects[side+' concentric iris and pupil']
            glint = bpy.data.objects[side+' eye glint']
            # The polar endpoints of the complete sphere give its exact centre.
            pole = eye.data.vertices[0].co.copy()
            opposite = eye.data.vertices[-1].co.copy()
            centre = (pole+opposite)/2
            radius = (pole-centre).length
            # These are the original eye construction anchors.
            anchor_x = -0.103 if side == 'L' else 0.103
            anchor_z = 2.361
            eye_info.append((side, eye, glint, centre, radius,
                             (pole-centre).normalized(), glint.location.copy(),
                             glint.rotation_euler.to_quaternion(), anchor_x, anchor_z))

    def deform(obj):
        upper = max(v.co.z for v in obj.data.vertices)-0.0005
        count = 280
        heights = [LOWER_Z+(upper-LOWER_Z)*i/count for i in range(count+1)]
        rows = [section(obj, z) for z in heights]
        rear_join = 2.425
        eps = 0.009
        low_rear = rows[0][2]
        low_derivative = (low_rear-section(obj, LOWER_Z-eps)[2])/eps
        end_rear = shrink_depth(section(obj, rear_join)[2], rear_join)
        end_derivative = (shrink_depth(section(obj, rear_join+eps)[2], rear_join+eps)
                         -shrink_depth(section(obj, rear_join-eps)[2], rear_join-eps))/(2*eps)
        changed = 0
        for vertex in obj.data.vertices:
            p = vertex.co
            z = p.z
            if z <= LOWER_Z:
                continue
            idx = max(0.0, min(float(count), (z-LOWER_Z)/(upper-LOWER_Z)*count))
            lo = min(count-1, int(idx))
            t = idx-lo
            width, front, rear = [rows[lo][k]*(1-t)+rows[lo+1][k]*t for k in range(3)]
            old_centre = (front+rear)/2
            old_depth = max(0.0001, (rear-front)/2)
            target_front = shrink_depth(front, z)
            target_rear = (hermite(z, LOWER_Z, rear_join, low_rear, end_rear,
                                  low_derivative, end_derivative)
                           if z < rear_join else shrink_depth(rear, z))
            yy = (target_front+target_rear)/2+(p.y-old_centre)*(target_rear-target_front)/(2*old_depth)
            front_mask = max(0.0, min(1.0, (old_centre-p.y)/old_depth))**6
            # A short lower face with a broad, shallow anterior recess.
            window = ease(LOWER_Z, 2.09, z)*(1-ease(2.32, 2.40, z))
            throat = 0.005*math.exp(-((z-2.140)/0.105)**2)
            oral = 0.004*math.exp(-((z-2.235)/0.076)**2)
            yy += front_mask*window*(throat-oral)
            p.x *= 1-WIDTH_REDUCTION*head_weight(z)
            p.y = yy
            p.z = new_z(z)
            changed += 1
        obj.data.update()
        return changed

    changed = deform(body)
    if shell is not None:
        shell.hide_set(False)
        bpy.context.view_layer.objects.active = shell
        for modifier in list(shell.modifiers):
            if modifier.type == 'SUBSURF':
                bpy.ops.object.modifier_apply(modifier=modifier.name)
        deform(shell)
        shell.hide_set(True)

    if fit_features:
        for side, eye, glint, old_centre, radius, old_axis, glint_location, glint_rotation, anchor_x, anchor_z in eye_info:
            x = anchor_x*(1-WIDTH_REDUCTION*head_weight(anchor_z))
            z = new_z(anchor_z)
            surface, normal = face_surface(body, x, z)
            exposed = float(eye.get('exposed_cap_height', 0.0187))
            centre = surface-normal*(radius-exposed)
            aperture = surface-normal*0.0068
            look_x, look_z = aperture.x, aperture.z
            look_y = centre.y-math.sqrt(max(0.0001, radius**2-(look_x-centre.x)**2-(look_z-centre.z)**2))
            axis = (Vector((look_x, look_y, look_z))-centre).normalized()
            rotation = old_axis.rotation_difference(axis)
            for vertex in eye.data.vertices:
                vertex.co = centre+rotation@(vertex.co-old_centre)
            eye.data.update()
            eye['frontal_pupil_center'] = (look_x, look_y, look_z)
            eye['head_reduction_anchor'] = (x, surface.y, z)
            glint.location = centre+rotation@(glint_location-old_centre)
            glint.rotation_euler = (rotation@glint_rotation).to_euler()
        mouth = bpy.data.objects['Short closed oral seam']
        for vertex in mouth.data.vertices:
            z = vertex.co.z
            vertex.co.x *= 1-WIDTH_REDUCTION*head_weight(z)
            vertex.co.z = new_z(z)
        refit_mouth(body, mouth)

    body['head_revision'] = 'Reduced head width/depth, compact lower face and earlier sloping nape'
    body['preserved_body_below_z'] = LOWER_Z
    if 'preserved_crown_above_z' in body:
        del body['preserved_crown_above_z']
    body['head_width_reduction'] = WIDTH_REDUCTION
    body['head_rear_depth_reduction'] = REAR_REDUCTION
    body['head_height_reduction'] = HEIGHT_REDUCTION
    body['throat_front_only'] = True
    print('HEAD_VOLUME_SCULPT_OK', changed, 'vertices; body protected below', LOWER_Z)

