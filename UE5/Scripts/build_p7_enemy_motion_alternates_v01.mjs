import fs from 'node:fs/promises';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const sharp = require('sharp');
const root = path.resolve(import.meta.dirname, '..', '..');
const sourceRoot = path.join(root, 'UE5', 'SourceAssets', 'P7');
const enemyRoot = path.join(sourceRoot, 'Enemies', 'A1');
const generatedRoot = path.join(enemyRoot, 'GenerationSources', 'v02');
const reviewPath = path.join(enemyRoot, 'Review', 'P7_A1_EnemyMotionAlternateReview_v01.png');
const manifestPath = path.join(sourceRoot, 'Manifests', 'A1', 'P7_A1_EnemyMotionAlternateAssets_v01.csv');

const specs = [
  {
    role: 'Normal',
    assetId: 'P7-ENM-A1-NORMAL-F02-ALT',
    frame0: path.join(enemyRoot, 'P7_A1_Enemy_Normal_v01.png'),
    current: path.join(enemyRoot, 'P7_A1_Enemy_Normal_Frame02_v01.png'),
    sourceName: 'P7_A1_Enemy_Normal_Frame02_v02_Source.png',
    outputName: 'P7_A1_Enemy_Normal_Frame02_v02.png',
    frame3: path.join(enemyRoot, 'P7_A1_Enemy_Normal_Frame03_v01.png'),
    label: '普通型 · 低重心过步',
    prompt: '保持普通型珊瑚红圆胖身份，只调整姿势：降低并轻微前倾身体，脚收至身体正下方形成明显压缩过步，前后臂略微错开摆动；完整保留奶油色脸腹、卷曲双色冠、青色耳鳍、琥珀眼、深酒红描边和朝右四分之三视角。透明背景，柔和 3D 卡通游戏精灵。',
  },
  {
    role: 'Jammer',
    assetId: 'P7-ENM-A1-JAMMER-F02-ALT',
    frame0: path.join(sourceRoot, 'Enemies', 'A0', 'P7_A0_Enemy_Jammer_v01.png'),
    current: path.join(enemyRoot, 'P7_A1_Enemy_Jammer_Frame02_v01.png'),
    sourceName: 'P7_A1_Enemy_Jammer_Frame02_v02_Source.png',
    outputName: 'P7_A1_Enemy_Jammer_Frame02_v02.png',
    frame3: path.join(enemyRoot, 'P7_A1_Enemy_Jammer_Frame03_v01.png'),
    label: '干扰型 · 闪电压步',
    prompt: '保持干扰型淡紫圆团身份，只调整姿势：身体压低约15%并前倾，后脚并拢收在腹下，近侧前爪抬起、远侧前爪向前伸，头冠轻微向后拖曳；脸和橙色闪电标记完整且不变形，朝右四分之三视角。透明背景，柔和 3D 卡通游戏精灵。',
  },
];

const sha256 = async (file) => createHash('sha256').update(await fs.readFile(file)).digest('hex');
const escapeXml = (text) => String(text).replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;').replaceAll('"', '&quot;');

await fs.mkdir(path.dirname(reviewPath), { recursive: true });
const entries = [];
for (const spec of specs) {
  const generatedSource = path.join(generatedRoot, spec.sourceName);
  const output = path.join(enemyRoot, spec.outputName);
  const generatedInfo = await sharp(generatedSource).metadata();
  if (!generatedInfo.width || !generatedInfo.height || !generatedInfo.hasAlpha) {
    throw new Error(`Generated source is missing dimensions or true alpha: ${generatedSource}`);
  }
  if (generatedInfo.width !== generatedInfo.height) {
    throw new Error(`Expected a square generated sprite: ${generatedInfo.width}x${generatedInfo.height}`);
  }
  await sharp(generatedSource)
    .ensureAlpha()
    .resize(1254, 1254, { fit: 'contain', background: { r: 0, g: 0, b: 0, alpha: 0 }, kernel: 'lanczos3' })
    .png({ compressionLevel: 9 })
    .toFile(output);
  const outputInfo = await sharp(output).metadata();
  if (outputInfo.width !== 1254 || outputInfo.height !== 1254 || !outputInfo.hasAlpha) {
    throw new Error(`Unexpected normalized sprite metadata for ${output}`);
  }
  entries.push({ ...spec, generatedSource, output, generatedInfo, outputInfo });
}

const cardWidth = 406;
const cardHeight = 306;
const left = 38;
const gap = 16;
const top = 124;
const rowGap = 18;
const cardSvg = [];
const overlays = [];
const cardEntries = [
  { role: 'Normal', key: 'frame0', title: 'F00 · 已通过基准' },
  { role: 'Normal', key: 'current', title: 'F01 · 现有候选 v01' },
  { role: 'Normal', key: 'alt', title: 'F01 · 新压步候选 v02' },
  { role: 'Normal', key: 'frame3', title: 'F02 · 现有伸步 v01' },
  { role: 'Jammer', key: 'frame0', title: 'F00 · 已通过基准' },
  { role: 'Jammer', key: 'current', title: 'F01 · 现有候选 v01' },
  { role: 'Jammer', key: 'alt', title: 'F01 · 新压步候选 v02' },
  { role: 'Jammer', key: 'frame3', title: 'F02 · 现有伸步 v01' },
];

for (let index = 0; index < cardEntries.length; index++) {
  const card = cardEntries[index];
  const group = entries.find((spec) => spec.role === card.role);
  const imagePath = card.key === 'alt' ? group.output : group[card.key];
  const buffer = await fs.readFile(imagePath);
  const x = left + (index % 4) * (cardWidth + gap);
  const y = top + Math.floor(index / 4) * (cardHeight + rowGap);
  const title = card.key === 'alt' ? `${card.role === 'Normal' ? '普通型' : '干扰型'} · 新候选 v02` : card.title;
  const large = await sharp(buffer).resize(196, 196, { fit: 'contain' }).png().toBuffer();
  const color48 = await sharp(buffer).resize(48, 48, { fit: 'contain' }).png().toBuffer();
  const gray48 = await sharp(buffer).greyscale().ensureAlpha().resize(48, 48, { fit: 'contain' }).png().toBuffer();
  const color = await sharp(color48).resize(96, 96, { kernel: 'nearest' }).png().toBuffer();
  const gray = await sharp(gray48).resize(96, 96, { kernel: 'nearest' }).png().toBuffer();
  const patternId = `checker-${index}`;
  cardSvg.push(`<rect x="${x}" y="${y}" width="${cardWidth}" height="${cardHeight}" rx="12" fill="#fffefa" stroke="#d8d3c6" stroke-width="2"/>`);
  cardSvg.push(`<text x="${x + 18}" y="${y + 31}" class="cardTitle">${escapeXml(title)}</text>`);
  cardSvg.push(`<rect x="${x + 18}" y="${y + 47}" width="218" height="218" rx="5" fill="url(#${patternId})" stroke="#d6d4ce"/>`);
  cardSvg.push(`<text x="${x + 248}" y="${y + 66}" class="miniHead">48 px 下采样</text>`);
  cardSvg.push(`<text x="${x + 259}" y="${y + 89}" class="miniLabel">彩色</text><text x="${x + 332}" y="${y + 89}" class="miniLabel">灰度</text>`);
  cardSvg.push(`<text x="${x + 18}" y="${y + 292}" class="fileLabel">${escapeXml(path.basename(imagePath))}</text>`);
  overlays.push({ input: large, left: x + 29, top: y + 58 });
  overlays.push({ input: color, left: x + 249, top: y + 101 });
  overlays.push({ input: gray, left: x + 322, top: y + 101 });
  cardSvg.push(`<defs><pattern id="${patternId}" width="18" height="18" patternUnits="userSpaceOnUse"><rect width="18" height="18" fill="#f5f5f2"/><rect width="9" height="9" fill="#dcded9"/><rect x="9" y="9" width="9" height="9" fill="#dcded9"/></pattern></defs>`);
}

const boardSvg = `<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="1730" height="820" viewBox="0 0 1730 820">
 <rect width="1730" height="820" fill="#f2f0e8"/>
 <text x="42" y="53" class="title">P7 A1 敌人动作候选 · 过步姿势 v02</text>
 <text x="44" y="88" class="subtitle">橙框标出新生成的压缩过步候选；保留 v01 与已通过基准不动。比较原尺寸、48px 彩色与灰度可读性。</text>
 <text x="42" y="116" class="role">普通型 · 低重心、短促过步</text>
 <text x="42" y="442" class="role">干扰型 · 闪电身份、紧凑压步</text>
 ${cardSvg.join('\n')}
 <text x="42" y="796" class="footer">新图只作为动作备选评审资产；未覆盖当前 v01 帧、未导入 UE，帧时长和游戏内比例仍待后续确认。</text>
 <style>
  text{font-family:'Microsoft YaHei','Noto Sans CJK SC','Arial',sans-serif;fill:#302d27}
  .title{font-size:30px;font-weight:700}.subtitle{font-size:16px;fill:#625d54}.role{font-size:18px;font-weight:700;fill:#527564}
  .cardTitle{font-size:16px;font-weight:700;fill:#384858}.miniHead{font-size:12px;fill:#6b685f}.miniLabel{font-size:11px;fill:#6b685f}.fileLabel{font-size:10px;fill:#78746a}.footer{font-size:13px;fill:#777268}
 </style>
</svg>`;

const reviewBase = await sharp(Buffer.from(boardSvg)).png().toBuffer();
await sharp(reviewBase).composite(overlays).png({ compressionLevel: 9 }).toFile(reviewPath);

const header = ['AssetId','Owner','Role','FrameIndex','SourceImagePath','SourceImageSHA256','ExportPath','ExportSHA256','Width','Height','Alpha','IntendedUse','ReferenceFrame0','AlternativeTo','Version','GenerationMethod','ReviewStatus','UnrealPath','ReviewBoard','PromptSummary'];
const csvEscape = (value) => `"${String(value).replaceAll('"', '""')}"`;
const rows = await Promise.all(entries.map(async (entry) => ({
  AssetId: entry.assetId,
  Owner: 'P7',
  Role: entry.role,
  FrameIndex: '1',
  SourceImagePath: path.relative(sourceRoot, entry.generatedSource).replaceAll('\\', '/'),
  SourceImageSHA256: await sha256(entry.generatedSource),
  ExportPath: path.relative(sourceRoot, entry.output).replaceAll('\\', '/'),
  ExportSHA256: await sha256(entry.output),
  Width: '1254',
  Height: '1254',
  Alpha: 'Straight RGBA transparent',
  IntendedUse: `Optional alternate frame 1 for ${entry.role.toLowerCase()} enemy motion loop`,
  ReferenceFrame0: path.relative(sourceRoot, entry.frame0).replaceAll('\\', '/'),
  AlternativeTo: path.relative(sourceRoot, entry.current).replaceAll('\\', '/'),
  Version: 'v02',
  GenerationMethod: 'Built-in ImageGen; identity-preserving pose-only variation; resized to 1254x1254 with Lanczos3',
  ReviewStatus: 'Candidate; pending user visual approval; not imported into Unreal',
  UnrealPath: '',
  ReviewBoard: 'Enemies/A1/Review/P7_A1_EnemyMotionAlternateReview_v01.png',
  PromptSummary: entry.prompt,
})));
await fs.writeFile(manifestPath, [header.join(','), ...rows.map((row) => header.map((key) => csvEscape(row[key])).join(','))].join('\n') + '\n', 'utf8');

console.log(JSON.stringify({ review: reviewPath, manifest: manifestPath, assets: entries.map((entry) => ({ role: entry.role, source: entry.generatedSource, export: entry.output, generatedDimensions: [entry.generatedInfo.width, entry.generatedInfo.height], outputDimensions: [entry.outputInfo.width, entry.outputInfo.height], hasAlpha: entry.outputInfo.hasAlpha })) }, null, 2));
