"""Read saved R1/H2/E2 triangle surfaces; export and diagnose actual sections.

Blender: --export-mesh --source LABEL=path [--output version_root]
Bundled Python: --models R1 H2 E2 [--output version_root]
This never saves or edits a source .blend. Numerical findings do not certify shape.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import sys
import numpy as np

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from h2_curve_diagnostic_utils import (X_PLANES, HORIZONTAL_Z, Z_VALUES,
    longitudinal_branches, arc_diagnostic, curve_summary, outer_profiles)
from n2_round_diagnostic_utils import triangle_plane_segments

OUT = ROOT/'versions/20261007_H2_E2_overall_refinement'
BASE = ROOT/'versions/20261007_N2_round_refinement/candidate_R1/Naiwa_Character.blend'
BASE_SHA = 'ea12e9af2c564527d0c473cd31b28974339fcceb15b666502a33f0ffa057bc24'
COLORS = {'R1':'#98703c', 'H2':'#167bad', 'E2':'#23994d'}


def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def dump(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2, allow_nan=False)+'\n', encoding='utf-8')


def export(args):
    import bpy
    def world_mesh(obj, evaluated=False):
        holder = obj.evaluated_get(bpy.context.evaluated_depsgraph_get()) if evaluated else obj
        mesh = holder.to_mesh() if evaluated else holder.data
        try:
            mesh.calc_loop_triangles()
            points = np.empty((len(mesh.vertices), 3), dtype=np.float32)
            mesh.vertices.foreach_get('co', points.ravel())
            matrix = np.asarray(holder.matrix_world, float)
            points = points.astype(float)@matrix[:3,:3].T + matrix[:3,3]
            triangles = np.empty((len(mesh.loop_triangles), 3), np.int32)
            mesh.loop_triangles.foreach_get('vertices', triangles.ravel())
            return points, triangles
        finally:
            if evaluated: holder.to_mesh_clear()
    assert sha(BASE) == BASE_SHA
    dest=args.output.resolve()/'diagnostics'; dest.mkdir(parents=True, exist_ok=True)
    for spec in args.source:
        label, name = spec.split('=', 1)
        path=Path(name).resolve(strict=True); before=sha(path)
        bpy.ops.wm.open_mainfile(filepath=str(path), load_ui=False)
        body=bpy.data.objects['Naiwa unified golden body']
        raw, rt=world_mesh(body)
        rig=bpy.data.objects['Naiwa_Rig']
        rig.animation_data.action=bpy.data.actions['Naiwa_Idle']; rig.animation_data.use_nla=False
        rig.data.pose_position='POSE'
        bpy.context.scene.frame_set(1); bpy.context.view_layer.update()
        idle, it=world_mesh(body, True)
        np.savez_compressed(dest/f'overall_mesh_{label}.npz', Raw=raw, Idle1=idle, triangles_Raw=rt, triangles_Idle1=it)
        assert before == sha(path), 'Read-only diagnostic changed a source'
        dump(dest/f'overall_mesh_{label}.json', {'label':label, 'path':str(path), 'source_sha256':before,
            'mesh_sha256':sha(dest/f'overall_mesh_{label}.npz'), 'raw_vertices':len(raw),
            'Idle1_vertices':len(idle), 'raw_triangles':len(rt), 'Idle1_triangles':len(it),
            'mesh':'Naiwa unified golden body', 'saved_source':False, 'world_coordinates':True,
            'pose':'Naiwa_Idle frame1; NLA disabled; armature POSE',
            'raw_definition':'saved mesh vertices and actual Blender loop triangulation, world matrix applied',
            'Idle1_definition':'evaluated surface after all enabled modifiers at Idle frame1, world matrix applied'})
        print('OVERALL_MESH_EXPORTED', label, flush=True)


def import_validator(args):
    """Consume already independently observed saved-mesh validation data."""
    output=args.output.resolve(); dest=output/'diagnostics';dest.mkdir(parents=True,exist_ok=True)
    provenance=json.loads((output/'section_raw.json').read_text(encoding='utf-8-sig'))
    source=args.validator_candidate.absolute()
    label=args.validator_label
    assert sha(source)==provenance['source_hashes'][label], 'Validator observations do not match candidate file'
    assert provenance['source_hashes']['R1']==BASE_SHA and sha(BASE)==BASE_SHA
    observed=output/'raw_mesh_measurements.npz'
    baseline=np.load(dest/'overall_mesh_R1.npz')
    with np.load(observed) as data:
        triangles=data['triangles']
        raw,idle=data[f'points_{label}'],data[f'points_{label}_Idle']
        # Validator emits the shared topology. Confirm identity against the
        # separately exported baseline, and confirm the observed Idle pose
        # has negligible deformation before reusing the Raw triangulation.
        assert np.array_equal(triangles,baseline['triangles_Raw']), 'Triangulation changed; require a separate native exporter'
        assert np.array_equal(baseline['triangles_Raw'],baseline['triangles_Idle1'])
        assert np.max(np.linalg.norm(raw-idle,axis=1))<1e-6, 'Idle triangulation requires independent export'
        assert np.max(np.linalg.norm(data['points_R1']-baseline['Raw'],axis=1))<1e-6
        assert np.max(np.linalg.norm(data['points_R1_Idle']-baseline['Idle1'],axis=1))<1e-6
        np.savez_compressed(dest/f'overall_mesh_{label}.npz',Raw=raw,Idle1=idle,triangles_Raw=triangles,triangles_Idle1=triangles)
    info={'label':label,'path':str(source),'source_sha256':sha(source),
        'mesh_sha256':sha(dest/f'overall_mesh_{label}.npz'),'raw_vertices':len(raw),'Idle1_vertices':len(idle),
        'raw_triangles':len(triangles),'Idle1_triangles':len(triangles),
        'mesh':'Naiwa unified golden body','saved_source':False,'world_coordinates':True,
        'pose':'Naiwa_Idle frame1; independent validator observation',
        'raw_definition':'actual saved world-space triangle coordinates independently exported by validate_h2_overall_refinement.py',
        'Idle1_definition':'actual evaluated Idle frame1 triangle coordinates independently exported by validator; Raw/Idle geometry equal within 1e-6',
        'existing_validation_observation':{'path':str(observed),'sha256':sha(observed),
            'section_provenance_path':str(output/'section_raw.json'),'section_provenance_sha256':sha(output/'section_raw.json'),
            'triangulation_matches_separate_R1_export':True}}
    dump(dest/f'overall_mesh_{label}.json',info)
    print('OVERALL_VALIDATOR_OBSERVATION_IMPORTED',label,flush=True)


def ordered_loops(segments):
    """Connect exact planar segments, deduplicating coincident triangle edges."""
    point={}; adjacent={}; edges=set()
    for a,b in segments:
        ka,kb=tuple(np.round(a,7)),tuple(np.round(b,7))
        if ka==kb: continue
        point[ka],point[kb]=a,b
        edges.add(tuple(sorted((ka,kb))))
    for edge in edges:
        for k in edge: adjacent.setdefault(k, set()).add(edge)
    unseen=set(edges); loops=[]
    while unseen:
        first=next(iter(unseen)); start=first[0]; current=start; chain=[start]
        while True:
            available=adjacent[current]&unseen
            if not available: break
            edge=next(iter(available)); unseen.remove(edge)
            current=edge[1] if edge[0]==current else edge[0]; chain.append(current)
            if current==start: break
        p=np.asarray([point[k] for k in chain])
        closed=len(chain)>3 and chain[-1]==start
        area=abs(float(np.sum(p[:-1,0]*p[1:,1]-p[1:,0]*p[:-1,1])/2)) if closed else 0.
        loops.append({'points':p, 'closed':closed, 'area':area})
    return loops


def transverse_diagnostic(loop):
    p=loop['points'][:, :2]
    # Deterministic positive winding, then a start at the furthest front point.
    signed=.5*np.sum(p[:-1,0]*p[1:,1]-p[1:,0]*p[:-1,1])
    if signed < 0: p=p[::-1]
    p=p[:-1]; start=int(np.argmin(p[:,1])); p=np.roll(p,-start,axis=0)
    perimeter=float(np.linalg.norm(np.diff(np.vstack((p,p[0])),axis=0),axis=1).sum())
    expanded=np.vstack((p,p,p,p[:1]))
    all_curve=arc_diagnostic(expanded[:,0], expanded[:,1])
    take=(all_curve['s']>=perimeter)&(all_curve['s']<2*perimeter)
    c={k:v[take] for k,v in all_curve.items() if isinstance(v,np.ndarray)}
    c['s']=c['s']-perimeter
    c['tangent_angle_degrees']-=360.
    k=c['signed_curvature']; s=c['s']
    summary={'closed':True,'area_XY':loop['area'],'perimeter':perimeter,
        'maximum_absolute_curvature':float(np.abs(k).max()),
        'signed_curvature_range':[float(k.min()),float(k.max())],
        'total_absolute_turn_degrees':float(np.degrees(np.trapezoid(np.abs(k),s))),
        'net_signed_turn_degrees':float(np.degrees(np.trapezoid(k,s))),
        'orientation':'positive XY winding; start at minimum Y; periodic cubic neighborhoods',
        'automatic_visual_acceptance':False}
    return c,summary


def quick_review(args):
    """Early, explicitly limited mouth/rear checks while full QA is queued."""
    dest=args.output.resolve()/'diagnostics'
    result={'scope':'Raw only; X=0,+0.08,+0.18; early diagnosis, not complete acceptance',
        'automatic_visual_acceptance':False,'models':{}}
    for model in args.models:
        info=json.loads((dest/f'overall_mesh_{model}.json').read_text(encoding='utf-8-sig'))
        assert sha(info['path'])==info['source_sha256']
        mesh_file=dest/f'overall_mesh_{model}.npz';assert sha(mesh_file)==info['mesh_sha256']
        rr={'source':info,'profiles':{}};result['models'][model]=rr
        with np.load(mesh_file) as data:
            points,triangles=data['Raw'],data['triangles_Raw'];tz=points[triangles,2]
            triangles=triangles[(tz.max(1)>=1.98)&(tz.min(1)<=2.645)]
            for x in (0.,.08,.18):
                _,branch=longitudinal_branches(points,triangles,x)
                q={};rr['profiles'][f'{x:+.2f}']=q
                for side,field in (('front','lower_boundary'),('rear','upper_boundary')):
                    q[side]=curve_summary(arc_diagnostic(branch[field],branch['parameter']))
                w=q['front']['whole_head_neck_shoulder']
                straight=[{'Z_start':a['Z_start'],'Z_end':a['Z_end'],'arc_length':a['arc_length']} for a in w['long_low_curvature_runs'] if a['Z_start']>=2.25 and a['Z_end']<=2.40]
                print('QUICK_ACTUAL',model,x,'mouth_peak',q['front']['mouth_underface_connection']['maximum_absolute_curvature'],
                    'mouth_straight',straight,'rear_peak',q['rear']['whole_head_neck_shoulder']['maximum_absolute_curvature'],
                    'rear_sharp_count',len(q['rear']['whole_head_neck_shoulder']['short_high_curvature_runs']),flush=True)
    dump(dest/'quick_actual_curve_review.json',result)


def plot_sections(dest, models, data, mode):
    from PIL import Image, ImageDraw, ImageFont
    from analyze_n2_round_refinement import Panel
    f=lambda n:ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',n)
    im=Image.new('RGB',(1860,1840),'#f3f5f8'); d=ImageDraw.Draw(im)
    d.text((25,15),f'{mode} 实际三角纵剖面：'+ ' / '.join(models)+'，同尺度',font=f(32),fill='#24364a')
    d.text((25,65),'X=0、±0.04、±0.08、±0.12、±0.18；全段 Z1.98～2.645。原始交线，无目标曲面拟合。',font=f(22),fill='#536577')
    for i,x in enumerate(X_PLANES):
        col,row=i%3,i//3; left=20+col*615; top=130+row*560
        d.text((left+15,top),f'X = {x:+.2f}',font=f(24),fill='#24364a')
        p=Panel(im,(left+40,top+40,left+585,top+515),(-.46,1.98,.41,2.645)); p.grid(.10,(-.46,1.98,.41,2.645))
        for model in models:
            for seg in data[(model,mode)]['sections'][x]:
                take=seg[:,2]>=1.98
                if take.all():p.line(seg[:,(1,2)],COLORS.get(model,'#9356ad'),2)
        p.commit()
    for i,model in enumerate(models):
        d.text((30+i*175,1790),model,font=f(24),fill=COLORS.get(model,'#9356ad'))
    im.save(dest/f'actual_longitudinal_sections_{mode}.png')
    im=Image.new('RGB',(1860,2440),'#f3f5f8'); d=ImageDraw.Draw(im)
    d.text((25,15),f'{mode} 实际横截面：全部机位使用相同工程尺度',font=f(32),fill='#24364a')
    for i,z in enumerate(HORIZONTAL_Z):
        col,row=i%4,i//4; left=10+col*460; top=100+row*565
        d.text((left+15,top),f'Z = {z:.2f}',font=f(23),fill='#24364a')
        p=Panel(im,(left+15,top+35,left+445,top+520),(-.44,-.47,.44,.41));p.grid(.10,(-.44,-.47,.44,.41))
        for model in models:
            for seg in data[(model,mode)]['horizontal'][z]:p.line(seg[:,(0,1)],COLORS.get(model,'#9356ad'),2)
        p.commit()
    im.save(dest/f'actual_horizontal_sections_{mode}.png')


def plot_outer(dest, models):
    """Plot already measured envelope samples without repeating geometry work."""
    from PIL import Image,ImageDraw,ImageFont
    from analyze_n2_round_refinement import Panel
    f=lambda n:ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',n)
    outer={}
    with (dest/'actual_arc_samples.csv').open(encoding='utf-8',newline='') as handle:
        for row in csv.DictReader(handle):
            if row['model'] not in models or row['profile'] not in ('left','right','front_envelope','rear_envelope'):continue
            outer.setdefault((row['model'],row['pose'],row['profile']),[]).append((float(row['actual_horizontal']),float(row['actual_Z'])))
    for mode in ('Raw','Idle1'):
        im=Image.new('RGB',(1640,1070),'#f3f5f8');d=ImageDraw.Draw(im)
        d.text((25,15),f'{mode} 真实主体外轮廓：'+ ' / '.join(models),font=f(32),fill='#24364a')
        d.text((25,65),'实际横切三角面的外界，以同一工程尺度绘制；左图X/Z，右图Y/Z，不重新缩放候选。',font=f(22),fill='#536577')
        for col,(profiles,title,bounds) in enumerate(((('left','right'),'左右外轮廓 X / Z',(-.46,1.98,.46,2.645)),
            (('front_envelope','rear_envelope'),'前后投影包络 Y / Z',(-.46,1.98,.46,2.645)))):
            left=20+col*810;d.text((left+30,120),title,font=f(24),fill='#24364a')
            p=Panel(im,(left+35,160,left+785,975),bounds);p.grid(.10,bounds)
            for model in models:
                for profile in profiles:p.line(outer[(model,mode,profile)],COLORS.get(model,'#9356ad'),3)
            p.commit()
        for i,model in enumerate(models):d.text((30+i*175,1010),model,font=f(24),fill=COLORS.get(model,'#9356ad'))
        im.save(dest/f'actual_outer_profiles_{mode}.png')


def plot_curves(dest, models, data, mode, metric):
    from PIL import Image, ImageDraw, ImageFont
    f=lambda n:ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',n)
    im=Image.new('RGB',(1860,2200),'#f3f5f8');d=ImageDraw.Draw(im)
    title='带符号曲率' if metric=='signed_curvature' else '弧长切向角 (°)'
    d.text((25,15),f'{mode} {title}：实际曲线，统一纵轴与 Z 范围',font=f(32),fill='#24364a')
    d.text((25,65),'弧长步 0.0005；局部三次半窗 0.010。实线前界，虚线后界；诊断不等于视觉通过。',font=f(22),fill='#536577')
    lo,hi=(-80.,80.) if metric=='signed_curvature' else (-90.,360.)
    for i,x in enumerate(X_PLANES):
        col,row=i%3,i//3; left=15+col*615; top=120+row*670
        rect=(left+65,top+40,left+580,top+590)
        def p(z,v):return rect[0]+(z-1.98)/(.665)*(rect[2]-rect[0]),rect[3]-(v-lo)/(hi-lo)*(rect[3]-rect[1])
        d.text((left+10,top),f'X={x:+.2f} / {lo:g}～{hi:g}',font=f(23),fill='#24364a')
        for z in np.arange(2.0,2.61,.10):
            a,b=p(z,lo),p(z,hi);d.line((*a,*b),fill='#d2dbe5');d.text((a[0]-20,a[1]+8),f'{z:.1f}',font=f(16),fill='#607187')
        for v in np.linspace(lo,hi,5):
            a,b=p(1.98,v),p(2.645,v);d.line((*a,*b),fill='#d2dbe5');d.text((left+3,a[1]-10),f'{v:g}',font=f(16),fill='#607187')
        d.rectangle(rect,outline='#8393a3',width=2)
        for model in models:
            for branch in ('front','rear'):
                c=data[(model,mode)]['curves'][x][branch];z=c['position_YZ'][:,1];v=c[metric]
                coords=[p(a,np.clip(b,lo,hi)) for a,b in zip(z,v)]
                if branch=='front': d.line(coords,fill=COLORS.get(model,'#9356ad'),width=2)
                else:
                    for j in range(0,len(coords)-1,16):d.line(coords[j:j+8],fill=COLORS.get(model,'#9356ad'),width=2)
    for i,model in enumerate(models):d.text((30+i*175,2150),model,font=f(24),fill=COLORS.get(model,'#9356ad'))
    im.save(dest/f'actual_{metric}_{mode}.png')


def analyze(args):
    dest=args.output.resolve()/'diagnostics';dest.mkdir(parents=True,exist_ok=True)
    data={}; report={'models':{},'actual_surface_measurement':True,'automatic_visual_acceptance':False,
        'units':'Blender project world units','X_planes':list(X_PLANES),'horizontal_Z':list(HORIZONTAL_Z),
        'analysis_Z_range':[1.98,2.645],'method':{'section_source':'actual saved/evaluated triangles',
        'arc_step':.0005,'cubic_arc_half_window':.010,'model_displacement_function_used':False,
        'signed_curvature_orientation':'longitudinal bottom-to-top YZ; outer left/right bottom-to-top XZ'},
        'artifacts':[]}
    with (dest/'actual_arc_samples.csv').open('w',encoding='utf-8',newline='') as handle, (dest/'actual_transverse_arc_samples.csv').open('w',encoding='utf-8',newline='') as hh:
        csvw=csv.writer(handle);csvw.writerow(('model','pose','profile','plane_X','arc_s','actual_horizontal','actual_Z','tangent_deg','signed_curvature'))
        csvh=csv.writer(hh);csvh.writerow(('model','pose','plane_Z','arc_s','actual_X','actual_Y','tangent_deg','signed_curvature'))
        for model in args.models:
            info=json.loads((dest/f'overall_mesh_{model}.json').read_text(encoding='utf-8'))
            assert sha(info['path'])==info['source_sha256'], f'Stale source for {model}'
            mesh_file=dest/f'overall_mesh_{model}.npz';assert sha(mesh_file)==info['mesh_sha256']
            record={'source':info,'poses':{}};report['models'][model]=record
            with np.load(mesh_file) as mesh:
                for mode in ('Raw','Idle1'):
                    points,triangles=mesh[mode],mesh[f'triangles_{mode}']
                    # Retain only triangles that can cross the diagnostic region.
                    # This is a spatial prefilter of real triangles, not a fitted mesh.
                    tz=points[triangles,2]
                    triangles=triangles[(tz.max(axis=1)>=1.98)&(tz.min(axis=1)<=2.645)]
                    sections={};curves={};horizontal={}
                    summary={'longitudinal':{},'outer_profiles':{},'horizontal':{}}
                    record['poses'][mode]=summary
                    for x in X_PLANES:
                        segments,branch=longitudinal_branches(points,triangles,x)
                        sections[x]=segments;curves[x]={};rr={};summary['longitudinal'][f'{x:+.2f}']=rr
                        for side,field in (('front','lower_boundary'),('rear','upper_boundary')):
                            c=arc_diagnostic(branch[field],branch['parameter']);curves[x][side]=c
                            rr[side]={'method':c['method'],**curve_summary(c)}
                            csvw.writerows((model,mode,side,x,s,*p,t,k) for s,p,t,k in zip(c['s'],c['position_YZ'],c['tangent_angle_degrees'],c['signed_curvature']))
                    outer=outer_profiles(points,triangles)
                    for side,field in (('left','left_X'),('right','right_X'),('front_envelope','front_Y'),('rear_envelope','rear_Y')):
                        c=arc_diagnostic(outer[field],outer['Z']); summary['outer_profiles'][side]={'method':c['method'],**curve_summary(c)}
                        csvw.writerows((model,mode,side,'envelope',s,*p,t,k) for s,p,t,k in zip(c['s'],c['position_YZ'],c['tangent_angle_degrees'],c['signed_curvature']))
                    for z in HORIZONTAL_Z:
                        segments=triangle_plane_segments(points,triangles,2,z);horizontal[z]=segments
                        loops=ordered_loops(segments);closed=[c for c in loops if c['closed']]
                        row={'component_count':len(loops),'closed_component_count':len(closed)}
                        summary['horizontal'][f'{z:.2f}']=row
                        if closed:
                            loop=max(closed,key=lambda c:c['area']);c,ss=transverse_diagnostic(loop);row.update(ss)
                            csvh.writerows((model,mode,z,s,*p,t,k) for s,p,t,k in zip(c['s'],c['position_YZ'],c['tangent_angle_degrees'],c['signed_curvature']))
                        else:row['diagnostic_limitation']='No closed component at selected plane; raw segments still recorded and plotted'
                    data[(model,mode)]={'sections':sections,'curves':curves,'horizontal':horizontal}
            print('OVERALL_ACTUAL_ANALYZED',model,flush=True)
    for mode in ('Raw','Idle1'):
        plot_sections(dest,args.models,data,mode)
        for metric in ('signed_curvature','tangent_angle_degrees'):plot_curves(dest,args.models,data,mode,metric)
    plot_outer(dest,args.models)
    report['artifacts']=[p.name for p in dest.iterdir() if p.name.startswith('actual_')]
    report['comparisons_from_R1']={}
    if 'R1' in args.models:
        for model in args.models:
            if model=='R1':continue
            deltas={};report['comparisons_from_R1'][model]=deltas
            for mode in ('Raw','Idle1'):
                deltas[mode]={}
                for x in X_PLANES:
                    for side in ('front','rear'):
                        key=f'X{x:+.2f}_{side}'
                        a=report['models']['R1']['poses'][mode]['longitudinal'][f'{x:+.2f}'][side]
                        b=report['models'][model]['poses'][mode]['longitudinal'][f'{x:+.2f}'][side]
                        deltas[mode][key]={name:{'R1':a[name],model:b[name]} for name in ('whole_head_neck_shoulder','mouth_underface_connection','rear_head_neck_shoulder')}
    dump(dest/'overall_surface_diagnostics.json',report)
    lines=['# H2 / E2 实际曲面诊断','','全部诊断读取保存工程 Raw 和 Idle 第1帧的真实三角面，逐工程绑定 SHA256。弧长切向角与带符号曲率由实际剖面量测，不采用位移函数的连续性声明。','',
        '纵剖面 X=0、±0.04、±0.08、±0.12、±0.18；左右外轮廓由实际横切三角面的最小／最大X得到。横截面提取16条，选最大闭合主体环。周期邻域用同样三次拟合，避免在切片起点产生人为曲率峰。','',
        '长直段、短急弯、近水平平台及相邻组合为诊断候选，阈值全部记录。任何数值降低均不能单独判定消除阶梯；曲线图片若超纵轴显示范围会截顶，完整值在CSV及JSON。','']
    for mode in ('Raw','Idle1'):
        lines += [f'## {mode}','',''+f'![实际纵剖面](actual_longitudinal_sections_{mode}.png)','',''+f'![实际外轮廓](actual_outer_profiles_{mode}.png)','',''+f'![实际横截面](actual_horizontal_sections_{mode}.png)','',''+f'![带符号曲率](actual_signed_curvature_{mode}.png)','',''+f'![弧长切向角](actual_tangent_angle_degrees_{mode}.png)','']
    (dest/'overall_surface_diagnostics.md').write_text('\n'.join(lines),encoding='utf-8')
    print('OVERALL_DIAGNOSTICS_OK',args.models,flush=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=OUT)
    parser.add_argument('--export-mesh',action='store_true')
    parser.add_argument('--source',action='append',default=[],help='LABEL=absolute saved .blend')
    parser.add_argument('--import-validator',action='store_true',help='Read an existing output/raw_mesh_measurements.npz with source-hash checks')
    parser.add_argument('--quick-review',action='store_true',help='Early Raw X0/+0.08/+0.18 diagnosis; writes a separate limited-scope report')
    parser.add_argument('--plot-outer-only',action='store_true',help='Plot existing measured envelope CSV without re-running surface extraction')
    parser.add_argument('--validator-label',default='H2')
    parser.add_argument('--validator-candidate',type=Path)
    parser.add_argument('--models',nargs='+',default=['R1','H2','E2'])
    argv=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else sys.argv[1:]
    args=parser.parse_args(argv)
    if args.import_validator:
        assert args.validator_candidate
        import_validator(args)
    elif args.quick_review:quick_review(args)
    elif args.plot_outer_only:plot_outer(args.output.resolve()/'diagnostics',args.models)
    elif args.export_mesh:
        assert args.source, 'Export needs --source LABEL=path'
        export(args)
    else:analyze(args)


if __name__=='__main__':main()
