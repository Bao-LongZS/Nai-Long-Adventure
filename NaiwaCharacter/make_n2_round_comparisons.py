"""Pillow N2/R1 panels with identical framing and explicit diagnostic labels.

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
OUT=ROOT/'versions/20261007_N2_round_refinement'
CMP=OUT/'comparisons'
MODELS=('N2','R1')
VIEWS=('front','three_quarter','side')
LABELS={'N2':'固定 N2 / 精修前','R1':'R1 / 圆润精修候选'}
VIEW_LABELS={'front':'0° 正面','three_quarter':'45° 斜侧','side':'90° 侧面'}


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def verify_protocol(standard_only):
    protocol=json.loads((OUT/'render_protocol.json').read_text(encoding='utf-8'))
    assert not protocol['saved_source_models'], 'Render mutated input models'
    grouped={}
    for model in MODELS:
        source=protocol['sources'][model]
        assert digest(source['path'])==source['sha256'],f'Stale render source {model}'
        if not standard_only:
            assert source['diagnostics_complete'],f'Missing final reflection/grazing diagnostics {model}'
    assert protocol['sources']['N2']['standard_lighting']==protocol['sources']['R1']['standard_lighting'], 'Different original light conditions'
    for record in protocol['files']:
        if record['model'] not in MODELS:
            continue
        model=record['model']
        assert record['source_sha256']==protocol['sources'][model]['sha256'],f'Mixed source versions {record["file"]}'
        assert (OUT/record['file']).is_file(),record['file']
        key=Path(record['file']).name.removeprefix(model+'_')
        settings={k:record[k] for k in ('pose','mode','engine','diagnostic','view_transform','exposure','gamma')}
        if key in grouped:
            assert settings==grouped[key],f'Different camera, lighting or shading for {key}'
        grouped[key]=settings
    return protocol


def panel(prefix,views,title,subtitle,footer,destination,input_dir=CMP,crop=None):
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
        images={k:v.crop(crop) for k,v in images.items()}
    tile_w=600
    input_w,input_h=next(iter(images.values())).size
    tile_h=round(input_h*tile_w/input_w)
    margin,gutter,header,row_bar,foot=30,20,170,46,130
    size=(2*margin+2*tile_w+gutter,header+len(views)*(tile_h+row_bar+gutter)+foot)
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
    return {'output':destination,'inputs':[str((input_dir/f).relative_to(OUT)) for f in filenames],
            'raw_size':list(raw_size),'display_size':[tile_w,tile_h],
            'crop':list(crop) if crop else None,'crop_same_for_all':True,
            'per_model_scale':False,'camera_reregistration':False}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--standard-only',action='store_true')
    args=parser.parse_args()
    protocol=verify_protocol(args.standard_only)
    rows=[]
    standard='固定 N2 输入 · 原交付灯光 / 同机位 / 同曝光 · Idle 第 1 帧'
    common='下颊、喉部与颈肩局部圆顺；保留 N2 的宏观颈部回收。'
    for prefix,views,title,name in (
        ('head',VIEWS,'N2 → R1：头部彩色三视图','comparison_head.png'),
        ('head_gray',VIEWS,'N2 → R1：原灯光灰模三视图','comparison_head_gray.png'),
        ('full',('front',),'N2 → R1：全身比例与肩部','comparison_full.png'),
        ('body_id',VIEWS,'N2 → R1：主体无光对象 ID','comparison_body_id.png')):
        rows.append(panel(prefix,views,title,standard,common,name))
    rows.append(panel('common',('ORTHO',),'N2 → R1：固定共同参考机位',
                      '50° / −2.5° · 正交尺度 0.86 · 固定原注册系数',
                      '完整 900 × 900 原始画幅；参考误差由白发光主体 ID 独立测量。',
                      'comparison_fixed_reference.png',input_dir=OUT/'reference_comparison'))
    rows.append(panel('head_gray',VIEWS,'N2 → R1：下颊—喉部灰模细节',
                      '原灯光 / 同机位 / 同曝光 · 两列使用完全相同的固定像素裁切',
                      '固定裁切：原图 x=40～870，y=340～770；只放大观察，不参与参考重新注册。',
                      'comparison_lowerface_detail.png',crop=(40,340,870,770)))
    if not args.standard_only:
        rows.append(panel('reflection',VIEWS,'N2 → R1：真实环境反射条纹诊断',
                          '中性材质 · Roughness 0.20 · 真实世界环境条纹反射 · Cycles',
                          '诊断测试：观察条纹是否出现局部折断、重复横带；条纹没有贴到主体表面。',
                          'comparison_reflection.png'))
        for variant in ('A','B'):
            rows.append(panel('grazing_'+variant,VIEWS,'N2 → R1：低角掠射灰模 · 灯位 '+variant,
                              '中性粗糙度 1.0 灰模 · 横向窄条面光 · 两版同一测试条件',
                              '诊断测试：改变光源方位检查下颊、侧颈、后肩及后脑—后颈的浅波纹。',
                              'comparison_grazing_'+variant+'.png'))
        rows.append(panel('reflection',VIEWS,'N2 → R1：下颊反射条纹细节',
                          '真实反射诊断 · 两列同一像素区域裁切',
                          '固定裁切：原图 x=40～870，y=340～770；用于检查嘴下凸缘处反射连续性。',
                          'comparison_reflection_detail.png',crop=(40,340,870,770)))
    (CMP/'assembly_protocol.json').write_text(json.dumps({
        'model_order':list(MODELS),'source_sha256':{m:protocol['sources'][m]['sha256'] for m in MODELS},
        'same_render_conditions_verified':True,'standard_only':args.standard_only,'panels':rows},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('N2_ROUND_COMPARISON_ASSEMBLY_OK',len(rows),'panels',flush=True)


if __name__=='__main__':
    main()
