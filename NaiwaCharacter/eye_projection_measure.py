"""Measure the actual eye/skin intersection in the frontal X/Z projection.

This module is read-only.  It measures ray-visible geometry and EyeColor faces,
not an intended ellipse or a sculpt's construction parameters.
"""
import math
from collections import deque

import numpy as np
from mathutils import Vector


def coordinates(obj):
    points = np.empty(len(obj.data.vertices) * 3, dtype=np.float32)
    obj.data.vertices.foreach_get('co', points)
    return points.reshape((-1, 3))


def _statistics(rows, step):
    points = np.asarray(rows, dtype=float).reshape((-1, 2))
    if len(points) < 3:
        raise ValueError('Eye projection has too few visible samples')
    covariance = np.cov(points.T, bias=True)
    values, vectors = np.linalg.eigh(covariance)
    if values[0] <= 0:
        raise ValueError('Eye projection has a degenerate covariance')
    axis = vectors[:, 1]
    angle = math.degrees(math.atan2(float(axis[1]), float(axis[0])))
    if angle > 90:
        angle -= 180
    if angle < -90:
        angle += 180
    spans = np.ptp(points, axis=0)
    return {
        'area': float(len(points) * step * step),
        'sample_count': int(len(points)),
        'center': points.mean(axis=0).tolist(),
        'bounds': [points.min(axis=0).tolist(), points.max(axis=0).tolist()],
        'axis_ratio': float(math.sqrt(values[1] / values[0])),
        'major_axis_degrees': float(angle),
        'width_height_ratio': float(spans[0] / spans[1]),
        'covariance_xz': covariance.tolist(),
    }


def aperture_topology(points, xs, zs, step):
    """Count actual ray-visible components and enclosed body intrusion islands."""
    mask=np.zeros((len(zs),len(xs)),bool)
    for x,z in points:
        mask[int(round((z-zs[0])/step)),int(round((x-xs[0])/step))]=True
    def components(value):
        visited=np.zeros_like(mask);counts=[];border_flags=[]
        for y,x in np.argwhere(mask==value):
            if visited[y,x]:continue
            queue=deque([(int(y),int(x))]);visited[y,x]=True;count=0;border=False
            while queue:
                yy,xx=queue.popleft();count+=1
                border|=yy==0 or xx==0 or yy==mask.shape[0]-1 or xx==mask.shape[1]-1
                for ny,nx in ((yy-1,xx),(yy+1,xx),(yy,xx-1),(yy,xx+1)):
                    if 0<=ny<mask.shape[0] and 0<=nx<mask.shape[1] and not visited[ny,nx] and mask[ny,nx]==value:
                        visited[ny,nx]=True;queue.append((ny,nx))
            counts.append(count);border_flags.append(border)
        return counts,border_flags
    visible,_=components(True);hidden,touches_border=components(False)
    holes=[n for n,b in zip(hidden,touches_border) if not b]
    significant=[n for n in holes if n*step*step>=1e-6]
    return {'visible_component_count':len(visible),'visible_component_sample_counts':sorted(visible,reverse=True),
        'enclosed_skin_hole_count':len(holes),'significant_enclosed_skin_holes':len(significant),
        'significant_hole_minimum_area':1e-6,'maximum_enclosed_skin_hole_area':max(holes,default=0)*step*step,
        'enclosed_skin_hole_total_area':sum(holes)*step*step,
        'method':'four-neighbor components of independently ray-visible aperture; external occlusion is excluded'}


def visible_metrics(eye, body, step=0.0006, skin_epsilon=0.0000005):
    """Return aperture/green/black areas and shapes from shared frontal rays.

    Both objects can have nonidentity transforms.  The sample grid is anchored
    to world zero, so a change in a hidden globe's bounding box does not shift
    the sampling phase.  The tiny depth tolerance suppresses coplanar edge
    ambiguity and is applied identically to the baseline and revised meshes.
    """
    if step <= 0 or skin_epsilon < 0:
        raise ValueError('Invalid frontal projection sampling parameters')
    colors = eye.data.color_attributes.get('EyeColor')
    if colors is None or colors.domain != 'CORNER':
        raise ValueError('Expected per-corner EyeColor on ' + eye.name)
    local_points = coordinates(eye)
    world_points = np.asarray([tuple(eye.matrix_world @ Vector(point))
                               for point in local_points], dtype=float)
    xs = np.arange(math.ceil(float(world_points[:, 0].min()) / step) * step,
                   math.floor(float(world_points[:, 0].max()) / step) * step + step * 0.5,
                   step)
    zs = np.arange(math.ceil(float(world_points[:, 2].min()) / step) * step,
                   math.floor(float(world_points[:, 2].max()) / step) * step + step * 0.5,
                   step)
    eye_inverse = eye.matrix_world.inverted()
    body_inverse = body.matrix_world.inverted()
    world_direction = Vector((0, 1, 0))
    eye_direction = (eye_inverse.to_3x3() @ world_direction).normalized()
    body_direction = (body_inverse.to_3x3() @ world_direction).normalized()
    ray_y = min(-2.0, float(world_points[:, 1].min()) - 1.0)
    points, green, black, gaps = [], [], [], []
    for x in xs:
        for z in zs:
            world_origin = Vector((float(x), ray_y, float(z)))
            eye_hit, eye_point, _, polygon_index = eye.ray_cast(
                eye_inverse @ world_origin, eye_direction)
            if not eye_hit:
                continue
            body_hit, body_point, _, _ = body.ray_cast(
                body_inverse @ world_origin, body_direction)
            eye_y = float((eye.matrix_world @ eye_point).y)
            if body_hit:
                body_y = float((body.matrix_world @ body_point).y)
                if eye_y >= body_y - skin_epsilon:
                    continue
                gaps.append(body_y - eye_y)
            point = (float(x), float(z))
            points.append(point)
            if not 0 <= polygon_index < len(eye.data.polygons):
                raise ValueError('Eye modifier changed polygon indexing')
            polygon = eye.data.polygons[polygon_index]
            color = colors.data[polygon.loop_start].color
            (black if color[1] < 0.04 else green).append(point)
    result = {
        'aperture': _statistics(points, step),
        'green': _statistics(green, step),
        'black': _statistics(black, step),
        'sampling': {'step': float(step), 'skin_epsilon': float(skin_epsilon),
                     'grid_anchor_xz': [0.0, 0.0]},
        'aperture_topology': aperture_topology(points,xs,zs,step),
    }
    if gaps:
        result['visible_surface_gap'] = {
            'minimum': float(min(gaps)), 'maximum': float(max(gaps)),
            'mean': float(np.mean(gaps)),
        }
    return result
