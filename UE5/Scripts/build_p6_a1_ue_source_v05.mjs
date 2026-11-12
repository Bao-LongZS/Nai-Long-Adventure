import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const modelDir = path.join(root, 'UE5/SourceAssets/P6/Environment/A1_CourtyardKit/Models');
const sourceObj = path.join(modelDir, 'P6_A1_CourtyardKit_Blockout_v05_M2Camera.obj');
const sourceMtl = path.join(modelDir, 'P6_A1_CourtyardKit_Blockout_v05_M2Camera.mtl');
const outputObj = path.join(modelDir, 'P6_A1_CourtyardKit_v05_M2Camera_UECombined.obj');
const outputMtl = path.join(modelDir, 'P6_A1_CourtyardKit_v05_M2Camera_UECombined.mtl');
for (const file of [outputObj, outputMtl]) if (fs.existsSync(file)) throw new Error(`Refusing to overwrite ${file}`);

const lines = fs.readFileSync(sourceObj, 'utf8').split(/\r?\n/);
const vertices = [];
const facesByMaterial = new Map();
const materials = new Set();
let currentMaterial = '';
let objects = 0;
let faces = 0;
const comments = [];
for (const line of lines) {
  if (line.startsWith('o ')) { objects++; continue; }
  const vertex = line.match(/^v\s+([\d.-]+)\s+([\d.-]+)\s+([\d.-]+)/);
  if (vertex) {
    const [x, y, z] = vertex.slice(1).map(Number);
    vertices.push([x, y, z]);
    continue;
  }
  const material = line.match(/^usemtl\s+(.+)$/);
  if (material) {
    currentMaterial = material[1].trim();
    materials.add(currentMaterial);
    if (!facesByMaterial.has(currentMaterial)) facesByMaterial.set(currentMaterial, []);
    continue;
  }
  if (line.startsWith('f ')) {
    if (!currentMaterial) throw new Error(`Face without material: ${line}`);
    const tokens = line.trim().split(/\s+/).slice(1);
    const indexed = tokens.map(token => {
      const match = token.match(/^(\d+)$/);
      if (!match) throw new Error(`Unexpected OBJ face index: ${token}`);
      const index = Number(match[1]);
      if (index < 1 || index > vertices.length) throw new Error(`Out-of-range OBJ face index: ${token}`);
      return `${index}/${index}`;
    });
    facesByMaterial.get(currentMaterial).push(`f ${indexed.join(' ')}`);
    faces++;
    continue;
  }
  if (line.startsWith('#')) comments.push(line);
}
if (objects !== 1523 || vertices.length !== 54944 || faces !== 49534 || materials.size !== 41) {
  throw new Error(`Unexpected v05 geometry: objects=${objects}, vertices=${vertices.length}, faces=${faces}, materials=${materials.size}`);
}
const bounds = [0, 1, 2].map(axis => [
  Math.min(...vertices.map(vertex => vertex[axis])),
  Math.max(...vertices.map(vertex => vertex[axis])),
]);
const rangeX = Math.max(1, bounds[0][1] - bounds[0][0]);
const rangeY = Math.max(1, bounds[1][1] - bounds[1][0]);
const out = [
  'mtllib P6_A1_CourtyardKit_v05_M2Camera_UECombined.mtl',
  ...comments,
  '# Material-batched camera composition review mesh; canonical v04 groups were preserved before batching.',
  '# OBJ Y is pre-negated to cancel the UE OBJ translator handedness conversion.',
  'o P6_A1_CourtyardKit_v05_M2Camera_UECombined',
];
for (const [x, y, z] of vertices) {
  out.push(`v ${x} ${-y} ${z}`);
  const u = (x - bounds[0][0]) / rangeX;
  const v = (y - bounds[1][0]) / rangeY;
  out.push(`vt ${u.toFixed(6)} ${v.toFixed(6)}`);
}
for (const [material, materialFaces] of facesByMaterial) {
  if (materialFaces.length) out.push(`usemtl ${material}`, ...materialFaces);
}
fs.writeFileSync(outputObj, out.join('\n'), 'utf8');
fs.copyFileSync(sourceMtl, outputMtl);
const report = {
  status: 'built',
  source_obj: path.relative(root, sourceObj).replaceAll('\\', '/'),
  output_obj: path.relative(root, outputObj).replaceAll('\\', '/'),
  original_object_groups: objects,
  imported_object_groups: 1,
  vertex_count: vertices.length,
  preview_uv_count: vertices.length,
  face_count: faces,
  used_material_count: materials.size,
  bounds_xyz_cm: bounds,
  policy: 'Review-only static mesh; generated planar UVs are preview-only, not final unwrap.',
};
const reportPath = path.join(modelDir, 'P6_A1_CourtyardKit_v05_M2Camera_UECombined_build.json');
if (fs.existsSync(reportPath)) throw new Error(`Refusing to overwrite ${reportPath}`);
fs.writeFileSync(reportPath, JSON.stringify(report, null, 2) + '\n', 'utf8');
console.log(JSON.stringify(report, null, 2));
