"""Fixed registration comparison of unlit R1/H2/E2 subject silhouettes.

The original reference rows and affine registration are immutable. Runs in
bundled Python after render_h2_e2_comparisons.py; never loads/saves a .blend.
"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw,ImageFont

ROOT=Path(__file__).resolve().parent
OUT=ROOT/'versions/20261007_H2_E2_overall_refinement'
REF=ROOT/'references/20261005/pdf_page03_image03_IM60.jpg'
SCALE=.41175526202447044
TRANSLATION=np.array([245.38813136509236,-17.159665982101984])
ROWS=(90,180,220,260,300)
REF_BOUNDS={90:[344.,490.],180:[302.,546.],220:[331.,569.],260:[344.,592.],300:[336.,612.]}
EXPECTED_CAMERA={'type':'ORTHO','yaw':50.,'pitch':-2.5,'target':[0,-.12,2.425],
    'distance':7.,'scale':.86,'lens':50.,'resolution':[900,900]}


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def font(n):return ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',n)
def row_bounds(mask,y):
    raw_y=int(round((y-TRANSLATION[1])/SCALE))
    xs=np.flatnonzero(mask[max(0,raw_y-1):min(len(mask),raw_y+2)].any(axis=0))
    assert len(xs), f'Empty mask at reference row {y}'
    raw=np.array([xs.min(),xs.max()],int);reg=SCALE*raw+TRANSLATION[0]
    return {'raw_model_row_pixels':raw_y,'raw_model_front_back_x_pixels':raw.tolist(),
        'registered_front_back_x_pixels':reg.tolist(),'front_back_error_pixels':(reg-REF_BOUNDS[y]).tolist()}


def registered(im,model):
    scale=SCALE if model else 1.;tx,ty=TRANSLATION if model else (0.,0.)
    return im.transform((620,470),Image.Transform.AFFINE,(1/(1.5*scale),0,(270-tx)/scale,
        0,1/(1.5*scale),(40-ty)/scale),Image.Resampling.BICUBIC,fillcolor='white')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=OUT)
    parser.add_argument('--render-root',type=Path,help='Read candidate render protocol/images from another version root; keep audit output separate')
    parser.add_argument('--models',nargs='+',default=['R1','H2','E2'])
    args=parser.parse_args();out=args.output.resolve();dest=out/'reference_comparison'
    dest.mkdir(parents=True,exist_ok=True)
    render_root=(args.render_root or out).resolve()
    protocol_path=render_root/'render_protocol.json'
    protocol=json.loads(protocol_path.read_text(encoding='utf-8-sig'))
    assert protocol['common_reference_camera']==EXPECTED_CAMERA, 'Reference camera changed'
    assert protocol['fixed_reference_registration']=={'scale':SCALE,'translation':TRANSLATION.tolist()}, 'Registration changed'
    result={'fixed_registration':{'scale':SCALE,'translation':TRANSLATION.tolist(),'recalculated':False,
        'common_pose':EXPECTED_CAMERA},'reference':{'path':str(REF),'sha256':sha(REF),
        'resolution':list(Image.open(REF).size),'sample_bounds':REF_BOUNDS,'boundary_uncertainty_pixels':3},
        'models':{},'numeric_pass_is_not_visual_acceptance':True,
        'render_protocol':{'path':str(protocol_path),'sha256':sha(protocol_path)},
        'method':'Actual evaluated unified body ID, white unlit emission on black; coverage threshold128; fixed raw row +/-1. Original manually checked reference RGB bounds retained. No camera/image scale refit.'}
    image_roots={}
    for model in args.models:
        if model=='R1' and model not in protocol['sources']:
            baseline_protocol=json.loads((OUT/'render_protocol.json').read_text(encoding='utf-8-sig'))
            assert baseline_protocol['common_reference_camera']==EXPECTED_CAMERA
            assert baseline_protocol['fixed_reference_registration']==protocol['fixed_reference_registration']
            info=baseline_protocol['sources'][model];image_root=OUT/'reference_comparison'
        else:
            info=protocol['sources'][model];image_root=render_root/'reference_comparison'
        image_roots[model]=image_root
        assert sha(info['path'])==info['sha256'],f'Stale render: {model}'
        idpath=image_root/f'{model}_common_body_id.png';pixels=np.asarray(Image.open(idpath).convert('RGB'))
        assert pixels.shape[:2]==(900,900)
        mask=pixels[:,:,0]>=128;binary=dest/f'overall_{model}_body_binary.png';Image.fromarray(np.uint8(mask)*255).save(binary)
        rows={str(y):row_bounds(mask,y) for y in ROWS}
        neck=np.abs([rows[str(y)]['front_back_error_pixels'] for y in (220,260,300)]).mean(axis=0)
        result['models'][model]={'source_path':info['path'],'source_sha256':info['sha256'],
            'ID_sha256':sha(idpath),'binary_sha256':sha(binary),'sample_rows':rows,
            'neck_mean_absolute_front_back_error_pixels':neck.tolist()}
    base=result['models']['R1']
    for model,record in result['models'].items():
        deterioration={str(y):(np.abs(record['sample_rows'][str(y)]['front_back_error_pixels'])-
            np.abs(base['sample_rows'][str(y)]['front_back_error_pixels'])).tolist() for y in ROWS}
        record['absolute_error_deterioration_from_R1_pixels']=deterioration
        record['diagnostic_targets']={'neck_front_mean_at_most_16_pixels':record['neck_mean_absolute_front_back_error_pixels'][0]<=16.,
            'rear_all_sample_rows_deterioration_at_most_3_pixels':all(r[1]<=3. for r in deterioration.values()),
            'rear_neck_rows_deterioration_at_most_3_pixels':all(deterioration[str(y)][1]<=3. for y in (220,260,300)),
            'automatic_visual_acceptance':False}
    old=json.loads((ROOT/'versions/20261007_N2_round_refinement/reference_comparison/reference_review.json').read_text(encoding='utf-8'))
    result['R1_baseline_consistency']={'all_five_rows_exact':all(base['sample_rows'][str(y)]==old['sample_rows']['R1']['sample_rows'][str(y)] for y in ROWS)}
    assert result['R1_baseline_consistency']['all_five_rows_exact'], 'R1 baseline rows differ'
    (dest/'overall_reference_review.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    for kind in ('color','body_ID'):
        im=Image.new('RGB',(640*(len(args.models)+1),860),'white');d=ImageDraw.Draw(im)
        d.text((20,15),'R1 / H2 / E2 固定参考：沿用相机与原注册',font=font(32),fill='#24364a')
        d.text((20,64),'yaw50°、pitch−2.5°、正交0.86；五行原图边界和注册固定，无候选重新对齐。',font=font(22),fill='#536577')
        for i,model in enumerate(['reference']+args.models):
            left=20+i*640;d.text((left,112),'图1.3原始图' if model=='reference' else model,font=font(26),fill='#24364a')
            path=REF if model=='reference' else (image_roots[model]/f'{model}_common_ORTHO.png' if kind=='color' else dest/f'overall_{model}_body_binary.png')
            pp=registered(Image.open(path).convert('RGB'),model!='reference');pd=ImageDraw.Draw(pp)
            for y in ROWS:
                yy=(y-40)*1.5;pd.line((0,yy,620,yy),fill='#7496a2');pd.text((3,yy+2),str(y),font=font(15),fill='#d25478')
                bounds=REF_BOUNDS[y] if model=='reference' else result['models'][model]['sample_rows'][str(y)]['registered_front_back_x_pixels']
                for edge in bounds:
                    xx=(edge-270)*1.5;pd.line((xx,yy-8,xx,yy+8),fill='#d25478',width=3)
            im.paste(pp,(left,155))
            if model!='reference':
                row=result['models'][model]
                a,b=row['neck_mean_absolute_front_back_error_pixels']
                d.text((left,640),f'颈前/后平均绝对误差 {a:.2f} / {b:.2f}px',font=font(22),fill='#24364a')
                for j,y in enumerate((220,260,300)):
                    a,b=row['sample_rows'][str(y)]['front_back_error_pixels'];d.text((left,677+34*j),f'y{y} 前/后误差 {a:+.2f}/{b:+.2f}px',font=font(21),fill='#536577')
        d.text((20,817),'数值目标是诊断；整体阶梯、灰模、反射条纹和多视图验收另行记录。',font=font(22),fill='#536577')
        im.save(dest/f'overall_fixed_reference_{kind}.png')
    lines=['# H2 / E2 固定参考诊断','','相机、原注册、参考五行边界完全沿用。R1新量测与旧记录逐行精确一致。模型纯主体ID排除眼球、嘴和地面，避免阴影干扰。','',
        '| 原图行 | 参考前/后界 | '+' | '.join(args.models)+' |','| --- | --- | '+' | '.join('---' for m in args.models)+' |']
    for y in ROWS:
        vals=[' / '.join(f'{v:+.2f}' for v in result['models'][m]['sample_rows'][str(y)]['front_back_error_pixels'])+'px' for m in args.models]
        lines.append(f'| y{y} | {REF_BOUNDS[y][0]:g} / {REF_BOUNDS[y][1]:g} | '+' | '.join(vals)+' |')
    lines += ['','误差为模型界−参考界。前颈 y220/260/300 平均绝对误差约≤16px、后界误差恶化不超过3px为候选诊断目标，不凌驾于多视图造型。','',
        '![固定参考](overall_fixed_reference_color.png)','','![主体ID固定参考](overall_fixed_reference_body_ID.png)','']
    (dest/'overall_reference_review.md').write_text('\n'.join(lines),encoding='utf-8')
    print('OVERALL_REFERENCE_OK', {m:r['neck_mean_absolute_front_back_error_pixels'] for m,r in result['models'].items()},flush=True)


if __name__=='__main__':main()
