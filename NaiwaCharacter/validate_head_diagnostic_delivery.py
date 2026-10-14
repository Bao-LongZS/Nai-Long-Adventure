"""Validate deliverable integrity and links without invoking Blender."""
import ast
import hashlib
import json
import re
import zipfile
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlparse
from PIL import Image

ROOT=Path(__file__).resolve().parent
OUT=ROOT/'diagnostics/20261005_head_geometry'
VERSION=ROOT/'versions/20261005_head_side_continuity'
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()

class Links(HTMLParser):
    def __init__(self):super().__init__();self.links=[]
    def handle_starttag(self,tag,attrs):
        for k,v in attrs:
            if k in ('href','src'):self.links.append(v)

def main():
    required=['index.html','DIAGNOSTIC_REPORT.md','eye_side_sections.png','eye_gray_curvature.png',
        'head_midline_section.png','head_horizontal_sections.png','reference_head_comparison.png',
        'reference_camera_candidates.png','reference_alignment.md','reference_alignment.json',
        'HEAD_GENERATION_PARAMETERS.md','head_generation_parameters.json','head_generation_source_manifest.json',
        'head_generation_code.zip','head_geometry_measurements.json','head_geometry_sections.json',
        'eye_measurements.csv','head_section_measurements.csv','independent_data_validation.json']
    assert all((OUT/f).is_file() for f in required),[f for f in required if not (OUT/f).is_file()]
    previous=json.loads((VERSION/'SHA256SUMS.json').read_text(encoding='utf-8'))
    source_integrity={n:{'expected':previous[n],'actual':digest(VERSION/n)} for n in ['Naiwa_Character.blend','Naiwa_Character.fbx']}
    assert all(v['expected']==v['actual'] for v in source_integrity.values())
    assert not list(OUT.glob('*.blend')) and not list(OUT.glob('*.fbx'))
    with zipfile.ZipFile(OUT/'head_generation_code.zip') as z:
        assert z.testzip() is None
        assert not any(n.lower().endswith(('.blend','.fbx')) for n in z.namelist())
        manifest=json.loads((OUT/'head_generation_source_manifest.json').read_text(encoding='utf-8'))
        for entry in manifest['files']:
            blob=z.read(entry['path'])
            assert hashlib.sha256(blob).hexdigest()==entry['sha256'],entry['path']
            assert digest(ROOT/Path(entry['path']).relative_to('NaiwaCharacter'))==entry['sha256'],entry['path']
        zip_entries=len(z.namelist());zip_uncompressed_bytes=sum(i.file_size for i in z.infolist())
    links=Links();links.feed((OUT/'index.html').read_text(encoding='utf-8'))
    for md in OUT.glob('*.md'):
        links.links.extend(re.findall(r'\]\(([^)]+)\)',md.read_text(encoding='utf-8')))
    checked_links=[]
    for target in links.links:
        if target.startswith('#') or urlparse(target).scheme:continue
        target=unquote(target.split('#')[0])
        # This result is written only after all checks have passed.
        if target!='diagnostic_validation.json':assert (OUT/target).exists(),target
        checked_links.append(target)
    scripts=['geometry_diagnostic_utils.py','diagnose_head_geometry.py','plot_head_geometry_diagnostics.py',
        'render_eye_geometry_diagnostic.py','render_reference_alignment.py','make_reference_head_diagnostic.py',
        'write_head_diagnostic_summary.py','build_head_diagnostic_report.py','validate_head_diagnostic_delivery.py']
    for name in scripts:ast.parse((ROOT/name).read_text(encoding='utf-8-sig'),filename=name)
    images={}
    for path in OUT.glob('*.png'):
        with Image.open(path) as im:images[path.name]=list(im.size);im.verify()
    aligned=json.loads((OUT/'reference_alignment.json').read_text(encoding='utf-8'))
    for tr in aligned['transforms']:
        assert tr['scale_X_equals_Y']
        assert abs(tr['uniform_scale']*tr['D_pixels']-150)<1e-8
    data_check=json.loads((OUT/'independent_data_validation.json').read_text(encoding='utf-8'))
    assert data_check['status']=='PASS' and not data_check['failed_checks']
    for name,expected in data_check['input_sha256'].items():assert digest(OUT/name)==expected,name
    result={'status':'passed','source_integrity':source_integrity,'no_model_output_created':True,
        'required_artifacts':required,'checked_local_links':sorted(set(checked_links)),
        'code_zip_crc_check':'passed','code_zip_manifest_hashes':'all archived and local source hashes match',
        'code_zip_entries':zip_entries,'code_zip_uncompressed_bytes':zip_uncompressed_bytes,
        'diagnostic_scripts_syntax':'passed','image_integrity_and_dimensions':images,
        'reference_scaling':'uniform XY; both displayed near-eye diameters 150 px',
        'manual_visual_QA':['eye side sections: clear labels and continuous black/green curve',
            'midline: readable crop and height markers','horizontal: same geometric scale, four measured contours',
            'gray close-ups: unchanged curved eye and separate glint visible',
            'reference composite and candidate grid: labels legible, correct facing and original aspect'],
        'independent_numeric_validation':data_check}
    (OUT/'diagnostic_validation.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    hashes={p.name:digest(p) for p in sorted(OUT.iterdir()) if p.is_file() and p.name!='SHA256SUMS.json'}
    (OUT/'SHA256SUMS.json').write_text(json.dumps(hashes,ensure_ascii=False,indent=2),encoding='utf-8')
    print('HEAD_DIAGNOSTIC_DELIVERY_VALIDATED',len(required),len(images),zip_entries)

if __name__=='__main__':main()
