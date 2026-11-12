import fs from 'node:fs/promises';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const sharp = require('sharp');
const root = path.resolve(import.meta.dirname, '..', '..');
const sourceRoot = path.join(root, 'UE5', 'SourceAssets', 'P7');
const assetRoot = path.join(sourceRoot, 'UI', 'A1');
const reviewRoot = path.join(assetRoot, 'Review');
const manifestRoot = path.join(sourceRoot, 'Manifests', 'A1');
const svgPath = path.join(assetRoot, 'P7_A1_ResultBadgeAtlas_v03.svg');
const pngPath = path.join(assetRoot, 'P7_A1_ResultBadgeAtlas_v03.png');
const reviewPath = path.join(reviewRoot, 'P7_A1_ResultBadgeComparisonReview_v03.png');
const manifestPath = path.join(manifestRoot, 'P7_A1_ResultBadgeAssets_v03.csv');
const priorPngPath = path.join(assetRoot, 'P7_A1_ResultBadgeAtlas_v02.png');
const hash = async (file) => createHash('sha256').update(await fs.readFile(file)).digest('hex');
const b64 = (buffer) => buffer.toString('base64');
const csvEscape = (value) => `"${String(value).replaceAll('"', '""')}"`;

const svg = `<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="832" height="416" viewBox="0 0 832 416">
 <title>P7 A1 result badge atlas v03</title>
 <desc>Victory badge with a large mint check and defeat badge with a bold coral down arrow. The milk egg remains whole.</desc>
 <g id="P7-UI-A1-RESULT-WIN" transform="translate(16 16)" stroke-linejoin="round" stroke-linecap="round">
  <circle cx="192" cy="192" r="160" fill="#FFF3D7" stroke="#14264A" stroke-width="14"/>
  <circle cx="192" cy="192" r="143" fill="none" stroke="#F4C84A" stroke-width="9"/>
  <path d="M93 270C70 231 70 180 91 145M291 270C314 231 314 180 293 145" fill="none" stroke="#55B97C" stroke-width="8"/>
  <ellipse cx="83" cy="233" rx="8" ry="16" transform="rotate(-44 83 233)" fill="#55B97C"/>
  <ellipse cx="79" cy="197" rx="8" ry="16" transform="rotate(-18 79 197)" fill="#55B97C"/>
  <ellipse cx="84" cy="161" rx="8" ry="16" transform="rotate(15 84 161)" fill="#55B97C"/>
  <ellipse cx="301" cy="233" rx="8" ry="16" transform="rotate(44 301 233)" fill="#55B97C"/>
  <ellipse cx="305" cy="197" rx="8" ry="16" transform="rotate(18 305 197)" fill="#55B97C"/>
  <ellipse cx="300" cy="161" rx="8" ry="16" transform="rotate(-15 300 161)" fill="#55B97C"/>
  <path d="M192 76C159 76 131 87 113 102V199C113 251 145 286 192 312C239 286 271 251 271 199V102C253 87 225 76 192 76Z" fill="#E6F3E5" stroke="#14264A" stroke-width="11"/>
  <path d="M192 105C155 105 140 136 140 178V226C140 260 162 282 192 282C222 282 244 260 244 226V178C244 136 229 105 192 105Z" fill="#F4C84A" stroke="#14264A" stroke-width="9"/>
  <path d="M162 135C170 119 181 111 193 110" fill="none" stroke="#FFF8E8" stroke-width="8"/>
  <circle cx="177" cy="183" r="6" fill="#14264A"/><circle cx="207" cy="183" r="6" fill="#14264A"/>
  <path d="M179 206Q192 221 205 206" fill="none" stroke="#8D4F2C" stroke-width="7"/>
  <path d="M174 230Q192 240 210 230" fill="none" stroke="#55B97C" stroke-width="7"/>
  <path d="M192 45L201 63L221 65L206 79L210 99L192 89L174 99L178 79L163 65L183 63Z" fill="#F4C84A" stroke="#9A5A2E" stroke-width="5"/>
  <path d="M247 253L270 275L326 211" fill="none" stroke="#14264A" stroke-width="27"/>
  <path d="M247 253L270 275L326 211" fill="none" stroke="#55B97C" stroke-width="16"/>
 </g>
 <g id="P7-UI-A1-RESULT-LOSS" transform="translate(432 16)" stroke-linejoin="round" stroke-linecap="round">
  <path d="M192 32C252 32 307 57 333 101V216C333 274 278 323 192 352C106 323 51 274 51 216V101C77 57 132 32 192 32Z" fill="#FBE7DE" stroke="#14264A" stroke-width="14"/>
  <path d="M192 51C246 51 290 72 313 108V213C313 260 267 301 192 328C117 301 71 260 71 213V108C94 72 138 51 192 51Z" fill="none" stroke="#EF6256" stroke-width="9"/>
  <path d="M192 97C162 97 136 108 120 123V203C120 245 147 273 192 294C237 273 264 245 264 203V123C248 108 222 97 192 97Z" fill="#FFF3D7" stroke="#14264A" stroke-width="10"/>
  <path d="M192 126C155 126 140 157 140 198V234C140 264 162 281 192 281C222 281 244 264 244 234V198C244 157 229 126 192 126Z" fill="#F4C84A" stroke="#14264A" stroke-width="9"/>
  <path d="M161 153C169 138 181 132 193 131" fill="none" stroke="#FFF8E8" stroke-width="8"/>
  <path d="M169 194Q179 183 189 194M197 194Q207 183 217 194" fill="none" stroke="#14264A" stroke-width="7"/>
  <path d="M179 224Q192 210 205 224" fill="none" stroke="#8D4F2C" stroke-width="7"/>
  <path d="M295 108V231M271 207L295 233L319 207" fill="none" stroke="#14264A" stroke-width="25"/>
  <path d="M295 108V231M271 207L295 233L319 207" fill="none" stroke="#EF6256" stroke-width="15"/>
  <path d="M99 111L87 95M285 276L298 287L286 300L273 288Z" fill="#EF6256" stroke="#14264A" stroke-width="5"/>
 </g>
</svg>`;
await fs.mkdir(reviewRoot, { recursive: true });
await fs.mkdir(manifestRoot, { recursive: true });
await fs.writeFile(svgPath, svg, 'utf8');
await sharp(svgPath, { density: 72 }).png({ compressionLevel: 9 }).toFile(pngPath);

const atlas = await sharp(pngPath).ensureAlpha().raw().toBuffer({ resolveWithObject: true });
if (atlas.info.width !== 832 || atlas.info.height !== 416 || atlas.info.channels !== 4) {
  throw new Error(`Unexpected atlas metadata: ${atlas.info.width}x${atlas.info.height}x${atlas.info.channels}`);
}
let gutterAlphaPixels = 0;
for (const [x0, x1] of [[0, 16], [400, 432], [816, 832]]) {
  for (let y = 0; y < atlas.info.height; y++) {
    for (let x = x0; x < x1; x++) if (atlas.data[(y * atlas.info.width + x) * 4 + 3] !== 0) gutterAlphaPixels++;
  }
}
for (const [y0, y1] of [[0, 16], [400, 416]]) {
  for (let y = y0; y < y1; y++) {
    for (let x = 0; x < atlas.info.width; x++) if (atlas.data[(y * atlas.info.width + x) * 4 + 3] !== 0) gutterAlphaPixels++;
  }
}
if (gutterAlphaPixels) throw new Error(`Nontransparent atlas gutters: ${gutterAlphaPixels}`);

const oldAtlas = await fs.readFile(priorPngPath);
const currentAtlas = await fs.readFile(pngPath);
const cells = {
  winV02: await sharp(oldAtlas).extract({ left: 16, top: 16, width: 384, height: 384 }).png().toBuffer(),
  winV03: await sharp(currentAtlas).extract({ left: 16, top: 16, width: 384, height: 384 }).png().toBuffer(),
  lossV02: await sharp(oldAtlas).extract({ left: 432, top: 16, width: 384, height: 384 }).png().toBuffer(),
  lossV03: await sharp(currentAtlas).extract({ left: 432, top: 16, width: 384, height: 384 }).png().toBuffer(),
};
const panelMarkup = [];
const overlays = [];
const panels = [
  { key: 'winV02', x: 34, y: 144, title: '守护成功 · v02 原候选', note: '圆章 / 星标 / 绿盾 / 叶冠' },
  { key: 'winV03', x: 868, y: 144, title: '守护成功 · v03 新候选', note: '圆章 / 大勾记 / 绿盾' },
  { key: 'lossV02', x: 34, y: 534, title: '奶蛋失守 · v02 原候选', note: '盾形 / 难过表情 / 小碎片' },
  { key: 'lossV03', x: 868, y: 534, title: '奶蛋失守 · v03 新候选', note: '盾形 / 难过表情 / 大下箭头' },
];
for (let i = 0; i < panels.length; i++) {
  const panel = panels[i];
  const frame = cells[panel.key];
  const main240 = await sharp(frame).resize(240, 240, { fit: 'contain' }).png().toBuffer();
  const color48 = await sharp(frame).resize(48, 48, { fit: 'contain' }).png().toBuffer();
  const gray48 = await sharp(frame).greyscale().ensureAlpha().resize(48, 48, { fit: 'contain' }).png().toBuffer();
  const color96 = await sharp(color48).resize(96, 96, { kernel: 'nearest' }).png().toBuffer();
  const gray96 = await sharp(gray48).resize(96, 96, { kernel: 'nearest' }).png().toBuffer();
  const patternId = `checker-${i}`;
  panelMarkup.push(`<rect x="${panel.x}" y="${panel.y}" width="818" height="350" rx="16" fill="#fffefa" stroke="#d8d3c6" stroke-width="2"/>`);
  panelMarkup.push(`<text x="${panel.x + 26}" y="${panel.y + 39}" class="panelTitle">${panel.title}</text>`);
  panelMarkup.push(`<rect x="${panel.x + 26}" y="${panel.y + 58}" width="240" height="240" rx="6" fill="url(#${patternId})" stroke="#d6d4ce"/>`);
  panelMarkup.push(`<text x="${panel.x + 300}" y="${panel.y + 85}" class="miniHead">48px 实际下采样 · 2×最近邻</text>`);
  panelMarkup.push(`<text x="${panel.x + 309}" y="${panel.y + 110}" class="miniLabel">彩色</text><text x="${panel.x + 426}" y="${panel.y + 110}" class="miniLabel">灰度</text>`);
  panelMarkup.push(`<image x="${panel.x + 302}" y="${panel.y + 118}" width="96" height="96" href="data:image/png;base64,${b64(color96)}"/>`);
  panelMarkup.push(`<image x="${panel.x + 420}" y="${panel.y + 118}" width="96" height="96" href="data:image/png;base64,${b64(gray96)}"/>`);
  panelMarkup.push(`<text x="${panel.x + 300}" y="${panel.y + 262}" class="note">${panel.note}</text>`);
  panelMarkup.push(`<text x="${panel.x + 300}" y="${panel.y + 291}" class="note">完整蛋体；不以内裂缝表示失败</text>`);
  overlays.push({ input: main240, left: panel.x + 26, top: panel.y + 58 });
  panelMarkup.push(`<defs><pattern id="${patternId}" width="18" height="18" patternUnits="userSpaceOnUse"><rect width="18" height="18" fill="#f5f5f2"/><rect width="9" height="9" fill="#dcded9"/><rect x="9" y="9" width="9" height="9" fill="#dcded9"/></pattern></defs>`);
}
const reviewSvg = `<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="1720" height="930" viewBox="0 0 1720 930">
 <rect width="1720" height="930" fill="#f2f0e8"/>
 <text x="42" y="54" class="title">P7 A1 结算徽章对照 · v02 / v03</text>
 <text x="44" y="88" class="subtitle">新候选强化胜利勾记与失守下箭头；保留 v02 对照。检查图形轮廓、48px 彩色与灰度可读性。</text>
 ${panelMarkup.join('\n')}
 <text x="42" y="914" class="footer">透明图集不含文字；两版均为候选稿。奶蛋主体保持完整，最终样式与 HUD 实际位置待后续验收。</text>
 <style>
 text{font-family:'Microsoft YaHei','Noto Sans CJK SC','Arial',sans-serif;fill:#302d27}
 .title{font-size:31px;font-weight:700}.subtitle{font-size:16px;fill:#625d54}.panelTitle{font-size:21px;font-weight:700;fill:#384858}
 .miniHead{font-size:14px;fill:#6b685f}.miniLabel{font-size:12px;fill:#6b685f}.note{font-size:15px;fill:#625d54}.footer{font-size:14px;fill:#777268}
 </style>
</svg>`;
await sharp(Buffer.from(reviewSvg)).png().toBuffer().then((buffer) => sharp(buffer).composite(overlays).png({ compressionLevel: 9 }).toFile(reviewPath));

const sourceHash = await hash(svgPath);
const exportHash = await hash(pngPath);
const rows = [
  { id: 'P7-UI-A1-RESULT-WIN', use: 'Settlement victory: protected milk-egg guardian', index: 0, x: 16, note: 'Circular seal, large mint check mark, smiling egg and intact mint shield; labels are review-only.' },
  { id: 'P7-UI-A1-RESULT-LOSS', use: 'Settlement defeat: milk-egg guardian lost', index: 1, x: 432, note: 'Shield-shaped seal, sad egg with an intact center and large coral down arrow; labels are review-only.' },
];
const fields = ['AssetId','Owner','SourcePath','SourceHash','ExportPath','ExportHash','Version','IntendedUse','Width','Height','FrameCount','FrameIndex','AlphaMode','ColorSpace','ExportPreset','UnrealPath','AtlasX','AtlasY','AtlasW','AtlasH','ReviewStatus','ReuseStatus','EvidencePath','Notes'];
const manifestRows = rows.map((row) => ({
  AssetId: row.id, Owner: 'P7', SourcePath: 'UI/A1/P7_A1_ResultBadgeAtlas_v03.svg', SourceHash: sourceHash,
  ExportPath: 'UI/A1/P7_A1_ResultBadgeAtlas_v03.png', ExportHash: exportHash, Version: 'v03', IntendedUse: row.use,
  Width: '384', Height: '384', FrameCount: '2', FrameIndex: String(row.index), AlphaMode: 'Straight RGBA with transparent background',
  ColorSpace: 'sRGB', ExportPreset: '8-bit transparent PNG; 2-frame horizontal atlas with 16px outer margins and 32px gutter',
  UnrealPath: '/Game/Naiwa/P7/UI/A1/T_P7_A1_ResultBadgeAtlas_v03', AtlasX: String(row.x), AtlasY: '16', AtlasW: '384', AtlasH: '384',
  ReviewStatus: 'A1 candidate v03; user visual review pending; not imported', ReuseStatus: 'P7-exclusive; Common review not started',
  EvidencePath: 'UI/A1/Review/P7_A1_ResultBadgeComparisonReview_v03.png', Notes: row.note,
}));
await fs.writeFile(manifestPath, [fields.join(','), ...manifestRows.map((row) => fields.map((field) => csvEscape(row[field])).join(','))].join('\n') + '\n', 'utf8');

console.log(JSON.stringify({ svg: svgPath, png: pngPath, review: reviewPath, manifest: manifestPath, sourceSha256: sourceHash, exportSha256: exportHash, dimensions: [atlas.info.width, atlas.info.height], channels: atlas.info.channels, gutterAlphaPixels }, null, 2));
