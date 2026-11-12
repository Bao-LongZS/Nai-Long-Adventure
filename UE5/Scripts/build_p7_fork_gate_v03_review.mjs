import fs from 'node:fs/promises';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const sharp = require('sharp');
const root = path.resolve(import.meta.dirname, '..', '..');
const env = path.join(root, 'UE5', 'SourceAssets', 'P7', 'Environment', 'A1');
const manifests = path.join(root, 'UE5', 'SourceAssets', 'P7', 'Manifests', 'A1');
const source = path.join(env, 'P7_A1_ForkGateStates_v03.svg');
const output = path.join(env, 'P7_A1_ForkGateStates_v03.png');
const board = path.join(env, 'Review', 'P7_A1_ForkGateTopologyReview_v03.png');
const manifest = path.join(manifests, 'P7_A1_ForkGateStates_v03.csv');
await fs.mkdir(path.dirname(board), { recursive: true });

const sourceHash = async (file) => createHash('sha256').update(await fs.readFile(file)).digest('hex').toUpperCase();
await sharp(source, { density: 72 }).png({ compressionLevel: 9 }).toFile(output);
const atlas = await sharp(output).ensureAlpha().raw().toBuffer({ resolveWithObject: true });
if (atlas.info.width !== 2928 || atlas.info.height !== 528 || atlas.info.channels !== 4) {
  throw new Error(`Unexpected v03 PNG shape: ${atlas.info.width}x${atlas.info.height}x${atlas.info.channels}`);
}
let gutterAlphaPixels = 0;
for (const [x0, x1] of [[960, 984], [1944, 1968]]) {
  for (let y = 0; y < 528; y++) {
    for (let x = x0; x < x1; x++) {
      if (atlas.data[(y * atlas.info.width + x) * 4 + 3] !== 0) gutterAlphaPixels++;
    }
  }
}
if (gutterAlphaPixels) throw new Error(`Found ${gutterAlphaPixels} nontransparent atlas gutter pixels`);

const terrain = path.join(env, 'P7_A1_TerrainTileAtlas_v02.png');
const base = await sharp(terrain).extract({ left: 1008, top: 1128, width: 960, height: 528 }).png().toBuffer();
const positions = [0, 984, 1968];
const crops = [];
for (const left of positions) {
  const gate = await sharp(output).extract({ left, top: 0, width: 960, height: 528 }).png().toBuffer();
  const scene = await sharp(base).composite([{ input: gate, blend: 'over' }]).png().toBuffer();
  crops.push(await sharp(scene).resize({ width: 552, height: 304, fit: 'fill', kernel: 'lanczos3' }).png().toBuffer());
}
const mini = [];
const miniGray = [];
for (const left of positions) {
  const gate = await sharp(output).extract({ left, top: 0, width: 960, height: 528 }).png().toBuffer();
  const scene = await sharp(base).composite([{ input: gate, blend: 'over' }]).png().toBuffer();
  const small = await sharp(scene).resize({ width: 48, height: 26, fit: 'fill', kernel: 'lanczos3' }).png().toBuffer();
  mini.push(await sharp(small).resize({ width: 144, height: 78, kernel: 'nearest' }).png().toBuffer());
  const graySmall = await sharp(scene).greyscale().resize({ width: 48, height: 26, fit: 'fill', kernel: 'lanczos3' }).png().toBuffer();
  miniGray.push(await sharp(graySmall).resize({ width: 144, height: 78, kernel: 'nearest' }).png().toBuffer());
}
const b64 = (buf) => buf.toString('base64');
const cards = [
  { x: 48, title: 'A 直行 / 两节点共用', detail: '西 → 东开放；北支路封闭。Alpha01 (1,5) / (18,5)' },
  { x: 650, title: 'B 分岔', detail: '西 → 北开放；东支路封闭。Alpha01 (1,5)' },
  { x: 1252, title: 'B 汇合', detail: '北 → 东开放；西支路封闭。Alpha01 (18,5)' },
];
const cardSvg = cards.map((card, i) => `
  <rect x="${card.x}" y="148" width="600" height="660" rx="14" fill="#FFFEFA" stroke="#D9D2C1" stroke-width="2"/>
  <text x="${card.x + 24}" y="198" class="cardTitle">${card.title}</text>
  <text x="${card.x + 24}" y="230" class="detail">${card.detail}</text>
  <image x="${card.x + 24}" y="252" width="552" height="304" href="data:image/png;base64,${b64(crops[i])}"/>
  <text x="${card.x + 24}" y="604" class="smallHead">48px 地图格真实下采样 · 3× 最近邻显示</text>
  <text x="${card.x + 70}" y="619" class="microHead">彩色</text><text x="${card.x + 222}" y="619" class="microHead">灰度</text>
  <rect x="${card.x + 24}" y="626" width="148" height="82" fill="#E7DFC7" stroke="#D9D2C1"/>
  <image x="${card.x + 26}" y="628" width="144" height="78" href="data:image/png;base64,${b64(mini[i])}"/>
  <rect x="${card.x + 176}" y="626" width="148" height="82" fill="#E7DFC7" stroke="#D9D2C1"/>
  <image x="${card.x + 178}" y="628" width="144" height="78" href="data:image/png;base64,${b64(miniGray[i])}"/>
  <text x="${card.x + 338}" y="650" class="note">透明门片叠在现有地面 T 格上</text>
  <text x="${card.x + 338}" y="682" class="note">箭头沿来袭方向指向出口</text>
  <text x="${card.x + 338}" y="714" class="note">封路闸条横跨被禁支路</text>`).join('');
const reviewSvg = `<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="1900" height="870" viewBox="0 0 1900 870">
 <rect width="1900" height="870" fill="#F3F0E7"/>
 <text x="48" y="68" class="title">P7 A1 分岔门 v03 · Alpha01 T 形路线复核</text>
 <text x="50" y="108" class="subtitle">按路表分别制作 A 直行、B 分岔、B 汇合三种状态；门片逐格裁切，atlas gutter 保持全透明。</text>
 ${cardSvg}
 <text x="48" y="850" class="footer">参考：P7_Alpha01_Level_v01.csv · RouteA y=5 · RouteB (1,5)→(1,4)…(18,4)→(18,5) · 用户已通过并导入 UE、接入 HUD；实际 viewport 视觉验收待完成</text>
 <style>
  text{font-family:'Microsoft YaHei','Noto Sans CJK SC','Arial',sans-serif;fill:#302D27}
  .title{font-size:40px;font-weight:700}.subtitle{font-size:21px;fill:#615D54}
  .cardTitle{font-size:27px;font-weight:700;fill:#087B89}.detail{font-size:17px;fill:#615D54}
  .smallHead{font-size:17px;font-weight:700;fill:#4D483E}.microHead{font-size:13px;fill:#615D54}.note{font-size:15px;fill:#615D54}
  .footer{font-size:15px;fill:#777268}
 </style>
</svg>`;
await sharp(Buffer.from(reviewSvg)).png({ compressionLevel: 9 }).toFile(board);

const shaSource = await sourceHash(source);
const shaExport = await sourceHash(output);
const headers = ['AssetId','Owner','SourcePath','SourceHash','ExportPath','ExportHash','Version','IntendedUse','Width','Height','FrameCount','FrameIndex','AlphaMode','ColorSpace','ExportPreset','UnrealPath','AtlasX','AtlasY','AtlasW','AtlasH','ReviewStatus','ReuseStatus','EvidencePath','Notes'];
const rows = [
  ['P7-ENV-A1-GATE-A-STRAIGHT','A through, reused at split and merge'],
  ['P7-ENV-A1-GATE-B-SPLIT','B split, west to north'],
  ['P7-ENV-A1-GATE-B-MERGE','B merge, north to east'],
];
const csvEscape = (value) => `"${String(value).replaceAll('"', '""')}"`;
function parseCsv(content) {
  const records = [];
  let record = [];
  let field = '';
  let quoted = false;
  for (let i = 0; i < content.length; i++) {
    const ch = content[i];
    if (quoted && ch === '"' && content[i + 1] === '"') { field += '"'; i++; }
    else if (ch === '"') quoted = !quoted;
    else if (ch === ',' && !quoted) { record.push(field); field = ''; }
    else if ((ch === '\n' || ch === '\r') && !quoted) {
      if (ch === '\r' && content[i + 1] === '\n') i++;
      record.push(field); field = '';
      if (record.some((part) => part.length)) records.push(record);
      record = [];
    } else field += ch;
  }
  if (field.length || record.length) { record.push(field); records.push(record); }
  return records;
}
const priorCsv = await fs.readFile(manifest, 'utf8').catch(() => '');
const priorRecords = priorCsv ? parseCsv(priorCsv.replace(/^\uFEFF/, '')) : [];
const priorHeader = priorRecords[0] ?? [];
const priorById = new Map(priorRecords.slice(1).map((values) => [
  Object.fromEntries(priorHeader.map((name, index) => [name, values[index] ?? ''])).AssetId,
  Object.fromEntries(priorHeader.map((name, index) => [name, values[index] ?? ''])),
]));
const csvRows = rows.map(([assetId, use], i) => {
  const x = positions[i];
  const previous = priorById.get(assetId);
  const hashesUnchanged = previous?.SourceHash?.toLowerCase() === shaSource.toLowerCase() &&
    previous?.ExportHash?.toLowerCase() === shaExport.toLowerCase();
  const values = {
    AssetId: assetId, Owner: 'P7', SourcePath: 'Environment/A1/P7_A1_ForkGateStates_v03.svg', SourceHash: shaSource,
    ExportPath: 'Environment/A1/P7_A1_ForkGateStates_v03.png', ExportHash: shaExport, Version: 'v03', IntendedUse: use,
    Width: '960', Height: '528', FrameCount: '3', FrameIndex: String(i), AlphaMode: 'RGBA transparent', ColorSpace: 'sRGB',
    ExportPreset: '8-bit PNG; cells 960×528; 24px transparent gutter',
    UnrealPath: '/Game/Naiwa/P7/Environment/A1/T_P7_A1_ForkGateStates_v03',
    AtlasX: String(x), AtlasY: '0', AtlasW: '960', AtlasH: '528',
    ReviewStatus: hashesUnchanged && previous ? previous.ReviewStatus : 'Candidate v03; user review pending',
    ReuseStatus: hashesUnchanged && previous ? previous.ReuseStatus : 'P7-exclusive; Common review not started',
    EvidencePath: hashesUnchanged && previous ? previous.EvidencePath : 'Environment/A1/Review/P7_A1_ForkGateTopologyReview_v03.png',
    Notes: hashesUnchanged && previous ? previous.Notes : 'Alpha01 T junction aligned; each cell clipped; v02 preserved as historical candidate; no UE import/runtime evidence',
  };
  return headers.map((header) => csvEscape(values[header] ?? '')).join(',');
});
await fs.writeFile(manifest, `${headers.map(csvEscape).join(',')}\n${csvRows.join('\n')}\n`, 'utf8');
console.log(JSON.stringify({ output, board, manifest, sourceSha256: shaSource, exportSha256: shaExport, gutterAlphaPixels }, null, 2));
