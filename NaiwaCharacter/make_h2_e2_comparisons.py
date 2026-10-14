"""Pillow R1/H2/E2 panels with identical framing and explicit diagnostic labels.

python this.py --standard-only  # Provisional shape iteration
python this.py                  # Final full color/gray/reflection/grazing panels
"""
import argparse
import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw
from make_spherical_eye_comparisons import font, centered

ROOT=Path(__file__).resolve().parent
OUT=ROOT/'versions/20261007_H2_E2_overall_refinement'
CMP=OUT/'comparisons'
MODELS=('R1','H2')
VIEWS=('front','three_quarter','side')
LABELS={'R1':'固定 R1 / 整体修正前','H2':'H2 / 连续整体修正','E2':'E2 / 眼部校正'}
VIEW_LABELS={'front':'0° 正面','three_quarter':'45° 斜侧','side':'90° 侧面',
             'yaw45_pitch-5':'45° / −5° 固定补充机位',
             'yaw55_pitch0':'55° / 0° 固定补充机位',
             'lens135':'135 mm 长焦 · 固定距离 3.225'}


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def verify_protocol(standard_only):
    protocol=json.loads((OUT/'render_protocol.json').read_text(encoding='utf-8-sig'))
    assert not protocol['saved_source_models'], 'Render mutated input models'
    assert protocol['same_camera_light_exposure_per_comparison'], 'Comparison conditions differ'
    grouped={}
    seen=set()
    for model in MODELS:
        source=protocol['sources'][model]
        assert digest(source['path'])==source['sha256'],f'Stale render source {model}'
        if not standard_only:
            assert source['diagnostics_complete'],f'Missing final reflection/grazing diagnostics {model}'
    assert all(protocol['sources'][m]['standard_lighting']==protocol['sources'][MODELS[0]]['standard_lighting'] for m in MODELS), 'Different original light conditions'
    for record in protocol['files']:
        if record['model'] not in MODELS:
            continue
        model=record['model']
        assert record['source_sha256']==protocol['sources'][model]['sha256'],f'Mixed source versions {record["file"]}'
        assert (OUT/record['file']).is_file(),record['file']
        assert record['file'] not in seen, f'Duplicate render record {record["file"]}'
        seen.add(record['file'])
        key=Path(record['file']).name.removeprefix(model+'_')
        settings={k:record[k] for k in ('pose','mode','engine','diagnostic','view_transform','exposure','gamma')}
        if key in grouped:
            assert settings==grouped[key],f'Different camera, lighting or shading for {key}'
        grouped[key]=settings
    return protocol


def panel(prefix,views,title,subtitle,footer,destination,input_dir=None,crop=None):
    input_dir = input_dir or CMP
    filenames=[f'{m}_{prefix}_{v}.png' for v in views for m in MODELS]
    images={}
    sizes=set()
    for filename in filenames:
        with Image.open(input_dir/filename) as im:
            sizes.add(im.size)
            images[filename]=im.convert('RGB')
    assert len(sizes)==1, 'Different image dimensions'
    raw_size=sizes.pop()
    if crop is not None:
        assert 0 <= crop[0] < crop[2] <= raw_size[0] and 0 <= crop[1] < crop[3] <= raw_size[1], 'Crop exceeds raw image'
        images={k:v.crop(crop) for k,v in images.items()}
    tile_w=600
    input_w,input_h=next(iter(images.values())).size
    tile_h=round(input_h*tile_w/input_w)
    margin,gutter,header,row_bar,foot=30,20,170,46,130
    size=(2*margin+len(MODELS)*tile_w+(len(MODELS)-1)*gutter,header+len(views)*(tile_h+row_bar+gutter)+foot)
    canvas=Image.new('RGB',size,'#f0f3f7')
    draw=ImageDraw.Draw(canvas)
    draw.rectangle((0,0,size[0],108),fill='#17263b')
    draw.text((margin,19),title,font=font(30),fill='white')
    draw.text((margin,65),subtitle,font=font(21),fill='#cbd6e6')
    for col,model in enumerate(MODELS):
        centered(draw,(margin+col*(tile_w+gutter)+tile_w/2,126),LABELS[model],24,'#17263b')
    for row,view in enumerate(views):
        y=header+row*(tile_h+row_bar+gutter)
        draw.text((margin,y+5),VIEW_LABELS.get(view,'50° / −2.5° 固定共同机位'),font=font(22),fill='#43566d')
        for col,model in enumerate(MODELS):
            tile=images[f'{model}_{prefix}_{view}.png'].resize((tile_w,tile_h),Image.Resampling.LANCZOS)
            canvas.paste(tile,(margin+col*(tile_w+gutter),y+row_bar))
    draw.text((margin,size[1]-110),footer,font=font(20),fill='#43566d')
    draw.text((margin,size[1]-75),'同一相机尺度与相同显示缩放；未按眼径或各候选外形重新对齐。',font=font(20),fill='#68788c')
    draw.text((margin,size[1]-42),'原始输入不修改；候选通过最终几何与多光照复核后才形成采用结论。',font=font(19),fill='#68788c')
    path=CMP/destination
    canvas.save(path)
    return {'output':destination,'output_sha256':digest(path),
            'inputs':[str((input_dir/f).relative_to(OUT)) for f in filenames],
            'input_sha256':{str((input_dir/f).relative_to(OUT)):digest(input_dir/f) for f in filenames},
            'raw_size':list(raw_size),'display_size':[tile_w,tile_h],
            'crop':list(crop) if crop else None,'crop_same_for_all':True,
            'per_model_scale':False,'camera_reregistration':False}


def main():
    global MODELS, OUT, CMP
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--standard-only',action='store_true')
    parser.add_argument('--labels','--models',dest='labels',nargs='+',choices=['R1','H2','E2'],default=['R1','H2'])
    parser.add_argument('--output',type=Path,default=OUT,help='Render root containing render_protocol.json and comparisons/')
    args=parser.parse_args()
    assert len(args.labels)==len(set(args.labels)), 'Duplicate model label'
    MODELS=tuple(args.labels)
    OUT=args.output.resolve();CMP=OUT/'comparisons'
    chain=' → '.join(MODELS)
    protocol=verify_protocol(args.standard_only)
    rows=[]
    standard='固定 R1 输入 · 原交付灯光 / 同机位 / 同曝光 · Idle 第 1 帧'
    common='整体曲线优先；实际网格、灰模和反射用于核对头颈肩连续性。'
    for prefix,views,title,name in (
        ('head',VIEWS,f'{chain}：头部彩色三视图','comparison_head.png'),
        ('head_gray',VIEWS,f'{chain}：原灯光灰模三视图','comparison_head_gray.png'),
        ('full',VIEWS,f'{chain}：全身三视图与肩背','comparison_full.png'),
        ('full_gray',VIEWS,f'{chain}：全身灰模三视图','comparison_full_gray.png'),
        ('full_body_id',VIEWS,f'{chain}：全身主体轮廓 ID','comparison_full_body_id.png'),
        ('body_id',VIEWS,f'{chain}：主体无光对象 ID','comparison_body_id.png')):
        rows.append(panel(prefix,views,title,standard,common,name))
    rows.append(panel('common',('ORTHO',),f'{chain}：固定共同参考机位',
                      '50° / −2.5° · 正交尺度 0.86 · 固定原注册系数',
                      '完整 900 × 900 原始画幅；参考误差由白发光主体 ID 独立测量。',
                      'comparison_fixed_reference.png',input_dir=OUT/'reference_comparison'))
    rows.append(panel('extra',('yaw45_pitch-5','yaw55_pitch0','lens135'),
                      f'{chain}：补充机位交叉检查',
                      '45° / 55° / 原长焦 · 每行各候选相同相机、灯光与曝光',
                      '只用于多视角形体检查；未重新拟合注册或单独按候选调整镜头。',
                      'comparison_reference_camera_crosschecks.png',input_dir=OUT/'reference_comparison'))
    rows.append(panel('head_gray',VIEWS,f'{chain}：下颊—喉部灰模细节',
                      '原灯光 / 同机位 / 同曝光 · 各列使用完全相同的固定像素裁切',
                      '固定裁切：原图 x=40～870，y=340～770；只放大观察，不参与参考重新注册。',
                      'comparison_lowerface_detail.png',crop=(40,340,870,770)))
    rows.append(panel('head',VIEWS,f'{chain}：球眼与真实皮肤接触',
                      '原灯光 / 同机位 / 同曝光 · 各列眼部同一像素裁切',
                      '固定裁切：原图 x=40～870，y=180～430；检视球眼暴露孔、瞳孔与高光。',
                      'comparison_eyes_detail.png',crop=(40,180,870,430)))
    if not args.standard_only:
        rows.append(panel('reflection',VIEWS,f'{chain}：真实环境反射条纹诊断',
                          '中性材质 · Roughness 0.20 · 真实世界环境条纹反射 · Cycles',
                          '诊断测试：观察条纹是否出现局部折断、重复横带；条纹没有贴到主体表面。',
                          'comparison_reflection.png'))
        for variant in ('A','B'):
            rows.append(panel('grazing_'+variant,VIEWS,f'{chain}：低角掠射灰模 · 灯位 '+variant,
                              '中性粗糙度 1.0 灰模 · 横向窄条面光 · 两版同一测试条件',
                              '诊断测试：改变光源方位检查下颊、侧颈、后肩及后脑—后颈的浅波纹。',
                              'comparison_grazing_'+variant+'.png'))
        rows.append(panel('reflection',VIEWS,f'{chain}：下颊反射条纹细节',
                          '真实反射诊断 · 各列同一像素区域裁切',
                          '固定裁切：原图 x=40～870，y=340～770；用于检查嘴下凸缘处反射连续性。',
                          'comparison_reflection_detail.png',crop=(40,340,870,770)))
        pose_protocol=json.loads((OUT/'pose_comparison/pose_protocol.json').read_text(encoding='utf-8-sig'))
        assert not pose_protocol['saved_source_models']
        assert pose_protocol['same_camera_light_exposure']
        pose_settings={}
        for model in MODELS:
            assert pose_protocol['sources'][model]['sha256']==protocol['sources'][model]['sha256']
        for record in pose_protocol['files']:
            if record['model'] not in MODELS:continue
            assert record['source_sha256']==protocol['sources'][record['model']]['sha256'], f'Stale pose {record["file"]}'
            assert (OUT/'pose_comparison'/record['file']).is_file(), record['file']
            key=(record['action'],record['frame'],record['pose']['yaw'])
            settings={k:record[k] for k in ('pose','engine','exposure')}
            if key in pose_settings:assert pose_settings[key]==settings
            pose_settings[key]=settings
        for action,frame in (('Walk',9),('Laugh',7),('Jump',6)):
            rows.append(panel(f'Naiwa_{action}_{frame}',('three_quarter','side'),
                              f'{chain}：{action} 第 {frame} 帧',
                              '真实绑定动作 · 同相机 / 灯光 / 曝光 · 原灯光灰模',
                              '检视肩背过渡、眼体接触与嘴缝；动作曲线与骨架保持原工程。',
                              f'comparison_pose_{action}.png',input_dir=OUT/'pose_comparison'))
    recorded={record['file'].replace('\\','/') for record in protocol['files'] if record['model'] in MODELS}
    if not args.standard_only:
        recorded.update('pose_comparison/'+record['file'] for record in pose_protocol['files'] if record['model'] in MODELS)
    for row in rows:
        assert all(path.replace('\\','/') in recorded for path in row['inputs']), f'Unrecorded panel inputs: {row["output"]}'
    (CMP/'assembly_protocol.json').write_text(json.dumps({
        'model_order':list(MODELS),'source_sha256':{m:protocol['sources'][m]['sha256'] for m in MODELS},
        'same_render_conditions_verified':True,'standard_only':args.standard_only,'panels':rows},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('H2_E2_COMPARISON_ASSEMBLY_OK',len(rows),'panels',flush=True)


if __name__=='__main__':
    main()


