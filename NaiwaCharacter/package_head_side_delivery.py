"""Snapshot the current scripts and hash the small, verified delivery set."""
import ast
import hashlib
import json
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parent
OUT = ROOT / 'versions/20261005_head_side_continuity'
SEEDS = [
    'build_naiwa.py', 'revise_head_side_continuity.py', 'head_side_continuity_sculpt.py',
    'inspect_head_side_continuity.py', 'validate_head_revision.py',
    'validate_head_side_continuity.py', 'validate_delivery_rebuild.py', 'validate_fbx.py',
    'render_head_reshape_comparison.py', 'make_head_reshape_comparisons.py',
    'package_head_side_delivery.py',
]
files, pending = set(), [ROOT / name for name in SEEDS]
while pending:
    path = pending.pop()
    if path in files:
        continue
    tree = ast.parse(path.read_text(encoding='utf-8-sig'), filename=str(path))
    files.add(path)
    for node in ast.walk(tree):
        modules = [alias.name for alias in node.names] if isinstance(node, ast.Import) else \
                  [node.module] if isinstance(node, ast.ImportFrom) and node.module else []
        for module in modules:
            dependency = ROOT / (module.split('.')[0] + '.py')
            if dependency.is_file() and dependency not in files:
                pending.append(dependency)
files.update([ROOT / 'run_blender_stage.ps1', OUT / 'baseline_dimensions.json'])

digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
base = ROOT / 'versions/20261005_head_reshape/Naiwa_Character.blend'
record = json.loads((OUT / 'baseline_dimensions.json').read_text(encoding='utf-8'))
assert digest(base) == record['source_sha256'], 'Pinned original changed'
assert json.loads((OUT / 'head_side_continuity_validation.json').read_text())['independent_validation'] == 'passed'
assert json.loads((OUT / 'rebuild_validation.json').read_text())['independent_validation'] == 'passed'
manifest = {
    'fixed_source_blend': str(base), 'fixed_source_sha256': digest(base),
    'default_generator': 'NaiwaCharacter/build_naiwa.py',
    'scripts_and_record': {str(path.relative_to(ROOT)).replace('\\', '/'): digest(path)
                           for path in sorted(files)},
}
archive = OUT / 'generation_scripts.zip'
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as bundle:
    for path in sorted(files):
        bundle.write(path, 'NaiwaCharacter/' + path.relative_to(ROOT).as_posix())
with zipfile.ZipFile(archive) as bundle:
    assert bundle.testzip() is None
(OUT / 'generation_source_manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
sums = {path.name: digest(path) for path in sorted(OUT.iterdir())
        if path.is_file() and path.name != 'SHA256SUMS.json'}
(OUT / 'SHA256SUMS.json').write_text(json.dumps(sums, indent=2), encoding='utf-8')
print('HEAD_SIDE_DELIVERY_PACKAGE_OK', len(files), 'source files;', len(sums), 'delivery files')
