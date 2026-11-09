import { defineConfig } from 'vite';
import { fileURLToPath } from 'node:url';
import { readFileSync } from 'node:fs';
import { gunzipSync } from 'node:zlib';

const decodedAssetCache = new Map<string, Buffer>();
const compressedAssets = new Map([
  ['/assets/naiwa-character-e2.glb', 'naiwa-character-e2.glb.gz'],
  ['/assets/naiwa-run.glb', 'naiwa-run.glb.gz'],
]);

function serveCompressedGltf(request: import('node:http').IncomingMessage, response: import('node:http').ServerResponse, next: (error?: Error) => void) {
  const pathname = request.url?.split('?')[0];
  const assetName = pathname ? compressedAssets.get(pathname) : undefined;
  if (!assetName || (request.method !== 'GET' && request.method !== 'HEAD')) {
    next();
    return;
  }
  try {
    let decoded = decodedAssetCache.get(assetName);
    if (!decoded) {
      decoded = gunzipSync(readFileSync(new URL(`./public/assets/${assetName}`, import.meta.url)));
      decodedAssetCache.set(assetName, decoded);
    }
    response.statusCode = 200;
    response.setHeader('Content-Type', 'model/gltf-binary');
    response.setHeader('Content-Length', decoded.byteLength);
    response.setHeader('Cache-Control', 'public, max-age=31536000, immutable');
    if (request.method === 'HEAD') response.end();
    else response.end(decoded);
  } catch (error) {
    next(error instanceof Error ? error : new Error(String(error)));
  }
}

export default defineConfig({
  plugins: [{
    name: 'webnative-gltf-gzip-sidecar',
    configureServer(server) {
      server.middlewares.use(serveCompressedGltf);
    },
    configurePreviewServer(server) {
      server.middlewares.use(serveCompressedGltf);
    },
  }],
  server: {
    host: '0.0.0.0',
    strictPort: true,
  },
  preview: {
    host: '0.0.0.0',
    strictPort: true,
  },
  build: {
    target: 'es2022',
    sourcemap: true,
    rollupOptions: {
      input: {
        main: fileURLToPath(new URL('./index.html', import.meta.url)),
        p2Probe: fileURLToPath(new URL('./p2-probe.html', import.meta.url)),
      },
    },
  },
});
