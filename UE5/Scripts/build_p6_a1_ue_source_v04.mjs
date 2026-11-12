import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(scriptDir, '../..');
const modelDir = path.join(root, 'UE5/SourceAssets/P6/Environment/A1_CourtyardKit/Models');
const sourceObj = path.join(modelDir, 'P6_A1_CourtyardKit_Blockout_v04.obj');
const sourceMtl = path.join(modelDir, 'P6_A1_CourtyardKit_Blockout_v04.mtl');
const outputObj = path.join(modelDir, 'P6_A1_CourtyardKit_v04_UECombined.obj');
const outputMtl = path.join(modelDir, 'P6_A1_CourtyardKit_v04_UECombined.mtl');

const lines = fs.readFileSync(sourceObj, 'utf8').split(/\r?\n/);
let objectCount = 0;
let vertices = 0;
let faces = 0;
const materials = new Set();
const vertexLines = [];
const vertexPositions = [];
const facesByMaterial = new Map();
const comments = [];
let currentMaterial = null;
for (const line of lines) {
  if (/^o\s+/.test(line)) {
    objectCount++;
    continue;
  }
  const material = line.match(/^usemtl\s+(.+)$/);
  if (material) {
    currentMaterial = material[1].trim();
    materials.add(currentMaterial);
    if (!facesByMaterial.has(currentMaterial)) facesByMaterial.set(currentMaterial, []);
    continue;
  }
  if (/^v\s+/.test(line)) {
    const values = line.trim().split(/\s+/).slice(1).map(Number);
    if (values.length < 3 || values.slice(0, 3).some(value => !Number.isFinite(value))) {
      throw new Error(`Invalid OBJ vertex row: ${line}`);
    }
    const [x, y, z] = values;
    vertexPositions.push([x, y, z]);
    vertices++;
    // OBJ is right-handed; UE's OBJ translator flips Y. Pre-negate here so
    // the imported UE mesh remains aligned to P6's authored M2 XY coordinates.
    vertexLines.push(`v ${x} ${-y} ${z}`);
    continue;
  }
  if (/^f\s+/.test(line)) {
    if (!currentMaterial) throw new Error(`OBJ face has no active material: ${line}`);
    const tokens = line.trim().split(/\s+/).slice(1);
    if (tokens.length < 3) throw new Error(`Invalid OBJ face row: ${line}`);
    const indexed = tokens.map(token => {
      const match = token.match(/^(\d+)$/);
      if (!match) throw new Error(`Unexpected OBJ face index form: ${token}`);
      const index = Number(match[1]);
      if (index < 1 || index > vertexPositions.length) {
        throw new Error(`OBJ face references missing vertex: ${token}`);
      }
      return `${index}/${index}`;
    });
    facesByMaterial.get(currentMaterial).push(`f ${indexed.join(' ')}`);
    faces++;
    continue;
  }
  if (line.startsWith('#')) comments.push(line);
}
if (objectCount < 1 || vertices < 1000 || faces < 1000 || materials.size < 10) {
  throw new Error(`Unexpected v04 source structure: objects=${objectCount}, vertices=${vertices}, faces=${faces}, materials=${materials.size}`);
}
const xs = vertexPositions.map(vertex => vertex[0]);
const ys = vertexPositions.map(vertex => vertex[1]);
const minX = Math.min(...xs), maxX = Math.max(...xs);
const minY = Math.min(...ys), maxY = Math.max(...ys);
const rangeX = Math.max(maxX - minX, 1);
const rangeY = Math.max(maxY - minY, 1);
let vertexIndex = 0;
const withPreviewUVs = [
  'mtllib P6_A1_CourtyardKit_v04_UECombined.mtl',
  ...comments,
  '# Faces are batched by used material to reduce UE mesh sections; source order remains in the canonical OBJ.',
  '# OBJ Y is pre-negated to cancel the UE translator handedness conversion and preserve authored P6 coordinates.',
  'o P6_A1_CourtyardKit_v04_UECombined',
];
for (const line of vertexLines) {
  withPreviewUVs.push(line);
  const [x, y] = vertexPositions[vertexIndex++];
  const u = (x - minX) / rangeX;
  const v = (y - minY) / rangeY;
  withPreviewUVs.push(`vt ${u.toFixed(6)} ${v.toFixed(6)}`);
}
for (const [material, materialFaces] of facesByMaterial) {
  if (!materialFaces.length) continue;
  withPreviewUVs.push(`usemtl ${material}`);
  withPreviewUVs.push(...materialFaces);
}
fs.writeFileSync(outputObj, withPreviewUVs.join('\n'), 'utf8');
fs.copyFileSync(sourceMtl, outputMtl);
console.log(JSON.stringify({
  status: 'built',
  source: path.relative(root, sourceObj).replaceAll('\\', '/'),
  output: path.relative(root, outputObj).replaceAll('\\', '/'),
  original_object_count: objectCount,
  merged_object_count: 1,
  material_batches: facesByMaterial.size,
  material_sections_expected: materials.size,
  axis_policy: 'pre-negated OBJ Y to cancel the UE OBJ translator Y flip; canonical source coordinates remain unchanged',
  vertices,
  preview_uvs: vertexPositions.length,
  uv_policy: 'generated planar XY import-preview UVs only; not final unwrap',
  faces,
  materials: [...materials].length,
  source_mtl: path.relative(root, sourceMtl).replaceAll('\\', '/'),
  output_mtl: path.relative(root, outputMtl).replaceAll('\\', '/'),
}, null, 2));
