"""Assemble fixed C3 review panels without fitting, retouching or re-rendering.

Run with bundled Python: this.py --root VERSION_ROOT --labels E2 A
Only labels explicitly requested are displayed; source images stay untouched.
Reference coordinates use the already frozen model-pixel to reference-pixel
registration. Display crop and scale are identical for all models.
"""
import argparse
import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont
import numpy as np


SCRIPT_ROOT = Path(__file__).resolve().parent
REFERENCE = SCRIPT_ROOT / 'versions/20261007_H2_E2_overall_refinement/references/figure_1_3.jpg'
REGISTRATION = {'scale': .41175526202447044,
                'translation': [245.38813136509236, -17.159665982101984]}
REFERENCE_CROP = (260, 40, 660, 340)
VIEWS = ('front', 'three_quarter', 'side')
VIEW_LABELS = ('0° 正面', '45° 斜侧', '90° 侧面')
FONT_PATH = Path('C:/Windows/Fonts/msyh.ttc')
SCAN_ROWS = (90, 180, 220, 260, 300)
NECK_SCAN_ROWS = (220, 260, 300)
REFERENCE_SATURATION = .18
REFERENCE_SEARCH_X = (260, 700)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def font(size):
    return ImageFont.truetype(str(FONT_PATH), size)


def read_image(path):
    with Image.open(path) as img:
        return img.convert('RGB')


def compose(title, subtitle, columns, rows, destination, inputs):
    """Rows consist of unaltered image objects and a row label."""
    width = 495
    margin, gap = 24, 16
    h = max(round(img.height * width / img.width)
            for _, row in rows for img in row)
    canvas = Image.new('RGB', (margin * 2 + len(columns) * (width + gap) - gap,
                      126 + len(rows) * (h + 44 + gap) + 30), '#edf1f6')
    draw = ImageDraw.Draw(canvas)
    draw.text((margin, 14), title, fill='#18304e', font=font(30))
    draw.text((margin, 57), subtitle, fill='#52657b', font=font(19))
    for col, label in enumerate(columns):
        draw.text((margin + col * (width + gap), 92), label,
                  fill='#18304e', font=font(23))
    for row_index, (label, row) in enumerate(rows):
        y = 126 + row_index * (h + 44 + gap)
        for col, img in enumerate(row):
            x = margin + col * (width + gap)
            scaled = img.resize((width, round(img.height * width / img.width)),
                                Image.Resampling.LANCZOS)
            canvas.paste(scaled, (x, y))
            draw.text((x, y + h + 7), label, fill='#52657b', font=font(20))
    canvas.save(destination)
    return {'file': str(destination), 'sha256': digest(destination),
            'inputs': [{'path': str(p), 'sha256': digest(p)} for p in inputs],
            'display_method': 'uniform image scale; no photometric changes',
            'display_column_width_px': width}


def registered_crop(path):
    image = read_image(path)
    s = REGISTRATION['scale']
    tx, ty = REGISTRATION['translation']
    x0, y0, x1, y1 = REFERENCE_CROP
    # Mapping requested reference-space output back to fixed render-space.
    return image.transform((x1-x0, y1-y0), Image.Transform.AFFINE,
                           (1/s, 0, (x0-tx)/s, 0, 1/s, (y0-ty)/s),
                           resample=Image.Resampling.BICUBIC,
                           fillcolor=(75, 75, 75))


def contour_measurements(root, labels):
    """New frozen raster protocol: reference color / antialiased body-ID edges.

    The exact historical segmentation and raster interpolation were not saved in
    the three authorized source documents. This protocol therefore reports its
    E2 baseline alongside every candidate and does not compare new measurements
    directly with the historical 18.182/7.561 values.
    """
    reference = np.asarray(read_image(REFERENCE), dtype=np.float64) / 255.
    maximum = reference.max(axis=2)
    saturation = (maximum-reference.min(axis=2)) / np.maximum(maximum, 1.e-12)
    rows = []
    xmin, xmax = REFERENCE_SEARCH_X
    for y in SCAN_ROWS:
        inside = np.flatnonzero(saturation[y, xmin:xmax] >= REFERENCE_SATURATION) + xmin
        if not len(inside):
            raise RuntimeError(f'No reference silhouette at fixed row {y}')
        # Pixel centers are integers in this explicitly frozen raster protocol.
        rows.append({'reference_y': y, 'reference_front_x': float(inside[0]),
                     'reference_back_x': float(inside[-1])})
    protocol = {'schema_version': 1,
        'status': 'new_fixed_protocol; historical exact algorithm unavailable in authorized sources',
        'historical_values_background_only': {'E2_front': 18.182, 'E2_back': 7.561},
        'units': 'original figure_1_3.jpg pixels',
        'pixel_center_convention': 'integer pixel-index coordinates',
        'scan_rows': SCAN_ROWS, 'neck_mean_scan_rows': NECK_SCAN_ROWS,
        'reference_search_x_half_open': REFERENCE_SEARCH_X,
        'reference_segmentation': {'saturation_threshold': REFERENCE_SATURATION,
            'method': 'HSV saturation >= threshold; fill between first and last colored pixel per fixed row'},
        'model_segmentation': {'input': 'shadow-independent white emission body ID on black',
            'threshold': .5, 'row_sampling': 'linear between source raster rows',
            'edge_sampling': 'linear threshold crossing adjacent source raster pixel centers'},
        'registration': REGISTRATION,
        'signed_error': 'model_x_reference_space - reference_x',
        'mean_algorithm': 'unweighted arithmetic mean of absolute signed front/back errors at neck rows',
        'reference_sha256': digest(REFERENCE), 'reference_rows': rows,
        'models': {}, 'automatic_visual_acceptance': False}
    s = REGISTRATION['scale']
    tx, ty = REGISTRATION['translation']
    for label in labels:
        source = root/'review'/'key'/label/f'{label}_common_body_id_50.png'
        gray = np.asarray(read_image(source), dtype=np.float64).mean(axis=2)/255.
        result = []
        for ref_row in rows:
            y = (ref_row['reference_y']-ty)/s
            y0 = int(np.floor(y)); frac = y-y0
            if not 0 <= y0 < gray.shape[0]-1:
                raise RuntimeError('Fixed scan row falls outside source body ID')
            sampled = (1.-frac)*gray[y0] + frac*gray[y0+1]
            inside = np.flatnonzero(sampled >= .5)
            if not len(inside):
                raise RuntimeError(f'No body ID silhouette at row {ref_row["reference_y"]}')
            left, right = int(inside[0]), int(inside[-1])
            if left == 0 or right == len(sampled)-1:
                raise RuntimeError('Body ID contour clips source image')
            def crossing(a, b):
                va, vb = sampled[a], sampled[b]
                return float(a + (.5-va)/(vb-va)*(b-a))
            front = s*crossing(left-1, left)+tx
            back = s*crossing(right, right+1)+tx
            result.append(dict(ref_row, model_source_y=y, model_front_x=front,
                model_back_x=back, front_signed_error_px=front-ref_row['reference_front_x'],
                back_signed_error_px=back-ref_row['reference_back_x']))
        neck = [r for r in result if r['reference_y'] in NECK_SCAN_ROWS]
        protocol['models'][label] = {'body_id_source': str(source),
            'body_id_sha256': digest(source), 'rows': result,
            'neck_mean_abs_front_error_px': float(np.mean([abs(r['front_signed_error_px']) for r in neck])),
            'neck_mean_abs_back_error_px': float(np.mean([abs(r['back_signed_error_px']) for r in neck]))}
    if 'E2' in protocol['models']:
        base = protocol['models']['E2']
        for label, data in protocol['models'].items():
            if label == 'E2':
                continue
            data['front_error_improvement_fraction_vs_E2'] = 1. - data['neck_mean_abs_front_error_px']/base['neck_mean_abs_front_error_px']
            data['back_error_change_px_vs_E2'] = data['neck_mean_abs_back_error_px']-base['neck_mean_abs_back_error_px']
    return protocol


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', required=True, type=Path)
    parser.add_argument('--labels', nargs='+', choices=('E2', 'A', 'B'), default=('E2',))
    args = parser.parse_args()
    root = args.root.resolve()
    labels = list(dict.fromkeys(args.labels))
    out = root / 'review' / 'assembled'
    out.mkdir(parents=True, exist_ok=True)
    protocol = {'schema_version': 1, 'labels': labels,
                'fixed_reference_registration': REGISTRATION,
                'reference_display_crop_xyxy': REFERENCE_CROP,
                'reference_crop_units': 'original figure_1_3.jpg pixels',
                'source_model_files_saved': False,
                'no_candidate_camera_or_registration_fitting': True,
                'no_photometric_changes': True, 'files': []}
    contract = json.loads((root/'c3_render_contract.json').read_text(encoding='utf-8-sig'))
    assert contract['fixed_reference_registration'] == REGISTRATION
    protocol['render_contract_sha256'] = digest(root/'c3_render_contract.json')
    for label in labels:
        p = root/'review'/'key'/label/'protocol.json'
        meta = json.loads(p.read_text(encoding='utf-8-sig'))
        assert meta['complete'], f'Incomplete {label} key renders'
        assert meta['contract_sha256'] == protocol['render_contract_sha256']
        for record in meta['files']:
            assert digest(p.parent/record['file']) == record['sha256']
    contours = contour_measurements(root, labels)
    contour_contract = {k: v for k, v in contours.items()
                        if k not in {'models', 'automatic_visual_acceptance'}}
    contour_contract_path = root/'c3_reference_contour_protocol.json'
    if contour_contract_path.exists():
        existing = json.loads(contour_contract_path.read_text(encoding='utf-8-sig'))
        assert existing == json.loads(json.dumps(contour_contract)), 'Frozen contour protocol changed'
    else:
        contour_contract_path.write_text(
            json.dumps(contour_contract, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    contours['measurement_contract_sha256'] = digest(contour_contract_path)
    (out/'fixed_reference_contours.json').write_text(
        json.dumps(contours, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    def model_path(label, filename):
        return root/'review'/'key'/label/f'{label}_{filename}.png'
    for mode, title in (('color', '头颈彩色 · 固定机位'), ('gray', '头颈灰模 · 固定机位')):
        inputs = [model_path(label, f'head_{mode}_{view}')
                  for view in VIEWS for label in labels]
        rows = [(caption, [read_image(model_path(label, f'head_{mode}_{view}'))
                           for label in labels]) for view, caption in zip(VIEWS, VIEW_LABELS)]
        protocol['files'].append(compose(title,
            'Idle 1；相机、照明、曝光与裁切固定。按原图像比例显示。', labels, rows,
            out/f'comparison_head_{mode}.png', inputs))
    ref_inputs = [REFERENCE] + [model_path(label, 'common_ORTHO_50') for label in labels]
    reference = read_image(REFERENCE).crop(REFERENCE_CROP)
    images = [reference] + [registered_crop(p) for p in ref_inputs[1:]]
    protocol['files'].append(compose('原图 / E2 / 候选 · 固定 50° 注册',
        '同一原图像素坐标裁切；模型注册保持。原图姿态、镜头与照明不同。',
        ['图 1.3 原图'] + labels, [('头颈；固定注册，无重新拟合', images)],
        out/'comparison_reference_50.png', ref_inputs))
    full_inputs = [model_path(label, 'full_color_three_quarter') for label in labels]
    protocol['files'].append(compose('全身彩色 · 固定 45°',
        'Idle 1；同一机位与照明。原动作和材质沿用工程。', labels,
        [('45° 全身', [read_image(p) for p in full_inputs])],
        out/'comparison_full_45.png', full_inputs))
    diagnostic = root/'review'/'baseline_diagnostic'/'E2'
    if (diagnostic/'protocol.json').exists():
        names = ['front', 'three_quarter', 'side', 'three_quarter_moved_key']
        diag_inputs = [diagnostic/f'E2_head_gray_AO_off_{n}.png' for n in names]
        protocol['files'].append(compose('E2 基线诊断 · 关闭 AO',
            'Cycles 32 样本；灰模无 AO 材质节点，物理阴影保留。第四图仅移动主光。',
            ['0°', '45°', '90°', '45° 移动主光'],
            [('光照诊断；与标准灯光评审分开', [read_image(p) for p in diag_inputs])],
            out/'baseline_AO_off_diagnostic.png', diag_inputs))
    (out/'assembly_protocol.json').write_text(
        json.dumps(protocol, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print('C3_ASSEMBLY_OK', str(out), labels)


if __name__ == '__main__':
    main()
