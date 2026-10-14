"""Fixed-registration contour measurements from unlit unified-body ID renders.

Uses Pillow/NumPy outside Blender. Never changes old reference registration or
models. Numeric ranking is separate from the required multi-view visual review.
"""
import argparse
import hashlib
import json
import shutil
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parent
OUT=ROOT/'versions/20261006_head_neck_refinement'
DEST=OUT/'reference_comparison'
OLD=ROOT/'versions/20261006_spherical_eyes/reference_comparison'
SCALE=.41175526202447044
TRANSLATION=np.array([245.38813136509236,-17.159665982101984])
ROWS=(90,180,220,260,300)
REF_BOUNDS={90:[344.,490.],180:[302.,546.],220:[331.,569.],260:[344.,592.],300:[336.,612.]}
FONT='C:/Windows/Fonts/msyh.ttc'


def font(size):return ImageFont.truetype(FONT,size)
def load(path):return json.loads(path.read_text(encoding='utf-8'))
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def row_bounds(mask,reference_y):
    raw_y=int(round((reference_y-TRANSLATION[1])/SCALE))
    xs=np.where(mask[max(0,raw_y-1):min(len(mask),raw_y+2)].any(axis=0))[0]
    assert len(xs),'Mask must contain body at requested row'
    raw=[int(xs.min()),int(xs.max())]
    fixed=(SCALE*np.array(raw)+TRANSLATION[0]).tolist()
    return {'raw_model_row_pixels':raw_y,'raw_model_front_back_x_pixels':raw,
            'registered_front_back_x_pixels':fixed,
            'front_back_error_pixels':(np.array(fixed)-REF_BOUNDS[reference_y]).tolist()}


def registered_image(im,size,crop=(270.,40.),display_scale=1.5,model=True):
    s=SCALE if model else 1.
    tx,ty=TRANSLATION if model else (0.,0.)
    ox,oy=crop
    return im.transform(size,Image.Transform.AFFINE,
                        (1/(display_scale*s),0,(ox-tx)/s,
                         0,1/(display_scale*s),(oy-ty)/s),
                        Image.Resampling.BICUBIC,fillcolor='white')


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--models',nargs='+',choices=['A','N1','N2','C1'])
    ap.add_argument('--selected-neck',choices=['N1','N2'])
    ap.add_argument('--neck-only',action='store_true',help='Record only the authorized first neck stage')
    ap.add_argument('--c1-decision',choices=['adopt','retain_neck','pending'],default='pending')
    args=ap.parse_args()
    DEST.mkdir(parents=True,exist_ok=True)
    protocol=load(OUT/'render_protocol.json')
    old=load(OLD/'reference_review.json')
    models=args.models or [m for m in ('A','N1','N2','C1') if m in protocol['sources']]
    assert 'A' in models
    ref=ROOT/'references/20261005/pdf_page03_image03_IM60.jpg'
    shutil.copy2(ref,DEST/'reference_figure13.jpg')
    measurements={}
    for label in models:
        info=protocol['sources'][label]
        assert digest(Path(info['path']))==info['sha256'],f'Changed source {label}'
        file=DEST/f'{label}_common_body_id.png'
        raw=np.asarray(Image.open(file).convert('RGB'))
        # Emission ID is independent of colors/exposure/lights. The threshold
        # refers solely to raster pixel coverage at an anti-aliased edge.
        mask=raw[:,:,0]>=128
        Image.fromarray(np.uint8(mask)*255).save(DEST/f'{label}_common_body_binary.png')
        rr={str(y):row_bounds(mask,y) for y in ROWS}
        error=np.array([rr[str(y)]['front_back_error_pixels'] for y in (220,260,300)])
        measurements[label]={'sample_rows':rr,'neck_mean_absolute_front_back_error_pixels':np.abs(error).mean(0).tolist(),
            'source_sha256':info['sha256'],'raw_mask_resolution':[raw.shape[1],raw.shape[0]],
            'eye_projection':info.get('common_reference_eye_pixels',{}),
            'ID_sha256':digest(file),'binary_sha256':digest(DEST/f'{label}_common_body_binary.png')}
    baseline=np.array(measurements['A']['neck_mean_absolute_front_back_error_pixels'])
    for label,m in measurements.items():
        vals=np.array(m['neck_mean_absolute_front_back_error_pixels'])
        m['neck_error_reduction_fraction_from_A']=(1-vals/baseline).tolist()
        for y in ROWS:
            rr=m['sample_rows'][str(y)]
            before=np.array(measurements['A']['sample_rows'][str(y)]['front_back_error_pixels'])
            after=np.array(rr['front_back_error_pixels'])
            rr['error_reduction_fraction_from_A']=(1-np.abs(after)/np.maximum(np.abs(before),1e-9)).tolist()
    ranking=sorted((c for c in ('N1','N2') if c in measurements),key=lambda c:sum(measurements[c]['neck_mean_absolute_front_back_error_pixels']))
    old_threshold=[]
    for r in old['residuals']['yellow_outline_rows']:
        y=r['reference_y_pixels']
        old_error=r['model_minus_reference_front_back_pixels']
        new_error=measurements['A']['sample_rows'][str(y)]['front_back_error_pixels']
        old_threshold.append({'reference_y_pixels':y,'old_color_threshold_A_error_pixels':old_error,
                              'new_body_ID_A_error_pixels':new_error,
                              'ID_minus_old_threshold_pixels':(np.array(new_error)-old_error).tolist()})
    c1_delta=None
    if 'C1' in measurements and args.selected_neck:
        parent=measurements[args.selected_neck]
        c1=measurements['C1']
        c1_delta={'neck_parent':args.selected_neck,'upper_rows':{},'reference_eye_projection_delta':{}}
        for y in (90,180):
            a=np.array(parent['sample_rows'][str(y)]['front_back_error_pixels'])
            b=np.array(c1['sample_rows'][str(y)]['front_back_error_pixels'])
            c1_delta['upper_rows'][str(y)]={'parent_error_pixels':a.tolist(),'C1_error_pixels':b.tolist(),
                'absolute_error_reduction_pixels':(np.abs(a)-np.abs(b)).tolist()}
        for side in ('L','R'):
            aa=parent['eye_projection'][side]['aperture'];bb=c1['eye_projection'][side]['aperture']
            c1_delta['reference_eye_projection_delta'][side]={
                'raw_center_pixel_delta':(np.array(bb['center_pixels'])-aa['center_pixels']).tolist(),
                'raw_D_pixel_delta':bb['D_pixels']-aa['D_pixels'],
                'note':'Projected changes can arise from body occlusion; this is not an eye vertex movement.'}

    result={'source_hashes':{m:protocol['sources'][m]['sha256'] for m in models},
        'fixed_registration':{'scale':SCALE,'translation':TRANSLATION.tolist(),
                              'recalculated':False,'uniform_xy':True,'common_pose':protocol['common_reference_camera']},
        'reference_original_pixels':{'path':str(ref),'packaged_copy':'reference_figure13.jpg','sha256':digest(ref),
                                      'resolution':[900,1130],'sample_bounds':REF_BOUNDS,'boundary_uncertainty_pixels':3},
        'sample_rows':measurements,'old_color_threshold_comparison':old_threshold,
        'method':{'model_contour':'Unified evaluated body only, white unlit emission ID, other geometry hidden. Black background. Binarized coverage threshold128; row +/-1 raw pixel.',
                  'reference_contour':'Retained visually checked reference RGB silhouette bounds; no new reference camera or scale fit.',
                  'model_shadow_independent':True,'extra_views_in_numeric_ranking':False},
        'neck_numeric_preference':ranking[0] if ranking else None,'neck_numeric_ranking':ranking,
        'C1_numeric_delta':c1_delta,
        'recommendation':{'selected_neck':args.selected_neck,'c1_decision':args.c1_decision,
                           'scope':'neck_only' if args.neck_only else 'neck_and_crown',
                           'status':('neck decision recorded after multi-view inspection' if args.selected_neck else 'neck visual review pending') if args.neck_only else ('visual review pending' if not args.selected_neck or args.c1_decision=='pending' else 'decision recorded after multi-view inspection'),
                           'numeric_ranking_is_not_visual_acceptance':True},
        'cross_references':{'fig6_2':'PDF page8 thinking pose; hand obscures under-mouth/neck. Use visible short lower face and broad neck transition only.',
                            'fig6_5':'PDF page9 stance pose; crouching and shoulder/arm pose affect chest. Use continuous head-neck silhouette only.',
                            'pose_lens_dimensions_not_transferred':True},
        'artifacts':['reference_stable_comparison.png','body_ID_contour_comparison.png','reference_figure13.jpg']}
    visual_path=DEST/'visual_assessment.json'
    if visual_path.exists():
        result['visual_assessment']=load(visual_path)
    (DEST/'reference_review.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

    # Numeric findings are reproducible; manual visual review is kept separately
    # and incorporated only after actual image inspection.
    chosen=args.selected_neck or '待多视角选择'
    md=['# 下脸—颈部固定参考复核', '',
        (f'第一阶段颈部选择：**{chosen}**。本次范围仅为下脸—颈部，后续头壳未实施。' if args.neck_only else f'颈部选择：**{chosen}**。C1决定：**{args.c1_decision}**。')+'数值排序只用于比较轮廓误差，采用与否需结合灰模、多视角和眼接触验收。', '',
        '## 固定相机、注册与原图', '',
        '图1.3原始嵌入图为900×1130，原图近眼外缘中心(380.5,128)、冠顶(419.2568,68)、嘴近似中心(325,180)。[原始图](reference_figure13.jpg)随材料保留。', '',
        '所有候选共同参考机位为yaw50°、pitch−2.5°、roll0°、正交0.86、target(0,−0.12,2.425)、distance7、900×900、Standard、曝光−0.4。头部0/45/90及全身直接读取既有相机协议；额外45°/−5°、55°/0°与135mm长焦只作视觉交叉检查，不进入固定注册排名。', '',
        '```text',f'reference_x = {SCALE:.17f} × model_x + {TRANSLATION[0]:.14f}',
        f'reference_y = {SCALE:.17f} × model_y {TRANSLATION[1]:+.14f}', '```', '',
        '上述注册沿用球面眼交付，完全固定，没有从新头顶、嘴或眼径重拟合。原始模型像素、工程SHA256、掩码SHA256和注册系数保存在[JSON](reference_review.json)。', '',
        '## 无光照主体轮廓', '',
        '模型量测只显示实际评估黄色统一主体，改为白色自发光、黑色背景，并隐藏眼球、嘴和地面。主体ID以128覆盖阈值转为二值遮罩；该阈值只确定抗锯齿边界，不依赖灯光、材质颜色或阴影。每条参考行取对应原模型像素行±1。参考原图继续使用已核对的前/后界，约±3px误差。', '',
        '![固定注册彩色对照](reference_stable_comparison.png)', '',
        '![无光照主体ID轮廓](body_ID_contour_comparison.png)', '',
        '| 参考行y | 参考前/后界x | '+' | '.join(models)+' |',
        '| --- | --- | '+' | '.join(['---']*len(models))+' |']
    for y in ROWS:
        vals=[' / '.join(f'{v:+.2f}' for v in measurements[m]['sample_rows'][str(y)]['front_back_error_pixels'])+'px' for m in models]
        md.append(f'| {y} | {REF_BOUNDS[y][0]:.0f} / {REF_BOUNDS[y][1]:.0f} | '+' | '.join(vals)+' |')
    md+=['', '表内候选为“模型−参考”的误差；正X向右，负前界误差表示前颈仍过于向画面左侧伸出。以下改善比例以新的纯主体ID基准A计算：', '',
          '| 候选 | y220/260/300平均前界绝对误差 | 平均后界绝对误差 | 前/后改善 |', '| --- | ---: | ---: | --- |']
    for label,m in measurements.items():
        f,b=m['neck_mean_absolute_front_back_error_pixels'];rf,rb=m['neck_error_reduction_fraction_from_A']
        md.append(f'| {label} | {f:.2f}px | {b:.2f}px | {100*rf:.1f}% / {100*rb:.1f}% |')
    md+=['',f'仅按两侧平均轮廓误差的排序：{", ".join(ranking) if ranking else "尚无颈部候选"}。指令中N1约25%–45%、N2约40%–60%的前界改善是验收目标，不能以位移参数推定已经达到。', '',
         '## 新ID基准与旧彩色阈值的区别', '',
         '旧报告用黄色RGB阈值识别彩色图，新报告用不受阴影影响的实际主体ID。差异不能宣称由几何改变引起；固定A工程不变。', '',
         '| y | 旧阈值A前/后误差 | 新ID A前/后误差 | ID−旧阈值 |', '| --- | --- | --- | --- |']
    for r in old_threshold:
        fmt=lambda vs:' / '.join(f'{v:+.2f}' for v in vs)
        md.append(f'| {r["reference_y_pixels"]} | {fmt(r["old_color_threshold_A_error_pixels"])} | {fmt(r["new_body_ID_A_error_pixels"])} | {fmt(r["ID_minus_old_threshold_pixels"])}px |')
    md+=['', '此批固定A彩色图与上一轮头部0/45/90、全身及共同参考原图逐像素一致；新ID和旧阈值偏差约0–0.412参考像素。', '',
         ('## 后续阶段' if args.neck_only else '## C1与投影遮挡'), '']
    if c1_delta:
        for y,r in c1_delta['upper_rows'].items():
            a,b=r['parent_error_pixels'],r['C1_error_pixels']
            md.append(f'- y={y}：选中颈版前/后误差({a[0]:+.2f},{a[1]:+.2f})px，C1为({b[0]:+.2f},{b[1]:+.2f})px。')
        for side,r in c1_delta['reference_eye_projection_delta'].items():
            dx,dy=r['raw_center_pixel_delta']
            md.append(f'- {side}眼参考相机原900px投影：外缘中心改变({dx:+.4f},{dy:+.4f})px、等面积外径改变{r["raw_D_pixel_delta"]:+.4f}px。该量变化可能来自黄色主体遮挡，不能当作眼球网格位移。')
    else:
        md.append('本次仅实施第一阶段，下脸—颈部以外的主体保持固定 A。' if args.neck_only else 'C1尚未同时具备模型渲染与已选颈版记录；不预置采用结论。')
    md+=['', '## 多视角视觉核对', '']
    if 'visual_assessment' in result:
        v=result['visual_assessment']
        md.extend(v.get('paragraphs',[]))
    else:
        md.append('待实际查看颈部候选的彩色、灰模、90°中线、45°轮廓及额外镜头后补充；数值分析不代替鼓包、喉坑、横向颈环、接口台阶与眼接触检查。')
    md+=['', '图6.2位于PDF第8页底部，为思考姿态，手托下巴并遮挡部分口颈；图6.5位于第9页底部，为蹲姿架势，肩臂动作改变胸部展开。两图只交叉确认短下脸与宽缓颈前转折，未套用其姿态、镜头和局部尺寸。', '',
         '本分析由 `analyze_head_neck_reference.py` 重复生成；它只读取当前模型哈希、已渲染图像和固定注册，写分析材料，不保存或修改Blender工程。', '']
    (DEST/'reference_review.md').write_text('\n'.join(md),encoding='utf-8')

    # Include reference + models, preserving the same affine mapping everywhere.
    n=len(models)+1
    panel=(620,470)
    canvas=Image.new('RGB',(n*640,845),'white')
    dr=ImageDraw.Draw(canvas)
    dr.text((20,18),'下脸—颈部精修：固定相机与原注册，无重新对齐',font=font(34),fill='#203448')
    dr.text((20,68),f'共用yaw50° / pitch−2.5° / 正交0.86；s={SCALE:.12f}，平移(245.3881,−17.1597)。',font=font(22),fill='#536577')
    for i,label in enumerate(['reference']+models):
        x=20+i*640
        dr.text((x,115),'图1.3原图' if label=='reference' else label,font=font(26),fill='#203448')
        im=Image.open(ref if label=='reference' else DEST/f'{label}_common_ORTHO.png').convert('RGB')
        pp=registered_image(im,panel,model=label!='reference')
        pd=ImageDraw.Draw(pp)
        for y in ROWS:
            yy=(y-40)*1.5
            pd.line((0,yy,panel[0],yy),fill='#8393a3',width=1)
            pd.text((4,yy+1),str(y),font=font(16),fill='#da5174')
        pd.rectangle((0,0,panel[0]-1,panel[1]-1),outline='#8393a3')
        canvas.paste(pp,(x,155))
        if label=='reference':
            lines=['900×1130原图；五条行保留原像素', '模型误差依据无光照主体ID遮罩', '额外45°/−5°、55°/0°、135mm不参与排名']
        else:
            m=measurements[label]
            lines=[]
            for y in (220,260,300):
                f,b=m['sample_rows'][str(y)]['front_back_error_pixels']
                lines.append(f'y{y} 前/后误差 {f:+.2f}/{b:+.2f}px')
            red=m['neck_error_reduction_fraction_from_A']
            lines.append(f'颈前/后平均误差减少 {100*red[0]:.1f}%/{100*red[1]:.1f}%')
        for z,line in enumerate(lines):dr.text((x,640+z*34),line,font=font(21),fill='#203448')
    dr.text((20,803),'模型主体ID排除眼球、嘴与地面；表内为原图尺度屏幕误差，不能解释成世界坐标位移。',font=font(23),fill='#536577')
    canvas.save(DEST/'reference_stable_comparison.png')

    ids=Image.new('RGB',(n*640,735),'white')
    dd=ImageDraw.Draw(ids)
    dd.text((20,18),'无光照主体二值轮廓：固定注册五行量测',font=font(32),fill='#203448')
    for i,label in enumerate(['reference']+models):
        x=20+i*640
        dd.text((x,80),'图1.3参考' if label=='reference' else label+' 纯主体ID',font=font(25),fill='#203448')
        im=Image.open(ref if label=='reference' else DEST/f'{label}_common_body_binary.png').convert('RGB')
        pp=registered_image(im,panel,model=label!='reference')
        pd=ImageDraw.Draw(pp)
        for y in ROWS:
            yy=(y-40)*1.5
            bounds=REF_BOUNDS[y] if label=='reference' else measurements[label]['sample_rows'][str(y)]['registered_front_back_x_pixels']
            pd.line((0,yy,panel[0],yy),fill='#339bb0',width=1)
            for edge in bounds:
                xx=(edge-270)*1.5
                pd.line((xx,yy-8,xx,yy+8),fill='#e55976',width=3)
        ids.paste(pp,(x,125))
    dd.text((20,628),'白色仅代表黄色主体几何；分割阈值只用于抗锯齿像素覆盖，不使用黄色RGB或阴影。',font=font(22),fill='#536577')
    dd.text((20,668),'模型原始像素、二值掩码与旧彩色阈值的差异都保存在reference_review.json。',font=font(22),fill='#536577')
    ids.save(DEST/'body_ID_contour_comparison.png')
    print('HEAD_NECK_REFERENCE_ANALYSIS_OK',models,ranking,flush=True)
    for label,m in measurements.items():
        print(label,'neck reduction',m['neck_error_reduction_fraction_from_A'],flush=True)


if __name__=='__main__':main()
