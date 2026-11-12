import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const kitDir = path.join(repoRoot, 'UE5/SourceAssets/P6/Environment/A1_CourtyardKit');
const modelDir = path.join(kitDir, 'Models');
const sourceObj = path.join(modelDir, 'P6_A1_CourtyardKit_Blockout_v03.obj');
const sourceMtl = path.join(modelDir, 'P6_A1_CourtyardKit_Blockout_v03.mtl');
const targetObj = path.join(modelDir, 'P6_A1_CourtyardKit_Blockout_v04.obj');
const targetMtl = path.join(modelDir, 'P6_A1_CourtyardKit_Blockout_v04.mtl');

const baseObj = fs.readFileSync(sourceObj, 'utf8')
  .replace('P6_A1_CourtyardKit_Blockout_v03.mtl', 'P6_A1_CourtyardKit_Blockout_v04.mtl')
  .replace('Refined v03 source blockout;', 'Refined v04 source blockout;');
let nextVertex = (baseObj.match(/^v\s/gm) || []).length;
const detail = [];
const fmt = n => Number(n.toFixed(3)).toString();
const addVertex = p => { detail.push(`v ${fmt(p[0])} ${fmt(p[1])} ${fmt(p[2])}`); return ++nextVertex; };
const addFace = ids => detail.push(`f ${ids.join(' ')}`);
const startObject = (name, material) => detail.push(`o ${name}`, `usemtl ${material}`);

function box(cx, cy, cz, sx, sy, sz, yaw = 0) {
  const start = nextVertex + 1;
  const a = yaw * Math.PI / 180, co = Math.cos(a), si = Math.sin(a);
  const local = [[-.5,-.5,-.5],[.5,-.5,-.5],[.5,.5,-.5],[-.5,.5,-.5],[-.5,-.5,.5],[.5,-.5,.5],[.5,.5,.5],[-.5,.5,.5]];
  for (const [x,y,z] of local) addVertex([cx+x*sx*co-y*sy*si, cy+x*sx*si+y*sy*co, cz+z*sz]);
  for (const face of [[0,3,2,1],[4,5,6,7],[0,1,5,4],[1,2,6,5],[2,3,7,6],[3,0,4,7]]) addFace(face.map(i => start + i));
}

function sphere(name, material, cx, cy, cz, rx, ry, rz) {
  startObject(name, material);
  const sides = 8, rings = 4, start = nextVertex + 1;
  for (let r=0; r<=rings; r++) {
    const phi = -Math.PI/2 + Math.PI*r/rings;
    for (let i=0; i<sides; i++) {
      const theta = 2*Math.PI*i/sides;
      addVertex([cx+rx*Math.cos(phi)*Math.cos(theta), cy+ry*Math.cos(phi)*Math.sin(theta), cz+rz*Math.sin(phi)]);
    }
  }
  for (let r=0; r<rings; r++) for (let i=0; i<sides; i++) {
    const j=(i+1)%sides;
    addFace([start+r*sides+i,start+r*sides+j,start+(r+1)*sides+j,start+(r+1)*sides+i]);
  }
}

// Five framed reliefs make the broad stage fascia read as built timber, not one box.
for (const [index, x] of [-2330,-1890,-1450,-1010,-570].entries()) {
  const name = `P6_Stage_Fascia_Relief_${String(index+1).padStart(2,'0')}`;
  startObject(name, 'TimberEdge'); box(x, 705, 352, 304, 20, 58);
  startObject(`${name}_Inlay`, 'WoodGrainLight');
  box(x-132, 691, 352, 3, 3, 48); box(x+132, 691, 352, 3, 3, 48);
  box(x, 691, 375, 260, 3, 3); box(x, 691, 329, 260, 3, 3);
  box(x, 689, 352, 26, 3, 26, 45);
  sphere(`${name}_Boss`, 'Brass', x, 687, 352, 8, 4, 8);
  for (const side of [-1,1]) sphere(`${name}_Stud_${side}`, 'Brass', x+side*115, 687, 352, 3.5, 3, 3.5);
}

// Fine grain lines sit inside the stage boards, between the existing transverse seams.
startObject('P6_Stage_Deck_Grain_Lines', 'WoodGrainDark');
for (let board=0; board<11; board++) {
  const y = 870 + board*105;
  for (let streak=0; streak<3; streak++) {
    const x = -2230 + ((board*197 + streak*311) % 1640);
    const length = 150 + ((board*83 + streak*61) % 240);
    box(x, y-22+streak*18, 224, length, 2.4, 1.5);
  }
}
startObject('P6_Stage_Deck_Grain_Highlights', 'WoodGrainLight');
for (let board=0; board<11; board++) {
  const y = 888 + board*105;
  const x = -2120 + ((board*271) % 1390);
  box(x, y, 224, 190 + (board%4)*46, 1.8, 1.1);
}

// Add restrained strap hinges and latch plates to the existing double-leaf door.
startObject('P6_House_Door_Ironwork', 'IronWarm');
for (const x of [1642,1760,1784,1902]) {
  for (const z of [188,398,608]) {
    box(x, 968, z, 22, 8, 58);
  }
}
for (const x of [1642,1760,1784,1902]) for (const z of [168,418,588,628]) {
  sphere(`P6_House_Door_Rivet_${x}_${z}`, 'Brass', x, 963, z, 3.8, 3.2, 3.8);
}
startObject('P6_House_Window_Latches', 'IronWarm');
for (const x of [1000,2500]) {
  box(x, 946, 647, 36, 8, 42);
  box(x, 939, 647, 5, 8, 31);
  sphere(`P6_House_Window_Latch_Knob_${x}`, 'Brass', x, 936, 647, 6, 4, 6);
}

const extraMaterials = `
newmtl WoodGrainDark
Kd 0.24 0.14 0.09
Ka 0.08 0.05 0.04
Ns 8

newmtl WoodGrainLight
Kd 0.55 0.36 0.20
Ka 0.21 0.13 0.07
Ns 12

newmtl IronWarm
Kd 0.42 0.29 0.16
Ka 0.16 0.10 0.05
Ns 30
`;
let mtlText = `${fs.readFileSync(sourceMtl, 'utf8').trimEnd()}\n${extraMaterials.trimStart()}`;
for (const [name, values] of Object.entries({
  Timber: [0.39,0.25,0.17],
  RoofTile: [0.32,0.39,0.47],
  RoofTileLight: [0.40,0.47,0.54],
  RoofTileShade: [0.26,0.33,0.41],
  RoofTileGlaze: [0.43,0.49,0.56],
})) {
  const materialStart = mtlText.indexOf(`newmtl ${name}`);
  if (materialStart < 0) throw new Error(`Missing material to retune: ${name}`);
  const kdStart = mtlText.indexOf('Kd ', materialStart);
  const lineEnd = mtlText.indexOf('\n', kdStart);
  if (kdStart < 0 || lineEnd < 0) throw new Error(`Missing Kd row for ${name}`);
  mtlText = `${mtlText.slice(0,kdStart)}Kd ${values.join(' ')}${mtlText.slice(lineEnd)}`;
}
const objText = `${baseObj.trimEnd()}\n# v04 adds stage fascia relief, fine board grain, door strap hinges and window latches.\n${detail.join('\n')}\n`;
fs.writeFileSync(targetObj, objText, 'ascii');
fs.writeFileSync(targetMtl, mtlText, 'ascii');
const lines = objText.split(/\r?\n/);
const points = lines.filter(line => line.startsWith('v ')).map(line => line.split(/\s+/).slice(1).map(Number));
const bounds = [0,1,2].map(axis => [Math.min(...points.map(p => p[axis])), Math.max(...points.map(p => p[axis]))]);
console.log(JSON.stringify({
  obj: path.relative(repoRoot, targetObj).replaceAll('\\','/'),
  mtl: path.relative(repoRoot, targetMtl).replaceAll('\\','/'),
  object_groups: lines.filter(line => line.startsWith('o ')).length,
  vertices: points.length,
  faces: lines.filter(line => line.startsWith('f ')).length,
  materials: (mtlText.match(/^newmtl /gm) || []).length,
  added_groups: ['5 stage fascia reliefs', '44 deck-grain strokes/highlights', 'door strap hinges/rivets', '2 window latches'],
  preview_palette: 'raised timber and slate-roof base tones for clearer midtone separation',
  bounds_cm: {x:bounds[0],y:bounds[1],z:bounds[2]},
  m2_anchors: 'unchanged; additive detail only'
}, null, 2));
