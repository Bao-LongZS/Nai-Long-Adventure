"""Compose read-only camera review using one body-based registration for all models.

Input render pixels and source mesh files are preserved. Only uniform scale and
translation are used in the head comparison; eye diameter never sets the scale.
Run with ordinary Python/Pillow, after the Blender reference render stages.
"""
import hashlib
import json
import math
import shutil
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent
OUT = ROOT / 'versions/20261006_spherical_eyes'
DEST = OUT / 'reference_comparison'
REF = ROOT / 'references/20261005/pdf_page03_image03_IM60.jpg'
FONT = 'C:/Windows/Fonts/msyh.ttc'


def font(size):
    return ImageFont.truetype(FONT, size)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(path):
    return json.loads(path.read_text(encoding='utf-8'))


def affine_panel(im, scale, translation, crop, size, display_scale):
    # Image.transform expects destination-to-source coefficients.
    tx, ty = translation
    ox, oy = crop
    return im.transform(size, Image.Transform.AFFINE,
                        (1 / (display_scale * scale), 0, (ox - tx) / scale,
                         0, 1 / (display_scale * scale), (oy - ty) / scale),
                        resample=Image.Resampling.BICUBIC, fillcolor='white')


def mark(panel, point, crop, display_scale, label, color):
    x, y = (np.array(point) - crop) * display_scale
    dr = ImageDraw.Draw(panel)
    dr.line((x - 6, y, x + 6, y), fill=color, width=2)
    dr.line((x, y - 6, x, y + 6), fill=color, width=2)
    dr.text((x + 8, y - 20), label, font=font(18), fill=color)


def yellow_row_bounds(im, row, scale=1, translation=(0, 0)):
    a = np.asarray(im, float)
    tx, ty = translation
    y = int(round((row - ty) / scale))
    yellow = ((a[:, :, 0] > 1.02 * a[:, :, 1]) &
              (a[:, :, 1] > 1.15 * a[:, :, 2]) & (a[:, :, 0] > 100))
    xs = np.where(yellow[max(0, y - 1):min(len(a), y + 2)].any(axis=0))[0]
    if not len(xs):
        return None
    return {'sample_row_original_pixels': y,
            'raw_x_range_pixels': [int(xs.min()), int(xs.max())],
            'registered_x_range_reference_pixels':
                [float(scale * xs.min() + tx), float(scale * xs.max() + tx)]}


def main():
    search = load(OUT / 'reference_search/camera_search.json')
    protocol = load(DEST / 'reference_and_gaze_protocol.json')
    baseline = load(ROOT / 'diagnostics/20261005_head_geometry/reference_alignment.json')
    row = next(r for r in search['records'] if r['candidate'] == 'A' and
               r['pose']['type'] == 'ORTHO' and r['pose']['yaw'] == 50 and
               r['pose']['pitch'] == -2.5)
    # The source body and mouth are identical in original, A and B. Fit once.
    s600 = row['body_landmark_uniform_scale_to_reference']
    s900 = s600 * 600 / 900
    trans = row['body_landmark_translation_to_reference']
    ref_features = search['reference_landmarks']
    refD = baseline['reference_eye']['equivalent_area_diameter_pixels']
    ref_image = Image.open(REF).convert('RGB')
    shutil.copy2(REF, DEST/'reference_figure13.jpg')
    pupil_roi = np.asarray(ref_image, float)[105:153,353:408]
    pupil_mask = ((pupil_roi[:,:,0]<50)&(pupil_roi[:,:,1]<60)&(pupil_roi[:,:,2]<50))
    py, px = np.where(pupil_mask)
    ref_pupil_center = [float(px.mean()+353),float(py.mean()+105)]
    raw = {label: Image.open(DEST / f'{label}_common_ORTHO.png').convert('RGB')
           for label in ('original', 'A', 'B')}
    common = {r['model']: r for r in protocol['records']
              if r['gaze_degrees'] == 0 and r['pose']['type'] == 'ORTHO'}
    measurements = {}
    for label, rec in common.items():
        eyes = {}
        for side in ('L', 'R'):
            ap = rec['eyes'][side]['aperture']
            pu = rec['eyes'][side]['pupil']
            center = s900 * np.array(ap['center_pixels']) + trans
            pupil = s900 * np.array(pu['center_pixels']) + trans
            eyes[side] = {'raw_aperture_pixels': ap, 'raw_pupil_pixels': pu,
                          'registered_center_reference_pixels': center.tolist(),
                          'registered_D_reference_pixels': ap['D_pixels'] * s900,
                          'registered_pupil_center_reference_pixels': pupil.tolist(),
                          'pupil_minus_outer_center_reference_pixels': (pupil-center).tolist()}
        measurements[label] = {'eyes': eyes, 'far_D_over_near_D':
                               rec['eyes']['L']['aperture']['D_pixels'] /
                               rec['eyes']['R']['aperture']['D_pixels']}

    crown = s600 * np.array(row['crown_pixels']) + trans
    mouth = s600 * np.array(row['mouth_pixels']) + trans
    contours = []
    for y in (90, 180, 220, 260, 300):
        rb = yellow_row_bounds(ref_image, y)
        mb = yellow_row_bounds(raw['A'], y, s900, trans)
        delta = np.array(mb['registered_x_range_reference_pixels']) - rb['registered_x_range_reference_pixels']
        contours.append({'reference_y_pixels': y, 'reference': rb, 'model': mb,
                         'model_minus_reference_front_back_pixels': delta.tolist(),
                         'model_minus_reference_front_back_over_reference_D': (delta/refD).tolist()})

    paths = {'original': ROOT/'versions/20261005_head_side_continuity/Naiwa_Character.blend',
             'A': OUT/'candidate_A/Naiwa_Character.blend', 'B': OUT/'candidate_B/Naiwa_Character.blend'}
    provenance = {label: {'path': str(path), 'sha256': digest(path),
                          'mtime_seconds': path.stat().st_mtime,
                          'comparison_render_newer_than_source': (DEST/f'{label}_common_ORTHO.png').stat().st_mtime >= path.stat().st_mtime}
                  for label, path in paths.items()}
    regression = load(OUT/'spherical_eyes_validation.json')
    for label in ('A', 'B'):
        checked = regression['candidates'][label]
        provenance[label]['current_validation_report'] = 'spherical_eyes_validation.json'

    # Four columns use the same crop in reference coordinates, including the neck.
    panel_size = (620, 470)
    display_scale = 1.5
    crop = np.array([270., 40.])
    canvas = Image.new('RGB', (2560, 790), 'white')
    dr = ImageDraw.Draw(canvas)
    dr.text((24, 20), '图1.3与原模型 / A / B：统一主体尺度的稳定对照', font=font(34), fill='#203448')
    dr.text((24, 70), '共用偏航50°、仰角−2.5°、正交0.86；只从未改头顶和嘴拟合一次整体缩放与平移。', font=font(23), fill='#536577')
    titles = [('reference', 'PDF图1.3原始嵌入图'), ('original', '原模型：跟随脸面眼冠'),
              ('A', 'A：较低露出球体'), ('B', 'B：较高露出球体')]
    for i, (label, title) in enumerate(titles):
        x = 20 + i * 640
        dr.text((x, 116), title, font=font(24), fill='#203448')
        im = ref_image if label == 'reference' else raw[label]
        sc = 1 if label == 'reference' else s900
        tr = (0, 0) if label == 'reference' else trans
        panel = affine_panel(im, sc, tr, crop, panel_size, display_scale)
        feats = ref_features if label == 'reference' else {
            'near_eye': measurements[label]['eyes']['R']['registered_center_reference_pixels'],
            'far_visible_eye_centroid_approx': measurements[label]['eyes']['L']['registered_center_reference_pixels'],
            'crown': crown, 'mouth_approx': mouth}
        for key, symbol, color in [('near_eye', 'E', '#e63c91'), ('crown', 'C', '#1879a6'),
                                   ('mouth_approx', 'M', '#984517')]:
            mark(panel, feats[key], crop, display_scale, symbol, color)
        pd = ImageDraw.Draw(panel)
        for y in (220, 260):
            yy = (y-crop[1])*display_scale
            pd.line((0, yy, panel_size[0], yy), fill='#87929d', width=1)
        pd.rectangle((0, 0, panel_size[0]-1, panel_size[1]-1), outline='#87929d', width=1)
        canvas.paste(panel, (x, 158))
        if label == 'reference':
            lines = [f'近眼原始外径≈{refD:.2f} px', '远/近外径≈0.349（近似椭圆）', '原图900×1130；未按眼径重缩放']
        else:
            m = measurements[label]
            lines = [f'稳定尺度近眼外径={m["eyes"]["R"]["registered_D_reference_pixels"]:.2f} px',
                     f'远/近外径={m["far_D_over_near_D"]:.3f}', f'原渲染900×900；共用缩放{s900:.6f}']
        for n, line in enumerate(lines):
            dr.text((x, 640+n*31), line, font=font(21), fill='#203448')
    dr.text((24, 746), 'E近眼外缘中心 / C头顶 / M嘴；横线为参考y=220、260px。轮廓残差见JSON，不能据此直接求缩头倍率。',
            font=font(21), fill='#536577')
    canvas.save(DEST/'reference_stable_comparison.png')

    # Gaze test panels retain a common image scale. No test is saved as a model.
    gaze_canvas = Image.new('RGB', (1830, 2110), 'white')
    gd = ImageDraw.Draw(gaze_canvas)
    gd.text((20, 18), '注视诊断：0°基础 / 镜像外展20° / 镜像外展35°', font=font(32), fill='#203448')
    gd.text((20, 67), '仅改变同球面黑瞳材质与高光接触点；绿球与主体不动；20°/35°没有写回工程。', font=font(23), fill='#536577')
    for rowi, (label, view) in enumerate([('A', 'reference'), ('A', 'front'), ('B', 'reference'), ('B', 'front')]):
        y = 125+rowi*490
        gd.text((20, y), f'{label} / '+('统一参考相机' if view=='reference' else '正面'), font=font(24), fill='#203448')
        for coli, angle in enumerate((0, 20, 35)):
            if angle == 0:
                file = DEST/f'{label}_common_ORTHO.png' if view=='reference' else OUT/f'comparisons/{label}_head_front.png'
            else:
                file = DEST/f'{label}_gaze{angle}_common_ORTHO.png' if view=='reference' else DEST/f'{label}_gaze{angle}_front.png'
            im = Image.open(file).convert('RGB')
            # Select a shared physical head field per row. Fixed crop bounds,
            # independent of eye size, keep the gaze alternatives comparable.
            if view == 'reference':
                im = im.crop((30, 130, 790, 750)).resize((570, 465), Image.Resampling.LANCZOS)
            else:
                # The base front render uses a 900x1000 frame at scale .85;
                # variants use 900x900 at scale .86 and z target 2.425.
                # Align by exact camera units rather than eye bounds.
                if angle==0:
                    src_s = .86/.85*1000/900
                    # x pixel/world=1000/.85 for portrait ortho camera;
                    # y target difference .055 moves base landmarks upwards.
                    shift_x = 450-src_s*450
                    shift_y = 500-src_s*450-(2.425-2.37)*1000/.85
                    im = im.transform((900,900),Image.Transform.AFFINE,
                                      (src_s,0,shift_x,0,src_s,shift_y),resample=Image.Resampling.BICUBIC)
                im=im.crop((60,100,840,735)).resize((570,465),Image.Resampling.LANCZOS)
            x=20+coli*605
            gaze_canvas.paste(im,(x,y+30))
            gd.text((x+420,y),f'{angle}°',font=font(24),fill='#203448')
    gaze_canvas.save(DEST/'reference_gaze_comparison.png')

    # Readable full search grids preserve their common original camera scale.
    for candidate in ('A', 'B'):
        grid = Image.new('RGB',(1450,1190),'white')
        gr = ImageDraw.Draw(grid)
        gr.text((20,15),f'{candidate}：规定的15个观察方向；同相机尺度0.86',font=font(28),fill='#203448')
        for pi,pitch in enumerate((-5,0,5)):
            for yi,yaw in enumerate((40,50,60,70,80)):
                rec=next(r for r in search['records'] if r['candidate']==candidate and r['pose']['type']=='ORTHO' and r['pose']['yaw']==yaw and r['pose']['pitch']==pitch)
                im=Image.open(OUT/'reference_search'/rec['filename']).convert('RGB').resize((275,275),Image.Resampling.LANCZOS)
                x,y=15+yi*288,65+pi*350
                grid.paste(im,(x,y))
                gr.text((x,y+280),f'{yaw}° / {pitch}°',font=font(20),fill='#203448')
                gr.text((x,y+310),f'远/近{rec["far_D_over_near_D"]:.3f} 得分{rec["pose_score"]:.3f}',font=font(17),fill='#536577')
        gr.text((20,1130),'总分为近似诊断指标；参考相机未知，存在造型与相机的耦合，不能作为唯一相机恢复。',font=font(21),fill='#536577')
        grid.save(DEST/f'reference_camera_grid_{candidate}.png')

    eye_residuals={label:{side:{'center_delta_pixels':
                    (np.array(data['registered_center_reference_pixels'])-
                     ref_features['near_eye' if side=='R' else 'far_visible_eye_centroid_approx']).tolist()}
                    for side,data in m['eyes'].items()}
                   for label,m in measurements.items()}
    review={'reference_figure_verification':{
        'figure':'PDF第3页图1.3奶蛙经典形象', 'embedded_image':str(REF),
        'packaged_original_image':'reference_figure13.jpg',
        'embedded_resolution':[900,1130], 'embedded_sha256':digest(REF),
        'page_screenshot_is_full_page':True, 'manual_outer_near_eye_bbox_pixels':[[352,101],[409,155]],
        'near_eye_equal_area_D_pixels':refD, 'far_visible_bbox_approx_pixels':[[309,120],[320,154]],
        'near_pupil_dark_interior_centroid_pixels':ref_pupil_center,
        'near_pupil_center_method':'Raw ROI [353,105,408,153], R<50 G<60 B<50. Dark interior centroid only; threshold area is not used as outer pupil diameter.',
        'far_visible_D_over_near_D_approx':search['reference_far_visible_D_over_near_D_approx'],
        'landmarks_original_pixels':ref_features,'boundary_uncertainty_px':2},
        'selected_common_camera':{**protocol['common_pose'],'roll_degrees':0,
            'resolution':[900,900], 'view_transform':'Standard','exposure':-.4,
            'status':'Practical common diagnostic camera, not uniquely recovered reference camera',
            'selection':'A best common ortho score; B favors 135mm 50/0 in its individual search. Use one 50/-2.5 ORTHO camera to compare original/A/B fairly.'},
        'camera_search_audit':{'yaw_grid':[40,50,60,70,80],'pitch_grid':[-5,0,5],
            'grid_records':30,'total_records':len(search['records']),
            'fine_sampling':{'A':[r['pose'] for r in search['records'] if r['candidate']=='A' and 'refine' in r['filename']],
                             'B':[r['pose'] for r in search['records'] if r['candidate']=='B' and 'refine' in r['filename']]},
            'best_per_candidate':{c:{'pose':r['pose'],'diagnostic_score':r['pose_score']} for c,r in search['selected_per_candidate'].items()},
            'score_limitations':'Weights are heuristic and far-reference ellipse is approximate. Includes eye center, near circularity, far visibility, crown and mouth. Body geometry prevents a joint exact fit.'},
        'fixed_scale_registration':{'model_original_resolution':[900,900],
            'model_pixels_to_reference_pixels_scale':s900,'model_to_reference_translation_pixels':trans,
            'search_render_scale_at_600px':s600,'fit_landmarks':['unchanged yellow crown','unchanged mouth center'],
            'shared_by':['original','A','B'],'scale_computed_once':True,
            'eye_diameter_used_for_scale':False,'uniform_xy_scale':True,
            'display_scale_reference_pixels_to_panel_pixels':display_scale,
            'display_crop_origin_reference_pixels':crop.tolist(),'display_panel_size_pixels':list(panel_size)},
        'source_provenance':provenance, 'common_camera_measurements':measurements,
        'candidate_recommendation':{'overall':'A for continuation with the primary Fig1.3 target',
            'front':'B gives a more centered pupil and less diagonal clipping. Both retain prescribed front outer area and spacing; A is slightly more oblique in visible contour.',
            'oblique':'B near exposed eye is rounder; A exposes less far eye, closer to Fig1.3. Both much rounder than original non-spherical eye.',
            'reference':'A common camera score .860 versus B .970, and far/near visible ratio is lower. Neither reproduces reference far-eye occlusion.',
            'gaze':'Saved basic gaze remains F=-Y. 35 degree temporary variant better centers near pupil at this oblique camera; reference eye gaze is unknown. Gaze tests do not justify changing the saved construction.'},
        'residuals':{'crown_model_minus_reference_pixels':(crown-ref_features['crown']).tolist(),
            'mouth_model_minus_reference_pixels':(mouth-ref_features['mouth_approx']).tolist(),
            'eye_centers_model_minus_reference':eye_residuals,'yellow_outline_rows':contours,
            'interpretation':'After shared crown/mouth registration the neck front remains too far forward (screen left), and back silhouette is less far right; upper crown posterior contour is also narrower. No head/neck geometry changed in this round.',
            'contour_method':'Original RGB pixel yellow threshold R>1.02G, G>1.15B, R>100; +/-1 raw pixel row. Threshold silhouette is approximate, not a mesh dimensional measurement.',
            'contour_uncertainty_registered_pixels_approx':3,
            'not_a_head_scaling_multiplier':True},
        'gaze_variants':{'angles_degrees':[0,20,35],'temporary_material_and_glint_only':True,
            'geometry_unchanged_asserted_in_renderer':True,'saved_to_blend':False,
            'pixel_measurements_source':'reference_and_gaze_protocol.json'},
        'artifacts':['reference_stable_comparison.png','reference_gaze_comparison.png',
                     'reference_camera_grid_A.png','reference_camera_grid_B.png']}
    (DEST/'reference_review.json').write_text(json.dumps(review,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('SPHERICAL_REFERENCE_REVIEW_OK',s900,trans)
    print('CONTOUR_RESIDUALS',[(r['reference_y_pixels'],r['model_minus_reference_front_back_pixels']) for r in contours])


if __name__ == '__main__':
    main()
