import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const modelDir = path.join(repoRoot, 'UE5/SourceAssets/P6/Environment/A1_CourtyardKit/Models');
const inputObj = path.join(modelDir, 'P6_A1_CourtyardKit_Blockout_v04.obj');
const inputMtl = path.join(modelDir, 'P6_A1_CourtyardKit_Blockout_v04.mtl');
const outputObj = path.join(modelDir, 'P6_A1_CourtyardKit_Blockout_v05_M2Camera.obj');
const outputMtl = path.join(modelDir, 'P6_A1_CourtyardKit_Blockout_v05_M2Camera.mtl');

if (fs.existsSync(outputObj) || fs.existsSync(outputMtl)) {
  throw new Error('Refusing to overwrite an existing v05 M2 camera candidate.');
}

const lines = fs.readFileSync(inputObj, 'utf8').split(/\r?\n/);
let currentObject = '';
let objectCount = 0;
let vertexCount = 0;
let faceCount = 0;
const changes = { house_y_offset_cm: -2250, stage_y_offset_cm: -2050, tree_y_offset_cm: -450 };
const transformCounts = { house_vertices: 0, stage_vertices: 0, tree_vertices: 0 };
const transformed = lines.map(line => {
  if (line.startsWith('o ')) {
    currentObject = line.slice(2).trim();
    objectCount++;
    return line;
  }
  if (line.startsWith('f ')) faceCount++;
  if (!line.startsWith('v ')) return line;
  const values = line.trim().split(/\s+/).slice(1).map(Number);
  if (values.length < 3 || values.slice(0, 3).some(value => !Number.isFinite(value))) {
    throw new Error(`Invalid vertex row in ${currentObject}: ${line}`);
  }
  let offset = 0;
  if (currentObject.startsWith('P6_House_')) {
    offset = changes.house_y_offset_cm;
    transformCounts.house_vertices++;
  } else if (currentObject.startsWith('P6_Stage_')) {
    offset = changes.stage_y_offset_cm;
    transformCounts.stage_vertices++;
  } else if (currentObject.startsWith('P6_Tree_')) {
    offset = changes.tree_y_offset_cm;
    transformCounts.tree_vertices++;
  }
  vertexCount++;
  if (!offset) return line;
  values[1] += offset;
  return `v ${values.map(value => Number(value.toFixed(3))).join(' ')}`;
});

if (objectCount !== 1523 || vertexCount !== 54944 || faceCount !== 49534) {
  throw new Error(`Unexpected canonical v04 geometry: objects=${objectCount}, vertices=${vertexCount}, faces=${faceCount}`);
}
if (transformCounts.house_vertices < 500 || transformCounts.stage_vertices < 500 || transformCounts.tree_vertices < 1000) {
  throw new Error(`Expected environment prefixes were not found: ${JSON.stringify(transformCounts)}`);
}

const outputLines = transformed.map(line => line.replace(
  'P6_A1_CourtyardKit_Blockout_v04.mtl',
  'P6_A1_CourtyardKit_Blockout_v05_M2Camera.mtl',
));
outputLines.splice(1, 0,
  '# v05 M2 camera composition candidate; canonical v04 remains untouched.',
  '# House/stage moved behind the evidence yard; tree shifted away from Noticeboard.',
);
fs.writeFileSync(outputObj, outputLines.join('\n'), 'utf8');
fs.copyFileSync(inputMtl, outputMtl);

const sha256 = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const report = {
  status: 'built',
  source: path.relative(repoRoot, inputObj).replaceAll('\\', '/'),
  output: path.relative(repoRoot, outputObj).replaceAll('\\', '/'),
  output_mtl: path.relative(repoRoot, outputMtl).replaceAll('\\', '/'),
  source_sha256: sha256(inputObj),
  output_sha256: sha256(outputObj),
  object_count: objectCount,
  vertex_count: vertexCount,
  face_count: faceCount,
  transforms_cm: changes,
  transformed_vertex_counts: transformCounts,
  coordinates_changed: ['P6_House_*', 'P6_Stage_*', 'P6_Tree_*'],
  unchanged: 'point-map actors, courtyard boundaries, canonical v04 source, M2 gameplay map',
};
const reportPath = path.join(modelDir, 'P6_A1_CourtyardKit_Blockout_v05_M2Camera_build.json');
if (fs.existsSync(reportPath)) throw new Error('Refusing to overwrite an existing v05 build report.');
fs.writeFileSync(reportPath, JSON.stringify(report, null, 2) + '\n', 'utf8');
console.log(JSON.stringify(report, null, 2));
