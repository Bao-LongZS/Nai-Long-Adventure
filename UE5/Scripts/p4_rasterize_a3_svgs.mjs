import { createHash } from 'node:crypto';
import { readFile, mkdir, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const sharp = require('sharp');
const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(scriptDir, '../..');
const sourceRoot = path.join(root, 'UE5/SourceAssets/P4/UI/Alpha');
const outputRoot = path.join(sourceRoot, 'RuntimeReady');
const reportPath = path.join(root, 'UE5/SourceAssets/P4/Manifests/P4_A3_RuntimeRaster_manifest_v01.json');

const assets = [
  { id: 'card_icon_bounce_ball', source: 'P4_A3_CardIcon_bounce_ball_v01.svg', output: 'T_P4_A3_CardIcon_bounce_ball.png', size: 512 },
  { id: 'card_icon_rounder_belly', source: 'P4_A3_CardIcon_rounder_belly_v02.svg', output: 'T_P4_A3_CardIcon_rounder_belly.png', size: 512 },
  { id: 'card_icon_cant_hold', source: 'P4_A3_CardIcon_cant_hold_v02.svg', output: 'T_P4_A3_CardIcon_cant_hold.png', size: 512 },
  { id: 'category_mark_bullet', source: 'CategoryMarks/P4_A3_CategoryMark_bullet_v01.svg', output: 'T_P4_A3_CategoryMark_bullet.png', size: 256 },
  { id: 'category_mark_charge', source: 'CategoryMarks/P4_A3_CategoryMark_charge_v01.svg', output: 'T_P4_A3_CategoryMark_charge.png', size: 256 },
  { id: 'category_mark_laugh', source: 'CategoryMarks/P4_A3_CategoryMark_laugh_v01.svg', output: 'T_P4_A3_CategoryMark_laugh.png', size: 256 },
];

const sha256 = (bytes) => createHash('sha256').update(bytes).digest('hex');
await mkdir(outputRoot, { recursive: true });

const entries = [];
for (const asset of assets) {
  const sourcePath = path.join(sourceRoot, asset.source);
  const outputPath = path.join(outputRoot, asset.output);
  const sourceBytes = await readFile(sourcePath);
  const pngBytes = await sharp(sourceBytes, { density: 192 })
    .resize(asset.size, asset.size, {
      fit: 'contain',
      background: { r: 0, g: 0, b: 0, alpha: 0 },
    })
    .png({ compressionLevel: 9, adaptiveFiltering: true })
    .toBuffer();
  const metadata = await sharp(pngBytes).metadata();
  if (metadata.width !== asset.size || metadata.height !== asset.size || metadata.hasAlpha !== true) {
    throw new Error(`Unexpected raster output for ${asset.id}: ${JSON.stringify(metadata)}`);
  }
  await writeFile(outputPath, pngBytes);
  entries.push({
    asset_id: asset.id,
    source_svg: path.relative(root, sourcePath).replaceAll('\\', '/'),
    source_sha256: sha256(sourceBytes),
    output_png: path.relative(root, outputPath).replaceAll('\\', '/'),
    output_sha256: sha256(pngBytes),
    dimensions_px: [asset.size, asset.size],
    alpha: 'straight alpha retained',
    rasterizer: `sharp ${sharp.versions.sharp} / libvips ${sharp.versions.vips}`,
  });
}

const report = {
  schema: 'naiwa.p4.a3_runtime_raster/1',
  version: 'v01',
  status: 'rasterized_pending_ue_import',
  output_directory: path.relative(root, outputRoot).replaceAll('\\', '/'),
  source_svg_preserved: true,
  assets: entries,
};
await writeFile(reportPath, `${JSON.stringify(report, null, 2)}\n`, 'utf8');
console.log(JSON.stringify({ status: report.status, outputs: entries.length, report: path.relative(root, reportPath) }));
