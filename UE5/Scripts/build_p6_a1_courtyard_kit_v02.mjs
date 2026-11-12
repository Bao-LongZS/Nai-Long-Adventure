import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const kitDir = path.join(repoRoot, 'UE5/SourceAssets/P6/Environment/A1_CourtyardKit');
const modelDir = path.join(kitDir, 'Models');
const sourceObj = path.join(modelDir, 'P6_A1_CourtyardKit_Blockout_v01.obj');
const sourceMtl = path.join(modelDir, 'P6_A1_CourtyardKit_Blockout_v01.mtl');
const targetObj = path.join(modelDir, 'P6_A1_CourtyardKit_Blockout_v02.obj');
const targetMtl = path.join(modelDir, 'P6_A1_CourtyardKit_Blockout_v02.mtl');
const baseObj = fs.readFileSync(sourceObj, 'utf8')
  .replace('P6_A1_CourtyardKit_Blockout_v01.mtl', 'P6_A1_CourtyardKit_Blockout_v02.mtl')
  .replace('Geometry is an A1 modular art blockout, not final collision or gameplay truth.', 'Refined v02 source blockout; layered detail only, not final collision or gameplay truth.');
const baseMtl = fs.readFileSync(sourceMtl, 'utf8');

const extraMaterials = `
newmtl Paving
Kd 0.44 0.43 0.40
Ka 0.18 0.17 0.15
Ns 8

newmtl PavingAlt
Kd 0.41 0.41 0.39
Ka 0.16 0.16 0.15
Ns 8

newmtl StoneWarm
Kd 0.53 0.49 0.42
Ka 0.19 0.17 0.15
Ns 10

newmtl Mortar
Kd 0.33 0.32 0.29
Ka 0.12 0.12 0.11
Ns 5

newmtl RoofTile
Kd 0.25 0.32 0.40
Ka 0.09 0.12 0.16
Ns 18

newmtl RoofTileLight
Kd 0.31 0.38 0.45
Ka 0.12 0.14 0.17
Ns 18

newmtl RoofTileShade
Kd 0.19 0.26 0.34
Ka 0.07 0.10 0.13
Ns 14

newmtl TimberEdge
Kd 0.46 0.29 0.18
Ka 0.17 0.10 0.06
Ns 16

newmtl CurtainGold
Kd 0.78 0.53 0.24
Ka 0.29 0.19 0.08
Ns 30

newmtl Iron
Kd 0.16 0.19 0.20
Ka 0.06 0.07 0.08
Ns 35
`;

const vertices = (baseObj.match(/^v\s/gm) || []).length;
let nextVertex = vertices;
const detail = [];
const fmt = n => Number(n.toFixed(3)).toString();
const addVertex = p => {
  detail.push(`v ${fmt(p[0])} ${fmt(p[1])} ${fmt(p[2])}`);
  return ++nextVertex;
};
const addFace = ids => detail.push(`f ${ids.join(' ')}`);
const startObject = (name, material) => {
  detail.push(`o ${name}`, `usemtl ${material}`);
};
const add = (a, b) => a.map((n, i) => n + b[i]);
const sub = (a, b) => a.map((n, i) => n - b[i]);
const scale = (a, k) => a.map(n => n * k);
const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
const normalize = a => scale(a, 1 / (Math.hypot(...a) || 1));

function boxGeometry(cx, cy, cz, sx, sy, sz, rotation = 0) {
  const start = nextVertex + 1;
  const angle = rotation * Math.PI / 180;
  const c = Math.cos(angle), s = Math.sin(angle);
  const local = [
    [-.5,-.5,-.5], [.5,-.5,-.5], [.5,.5,-.5], [-.5,.5,-.5],
    [-.5,-.5,.5], [.5,-.5,.5], [.5,.5,.5], [-.5,.5,.5],
  ];
  for (const [x,y,z] of local) addVertex([cx + x*sx*c - y*sy*s, cy + x*sx*s + y*sy*c, cz + z*sz]);
  for (const face of [[0,3,2,1],[4,5,6,7],[0,1,5,4],[1,2,6,5],[2,3,7,6],[3,0,4,7]]) {
    addFace(face.map(i => start + i));
  }
}

function addBox(name, material, ...dimensions) {
  startObject(name, material);
  boxGeometry(...dimensions);
}

function cylinderGeometry(a, b, radius, sides = 10) {
  const axis = normalize(sub(b, a));
  let ref = [0, 0, 1];
  if (Math.abs(axis[2]) > .94) ref = [0, 1, 0];
  const u = normalize(cross(axis, ref));
  const v = normalize(cross(axis, u));
  const start = nextVertex + 1;
  for (const center of [a, b]) {
    for (let i = 0; i < sides; i++) {
      const angle = 2 * Math.PI * i / sides;
      addVertex(add(center, add(scale(u, radius * Math.cos(angle)), scale(v, radius * Math.sin(angle)))));
    }
  }
  const capA = addVertex(a), capB = addVertex(b);
  for (let i = 0; i < sides; i++) {
    const j = (i + 1) % sides;
    addFace([start + i, start + j, start + sides + j, start + sides + i]);
    addFace([capA, start + j, start + i]);
    addFace([capB, start + sides + i, start + sides + j]);
  }
}

function addCylinderBetween(name, material, a, b, radius, sides = 10) {
  startObject(name, material);
  cylinderGeometry(a, b, radius, sides);
}

function addSphereGeometry(cx, cy, cz, rx, ry, rz, sides = 10, rings = 6) {
  const start = nextVertex + 1;
  for (let r = 0; r <= rings; r++) {
    const phi = -Math.PI / 2 + Math.PI * r / rings;
    for (let i = 0; i < sides; i++) {
      const theta = 2 * Math.PI * i / sides;
      addVertex([cx + rx * Math.cos(phi) * Math.cos(theta), cy + ry * Math.cos(phi) * Math.sin(theta), cz + rz * Math.sin(phi)]);
    }
  }
  for (let r = 0; r < rings; r++) for (let i = 0; i < sides; i++) {
    const j = (i + 1) % sides;
    const a = start + r * sides + i, b = start + r * sides + j;
    addFace([a, b, start + (r + 1) * sides + j, start + (r + 1) * sides + i]);
  }
}

function addTorusGeometry(cx, cy, cz, major, minor, majorSteps = 18, minorSteps = 8, normal = [0, 0, 1]) {
  normal = normalize(normal);
  const ref = Math.abs(normal[2]) > .94 ? [0, 1, 0] : [0, 0, 1];
  const u = normalize(cross(normal, ref));
  const v = normalize(cross(normal, u));
  const start = nextVertex + 1;
  for (let i = 0; i < majorSteps; i++) {
    const a = 2 * Math.PI * i / majorSteps;
    for (let j = 0; j < minorSteps; j++) {
      const b = 2 * Math.PI * j / minorSteps;
      const ring = major + minor * Math.cos(b);
      const radial = add(scale(u, Math.cos(a)), scale(v, Math.sin(a)));
      const point = add([cx, cy, cz], add(scale(radial, ring), scale(normal, minor * Math.sin(b))));
      addVertex(point);
    }
  }
  for (let i = 0; i < majorSteps; i++) for (let j = 0; j < minorSteps; j++) {
    const i2 = (i + 1) % majorSteps, j2 = (j + 1) % minorSteps;
    addFace([start + i * minorSteps + j, start + i2 * minorSteps + j, start + i2 * minorSteps + j2, start + i * minorSteps + j2]);
  }
}

function addTorus(name, material, cx, cy, cz, major, minor, normal = [0, 0, 1]) {
  startObject(name, material);
  addTorusGeometry(cx, cy, cz, major, minor, 18, 8, normal);
}

function addGableTiles(prefix, cx, cy, baseZ, halfWidth, halfLength, rise, rows, slopeSegments, radius) {
  const slopeLength = Math.hypot(halfWidth, rise);
  const segmentLength = slopeLength / slopeSegments * .97;
  for (const side of [-1, 1]) for (let row = 0; row < rows; row++) {
    const y = cy - halfLength + 56 + row * ((2 * halfLength - 112) / Math.max(1, rows - 1));
    const shade = row % 4 === 0 ? 'RoofTileLight' : row % 4 === 3 ? 'RoofTileShade' : 'RoofTile';
    startObject(`${prefix}_RoofTiles_${side < 0 ? 'L' : 'R'}_Course_${String(row + 1).padStart(2, '0')}`, shade);
    for (let col = 0; col < slopeSegments; col++) {
      const distance = (col + .5) * slopeLength / slopeSegments;
      const center = [cx + side * distance * halfWidth / slopeLength, y, baseZ + rise - distance * rise / slopeLength];
      const direction = [side * halfWidth / slopeLength, 0, -rise / slopeLength];
      cylinderGeometry(sub(center, scale(direction, segmentLength / 2)), add(center, scale(direction, segmentLength / 2)), radius, 8);
    }
  }
}

function addLineTiles(prefix, material, a, b, count, radius) {
  const delta = sub(b, a);
  for (let i = 0; i < count; i++) {
    const p0 = add(a, scale(delta, i / count));
    const p1 = add(a, scale(delta, (i + 1) / count));
    startObject(`${prefix}_${String(i + 1).padStart(2, '0')}`, material);
    cylinderGeometry(p0, p1, radius, 8);
  }
}

// Courtyard paving: shallow, walkable visual slabs with clear grout gaps.
for (let row = 0; row < 8; row++) for (let col = 0; col < 10; col++) {
  const x = -3400 + col * 680 + 340, y = -2600 + row * 650 + 325;
  const variation = ((row * 7 + col * 3) % 3) * 4;
  addBox(`P6_Paving_${String(row + 1).padStart(2, '0')}_${String(col + 1).padStart(2, '0')}`, (row + col) % 3 ? 'Paving' : 'PavingAlt', x, y, 2, 672 - variation, 642 - variation, 8, ((row + col) % 3) - 1);
}

// Low stone courses, joints and roof coping make the enclosure read as masonry.
for (let course = 0; course < 2; course++) {
  const z = 48 + course * 92;
  startObject(`P6_Wall_North_Masonry_Course_${course + 1}`, course ? 'StoneWarm' : 'Stone');
  for (let i = 0; i < 22; i++) boxGeometry(-3400 + i * 310, 2552, z, 292, 54, 82, i % 2 ? 1 : -1);
  for (const side of [-1, 1]) {
    startObject(`P6_Wall_${side < 0 ? 'West' : 'East'}_Masonry_Course_${course + 1}`, course ? 'StoneWarm' : 'Stone');
    for (let i = 0; i < 17; i++) boxGeometry(side * 3352, -2500 + i * 310, z, 54, 292, 82, i % 2 ? 1 : -1);
    startObject(`P6_Wall_South_${side < 0 ? 'Left' : 'Right'}_Masonry_Course_${course + 1}`, course ? 'StoneWarm' : 'Stone');
    const start = side < 0 ? -3400 : 1120;
    for (let i = 0; i < 14; i++) boxGeometry(start + i * 170, -2552, z, 154, 54, 82, i % 2 ? 1 : -1);
  }
}
addLineTiles('P6_Wall_North_CopingTile', 'RoofTile', [-3500, 2660, 366], [3500, 2660, 366], 20, 42);
for (const side of [-1, 1]) addLineTiles(`P6_Wall_${side < 0 ? 'West' : 'East'}_CopingTile`, 'RoofTile', [side * 3460, -2600, 366], [side * 3460, 2600, 366], 16, 42);
for (const side of [-1, 1]) {
  const start = side < 0 ? -3500 : 1120;
  addLineTiles(`P6_Wall_South_${side < 0 ? 'Left' : 'Right'}_CopingTile`, 'RoofTile', [start, -2660, 366], [side < 0 ? -1120 : 3500, -2660, 366], 8, 42);
}

// House facade joinery: framed double door, lattice windows, base stones and supports.
startObject('P6_House_Door_Joinery', 'TimberEdge');
boxGeometry(1602, 1026, 390, 34, 52, 700); boxGeometry(1938, 1026, 390, 34, 52, 700);
boxGeometry(1770, 1026, 744, 370, 52, 36);
for (let i = 0; i < 6; i++) boxGeometry(1638 + i * 53, 1025, 390, 42, 24, 548);
for (const z of [142, 638]) boxGeometry(1770, 1012, z, 300, 30, 35);
for (const x of [1752, 1788]) cylinderGeometry([x, 985, 405], [x, 1005, 405], 13, 10);
startObject('P6_House_Door_Threshold', 'StoneWarm');
boxGeometry(1770, 986, 20, 460, 180, 34); boxGeometry(1770, 876, 50, 400, 170, 28);

for (const [name, x] of [['Left', 1000], ['Right', 2500]]) {
  startObject(`P6_House_Window_${name}_Joinery`, 'TimberEdge');
  const y = 1028, z = 660;
  boxGeometry(x - 198, y, z, 28, 56, 438); boxGeometry(x + 198, y, z, 28, 56, 438);
  boxGeometry(x, y, z - 205, 420, 56, 28); boxGeometry(x, y, z + 205, 420, 56, 28);
  for (const dx of [-100, 0, 100]) boxGeometry(x + dx, y - 16, z, 15, 24, 358);
  for (const dz of [-90, 0, 90]) boxGeometry(x, y - 18, z + dz, 365, 24, 14);
  boxGeometry(x, y - 30, z - 235, 485, 86, 42);
  boxGeometry(x, y - 27, z + 236, 470, 78, 32);
}

for (let course = 0; course < 2; course++) {
  const z = 48 + course * 105;
  startObject(`P6_House_StoneBase_FrontCourse_${course + 1}`, course ? 'StoneWarm' : 'Stone');
  for (let i = 0; i < 10; i++) boxGeometry(510 + i * 280, 1032, z, 260, 56, 92, i % 2 ? 1 : -1);
  startObject(`P6_House_StoneBase_LeftSideCourse_${course + 1}`, 'StoneWarm');
  for (let i = 0; i < 6; i++) boxGeometry(510, 1160 + i * 220, z, 56, 205, 92);
  startObject(`P6_House_StoneBase_RightSideCourse_${course + 1}`, 'Stone');
  for (let i = 0; i < 6; i++) boxGeometry(3030, 1160 + i * 220, z, 56, 205, 92);
}
startObject('P6_House_Facade_TimberFrame', 'Timber');
for (const x of [510, 780, 1320, 2220, 2760, 3030]) boxGeometry(x, 1044, 570, 52, 44, 1080);
boxGeometry(1770, 1040, 1126, 2540, 64, 56);

// Barrel-tile roof courses with ridge and rake trim, matching the existing roof planes.
addGableTiles('P6_House', 1770, 1750, 1160, 1440, 825, 640, 14, 6, 58);
addLineTiles('P6_House_RidgeCap', 'RoofTileLight', [1770, 930, 1805], [1770, 2570, 1805], 14, 70);
for (const y of [900, 2600]) {
  addCylinderBetween(`P6_House_RakeTrim_Left_${y}`, 'TimberEdge', [330, y, 1160], [1770, y, 1800], 42, 10);
  addCylinderBetween(`P6_House_RakeTrim_Right_${y}`, 'TimberEdge', [1770, y, 1800], [3210, y, 1160], 42, 10);
}
startObject('P6_House_Eave_Brackets', 'TimberEdge');
for (const x of [520, 980, 1440, 2100, 2560, 3020]) {
  boxGeometry(x, 880, 1080, 54, 130, 280, 8);
  boxGeometry(x, 922, 990, 48, 38, 180, -26);
}

// Stage: stone facing, timber railings, curtain folds and a second tiled roof.
for (let course = 0; course < 2; course++) {
  startObject(`P6_Stage_StoneFacing_Course_${course + 1}`, course ? 'StoneWarm' : 'Stone');
  for (let i = 0; i < 9; i++) boxGeometry(-2380 + i * 235, 710, 48 + course * 88, 218, 62, 74, i % 2 ? 1 : -1);
  for (let i = 0; i < 5; i++) boxGeometry(-2530, 900 + i * 210, 48 + course * 88, 62, 194, 74);
}
startObject('P6_Stage_Railing_Upper', 'TimberEdge');
boxGeometry(-1450, 746, 458, 2180, 68, 64);
boxGeometry(-1450, 746, 282, 2180, 56, 42);
for (let i = 0; i < 15; i++) boxGeometry(-2460 + i * 144, 746, 370, 38, 50, 172);
for (const side of [-1, 1]) {
  const x = side < 0 ? -2410 : -490;
  startObject(`P6_Stage_Railing_Side_${side < 0 ? 'L' : 'R'}`, 'TimberEdge');
  boxGeometry(x, 1120, 458, 68, 760, 64); boxGeometry(x, 1120, 282, 54, 760, 42);
  for (let i = 0; i < 5; i++) boxGeometry(x, 800 + i * 155, 370, 48, 36, 172);
}
startObject('P6_Stage_Curtain_Pleats', 'Curtain');
for (let i = 0; i < 13; i++) boxGeometry(-2360 + i * 152, 1878, 730, 46, 35, 620, (i % 2 ? 2 : -2));
startObject('P6_Stage_Curtain_GoldValance', 'CurtainGold');
for (let i = 0; i < 11; i++) addSphereGeometry(-2290 + i * 168, 1860, 1054, 92, 26, 44, 10, 5);
addGableTiles('P6_Stage', -1450, 1510, 1180, 1350, 775, 520, 12, 5, 52);
addLineTiles('P6_Stage_RidgeCap', 'RoofTileLight', [-1450, 760, 1708], [-1450, 2260, 1708], 12, 66);
for (const y of [720, 2300]) {
  addCylinderBetween(`P6_Stage_RakeTrim_Left_${y}`, 'TimberEdge', [-2800, y, 1180], [-1450, y, 1700], 42, 10);
  addCylinderBetween(`P6_Stage_RakeTrim_Right_${y}`, 'TimberEdge', [-1450, y, 1700], [-100, y, 1180], 42, 10);
}
startObject('P6_Stage_Post_Capitals', 'TimberEdge');
for (const x of [-2440, -460]) {
  boxGeometry(x, 1480, 1035, 220, 205, 74);
  boxGeometry(x, 1480, 1066, 255, 235, 26);
  addCylinderBetween(`P6_Stage_Brace_${x}`, 'TimberEdge', [x, 1390, 930], [x + (x < -1000 ? 240 : -240), 1390, 1110], 42, 10);
}

// Entry gate and noticeboard receive the same roof-tile language at smaller scale.
addGableTiles('P6_Gate', 0, -2660, 565, 825, 150, 250, 4, 3, 42);
addLineTiles('P6_Gate_RidgeCap', 'RoofTileLight', [0, -2800, 832], [0, -2520, 832], 4, 54);
addGableTiles('P6_Noticeboard', -2700, 900, 805, 310, 135, 125, 3, 2, 30);
startObject('P6_Noticeboard_Detail', 'TimberEdge');
boxGeometry(-2700, 790, 398, 500, 40, 30);
for (let i = 0; i < 3; i++) boxGeometry(-2840 + i * 140, 788, 620, 12, 18, 270);
for (const x of [-2925, -2475]) for (const z of [445, 795]) addSphereGeometry(x, 746, z, 12, 10, 12, 8, 4);

// The stone well becomes a readable block-built landmark with pulley and bucket details.
for (let course = 0; course < 3; course++) {
  startObject(`P6_Well_Masonry_Course_${course + 1}`, course === 1 ? 'StoneWarm' : 'Stone');
  for (let i = 0; i < 16; i++) {
    const a = 2 * Math.PI * (i + (course % 2) * .5) / 16;
    const x = 2520 + 284 * Math.cos(a), y = -2240 + 284 * Math.sin(a);
    boxGeometry(x, y, 42 + course * 92, 116, 116, 82, a * 180 / Math.PI + 90);
  }
}
addCylinderBetween('P6_Well_PulleyAxle', 'TimberEdge', [2150, -2240, 650], [2890, -2240, 650], 38, 10);
addTorus('P6_Well_PulleyWheel', 'TimberEdge', 2520, -2240, 675, 112, 20, [1, 0, 0]);
addCylinderBetween('P6_Well_Rope_Drop', 'Timber', [2520, -2240, 610], [2520, -2240, 430], 12, 8);
addCylinderBetween('P6_Well_BucketHandle_Left', 'Iron', [2438, -2240, 430], [2444, -2240, 500], 10, 8);
addCylinderBetween('P6_Well_BucketHandle_Right', 'Iron', [2602, -2240, 430], [2596, -2240, 500], 10, 8);

// Trophy relief and curved handles.
addTorus('P6_Trophy_Rim', 'Brass', -320, 520, 485, 174, 17);
addSphereGeometry(-320, 520, 545, 42, 42, 42, 12, 6);
startObject('P6_Trophy_Cup_Flutes', 'Brass');
for (let i = 0; i < 10; i++) {
  const a = 2 * Math.PI * i / 10;
  cylinderGeometry([-320 + 116 * Math.cos(a), 520 + 116 * Math.sin(a), 305], [-320 + 162 * Math.cos(a), 520 + 162 * Math.sin(a), 435], 8, 6);
}
for (const side of [-1, 1]) {
  const points = [[-320 + side * 125, 520, 455],[-320 + side * 205, 520, 475],[-320 + side * 220, 520, 410],[-320 + side * 177, 520, 365],[-320 + side * 150, 520, 374]];
  startObject(`P6_Trophy_Handle_${side < 0 ? 'L' : 'R'}`, 'Brass');
  for (let i = 0; i < points.length - 1; i++) cylinderGeometry(points[i], points[i + 1], 17, 8);
}
addTorus('P6_Trophy_Pedestal_Bead', 'StoneWarm', -320, 520, 225, 108, 12);

// Small object refinements keep the silhouettes readable from the planned camera.
for (const [x, y, r] of [[980, 1750, 155], [1230, 1750, 132], [1480, 1750, 122]]) {
  addTorus(`P6_StorageJar_Rim_${x}`, 'StoneWarm', x, y, x === 980 ? 302 : x === 1230 ? 262 : 242, r, 14);
}
for (const [x, y, z, radius] of [[300, 2190, 210, 130], [3120, 950, 210, 130], [1050, 400, 100, 118]]) {
  addTorus(`P6_Pot_Rim_${x}`, 'StoneWarm', x, y, z, radius, 14);
}
startObject('P6_Bench_Seat_Slats', 'TimberEdge');
for (let i = 0; i < 4; i++) boxGeometry(2860, 69 + i * 42, 342, 750, 32, 28);
startObject('P6_Bench_CrossBrace', 'Timber');
boxGeometry(2860, 130, 152, 650, 42, 42);

// A more specific but still clue-neutral camera and sound detector face.
addBox('P6_Detector_Faceplate', 'Iron', 2300, 465, 620, 270, 26, 208);
addBox('P6_Detector_Screen', 'WindowAmber', 2300, 447, 663, 180, 12, 82);
addCylinderBetween('P6_Detector_MainDial_Base', 'TimberEdge', [2300, 430, 560], [2300, 456, 560], 50, 12);
addCylinderBetween('P6_Detector_MainDial_Knob', 'Brass', [2300, 412, 560], [2300, 432, 560], 26, 12);
startObject('P6_Detector_Grille', 'TimberEdge');
for (let i = 0; i < 5; i++) boxGeometry(2220 + i * 40, 447, 740, 8, 10, 36);
addCylinderBetween('P6_Detector_Antenna', 'Iron', [2405, 610, 770], [2440, 610, 930], 9, 8);

startObject('P6_PhotoTable_TopSlats', 'TimberEdge');
for (let i = 0; i < 5; i++) boxGeometry(1500, 885 + i * 86, 385, 730, 18, 16);
addTorus('P6_PhotoTable_LensRing', 'Brass', 1500, 975, 490, 62, 13, [0, -1, 0]);
addCylinderBetween('P6_PhotoTable_LensGlass', 'Iron', [1500, 962, 490], [1500, 972, 490], 48, 12);
addBox('P6_PhotoTable_CameraTop', 'TimberEdge', 1500, 1080, 575, 260, 180, 32);
addBox('P6_PhotoTable_CameraShutter', 'Iron', 1500, 979, 530, 95, 18, 64);
addSphereGeometry(1420, 995, 585, 18, 18, 18, 8, 4);
addSphereGeometry(1580, 995, 585, 18, 18, 18, 8, 4);

startObject('P6_Ledger_Desk_Planks', 'TimberEdge');
for (let i = 0; i < 4; i++) boxGeometry(-2250, -1120 + i * 80, 344, 590, 18, 14);
addBox('P6_Ledger_Book_Cover', 'TimberEdge', -2250, -1000, 405, 250, 176, 24);
addBox('P6_Ledger_Book_Pages', 'Paper', -2250, -1000, 390, 224, 150, 24);
for (let i = 0; i < 4; i++) addBox(`P6_Ledger_Book_PageRule_${i + 1}`, 'StoneWarm', -2250, -1048 + i * 30, 405, 160, 4, 3);

startObject('P6_Gate_OpenLeaf', 'TimberEdge');
boxGeometry(-570, -2390, 245, 58, 520, 390);
for (let i = 0; i < 6; i++) boxGeometry(-570, -2615 + i * 88, 245, 22, 66, 352);
boxGeometry(-570, -2390, 72, 72, 540, 44);
boxGeometry(-570, -2390, 418, 72, 540, 38);
startObject('P6_Gate_Hardware', 'Iron');
for (const x of [-50, 50]) addSphereGeometry(x, -2530, 245, 18, 14, 18, 8, 4);
addBox('P6_Gate_Lintel_Crown', 'TimberEdge', 0, -2640, 580, 1450, 250, 70);

startObject('P6_Lantern_House_Detail', 'CurtainGold');
for (const z of [840, 890, 1140]) addTorusGeometry(440, 832, z, z === 1140 ? 36 : 82, 9, 12, 6);
addCylinderBetween('P6_Lantern_House_Hanger', 'Iron', [440, 832, 1145], [440, 832, 1120], 12, 8);
addCylinderBetween('P6_Lantern_House_Tassel', 'Curtain', [440, 832, 830], [440, 832, 780], 18, 8);
startObject('P6_Lantern_Gate_Detail', 'CurtainGold');
for (const z of [450, 495, 700]) addTorusGeometry(730, -2780, z, z === 700 ? 32 : 72, 8, 12, 6);
addCylinderBetween('P6_Lantern_Gate_Tassel', 'Curtain', [730, -2780, 450], [730, -2780, 405], 16, 8);

// Layered low-poly foliage softens the prop silhouettes without changing clue content.
for (const [x, y, baseZ, count] of [[300,2190,270,7],[3120,950,270,7],[1050,400,105,5],[-2900,1680,250,8]]) {
  startObject(`P6_Foliage_Cluster_${x}_${y}`, 'Plant');
  for (let i = 0; i < count; i++) {
    const a = 2 * Math.PI * i / count;
    const spread = x === -2900 ? 210 : 90;
    addSphereGeometry(x + spread * Math.cos(a), y + spread * .72 * Math.sin(a), baseZ + (i % 3) * 34,
      x === -2900 ? 130 : 70, x === -2900 ? 95 : 58, x === -2900 ? 54 : 32, 8, 4);
  }
}
startObject('P6_TrashCan_Bands', 'Iron');
for (const z of [100, 390]) addTorusGeometry(-1280, -1430, z, 225 + (z === 390 ? 25 : 0), 10, 14, 6);
startObject('P6_Clothesline_Scarf_Seams', 'TimberEdge');
for (const x of [-1120, -800, -500]) boxGeometry(x, 2194, 440, 18, 12, 175);

// Retain visible object IDs and append details as selectable groups.
const objText = `${baseObj.trimEnd()}\n# v02 adds architectural courses, joinery, paving, roof tiles and prop relief; coordinates of v01 anchors remain unchanged.\n${detail.join('\n')}\n`;
const mtlText = `${baseMtl.trimEnd()}\n${extraMaterials.trimStart()}`;
fs.writeFileSync(targetObj, objText, 'ascii');
fs.writeFileSync(targetMtl, mtlText, 'ascii');

const lines = objText.split(/\r?\n/);
const points = lines.filter(line => line.startsWith('v ')).map(line => line.split(/\s+/).slice(1).map(Number));
const bounds = [0, 1, 2].map(axis => [Math.min(...points.map(p => p[axis])), Math.max(...points.map(p => p[axis]))]);
console.log(JSON.stringify({
  obj: path.relative(repoRoot, targetObj).replaceAll('\\', '/'),
  mtl: path.relative(repoRoot, targetMtl).replaceAll('\\', '/'),
  object_groups: lines.filter(line => line.startsWith('o ')).length,
  vertices: points.length,
  faces: lines.filter(line => line.startsWith('f ')).length,
  bounds_cm: { x: bounds[0], y: bounds[1], z: bounds[2] },
  added_geometry_note: 'Layered medium-poly visual detail; UVs, collision, LODs and runtime acceptance remain outstanding.',
}, null, 2));


