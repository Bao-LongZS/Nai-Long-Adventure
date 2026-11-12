import fs from 'node:fs/promises';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const sharp = require('sharp');
const root = path.resolve(import.meta.dirname, '..', '..');
const sourceRoot = path.join(root, 'UE5', 'SourceAssets', 'P7');
const vfxRoot = path.join(sourceRoot, 'VFX', 'A1');
const towerRoot = path.join(sourceRoot, 'Towers', 'A1');
const manifestPath = path.join(sourceRoot, 'Manifests', 'A1', 'P7_A1_TowerProjectileVFXAssets_v01.csv');
const sourcePath = path.join(vfxRoot, 'P7_A1_TowerProjectileVFX_v01.svg');
const exportPath = path.join(vfxRoot, 'P7_A1_TowerProjectileVFX_v01.png');
const reviewPath = path.join(vfxRoot, 'Review', 'P7_A1_TowerProjectileVFXReview_v01.png');
await fs.mkdir(path.dirname(reviewPath), { recursive: true });

const sha256 = async (file) => createHash('sha256').update(await fs.readFile(file)).digest('hex').toUpperCase();
await sharp(sourcePath, { density: 72 }).png({ compressionLevel: 9 }).toFile(exportPath);
const atlas = await sharp(exportPath).ensureAlpha().raw().toBuffer({ resolveWithObject: true });
if (atlas.info.width !== 848 || atlas.info.height !== 432 || atlas.info.channels !== 4) {
  throw new Error(`Unexpected projectile atlas shape: ${atlas.info.width}x${atlas.info.height}x${atlas.info.channels}`);
}
let gutterAlphaPixels = 0;
for (const [x0, x1] of [[0, 16], [208, 224], [416, 432], [624, 640], [832, 848]]) {
  for (let y = 0; y < atlas.info.height; y++) {
    for (let x = x0; x < x1; x++) if (atlas.data[(y * atlas.info.width + x) * 4 + 3] !== 0) gutterAlphaPixels++;
  }
}
for (const [y0, y1] of [[0, 16], [208, 224], [416, 432]]) {
  for (let y = y0; y < y1; y++) {
    for (let x = 0; x < atlas.info.width; x++) if (atlas.data[(y * atlas.info.width + x) * 4 + 3] !== 0) gutterAlphaPixels++;
  }
}
if (gutterAlphaPixels) throw new Error(`Nontransparent atlas gutter pixels: ${gutterAlphaPixels}`);

const oldAtlasPath = path.join(vfxRoot, 'P7_A1_VFXAtlas_v07.png');
const frameSources = {
  chicken: Array.from({ length: 4 }, (_, i) => ({ file: oldAtlasPath, left: 16 + i * 208, top: 16 })),
  dog: Array.from({ length: 4 }, (_, i) => ({ file: exportPath, left: 16 + i * 208, top: 16 })),
  mouse: Array.from({ length: 4 }, (_, i) => ({ file: exportPath, left: 16 + i * 208, top: 224 })),
};
const frames = {};
for (const [kind, specs] of Object.entries(frameSources)) {
  frames[kind] = await Promise.all(specs.map(async (spec) => await sharp(spec.file)
    .extract({ left: spec.left, top: spec.top, width: 192, height: 192 }).png().toBuffer()));
}
const towerImages = {
  chicken: await fs.readFile(path.join(towerRoot, 'P7_A1_Tower_SingleTarget_MilkChicken_L1_v01.png')),
  dog: await fs.readFile(path.join(towerRoot, 'P7_A1_Tower_Area_MilkDog_L1_v01.png')),
  mouse: await fs.readFile(path.join(towerRoot, 'P7_A1_Tower_Slow_MilkMouse_L1_v02.png')),
};
const b64 = (buffer) => buffer.toString('base64');
const types = [
  { key: 'chicken', y: 150, title: '奶鸡 · 单体强攻', descriptor: '保留现有 v07 金色尖弹与命中效果', towerName: 'P7_A1_Tower_SingleTarget_MilkChicken_L1_v01.png', frames: ['蓄光', '尖弹飞行', '单体命中', '余光消散'] },
  { key: 'dog', y: 456, title: '奶狗 · 范围攻击', descriptor: '共振弹群飞行 · 环形扩散冲击', towerName: 'P7_A1_Tower_Area_MilkDog_L1_v01.png', frames: ['喇叭蓄波', '共振弹飞行', '范围爆裂', '冲击环消散'] },
  { key: 'mouse', y: 762, title: '奶鼠 · 减速支援', descriptor: '薄荷晶体弹 · 命中后展开减速脉冲', towerName: 'P7_A1_Tower_Slow_MilkMouse_L1_v02.png', frames: ['晶体成形', '减速弹飞行', '减速场扩张', '脉冲环消散'] },
];
const rowMarkup = types.map((type) => `
  <rect x="40" y="${type.y}" width="1640" height="284" rx="16" fill="#FFFEFA" stroke="#D8D3C6" stroke-width="2"/>
  <image x="60" y="${type.y + 62}" width="180" height="180" preserveAspectRatio="xMidYMid meet" href="data:image/png;base64,${b64(towerImages[type.key])}"/>
  <text x="58" y="${type.y + 34}" class="groupTitle">${type.title}</text>
  <text x="58" y="${type.y + 260}" class="groupDesc">${type.descriptor}</text>
  ${type.frames.map((label, i) => {
    const x = 285 + i * 345;
    return `<rect x="${x}" y="${type.y + 18}" width="320" height="248" rx="10" fill="#EEF2EF"/>
      <image x="${x + 64}" y="${type.y + 28}" width="192" height="192" preserveAspectRatio="none" href="data:image/png;base64,${b64(frames[type.key][i])}"/>
      <text x="${x + 18}" y="${type.y + 247}" class="frameNo">F0${i}</text>
      <text x="${x + 160}" y="${type.y + 247}" class="frameLabel" text-anchor="middle">${label}</text>`;
  }).join('')}`).join('');
const svg = `<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="1720" height="1090" viewBox="0 0 1720 1090">
 <rect width="1720" height="1090" fill="#F2F0E8"/>
 <text x="48" y="67" class="title">P7 三塔独立子弹特效 · v01</text>
 <text x="50" y="108" class="subtitle">奶鸡保留已接入的 v07 金色尖弹；奶狗与奶鼠分别使用新的共振弹群和薄荷减速晶体弹。</text>
 ${rowMarkup}
 <text x="48" y="1073" class="footer">透明 RGBA 特效叠加；按攻击事件播放四帧。炮塔射程、伤害、范围和减速数值不变。</text>
 <style>
  text{font-family:'Microsoft YaHei','Noto Sans CJK SC','Arial',sans-serif;fill:#302D27}
  .title{font-size:37px;font-weight:700}.subtitle{font-size:20px;fill:#625D54}
  .groupTitle{font-size:24px;font-weight:700;fill:#384858}.groupDesc{font-size:15px;fill:#6F695F}
  .frameNo{font-size:14px;fill:#C18335}.frameLabel{font-size:17px;font-weight:600;fill:#384858}.footer{font-size:15px;fill:#777268}
 </style>
</svg>`;
await sharp(Buffer.from(svg)).png({ compressionLevel: 9 }).toFile(reviewPath);

const sourceHash = await sha256(sourcePath);
const exportHash = await sha256(exportPath);
const oldCsv = await fs.readFile(manifestPath, 'utf8').catch(() => '');
const oldLines = oldCsv ? oldCsv.replace(/^\uFEFF/, '').trimEnd().split(/\r?\n/) : [];
const header = ['AssetId','Owner','SourcePath','SourceHash','ExportPath','ExportHash','Version','TowerId','IntendedUse','Width','Height','FrameCount','FrameIndex','AlphaMode','ColorSpace','ExportPreset','UnrealPath','AtlasX','AtlasY','AtlasW','AtlasH','ReviewStatus','EvidencePath','Notes'];
const csvEscape = (value) => `"${String(value).replaceAll('"', '""')}"`;
const previousById = new Map();
if (oldLines.length > 1) {
  const parse = (line) => { const cells = []; let value = ''; let quoted = false;
    for (let i = 0; i < line.length; i++) { const c = line[i]; if (quoted && c === '"' && line[i + 1] === '"') { value += '"'; i++; }
      else if (c === '"') quoted = !quoted; else if (c === ',' && !quoted) { cells.push(value); value = ''; } else value += c; }
    cells.push(value); return cells; };
  const priorHeader = parse(oldLines[0]);
  for (const line of oldLines.slice(1)) { const values = parse(line); const row = Object.fromEntries(priorHeader.map((key, i) => [key, values[i] ?? ''])); previousById.set(row.AssetId, row); }
}
const rowDefs = [
  ['P7-VFX-A1-PROJECTILE-MILKDOG-F00','P7_Tower_MilkDog','MilkDog area projectile — resonance shell launches',16,16,0,'喇叭蓄波；暖橘短促声压环与三枚弹核'],
  ['P7-VFX-A1-PROJECTILE-MILKDOG-F01','P7_Tower_MilkDog','MilkDog area projectile — clustered shell travels',224,16,1,'三枚暖橘共振弹群沿目标方向飞行'],
  ['P7-VFX-A1-PROJECTILE-MILKDOG-F02','P7_Tower_MilkDog','MilkDog area projectile — radial area hit',432,16,2,'环形范围爆裂；只表现现有范围攻击命中'],
  ['P7-VFX-A1-PROJECTILE-MILKDOG-F03','P7_Tower_MilkDog','MilkDog area projectile — shock ring fades',640,16,3,'共振环与颗粒余波消散'],
  ['P7-VFX-A1-PROJECTILE-MILKMOUSE-F00','P7_Tower_MilkMouse','MilkMouse slow pulse — mint crystal forms',16,224,0,'薄荷色晶体弹从炮塔侧成形'],
  ['P7-VFX-A1-PROJECTILE-MILKMOUSE-F01','P7_Tower_MilkMouse','MilkMouse slow pulse — crystal dart travels',224,224,1,'薄荷晶体弹带减速波纹飞向敌人'],
  ['P7-VFX-A1-PROJECTILE-MILKMOUSE-F02','P7_Tower_MilkMouse','MilkMouse slow pulse — slowing field expands',432,224,2,'环形减速脉冲；不表现直接伤害'],
  ['P7-VFX-A1-PROJECTILE-MILKMOUSE-F03','P7_Tower_MilkMouse','MilkMouse slow pulse — pulse rings fade',640,224,3,'减速脉冲环与晶粒消散'],
];
const builtRows = rowDefs.map(([assetId,towerId,use,x,y,index,note]) => {
  const previous = previousById.get(assetId);
  const unchanged = previous?.SourceHash?.toLowerCase() === sourceHash.toLowerCase() && previous?.ExportHash?.toLowerCase() === exportHash.toLowerCase();
  const row = {
    AssetId: assetId, Owner: 'P7', SourcePath: 'VFX/A1/P7_A1_TowerProjectileVFX_v01.svg', SourceHash: sourceHash,
    ExportPath: 'VFX/A1/P7_A1_TowerProjectileVFX_v01.png', ExportHash: exportHash, Version: 'v01', TowerId: towerId,
    IntendedUse: use, Width: '192', Height: '192', FrameCount: '4', FrameIndex: String(index),
    AlphaMode: 'Straight RGBA transparent', ColorSpace: 'sRGB', ExportPreset: '8-bit PNG; 4x2 cells, 16px gutters',
    UnrealPath: '/Game/Naiwa/P7/VFX/A1/T_P7_A1_TowerProjectileVFX_v01', AtlasX: String(x), AtlasY: String(y),
    AtlasW: '192', AtlasH: '192', ReviewStatus: unchanged ? previous.ReviewStatus : 'User-directed; generated and wired; UE import and runtime viewport review pending',
    EvidencePath: unchanged ? previous.EvidencePath : 'VFX/A1/Review/P7_A1_TowerProjectileVFXReview_v01.png', Notes: unchanged ? previous.Notes : note,
  };
  return row;
});
const csv = [header.join(','), ...builtRows.map((row) => header.map((key) => csvEscape(row[key] ?? '')).join(','))].join('\n') + '\n';
await fs.writeFile(manifestPath, csv, 'utf8');
console.log(JSON.stringify({ export: exportPath, review: reviewPath, manifest: manifestPath, sourceSha256: sourceHash, exportSha256: exportHash, gutterAlphaPixels }, null, 2));
