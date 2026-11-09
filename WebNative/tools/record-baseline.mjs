import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import { readFile, stat, writeFile, mkdir } from 'node:fs/promises';
import { dirname, resolve, relative, sep } from 'node:path';
import { fileURLToPath } from 'node:url';

const scriptPath = fileURLToPath(import.meta.url);
const webRoot = resolve(dirname(scriptPath), '..');
const repoRoot = resolve(webRoot, '..');
const outputPath = resolve(webRoot, 'docs/baseline-v1.json');

const inputPaths = [
  'UE5/NaiwaP0/Nai_Long.uproject',
  'UE5/Scripts/p1_integrated_v7_config.json',
  'UE5/NaiwaP0/Content/Naiwa/P1/Maps/L_NaiwaP1_v7.umap',
  'UE5/NaiwaP0/Source/NaiwaRuntime/NaiwaP1GameMode.cpp',
  'UE5/NaiwaP0/Source/NaiwaRuntime/NaiwaP1GameMode.h',
  'UE5/NaiwaP0/Source/NaiwaRuntime/NaiwaP1PlayerController.cpp',
  'UE5/NaiwaP0/Source/NaiwaRuntime/NaiwaP1SwitchPlatform.cpp',
  'UE5/NaiwaP0/Source/NaiwaRuntime/NaiwaP2SceneLibrary.cpp',
  'UE5/NaiwaP0/Source/NaiwaRuntime/NaiwaP2SceneLibrary.h',
  'UE5/NaiwaP0/Source/NaiwaRuntime/NaiwaP2Types.h',
  'UE5/NaiwaP0/Source/NaiwaRuntime/NaiwaP2PhysicsProfile.h',
  'UE5/NaiwaP0/Source/NaiwaRuntime/NaiwaP2ProjectileDriver.h',
  'UE5/NaiwaP0/Source/NaiwaRuntime/NaiwaP2Ballistics.cpp',
  'UE5/NaiwaP0/Source/NaiwaRuntime/NaiwaP2Ballistics.h',
  'UE5/NaiwaP0/Source/NaiwaRuntime/NaiwaP2GameMode.cpp',
  'UE5/Records/p2_probe_fixtures/p2_ballistics_fixture.json',
  'UE5/Records/p2_probe_fixtures/p2_design_fixture.json',
  'UE5/NaiwaP0/Source/NaiwaRuntime/NaiwaP3M2SeededMap.cpp',
  'UE5/NaiwaP0/Source/NaiwaRuntime/NaiwaP3M2SeededMap.h',
  'UE5/NaiwaP0/Source/NaiwaRuntime/NaiwaP3M2LevelSession.cpp',
  'UE5/NaiwaP0/Source/NaiwaRuntime/NaiwaP3M2LevelSession.h',
  'UE5/NaiwaP0/Source/NaiwaRuntime/NaiwaP3M2Settings.cpp',
  'UE5/NaiwaP0/Source/NaiwaRuntime/NaiwaP3M2Settings.h',
  'UE5/Records/p3_formal/design_reference/P3_关卡设计参数_v1.json',
  'UE5/Records/p3_formal/formal_map_build_report.json',
  'UE5/NaiwaP0/Source/NaiwaRuntime/NaiwaMenuProgress.cpp',
  'UE5/NaiwaP0/Source/NaiwaRuntime/NaiwaMenuProgress.h',
  'NaiwaCharacter/versions/20261007_H2_E2_overall_refinement/candidate_E2/Naiwa_Character.blend',
  'NaiwaCharacter/versions/20261007_H2_E2_overall_refinement/candidate_E2/Naiwa_Character.fbx',
  'NaiwaCharacter/versions/20261007_H2_E2_overall_refinement/candidate_E2/fbx_validation.json',
  'NaiwaCharacter/deliveries/NaiwaEgg_S4-S5_v3_candidate/SK_NaiwaEgg_S4-S5.blend',
  'NaiwaCharacter/deliveries/NaiwaEgg_S4-S5_v3_candidate/SK_NaiwaEgg.fbx',
  'NaiwaCharacter/deliveries/NaiwaEgg_S4-S5_v3_candidate/NaiwaEgg_S4-S5_ue_import_report.json',
  'UE5/NaiwaP0/Content/Naiwa/P2/Art/Egg/SK_NaiwaEgg.uasset',
];

async function sha256File(path) {
  const bytes = await readFile(path);
  return createHash('sha256').update(bytes).digest('hex');
}

function gitOutput(args) {
  return execFileSync('git', args, { cwd: repoRoot, encoding: 'utf8' }).trim();
}

const sourceFiles = [];
for (const repoPath of inputPaths) {
  const absolutePath = resolve(repoRoot, repoPath);
  try {
    const fileStat = await stat(absolutePath);
    sourceFiles.push({
      path: repoPath.replaceAll(sep, '/'),
      bytes: fileStat.size,
      sha256: await sha256File(absolutePath),
      status: 'present',
    });
  } catch (error) {
    if (error.code !== 'ENOENT') throw error;
    sourceFiles.push({ path: repoPath.replaceAll(sep, '/'), status: 'missing' });
  }
}

const workingTreeLines = gitOutput(['status', '--porcelain=v1', '--untracked-files=all'])
  .split(/\r?\n/)
  .filter(Boolean)
  .filter((line) => !line.slice(3).replaceAll('\\', '/').startsWith('WebNative/'))
  .sort();
const workingTreeText = workingTreeLines.join('\n');
const project = JSON.parse(await readFile(resolve(repoRoot, 'UE5/NaiwaP0/Nai_Long.uproject'), 'utf8'));
const engineEvidencePath = 'UE5/Records/build_graybox.json';
const engineEvidence = JSON.parse(await readFile(resolve(repoRoot, engineEvidencePath), 'utf8'));

const baseline = {
  schemaVersion: 1,
  capturedAt: new Date().toISOString(),
  git: {
    head: gitOutput(['rev-parse', 'HEAD']),
    workingTree: workingTreeLines.length === 0 ? 'clean' : 'dirty',
    changedPathCountExcludingWebNative: workingTreeLines.length,
    statusSha256ExcludingWebNative: createHash('sha256').update(workingTreeText).digest('hex'),
  },
  unreal: {
    projectEngineAssociation: project.EngineAssociation,
    reportedVersion: engineEvidence.engine,
    versionEvidencePath: engineEvidencePath,
  },
  decisions: {
    playerCharacter: 'E2 model at the versioned candidate path; verify asset manifest and public-use rights before release.',
    baseEgg: 'S4-S5 v3 FBX has a PASS Unreal import report and is the source for the imported runtime SK_NaiwaEgg mesh.',
    p2Ballistics: 'Existing p2_ballistics_fixture.json is retained as historical evidence; its 0.004 s integration contract is not assumed to match the current projectile driver.',
  },
  sourceFiles,
};

await mkdir(dirname(outputPath), { recursive: true });
await writeFile(outputPath, `${JSON.stringify(baseline, null, 2)}\n`, 'utf8');

const missing = sourceFiles.filter((entry) => entry.status === 'missing').map((entry) => entry.path);
process.stdout.write(JSON.stringify({
  output: relative(repoRoot, outputPath).replaceAll(sep, '/'),
  head: baseline.git.head,
  workingTree: baseline.git.workingTree,
  changedPathCountExcludingWebNative: baseline.git.changedPathCountExcludingWebNative,
  sourceFiles: sourceFiles.length,
  missing,
}, null, 2));
