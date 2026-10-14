"""Snapshot relevant existing generation code and numeric parameters only."""
import ast
import hashlib
import json
import zipfile
from pathlib import Path

ROOT=Path(__file__).resolve().parent
OUT=ROOT/'diagnostics/20261005_head_geometry'
def read(path):return json.loads(path.read_text(encoding='utf-8'))
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    side=ROOT/'versions/20261005_head_side_continuity'
    reshape=ROOT/'versions/20261005_head_reshape'
    original=ROOT/'versions/20261005_eye_muzzle_body_delivery'
    b=read(reshape/'baseline_dimensions.json');rec=read(side/'head_side_continuity_build_report.json')
    m=read(OUT/'head_geometry_measurements.json');D1=b['targets']['D1']
    tree=ast.parse((ROOT/'build_naiwa.py').read_text(encoding='utf-8-sig'))
    keys=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign)
        and any(isinstance(t,ast.Name) and t.id=='outline_keys' for t in n.targets) and isinstance(n.value,ast.List))
    params={'schema_version':1,'source_blend':str(side/'Naiwa_Character.blend'),
        'source_sha256':sha(side/'Naiwa_Character.blend'),'geometry_modified_by_snapshot':False,
        'units':'Blender project coordinate units','coordinates':{'X':'right','Y':'back','Z':'up','F':[0,-1,0]},
        'actual_current_measurement':{'D_visible':m['D'],'eye_spacing':m['eye_spacing'],
            'eye_z':m['eye_z'],'mouth_z':m['mouth_z'],'horizontal_sections':m['horizontal_sections']},
        'default_generation_chain':['build_naiwa.py','revise_head_side_continuity.py',
            'versions/20261005_head_reshape/Naiwa_Character.blend','head_side_continuity_sculpt.fix_side_continuity'],
        'fixed_input_dependencies':['versions/20261005_head_reshape/Naiwa_Character.blend',
            'versions/20261005_eye_muzzle_body_delivery/Naiwa_Character.blend',
            'versions/20261005_eye_muzzle_body_delivery/stage_face_raise.blend',
            'iterations/before_eye_muzzle_body_revision/Naiwa_Character.blend'],
        'reproduction_boundary':'Default entry inherits the pinned previous model. Historical --from-primitives does not run the latest head reshape and continuity revisions. Archive excludes native model assets; fixed project inputs are needed.',
        'head_reshape_nominal':{'D0':b['D0'],'W0':b['W0'],'targets':b['targets'],
            'eye_z':b['eye_z'],'mouth_z':b['mouth_z'],'crown_z':b['crown_z'],
            'width_peak_fraction':.10,'width_weight':'0.10*S(mouth_z-0.035,eye_z,z)*(1-S(eye_z,crown_z,z))',
            'posterior_weight':'1-S(0.12,0.70,normalized_y)',
            'front_restore_weight':'S(2.17,2.22,z)*(1-S(2.58,2.635,z))',
            'muzzle_support_X_radius':1.65*D1,'muzzle_support_Z_radius':1.10*D1,
            'muzzle_kernel':'max(0,1-q)^3','mouth_groove_fraction_D':.012},
        'eye_cap_nominal':{'Dconstruct':D1,'a':D1/2,'h':.20*D1,
            'R':((D1/2)**2+(.20*D1)**2)/(2*.20*D1),'pupil_radius':.53*D1/2,
            'maximum_buried_radial_extent':.69*D1,'rim_inset':0.,'buried_back_coefficient_absolute':.12,
            'back_formula':'.12*max(0,-cos(phi))^2','surface_formula':'Y=skin(body,x,z)-cap+buried',
            'cap_formula':'sqrt(max(1e-10,R^2-rad^2))-(R-h)-rim_inset*(rad/a)^4',
            'complete_spherical_eye':False,'separate_pupil_geometry':False,
            'color_domain':'EyeColor CORNER; shared eye mesh','vertices_per_eye':5474,'azimuth_samples':96},
        'head_side_continuity':rec,
        'smoothstep':'S(t)=6t^5-15t^4+10t^3; clamp t to [0,1]',
        'historical_primitive_body':{'outline_keys_columns':['source_Z','X_halfwidth','front_Y','rear_Y'],
            'outline_keys':keys,'torso_lift_absolute':.160,'azimuth_samples':64,
            'Hermite_subintervals_per_key_interval':4,'head_width_gaussian_fraction':.11,
            'head_depth_gaussian_fraction':.08,'head_gaussian_center_Z':2.19,'head_gaussian_sigma':.20,
            'voxel_union_size':.012},
        'historical_analytic_head':{'X_profile_center_Z':2.330,'X_radius':.260,
            'Z_up_radius':.220,'Z_down_radius':.330,'depth_profile_center_Z':2.300,
            'depth_center_Y':-.125,'depth_radius':.295,'depth_Z_radius':.250,
            'bridge_source_Z':2.065,'bridge_X_and_front_end_Z':2.255,'bridge_back_end_Z':2.330,
            'interpretation':'Historical construction parameters, not final measured dimensions'}}
    assert len(keys)==26
    (OUT/'head_generation_parameters.json').write_text(json.dumps(params,indent=2,ensure_ascii=False),encoding='utf-8')
    seeds=['build_naiwa.py','revise_head_side_continuity.py','head_side_continuity_sculpt.py',
        'revise_head_reshape.py','head_reshape_sculpt.py','revise_eye_muzzle_body.py','eye_muzzle_sculpt.py',
        'body_finish_sculpt.py','geometry_diagnostic_utils.py','diagnose_head_geometry.py',
        'plot_head_geometry_diagnostics.py','render_eye_geometry_diagnostic.py','render_reference_alignment.py',
        'make_reference_head_diagnostic.py','write_head_diagnostic_summary.py','build_head_diagnostic_report.py',
        'validate_head_diagnostic_delivery.py','create_head_parameter_snapshot.py',
        'iterations/before_eye_muzzle_body_revision/build_naiwa.py']
    todo=[ROOT/n for n in seeds];selected=set();dependency_edges=[];function_spans={}
    while todo:
        path=todo.pop().resolve()
        if path in selected:continue
        assert path.is_file(),path
        selected.add(path);text=path.read_text(encoding='utf-8-sig');tree=ast.parse(text)
        modules=[];literals=[]
        for n in ast.walk(tree):
            if isinstance(n,ast.Import):modules.extend(a.name for a in n.names)
            elif isinstance(n,ast.ImportFrom) and n.module:modules.append(n.module)
            elif isinstance(n,ast.Constant) and isinstance(n.value,str) and n.value.endswith('.py'):literals.append(n.value)
        local=[]
        for module in modules:
            stem=module.split('.')[0]+'.py'
            dep=next((p for p in [path.parent/stem,ROOT/stem] if p.is_file()),None)
            if dep:local.append(dep)
        for literal in literals:
            dep=next((p for p in [path.parent/literal,ROOT/literal] if p.is_file()),None)
            if dep and ROOT in dep.resolve().parents:local.append(dep)
        for dep in local:
            dependency_edges.append([str(path.relative_to(ROOT)).replace('\\','/'),str(dep.relative_to(ROOT)).replace('\\','/')])
            todo.append(dep)
        function_spans[str(path.relative_to(ROOT)).replace('\\','/')]=[
            {'name':n.name,'start_line':n.lineno,'end_line':n.end_lineno} for n in tree.body if isinstance(n,(ast.FunctionDef,ast.AsyncFunctionDef))]
    selected.add(ROOT/'run_blender_stage.ps1')
    for folder in (side,reshape,original):
        selected.add(folder/'baseline_dimensions.json')
        selected.update(folder.glob('*build_report.json'))
    selected.update([OUT/'HEAD_GENERATION_PARAMETERS.md',OUT/'head_generation_parameters.json'])
    files=[{'path':'NaiwaCharacter/'+p.relative_to(ROOT).as_posix(),'sha256':sha(p),'bytes':p.stat().st_size} for p in sorted(selected)]
    manifest={'source_blend_sha256':params['source_sha256'],'model_assets_included':False,
        'dependency_discovery':'AST local imports and literal Python filenames, plus explicit historical source and fixed three-round records',
        'files':files,'dependency_edges':sorted(set(map(tuple,dependency_edges))),
        'function_line_spans':function_spans,'fixed_input_dependencies':params['fixed_input_dependencies'],
        'reproduction_boundary':params['reproduction_boundary']}
    manifest_path=OUT/'head_generation_source_manifest.json'
    manifest_path.write_text(json.dumps(manifest,indent=2,ensure_ascii=False),encoding='utf-8')
    with zipfile.ZipFile(OUT/'head_generation_code.zip','w',compression=zipfile.ZIP_DEFLATED) as z:
        for p in sorted(selected):z.write(p,'NaiwaCharacter/'+p.relative_to(ROOT).as_posix())
        z.write(manifest_path,'head_generation_source_manifest.json')
        z.writestr('README.txt','Source snapshot only. Preserve original project paths. No blend/FBX included. See HEAD_GENERATION_PARAMETERS.md for fixed-input requirements.\n')
    print('HEAD_PARAMETER_SNAPSHOT_OK',len(files),len(keys))

if __name__=='__main__':main()
