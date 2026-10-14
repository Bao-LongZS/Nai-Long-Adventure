"""One Raw BODY read for the C3 key-image gate; no full asset validation."""
import argparse, hashlib, json, sys
from pathlib import Path
import bpy
import numpy as np

HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
from geometry_diagnostic_utils import array
from c3_measurement_utils import measure_sections

BODY='Naiwa unified golden body'
def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()
def dump(path,value):path.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--root',type=Path,required=True);ap.add_argument('--candidate',type=Path,required=True)
    args=ap.parse_args(sys.argv[sys.argv.index('--')+1:]);root=args.root.resolve();candidate=args.candidate.resolve(strict=True)
    protocol_path=root/'measurement_protocol.json';protocol=json.loads(protocol_path.read_text(encoding='utf-8'));snapshot=json.loads((root/'baseline/snapshot.json').read_text(encoding='utf-8'))
    assert sha(protocol_path)==snapshot['measurement_protocol_sha256'],'Protocol hash differs from immutable baseline'
    assert sha(HERE/'c3_measurement_utils.py')==protocol['sections']['measurement_helper_sha256'],'Measurement implementation differs'
    before=sha(candidate);bpy.ops.wm.open_mainfile(filepath=str(candidate),load_ui=False)
    body=bpy.data.objects[BODY];local=array(body.data.vertices,'co',3,np.float32);matrix=np.asarray(body.matrix_world,float);world=local.astype(float)@matrix[:3,:3].T+matrix[:3,3]
    body.data.calc_loop_triangles();triangles=array(body.data.loop_triangles,'vertices',3,np.int32)
    cached=np.load(root/'baseline/baseline_arrays.npz');pre=snapshot['objects'][BODY]['array_prefix']
    assert np.array_equal(triangles,cached[pre+'_triangles']),'Raw BODY topology differs'
    assert matrix.tolist()==snapshot['objects'][BODY]['transform'],'Raw BODY transform differs'
    actual=measure_sections(world,triangles,protocol)
    old=json.loads((root/'baseline/measurements_Raw.json').read_text(encoding='utf-8'));comparison={}
    for x in (0.,-.08,.08,-.12,.12):
        key=str(x);a=old['longitudinal'][key];b=actual['longitudinal'][key]
        aw=a['angle80']['width'];bw=b['angle80']['width'];ap=a['maximum_absolute_curvature'];bp=b['maximum_absolute_curvature']
        an=a['natural_front_neck'];bn=b['natural_front_neck'];ar=an['maximum_world_Y_recess_from_endpoint_chord'];br=bn['maximum_world_Y_recess_from_endpoint_chord']
        comparison[key]={'E2_angle80_width':aw,'candidate_angle80_width':bw,'width_fraction_change':bw/aw-1.,'E2_absolute_curvature_peak':ap,'candidate_absolute_curvature_peak':bp,'peak_fraction_change':bp/ap-1.,'E2_peak_world':a['curvature_peak_world'],'candidate_peak_world':b['curvature_peak_world'],'E2_natural_neck_recess':ar,'candidate_natural_neck_recess':br,'neck_recess_fraction_change':br/ar-1.,'E2_natural_neck_signed_curvature_range':[an['minimum_signed_curvature'],an['maximum_signed_curvature']],'candidate_natural_neck_signed_curvature_range':[bn['minimum_signed_curvature'],bn['maximum_signed_curvature']]}
    report={'schema_version':1,'purpose':'limited key gate evidence from actual saved Raw BODY; not full delivery validation','candidate':str(candidate),'candidate_sha256':before,'baseline_source_sha256':snapshot['source_sha256'],'measurement_protocol_sha256':sha(protocol_path),'state':'Raw','only_object_read':BODY,'actual_sections':actual,'comparison':comparison,'source_E2_reopened':False}
    assert sha(candidate)==before,'Read-only measurement changed candidate'
    dump(candidate.parent/'key_actual_sections.json',report)
    dump(candidate.parent/'key_actual_sections_summary.json',{k:v for k,v in report.items() if k!='actual_sections'})
    print('C3_KEY_ACTUAL_SECTIONS_OK',json.dumps(comparison,ensure_ascii=False),flush=True)

if __name__=='__main__':main()
