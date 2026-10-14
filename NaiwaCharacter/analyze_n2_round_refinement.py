"""Actual N2/R1 surface sections, curve diagnostics, and fixed image registration.

Runs in the bundled Python using NumPy and Pillow. It never loads/saves Blender.
No displacement-C2 claim is used as a shape acceptance criterion.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from n2_round_diagnostic_utils import (sample_section_branches, curve_diagnostics,
    compare_curves, triangle_plane_segments, band_wave_diagnostic, section_segments,
    sample_axis_branches)

ROOT=Path(__file__).absolute().parent
OUT=ROOT/'versions/20261007_N2_round_refinement'
OLD=ROOT/'versions/20261006_head_neck_refinement'
PINNED_N2='9397186f3ae13e93e05cc0d492c12ceb68b743ab4d2a9a0d650426a47a66b0cb'
SCALE=.41175526202447044
TRANSLATION=np.array([245.38813136509236,-17.159665982101984])
REF_BOUNDS={90:[344.,490.],180:[302.,546.],220:[331.,569.],260:[344.,592.],300:[336.,612.]}
COLORS={'N2':'#be7b29','R1':'#237fb0'}
FONT='C:/Windows/Fonts/msyh.ttc'


def read(path):return json.loads(path.read_text(encoding='utf-8'))
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def font(size):return ImageFont.truetype(FONT,size)
def dump(path,data):path.write_text(json.dumps(data,ensure_ascii=False,indent=2,allow_nan=False)+'\n',encoding='utf-8')


class Panel:
    """Physical section panel with equal world-unit axis scaling."""
    def __init__(self,canvas,rect,bounds):
        self.canvas=canvas;self.rect=rect
        x0,y0,x1,y1=rect;u0,v0,u1,v1=bounds
        self.origin=(int(x0),int(y0))
        self.layer=Image.new('RGBA',(int(x1-x0),int(y1-y0)),(0,0,0,0))
        self.draw=ImageDraw.Draw(self.layer)
        self.scale=min((x1-x0)/(u1-u0),(y1-y0)/(v1-v0))
        self.uc,self.vc=(u0+u1)/2,(v0+v1)/2
        self.xc,self.yc=(x0+x1)/2,(y0+y1)/2
    def p(self,u,v):return self.xc+(u-self.uc)*self.scale,self.yc-(v-self.vc)*self.scale
    def lp(self,u,v):
        x,y=self.p(u,v);return x-self.origin[0],y-self.origin[1]
    def line(self,points,color,width=3):self.draw.line([self.lp(*p) for p in points],fill=color,width=width)
    def grid(self,step,bounds):
        u0,v0,u1,v1=bounds
        for u in np.arange(np.ceil(u0/step)*step,u1,step):self.line(((u,v0),(u,v1)),'#e0e6ed',1)
        for v in np.arange(np.ceil(v0/step)*step,v1,step):self.line(((u0,v),(u1,v)),'#e0e6ed',1)
        self.draw.rectangle((0,0,self.layer.width-1,self.layer.height-1),outline='#a4b3c2',width=2)
    def commit(self):self.canvas.paste(self.layer,self.origin,self.layer)


def image(w,h,title,subtitle):
    im=Image.new('RGB',(w,h),'#f3f5f8');d=ImageDraw.Draw(im)
    d.rectangle((0,0,w,118),fill='#17263b')
    d.text((30,18),title,font=font(34),fill='white')
    d.text((30,71),subtitle,font=font(21),fill='#cfdae8')
    return im,d


def legend(draw,x,y):
    for name in ('N2','R1'):
        draw.line((x,y+14,x+42,y+14),fill=COLORS[name],width=5)
        draw.text((x+53,y),name,font=font(22),fill='#253548');x+=140


def equal_trace(panel,section,axes,color,width=3):
    for segment in section_segments(section):panel.line(segment[:,axes],color,width)


def plot_curves(path,curves,edge='front'):
    xs=sorted(curves,key=float)
    label='下颊' if edge=='front' else '后脑—后颈'
    im,d=image(1440,200+len(xs)*500,label+'最终曲面：局部三次拟合曲率',
        '实际三角面剖面；Z步0.0005，严格半窗|Z−Z0|<0.010（39点）；N2与R1使用同一范围与尺度。')
    legend(d,1060,129)
    for row,x in enumerate(xs):
        top=180+row*500
        d.text((35,top-31),f'X = {float(x):+.3f}  /  '+('前界 −Y' if edge=='front' else '后界 +Y'),font=font(24),fill='#253548')
        rect=(110,top+20,1360,top+350);xmin,xmax=2.28,2.44;ymin,ymax=0.,(70. if edge=='front' else 20.)
        def point(z,k):return (rect[0]+(z-xmin)/(xmax-xmin)*(rect[2]-rect[0]),rect[3]-(k-ymin)/(ymax-ymin)*(rect[3]-rect[1]))
        for k in range(0,int(ymax)+1,10 if edge=='front' else 5):
            a,b=point(xmin,k),point(xmax,k);d.line((*a,*b),fill='#d6dee8',width=1)
            d.text((38,a[1]-13),str(k),font=font(20),fill='#617086')
        for z in np.arange(xmin,xmax+.0001,.02):
            a,b=point(z,ymin),point(z,ymax);d.line((*a,*b),fill='#d6dee8',width=1)
            d.text((a[0]-28,rect[3]+12),f'{z:.2f}',font=font(18),fill='#617086')
        d.rectangle(rect,outline='#687a8e',width=2)
        for model in ('N2','R1'):
            cc=curves[x][model][edge];take=(cc['Z']>=xmin)&(cc['Z']<=xmax)&np.isfinite(cc['curvature'])
            points=[point(z,min(k,ymax)) for z,k in zip(cc['Z'][take],cc['curvature'][take])]
            if points:d.line(points,fill=COLORS[model],width=3)
        cmp=curves[x]['comparison'][edge]
        a,b=cmp['source_peak'],cmp['target_peak']
        d.text((112,top+410),f'N2峰 {a["curvature"]:.2f} @Z{a["Z"]:.4f}    R1峰 {b["curvature"]:.2f} @Z{b["Z"]:.4f}    降低 {100*cmp["peak_reduction_fraction"]:.1f}%',font=font(22),fill='#253548')
    im.save(path)


def plot_longitudinal(path,sections):
    xs=sorted(sections,key=float);im,d=image(1440,220+len(xs)*690,'最终网格纵剖面：下颊至后脑—后颈',
        '橙 N2 / 蓝 R1；同工程坐标、Y/Z等比例；局部改动小，需联合曲率与反射条纹判断。')
    legend(d,1060,134)
    bounds=[-.49,2.02,.34,2.66]
    for row,x in enumerate(xs):
        top=205+row*690
        d.text((37,top-25),f'X = {float(x):+.3f}',font=font(27),fill='#253548')
        panel=Panel(im,(100,top+28,1335,top+620),bounds);panel.grid(.05,bounds)
        for model in ('N2','R1'):equal_trace(panel,sections[x][model],(1,2),COLORS[model],3)
        for z in (2.12,2.22,2.31,2.375,2.40,2.43):
            panel.line([(bounds[0],z),(bounds[2],z)],'#c6d1dc',1)
            px,py=panel.p(bounds[2],z);d.text((px+8,py-12),f'{z:.3f}',font=font(18),fill='#617086')
        panel.commit()
        d.text((150,top+638),'← 前方 −Y                     后方 +Y →',font=font(21),fill='#617086')
    im.save(path)


def plot_curvature_grid(path,curves,title,parameter_axis='Z',bounds=(2.28,2.44)):
    """Compact two-column actual-curvature comparison with one shared scale."""
    keys=sorted(curves,key=float);rows=(len(keys)+1)//2
    im,d=image(1760,190+rows*440,title,
        '橙 N2 / 蓝 R1；实际三角相交线；固定步0.0005、严格三次半窗0.010；各条与自身N2基线比较。')
    maxima=[]
    for rec in curves.values():
        for model in ('N2','R1'):
            c=rec[model]['front'];take=(c['Z']>=bounds[0])&(c['Z']<=bounds[1])&np.isfinite(c['curvature'])
            maxima.append(float(c['curvature'][take].max()))
    ymax=max(20.,np.ceil(max(maxima)/10.)*10.);ytick=10. if ymax<=80 else 20.
    for i,key in enumerate(keys):
        col,row=i%2,i//2;left=36+col*875;top=188+row*440
        d.text((left+14,top-37),f'{"X" if parameter_axis=="Z" else "Z"} = {float(key):+.3f}',font=font(25),fill='#253548')
        rect=(left+70,top+8,left+814,top+314)
        def point(z,k):return (rect[0]+(z-bounds[0])/(bounds[1]-bounds[0])*(rect[2]-rect[0]),rect[3]-k/ymax*(rect[3]-rect[1]))
        for k in np.arange(0,ymax+.0001,ytick):
            a,b=point(bounds[0],k),point(bounds[1],k);d.line((*a,*b),fill='#d6dee8',width=1)
            d.text((left+16,a[1]-12),f'{k:.0f}',font=font(18),fill='#617086')
        for z in np.linspace(bounds[0],bounds[1],9 if parameter_axis=='Z' else 7):
            a,b=point(z,0),point(z,ymax);d.line((*a,*b),fill='#d6dee8',width=1)
            d.text((a[0]-26,rect[3]+12),f'{z:.2f}',font=font(18),fill='#617086')
        d.rectangle(rect,outline='#687a8e',width=2)
        for model in ('N2','R1'):
            c=curves[key][model]['front'];take=(c['Z']>=bounds[0])&(c['Z']<=bounds[1])&np.isfinite(c['curvature'])
            pp=[point(z,k) for z,k in zip(c['Z'][take],c['curvature'][take])]
            d.line(pp,fill=COLORS[model],width=3)
        cmp=curves[key]['comparison']['front'];a,b=cmp['source_peak'],cmp['target_peak']
        d.text((left+72,top+368),f'N2 {a["curvature"]:.2f}  →  R1 {b["curvature"]:.2f}    降低 {cmp["peak_reduction_fraction"]*100:.1f}%',font=font(22),fill='#253548')
    im.save(path)


def plot_horizontal(path,raw,models):
    heights=sorted(set(r['Z'] for r in raw['N2']['horizontal']))
    im,d=image(1440,230+len(heights)*660,'实际水平截面与N2容差',
        '黄色统一主体的实际三角相交线；各层X/Y等比例且共用同一尺度，未拟合圆/椭圆。')
    legend(d,1060,133);bounds=[-.44,-.50,.44,.35]
    for row,z in enumerate(heights):
        top=190+row*660
        d.text((35,top),f'Z = {z:.3f}',font=font(26),fill='#253548')
        records={m:next(r for r in raw[m]['horizontal'] if abs(r['Z']-z)<1e-8) for m in models}
        panel=Panel(im,(150,top+52,1290,top+590),bounds);panel.grid(.05,bounds)
        for model,r in records.items():equal_trace(panel,r['body'],(0,1),COLORS[model],3)
        panel.line([(0,bounds[1]),(0,bounds[3])],'#c6d1dc',1);panel.commit()
        vals=['{} f={:+.6f}, r={:+.6f}, X宽={:.6f}'.format(m,r['front_Y'],r['rear_Y'],r['width_X']) for m,r in records.items()]
        d.text((55,top+606),'   |   '.join(vals),font=font(21),fill='#253548')
    im.save(path)


def plot_sides(path,sections):
    ys=sorted(sections,key=float);im,d=image(1440,220+len(ys)*570,'侧颈与后肩：实际Y平面纵剖面',
        '橙 N2 / 蓝 R1，X/Z等比例；波纹频段诊断仅辅助，外观以多方位掠射光与反射条纹复核。')
    legend(d,1060,130);bounds=[-.47,2.00,.47,2.50]
    for row,y in enumerate(ys):
        top=205+row*570;d.text((35,top-22),f'Y = {float(y):+.3f}',font=font(26),fill='#253548')
        panel=Panel(im,(80,top+20,1350,top+505),bounds);panel.grid(.05,bounds)
        for model in ('N2','R1'):equal_trace(panel,sections[y][model],(0,2),COLORS[model],3)
        panel.commit()
    im.save(path)


def plot_heatmap(path,field):
    with np.load(field,allow_pickle=False) as data:
        points=np.asarray(data['points_world'],float);delta=np.asarray(data['delta'],float)
        normals=np.asarray(data['source_normals'],float)
        if 'guard' in data:
            guard=np.asarray(data['guard'],float)
        else:
            guards=[np.asarray(data[k],float) for k in ('mouth_guard','shoulder_guard','eye_guard') if k in data]
            guard=np.prod(guards,axis=0) if guards else np.ones(len(points))
    total=np.linalg.norm(delta,axis=1);normal=np.sum(delta*normals,axis=1)
    tangent=np.sqrt(np.maximum(total*total-normal*normal,0))
    im,d=image(1880,1730,'R1实际写回位移热图与固定区域',
        '同源N2顶点；Z位移应精确为0。法向为dot(ΔP,n_N2)，切向改动同时记录，颜色并非曲率。')
    modes=[('全位移 |ΔP|',total,(.0,.015)),('源法向投影 Δn',normal,(-.006,.006)),('切向位移',tangent,(.0,.015)),('保护权重 guard',guard,(.0,1.))]
    bounds=[-.49,2.00,.35,2.53]
    chosen=np.flatnonzero((points[:,2]>=2.0)&(points[:,2]<=2.53));chosen=chosen[np.argsort(points[chosen,0])]
    for i,(name,values,limits) in enumerate(modes):
        col,row=i%2,i//2;x=35+col*940;y=200+row*720
        d.text((x+15,y-35),name,font=font(27),fill='#253548')
        panel=Panel(im,(x+25,y+5,x+860,y+590),bounds);panel.grid(.05,bounds)
        for idx in chosen:
            t=float(np.clip((values[idx]-limits[0])/(limits[1]-limits[0]),0,1))
            if limits[0]<0:
                start,finish=((37,111,178),(229,238,247)) if t<.5 else ((229,238,247),(202,73,35))
                tt=2*t if t<.5 else 2*t-1
            else:start,finish,tt=(229,238,247),(195,71,24),t
            color=tuple(round(a+(b-a)*tt) for a,b in zip(start,finish))
            px,py=panel.lp(points[idx,1],points[idx,2]);panel.draw.ellipse((px-1.2,py-1.2,px+1.2,py+1.2),fill=color)
        panel.commit();d.text((x+18,y+608),f'固定颜色尺 {limits[0]:+.3f} ～ {limits[1]:+.3f}',font=font(23),fill='#617086')
    im.save(path)
    return {'maximum_total_displacement':float(total.max()),'maximum_absolute_source_normal_projection':float(np.abs(normal).max()),
        'maximum_tangential_displacement':float(tangent.max()),'maximum_absolute_delta_Z':float(np.abs(delta[:,2]).max()),
        'frozen_source_normals_used':True,'field_file_sha256':digest(field)}


def row_bounds(mask,y):
    raw_y=int(round((y-TRANSLATION[1])/SCALE))
    xs=np.flatnonzero(mask[max(0,raw_y-1):min(len(mask),raw_y+2)].any(0))
    assert len(xs),f'No body at fixed y={y}'
    raw=[int(xs.min()),int(xs.max())];fixed=SCALE*np.asarray(raw)+TRANSLATION[0]
    return {'raw_model_row_pixels':raw_y,'raw_model_front_back_x_pixels':raw,
        'registered_front_back_x_pixels':fixed.tolist(),'front_back_error_pixels':(fixed-REF_BOUNDS[y]).tolist()}


def reference_analysis(out):
    dest=out/'reference_comparison';protocol=read(out/'render_protocol.json')
    prior=read(OLD/'reference_comparison/reference_review.json')
    expected=prior['fixed_registration']['common_pose'];actual=protocol['common_reference_camera']
    assert actual==expected,('Changed reference camera',actual,expected)
    measurements={}
    for model in ('N2','R1'):
        info=protocol['sources'][model];assert digest(Path(info['path']))==info['sha256']
        path=dest/f'{model}_common_body_id.png';raw=np.asarray(Image.open(path).convert('RGB'))
        mask=raw[:,:,0]>=128;Image.fromarray(np.uint8(mask)*255).save(dest/f'{model}_common_body_binary.png')
        rows={str(y):row_bounds(mask,y) for y in REF_BOUNDS}
        error=np.array([rows[str(y)]['front_back_error_pixels'] for y in (220,260,300)])
        mean=np.mean(np.abs(error),axis=0)
        baseline=np.array(prior['sample_rows']['A']['neck_mean_absolute_front_back_error_pixels'])
        measurements[model]={'sample_rows':rows,'neck_mean_absolute_front_back_error_pixels':mean.tolist(),
            'neck_error_reduction_fraction_from_A':(1-mean/baseline).tolist(),'source_sha256':info['sha256'],
            'ID_sha256':digest(path),'raw_mask_resolution':[raw.shape[1],raw.shape[0]]}
    deterioration={}
    for y in (220,260,300):
        a=np.abs(measurements['N2']['sample_rows'][str(y)]['front_back_error_pixels'])
        b=np.abs(measurements['R1']['sample_rows'][str(y)]['front_back_error_pixels'])
        deterioration[str(y)]=(b-a).tolist()
    passed=all(max(row)<=3.000001 for row in deterioration.values()) and measurements['R1']['neck_error_reduction_fraction_from_A'][0]>=.40
    result={'fixed_registration':{'scale':SCALE,'translation':TRANSLATION.tolist(),'recalculated':False,'common_pose':actual},
        'reference_bounds_original_pixels':REF_BOUNDS,'sample_rows':measurements,'absolute_error_deterioration_R1_from_N2_pixels':deterioration,
        'constraints':{'per_row_edge_deterioration_max_pixels':3.,'front_mean_error_reduction_from_A_min_fraction':.40,'passed':passed},
        'numeric_pass_is_not_visual_acceptance':True,'method':'Unlit unified body ID; threshold128; fixed raw image row +/-1; old registration and reference bounds unchanged.'}
    dump(dest/'reference_review.json',result)
    im,d=image(1920,990,'N2 / R1：固定参考相机与注册',
        '原图尺度；yaw50° / pitch−2.5° / 正交0.86；s=0.41175526202447044，未重新拟合或对齐。')
    original=ROOT/'references/20261005/pdf_page03_image03_IM60.jpg'
    for i,label in enumerate(('reference','N2','R1')):
        x=20+640*i;d.text((x+10,145),'图1.3原图' if label=='reference' else label,font=font(28),fill='#253548')
        source=Image.open(original if label=='reference' else dest/f'{label}_common_ORTHO.png').convert('RGB')
        s=SCALE if label!='reference' else 1.;tx,ty=TRANSLATION if label!='reference' else (0.,0.)
        panel=source.transform((600,505),Image.Transform.AFFINE,(1/(1.5*s),0,(270-tx)/s,0,1/(1.5*s),(40-ty)/s),Image.Resampling.BICUBIC,fillcolor='white')
        pd=ImageDraw.Draw(panel)
        for yy in (220,260,300):
            py=(yy-40)*1.5;pd.line((0,py,600,py),fill='#92a3b4',width=1);pd.text((5,py+2),str(yy),font=font(18),fill='#bc5271')
        im.paste(panel,(x+5,198))
        if label!='reference':
            for j,yy in enumerate((220,260,300)):
                f,b=measurements[label]['sample_rows'][str(yy)]['front_back_error_pixels']
                d.text((x+13,745+j*39),f'y{yy} 前 / 后误差 {f:+.2f} / {b:+.2f}px',font=font(24),fill='#253548')
            rf,rb=measurements[label]['neck_error_reduction_fraction_from_A']
            d.text((x+13,884),f'颈前 / 后改善 {rf*100:.1f}% / {rb*100:.1f}%',font=font(24),fill='#253548')
        else:
            d.text((x+13,746),'误差从纯主体ID独立量测\n参考前 / 后界沿用上一轮\n五官不参与黄色主体分割',font=font(24),fill='#617086',spacing=12)
    im.save(dest/'fixed_reference_comparison.png')
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=OUT)
    parser.add_argument('--sections-only',action='store_true')
    args=parser.parse_args();out=args.output.absolute();dest=out/'diagnostics';dest.mkdir(parents=True,exist_ok=True)
    raw=read(out/'section_raw.json');models=raw['models']
    assert raw['source_hashes']['N2']==PINNED_N2,'N2 baseline SHA changed'
    source_paths={'N2':OLD/'candidate_N2/Naiwa_Character.blend','R1':out/'candidate_R1/Naiwa_Character.blend'}
    for model,path in source_paths.items():assert digest(path)==raw['source_hashes'][model],f'Stale actual sections for {model}'
    curves={};sections={}
    for model in ('N2','R1'):
        rec=models[model]
        longitudinal=rec.get('longitudinal',[rec['midline']])
        for part in longitudinal:
            key=f'{float(part["plane_X"]):.3f}';sections.setdefault(key,{})[model]=part
            branches=sample_section_branches(part)
            diags={edge:curve_diagnostics(branches['Z'],branches[f'{edge}_Y']) for edge in ('front','rear')}
            curves.setdefault(key,{})[model]=diags
    mesh_file=out/'raw_mesh_measurements.npz'
    extra_keys=[]
    if mesh_file.exists():
        with np.load(mesh_file,allow_pickle=False) as mesh:
            for x in (-.15,-.08,-.04,.04,.08,.15):
                key=f'{x:.3f}';extra_keys.append(key)
                for model in ('N2','R1'):
                    section=triangle_plane_segments(mesh[f'points_{model}'],mesh['triangles'],0,x)
                    sections.setdefault(key,{})[model]=section
                    branch=sample_section_branches(section)
                    curves.setdefault(key,{})[model]={edge:curve_diagnostics(branch['Z'],branch[f'{edge}_Y']) for edge in ('front','rear')}
    for key,rec in curves.items():
        rec['comparison']={edge:compare_curves(rec['N2'][edge],rec['R1'][edge]) for edge in ('front','rear')}
    # Baseline method must reproduce the published read-only metric before
    # accepting any R1 ratio. This catches cubic window/scale changes.
    central=curves['0.000']['N2']['front']['peak'];assert abs(central['curvature']-33.7721911098)<.08 and abs(central['Z']-2.375)<=.0006,central
    core_keys=[k for k in curves if k not in extra_keys]
    core_curves={k:curves[k] for k in core_keys};core_sections={k:sections[k] for k in core_keys}
    plot_curves(dest/'actual_curvature_diagnostics.png',core_curves)
    plot_curves(dest/'actual_rear_curvature_diagnostics.png',core_curves,'rear')
    plot_longitudinal(dest/'actual_longitudinal_sections.png',core_sections)
    if extra_keys:
        plot_curvature_grid(dest/'actual_transition_curvature_diagnostics.png',{k:curves[k] for k in extra_keys},'口周横向过渡：附加六条实际纵剖面')
    plot_horizontal(dest/'actual_horizontal_sections.png',models,('N2','R1'))
    report={'source_hashes':raw['source_hashes'],'method':curves['0.000']['N2']['front']['method'],
        'N2_baseline_reproduction':central,'longitudinal_curvature':{key:{edge:rec['comparison'][edge] for edge in ('front','rear')} for key,rec in curves.items()},
        'geometry_measured':'actual saved triangle-plane intersection','visual_acceptance_recorded':False,
        'horizontal_constraints':[],'side_wave_diagnostics':{},'Idle_curve_diagnostics':{},'transverse_front_cheek_curvature':{},'artifacts':[
            'actual_curvature_diagnostics.png','actual_rear_curvature_diagnostics.png','actual_longitudinal_sections.png','actual_horizontal_sections.png','actual_curvature_samples.csv']}
    if extra_keys:report['artifacts'].append('actual_transition_curvature_diagnostics.png')
    validation_path=out/'round_validation.json'
    if validation_path.exists():
        validation=read(validation_path)
        assert validation['candidate_sha256']==raw['source_hashes']['R1'],'Stale independent geometric validation'
        report['independent_intermediate_width_check']={
            'sampling_step_Z':validation['section_constraints_raw']['intermediate_width_step_Z'],
            'maximum_absolute_width_fraction_change_raw':validation['section_constraints_raw']['maximum_intermediate_width_fraction_change'],
            'maximum_absolute_width_fraction_change_Idle':validation['section_constraints_Idle_frame1']['maximum_intermediate_width_fraction_change'],
            'width_fraction_limit':.015,'source_validation_file_sha256':digest(validation_path)}
    for before in models['N2']['horizontal']:
        after=next(r for r in models['R1']['horizontal'] if abs(r['Z']-before['Z'])<1e-8)
        row={'Z':before['Z'],'N2':{k:before[k] for k in ('front_Y','rear_Y','width_X')},'R1':{k:after[k] for k in ('front_Y','rear_Y','width_X')}}
        row['delta_R1_from_N2']={k:after[k]-before[k] for k in ('front_Y','rear_Y','width_X')}
        row['relative_X_width_change']=(after['width_X']/before['width_X']-1)
        tolerance={2.12:.003,2.22:.003,2.31:.004}.get(round(before['Z'],3))
        if tolerance is not None:row['endpoint_tolerance']=tolerance;row['endpoint_passed']=max(abs(row['delta_R1_from_N2'][k]) for k in ('front_Y','rear_Y'))<=tolerance+1e-7
        report['horizontal_constraints'].append(row)
    with (dest/'actual_curvature_samples.csv').open('w',encoding='utf-8-sig',newline='') as handle:
        writer=csv.writer(handle);writer.writerow(('X','model','edge','Z','actual_Y','fitted_Y','dY_dZ','d2Y_dZ2','curvature'))
        for x,rec in curves.items():
            for model in ('N2','R1'):
                for edge in ('front','rear'):
                    c=rec[model][edge]
                    for i,z in enumerate(c['Z']):
                        writer.writerow((x,model,edge,z,*[c[k][i] for k in ('Y','fitted_Y','dY_dZ','d2Y_dZ2','curvature')]))
    if mesh_file.exists():
        sides={}
        with np.load(mesh_file,allow_pickle=False) as mesh:
            triangles=mesh['triangles']
            for y in (-.10,0.,.10):
                key=f'{y:.3f}';sides[key]={};report['side_wave_diagnostics'][key]={}
                for model in ('N2','R1'):
                    points=mesh[f'points_{model}'];section=triangle_plane_segments(points,triangles,1,y);sides[key][model]=section
                    # X(Z) branches from Y-plane are the lateral silhouettes.
                    swap=section[:,:,[1,0,2]];branch=sample_section_branches(swap)
                    take=(branch['Z']>=2.07)&(branch['Z']<=2.28)
                    report['side_wave_diagnostics'][key][model]={edge:band_wave_diagnostic(branch['Z'][take],branch[f'{edge}_Y'][take]) for edge in ('front','rear')}
            for x in sorted(map(float,curves)):
                key=f'{x:.3f}';pairs={}
                for model in ('N2','R1'):
                    idle=f'points_{model}_Idle'
                    if idle not in mesh:continue
                    branch=sample_section_branches(triangle_plane_segments(mesh[idle],triangles,0,x))
                    pairs[model]=curve_diagnostics(branch['Z'],branch['front_Y'])
                if len(pairs)==2:report['Idle_curve_diagnostics'][key]=compare_curves(pairs['N2'],pairs['R1'])
            transverse={};xs=np.round(np.arange(-.23,.2300001,.0005),8)
            for z in (2.355,2.365,2.375,2.385,2.395):
                key=f'{z:.3f}';transverse[key]={}
                for model in ('N2','R1'):
                    part=triangle_plane_segments(mesh[f'points_{model}'],triangles,2,z)
                    sampled=sample_axis_branches(part,xs,0,1)
                    cc=curve_diagnostics(xs,sampled['lower_boundary'],peak_region=(-.22,.22),neighbor_region=(-.22,.22),expanded_region=(-.22,.22))
                    transverse[key][model]={'front':cc}
                comparison=compare_curves(transverse[key]['N2']['front'],transverse[key]['R1']['front'])
                # The instruction's ~20% objective concerns the under-cheek
                # longitudinal shoulder, not every transverse curvature peak.
                comparison.pop('diagnostic_target_20_percent',None)
                transverse[key]['comparison']={'front':comparison}
                def axis_keys(value):
                    if isinstance(value,dict):return {('X' if k=='Z' else k):axis_keys(v) for k,v in value.items()}
                    if isinstance(value,list):return [axis_keys(v) for v in value]
                    return value
                report['transverse_front_cheek_curvature'][key]={'section_Z':z,'parameter_axis':'X','comparison':axis_keys(comparison)}
            plot_curvature_grid(dest/'actual_transverse_cheek_curvature.png',transverse,'嘴下横向曲面：五层实际Y(X)曲率','X',(-.22,.22))
            with (dest/'actual_transverse_curvature_samples.csv').open('w',encoding='utf-8-sig',newline='') as handle:
                writer=csv.writer(handle);writer.writerow(('section_Z','model','sample_X','actual_front_Y','dY_dX','d2Y_dX2','curvature'))
                for key,rec in transverse.items():
                    for model in ('N2','R1'):
                        cc=rec[model]['front']
                        for i,x in enumerate(cc['Z']):writer.writerow((key,model,x,*[cc[k][i] for k in ('Y','dY_dZ','d2Y_dZ2','curvature')]))
        plot_sides(dest/'actual_side_neck_sections.png',sides);report['artifacts'].append('actual_side_neck_sections.png')
        report['artifacts']+=['actual_transverse_cheek_curvature.png','actual_transverse_curvature_samples.csv']
        report['actual_raw_mesh_data_sha256']=digest(mesh_file)
        for rec in report['side_wave_diagnostics'].values():
            rec['R1_band_energy_reduction_fraction']={edge:1-rec['R1'][edge]['band_spectral_energy']/max(rec['N2'][edge]['band_spectral_energy'],1e-20) for edge in ('front','rear')}
    field=out/'candidate_R1/deformation_field.npz'
    if field.exists():report['actual_displacements']=plot_heatmap(dest/'displacement_heatmap.png',field);report['artifacts'].append('displacement_heatmap.png')
    if not args.sections_only:report['fixed_reference']=reference_analysis(out)
    for model,path in source_paths.items():assert digest(path)==raw['source_hashes'][model],f'Model {model} changed during diagnostic analysis'
    dump(dest/'surface_diagnostics.json',report)
    md=['# N2→R1最终曲面独立诊断','',
        '剖面来自保存后的实际三角网格；位移C2标签不作为完成依据。数值目标通过还须联合多视角灰模、反射条纹与掠射光观察。','',
        '固定方法：Z步长0.0005，局部三次普通最小二乘，严格半窗 `|Z−Z0|<0.010`（39点）；曲率为 `abs(Y″)/(1+Y′²)^(3/2)`。邻峰同时检查Z=2.28–2.44，禁止仅将峰移出主区。','',
        f'N2基线复现：{central["curvature"]:.8f}/工程单位，Z={central["Z"]:.4f}，与指令约33.77一致。','',
        '| X纵剖面 | N2前峰 | R1前峰 | 下降 | R1邻域最大峰 |','| ---: | ---: | ---: | ---: | ---: |']
    for x,rec in curves.items():
        c=rec['comparison']['front'];md.append(f'| {x} | {c["source_peak"]["curvature"]:.3f}@{c["source_peak"]["Z"]:.4f} | {c["target_peak"]["curvature"]:.3f}@{c["target_peak"]["Z"]:.4f} | {100*c["peak_reduction_fraction"]:.1f}% | {c["target_neighbor_peak"]["curvature"]:.3f}@{c["target_neighbor_peak"]["Z"]:.4f} |')
    md+=['','保护边界同时扩展检查至Z=2.20–2.50，JSON保存全部局部峰、同Z源曲率与增量，避免只在主峰附近检查。每条前颊剖面在此扩展范围的最大同Z曲率增量：','']
    for x,rec in curves.items():
        worst=rec['comparison']['front']['maximum_curvature_increase_same_Z']
        md.append(f'- X={x}：Z={worst["Z"]:.4f}，N2 {worst["source_curvature"]:.3f}→R1 {worst["target_curvature"]:.3f}，增量{worst["delta_curvature"]:+.3f}。')
    if extra_keys:md+=['','![口周过渡六条附加纵剖面](actual_transition_curvature_diagnostics.png)','',
        '附加X=±0.04、±0.08、±0.15与各自N2实际源峰比较；外颊源曲率不同，不设统一的绝对峰值。','']
    md+=['','![实际下颊曲率](actual_curvature_diagnostics.png)','','![实际后脑—后颈曲率](actual_rear_curvature_diagnostics.png)','','![纵剖面](actual_longitudinal_sections.png)','','![水平截面](actual_horizontal_sections.png)','',
        '| Z | Δ前界Y | Δ后界Y | X宽相对变化 | 端点容差 |','| ---: | ---: | ---: | ---: | --- |']
    for row in report['horizontal_constraints']:
        delta=row['delta_R1_from_N2'];tol=f'{row["endpoint_tolerance"]:.3f} / {"通过" if row["endpoint_passed"] else "未通过"}' if 'endpoint_tolerance' in row else '此层无端点硬容差'
        md.append(f'| {row["Z"]:.3f} | {delta["front_Y"]:+.7f} | {delta["rear_Y"]:+.7f} | {row["relative_X_width_change"]*100:+.4f}% | {tol} |')
    if 'independent_intermediate_width_check' in report:
        ww=report['independent_intermediate_width_check']
        md+=['',f'邻近中间横截面由独立网格验收以Z步{ww["sampling_step_Z"]:.3f}检查；最大X宽相对变化 raw {ww["maximum_absolute_width_fraction_change_raw"]*100:.4f}%、Idle {ww["maximum_absolute_width_fraction_change_Idle"]*100:.4f}%，指令上限1.5%。','']
    if report['side_wave_diagnostics']:
        md+=['','![侧颈剖面](actual_side_neck_sections.png)','','侧颈波纹FFT只用于固定尺度的对照：Y=−0.10/0/+0.10纵剖面，Z=2.07–2.28，先去除三次低频趋势，再用Hann窗量0.1–0.4D0波长频段。有限窗口与宏观曲线会影响能量，不据此单独批准外观。','']
        for y,rec in report['side_wave_diagnostics'].items():md.append(f'- Y={y}：左右侧频段能量降低 {100*rec["R1_band_energy_reduction_fraction"]["front"]:.1f}% / {100*rec["R1_band_energy_reduction_fraction"]["rear"]:.1f}%。')
    if report['transverse_front_cheek_curvature']:
        md+=['','![嘴下横向实际曲率](actual_transverse_cheek_curvature.png)','',
            '横向检查在五个固定Z层从实际三角相交线取前界Y(X)，X步0.0005、严格三次半窗0.010，连续检查X=±0.22范围，辅助排查共同控制场的横向条带和保护边界。该检查不要求所有横向曲率峰下降20%。','',
            '| Z水平层 | N2横向峰 | R1横向峰 | 最大同X曲率增量 / 位置 |','| ---: | ---: | ---: | --- |']
        for z,rec in report['transverse_front_cheek_curvature'].items():
            c=rec['comparison'];worst=c['maximum_curvature_increase_same_Z']
            md.append(f'| {z} | {c["source_peak"]["curvature"]:.3f}@X{c["source_peak"]["X"]:+.4f} | {c["target_peak"]["curvature"]:.3f}@X{c["target_peak"]["X"]:+.4f} | {worst["delta_curvature"]:+.3f}@X{worst["X"]:+.4f} |')
    if report['Idle_curve_diagnostics']:
        md+=['','Idle第1帧实际评估表面前界峰也独立量测：','']
        for x,c in report['Idle_curve_diagnostics'].items():md.append(f'- X={x}：{c["source_peak"]["curvature"]:.3f}→{c["target_peak"]["curvature"]:.3f}，下降{100*c["peak_reduction_fraction"]:.1f}%；邻域峰{c["target_neighbor_peak"]["curvature"]:.3f}。')
    if 'actual_displacements' in report:
        dd=report['actual_displacements'];md+=['','![实际位移与保护权重](displacement_heatmap.png)','',f'所有点最大总位移 {dd["maximum_total_displacement"]:.8f}；源法向投影最大绝对值 {dd["maximum_absolute_source_normal_projection"]:.8f}；最大切向位移 {dd["maximum_tangential_displacement"]:.8f}；Z最大改动 {dd["maximum_absolute_delta_Z"]:.8f}。分区上限与固定五官/肩根由独立几何验收另行核对。','']
    if 'fixed_reference' in report:
        rr=report['fixed_reference'];md+=['','![固定参考](../reference_comparison/fixed_reference_comparison.png)','','参考注册完全沿用旧报告；y220/260/300每前后界绝对误差相对N2恶化≤3px，前颈平均相对A改善≥40%。','',f'固定参考数值约束：{"通过" if rr["constraints"]["passed"] else "未通过"}。','']
        for m in ('N2','R1'):
            r=rr['sample_rows'][m];f,b=r['neck_mean_absolute_front_back_error_pixels'];rf,rb=r['neck_error_reduction_fraction_from_A'];md.append(f'- {m}平均前/后误差 {f:.2f}/{b:.2f}px，改善 {100*rf:.2f}%/{100*rb:.2f}%。')
    md+=['','本报告保持视觉验收为待独立观察；不会用曲率数值、频段能量或位移热图替代反射条纹和实际外观。完整曲线/邻峰、实际剖面、数据哈希保存于JSON和CSV。','']
    (dest/'surface_diagnostics.md').write_text('\n'.join(md),encoding='utf-8')
    print('N2_ROUND_SURFACE_ANALYSIS_OK',flush=True)
    for x,rec in curves.items():print(x,'front reduction',rec['comparison']['front']['peak_reduction_fraction'],'neighbor reduction',rec['comparison']['front']['neighbor_peak_reduction_fraction'],flush=True)
    if 'fixed_reference' in report:print('FIXED_REFERENCE_CONSTRAINTS',report['fixed_reference']['constraints'],flush=True)


if __name__=='__main__':main()
