"""Join the existing round crown to the chest without a neck ring."""
import math
import bpy
from mathutils import Vector

LOWER_Z = 2.04
CROWN_Z = 2.36


def ease(a, b, value):
    t = max(0.0, min(1.0, (value - a) / (b - a)))
    return t * t * (3.0 - 2.0 * t)


def section(obj, z):
    front_hit, front, _, _ = obj.ray_cast(Vector((0, -2, z)), Vector((0, 1, 0)))
    rear_hit, rear, _, _ = obj.ray_cast(Vector((0, 2, z)), Vector((0, -1, 0)))
    assert front_hit and rear_hit, (obj.name, z)
    side_hit, side, _, _ = obj.ray_cast(Vector((2, (front.y + rear.y) / 2, z)), Vector((-1, 0, 0)))
    assert side_hit, (obj.name, z, 'side')
    return side.x, front.y, rear.y


def hermite(z, a, b, va, vb, da, db):
    t = (z - a) / (b - a)
    h = b - a
    return ((2*t**3 - 3*t*t + 1)*va + (t**3 - 2*t*t + t)*h*da
            + (-2*t**3 + 3*t*t)*vb + (t**3 - t*t)*h*db)


def sculpt_connection(body, shell=None):
    """Preserve topology, weights, crown, and everything below the upper chest."""
    # Samples are captured before editing, so this operation is reproducible.
    sample_count = 160
    heights = [LOWER_Z + (CROWN_Z - LOWER_Z)*i/sample_count for i in range(sample_count + 1)]
    source = [section(body, z) for z in heights]
    bottom, top = source[0], source[-1]
    eps = 0.012
    below = section(body, LOWER_Z - eps)
    above = section(body, CROWN_Z + eps)
    lower_slopes = [(bottom[k] - below[k])/eps for k in range(3)]
    upper_slopes = [(above[k] - top[k])/eps for k in range(3)]

    def target(z):
        return [hermite(z, LOWER_Z, CROWN_Z, bottom[k], top[k], lower_slopes[k], upper_slopes[k])
                for k in range(3)]

    # Width and rear depth must expand continuously toward the chest/back.
    target_rows = [target(z) for z in heights]
    assert all(b[0] <= a[0] + 1e-8 and b[2] <= a[2] + 1e-8
               for a, b in zip(target_rows, target_rows[1:])), 'Nonmonotonic side or rear bridge'

    def apply(obj, rows, profile_target=target):
        changed = 0
        for vertex in obj.data.vertices:
            p = vertex.co
            z = p.z
            if z <= LOWER_Z or z >= CROWN_Z:
                continue
            index = (z - LOWER_Z)/(CROWN_Z - LOWER_Z)*sample_count
            lo = min(sample_count - 1, int(index)); f = index - lo
            old = [rows[lo][k]*(1-f) + rows[lo+1][k]*f for k in range(3)]
            old_width, old_front, old_rear = old
            old_centre = (old_front + old_rear)/2
            old_depth = (old_rear - old_front)/2
            theta = math.atan2((p.y - old_centre)/old_depth, p.x/old_width)
            width, front, rear = profile_target(z)
            # Carry each vertex's radial position through the new section.
            # Normalizing onto an ellipse would raise an artificial ring at
            # the untouched crown, where the real mesh is not a perfect ellipse.
            x = p.x*width/old_width
            y = (front + rear)/2 + (p.y-old_centre)*(rear-front)/(old_rear-old_front)
            # Only the anterior surface carries a wide, shallow throat turn.
            # The mask reaches zero at both sides and over the whole rear.
            front_mask = max(0.0, -math.sin(theta))**6
            height_mask = ease(LOWER_Z, LOWER_Z + 0.055, z)*(1-ease(CROWN_Z - 0.045, CROWN_Z, z))
            throat = 0.006*math.exp(-((z - 2.130)/0.110)**2)
            oral = 0.003*math.exp(-((z - 2.240)/0.090)**2)
            y += front_mask*height_mask*(throat - oral)
            # Narrow endpoint feathers avoid replacing the untouched crown.
            amount = ease(LOWER_Z, LOWER_Z + 0.035, z)
            p.x += (x - p.x)*amount
            p.y += (y - p.y)*amount
            changed += 1
        obj.data.update()
        return changed

    changed = apply(body, source)
    bpy.context.view_layer.objects.active = body
    finish_group = body.vertex_groups.new(name='Continuous cheek and nape finish')
    for vertex in body.data.vertices:
        z = vertex.co.z
        weight = ease(LOWER_Z, LOWER_Z + 0.070, z)*(1-ease(CROWN_Z - 0.070, CROWN_Z, z))
        if weight > 0:
            finish_group.add([vertex.index], weight, 'REPLACE')
    finish = body.modifiers.new('Continuous cheek and nape finish', 'SMOOTH')
    finish.vertex_group = finish_group.name
    finish.factor = 0.48
    finish.iterations = 32
    bpy.ops.object.modifier_apply(modifier=finish.name)
    if shell is not None:
        # Bake the diagnostic shell before fitting its own old cross sections.
        bpy.context.view_layer.objects.active = shell
        shell.hide_set(False)
        for mod in list(shell.modifiers):
            if mod.type == 'SUBSURF':
                bpy.ops.object.modifier_apply(modifier=mod.name)
        shell_rows = [section(shell, z) for z in heights]
        shell_bottom, shell_top = shell_rows[0], shell_rows[-1]
        shell_below = section(shell, LOWER_Z - eps)
        shell_above = section(shell, CROWN_Z + eps)
        def shell_target(z):
            return [hermite(z, LOWER_Z, CROWN_Z, shell_bottom[k], shell_top[k],
                            (shell_bottom[k]-shell_below[k])/eps,
                            (shell_above[k]-shell_top[k])/eps) for k in range(3)]
        apply(shell, shell_rows, shell_target)
        shell.hide_set(True)
    body['head_revision'] = 'Continuous lower cheeks and sloping nape; round crown retained'
    body['preserved_body_below_z'] = LOWER_Z
    body['preserved_crown_above_z'] = CROWN_Z
    body['throat_front_only'] = True
    print('CONNECTION_SCULPT_OK', 'changed_vertices=', changed, 'lower=', LOWER_Z, 'crown=', CROWN_Z)


def refit_mouth(body, mouth):
    for vertex in mouth.data.vertices:
        p = vertex.co
        hit, surface, _, _ = body.ray_cast(Vector((p.x, -2, p.z)), Vector((0, 1, 0)))
        assert hit, 'Oral seam missed the sculpted face'
        p.y = surface.y - 0.0015
    mouth.data.update()
