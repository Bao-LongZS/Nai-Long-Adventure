import { gzipSync } from 'node:zlib';
import { readFile, stat, writeFile, rm } from 'node:fs/promises';

const root = new URL('../public/assets/', import.meta.url);
const outputs = [];
for (const name of ['naiwa-character-e2.glb', 'naiwa-run.glb']) {
  const source = new URL(name, root);
  const target = new URL(`${name}.gz`, root);
  const raw = await readFile(source);
  const compressed = gzipSync(raw, { level: 9, mtime: 0 });
  await writeFile(target, compressed);
  await rm(source);
  outputs.push({ name: `${name}.gz`, rawBytes: raw.byteLength, bytes: (await stat(target)).size });
}
process.stdout.write(`${JSON.stringify({ status: 'PASS_GZIP_ASSETS', assets: outputs }, null, 2)}\n`);
