import { createHash } from 'node:crypto';
import { readFile, stat, writeFile, mkdir } from 'node:fs/promises';
import { dirname, resolve, relative, sep } from 'node:path';
import { fileURLToPath } from 'node:url';

const webRoot = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const repoRoot = resolve(webRoot, '..');
const baselinePath = resolve(webRoot, 'docs/baseline-v1.json');
const outputPath = resolve(webRoot, 'public/assets/asset-manifest.json');
const baseline = JSON.parse(await readFile(baselinePath, 'utf8'));

const choices = [
  {
    id: 'naiwa-character-e2',
    sourcePath: 'UE5/NaiwaP0/Content/Naiwa/Gameplay/SK_Naiwa_Elastic.uasset',
    glbPath: 'WebNative/public/assets/naiwa-character-e2.glb.gz',
    role: 'UE P1 gameplay character mesh (gzip-compressed glTF binary)',
  },
  {
    id: 'naiwa-egg-base-s4-s5-v3',
    sourcePath: 'UE5/NaiwaP0/Content/Naiwa/P2/Art/Egg/SK_NaiwaEgg.uasset',
    glbPath: 'WebNative/public/assets/naiwa-egg-base.glb',
    role: 'UE P2 base egg SkeletalMesh; exported after the source FBX passed UE import validation',
  },
];

choices.push({
  id: 'naiwa-run-animation',
  sourcePath: 'UE5/NaiwaP0/Content/Naiwa/Gameplay/AS_Naiwa_Run.uasset',
  glbPath: 'WebNative/public/assets/naiwa-run.glb.gz',
  role: 'UE P1 gameplay run animation (gzip-compressed glTF binary)',
});

const assets = [];
for (const choice of choices) {
  const source = resolve(repoRoot, choice.sourcePath);
  const glb = resolve(repoRoot, choice.glbPath);
  const baselineEntry = baseline.sourceFiles.find((entry) => entry.path === choice.sourcePath);
  let sourceSha256 = baselineEntry?.sha256 ?? null;
  if (!sourceSha256) {
    try { sourceSha256 = createHash('sha256').update(await readFile(source)).digest('hex'); }
    catch (error) { if (error.code !== 'ENOENT') throw error; }
  }
  let output = { status: 'pending-export' };
  try {
    const fileStat = await stat(glb);
    output = {
      status: 'present',
      path: relative(webRoot, glb).replaceAll(sep, '/'),
      bytes: fileStat.size,
      sha256: createHash('sha256').update(await readFile(glb)).digest('hex'),
      container: glb.endsWith('.gz') ? 'gzip' : 'glb',
    };
  } catch (error) {
    if (error.code !== 'ENOENT') throw error;
  }
  assets.push({
    id: choice.id,
    role: choice.role,
    sourcePath: choice.sourcePath,
    sourceSha256,
    glb: output,
  });
}

const manifest = {
  schemaVersion: 1,
  createdAt: new Date().toISOString(),
  sourceBaseline: 'docs/baseline-v1.json',
  unrealEngine: baseline.unreal.reportedVersion,
  assets,
};
await mkdir(dirname(outputPath), { recursive: true });
await writeFile(outputPath, `${JSON.stringify(manifest, null, 2)}\n`, 'utf8');
process.stdout.write(JSON.stringify({ output: relative(repoRoot, outputPath).replaceAll(sep, '/'), assets: assets.map(({ id, glb }) => ({ id, status: glb.status })) }, null, 2));
