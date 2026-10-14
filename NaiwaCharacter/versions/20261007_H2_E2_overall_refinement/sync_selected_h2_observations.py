"""Reuse actual observations of the byte-identical selected H2; no rerender."""
import hashlib
import json
from pathlib import Path
import shutil

out=Path(__file__).resolve().parent
trial=out/'trials_H2/H2_O_distributed_contact'
native=out/'candidate_H2/Naiwa_Character.blend'
sha=lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
expected='7b2233033e755b614bf9b15f49e7f8a459e12ff775dc278215cbb34049193a49'
assert sha(native)==sha(trial/'Naiwa_Character.blend')==expected
validation=json.loads((native.parent/'validation_report.json').read_text(encoding='utf-8-sig'))
assert validation['passed'] and validation['candidate_sha256']==expected
dest=out/'diagnostics';dest.mkdir(exist_ok=True)
for path in (trial/'diagnostics').iterdir():
    if path.is_file() and (path.name.startswith('actual_') or path.name.startswith('overall_')):
        shutil.copy2(path,dest/path.name)
info=json.loads((dest/'overall_mesh_H2.json').read_text(encoding='utf-8'))
info['path']=str(native.resolve())
info['byte_identical_observation_origin']=str((trial/'Naiwa_Character.blend').resolve())
assert info['source_sha256']==expected and info['mesh_sha256']==sha(dest/'overall_mesh_H2.npz')
(dest/'overall_mesh_H2.json').write_text(json.dumps(info,indent=2)+'\n',encoding='utf-8')
report=json.loads((dest/'overall_surface_diagnostics.json').read_text(encoding='utf-8'))
report['models']['H2']['source']=info
(dest/'overall_surface_diagnostics.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
provenance={'selected_trial':'H2_O_distributed_contact','source_sha256':expected,
    'formal_candidate_sha256':sha(native),'byte_identical_copy':True,
    'independent_native_validation':'passed','shape_gate_review':'visual_review_pending.json',
    'complete_visual_FBX_rebuild_acceptance':'pending'}
(native.parent/'promotion_provenance.json').write_text(json.dumps(provenance,indent=2)+'\n',encoding='utf-8')
print('SELECTED_H2_OBSERVATIONS_SYNCED')
