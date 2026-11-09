import './style.css';
import { InputRouter } from './input/input-router';
import { P1SliceScene, type FrameSample } from './render/p1-slice-scene';

const root = document.querySelector<HTMLElement>('#app');
if (!root) throw new Error('Missing #app root element');

root.innerHTML = `
  <main class="app-shell">
    <header class="topbar">
      <div class="brand-lockup">
        <span class="brand-mark" aria-hidden="true">N</span>
        <div>
          <p class="eyebrow">NAI LONG ADVENTURE · WEB NATIVE</p>
          <h1>奶龙冒险 · P1试玩切片</h1>
        </div>
      </div>
      <div class="connection-state"><span class="state-dot"></span><span id="webgl-state">正在检查 WebGL 2</span></div>
    </header>

    <section class="play-layout" aria-label="P1 开场至第一个检查点试玩切片">
      <div class="scene-panel">
        <div class="scene-label"><span>正式 P1 v7 地图 · 开场至 CP01</span><span id="orientation-label">横屏体验最佳</span></div>
        <div id="canvas-wrap" class="canvas-wrap">
          <canvas id="game-canvas" aria-label="P1 开场至 CP01 游戏画面"></canvas>
          <div id="unsupported" class="unsupported-message" hidden>
            <strong>此浏览器未提供 WebGL 2</strong>
            <span>请使用较新的 Chrome、Edge 或 Safari 浏览器。</span>
          </div>
          <div class="scene-hint">正在加载正式关卡与奶龙模型…</div>
        </div>
        <div class="scene-footer"><span id="input-readout">等待键盘或触屏输入</span><span id="fps-readout">帧时间 --</span></div>
      </div>

      <aside class="side-panel">
        <div class="panel-heading"><p class="eyebrow">网页原生版施工</p><h2>P1 开场 → CP01</h2></div>
        <p class="panel-copy">P1 v7 关卡几何与当前 UE 游戏实际使用的奶龙骨骼网格、跑步动画均通过只读导出接入。可移动、跳跃、触发危险并复活；玩法规则仍需逐项对齐 UE。</p>
        <div class="device-card">
          <div><span class="mini-label">浏览器</span><strong id="browser-label">读取中…</strong></div>
          <div><span class="mini-label">屏幕 / DPR</span><strong id="screen-label">读取中…</strong></div>
        </div>
        <div class="benchmark-card">
          <div class="benchmark-copy"><span class="mini-label">目标画布</span><strong>约 1280 × 720 等效像素</strong></div>
          <button id="benchmark-button" class="primary-button" type="button" disabled>开始 5 分钟帧基准</button>
          <p id="benchmark-status" class="benchmark-status" aria-live="polite">实机测试完成后记录型号、系统与浏览器版本。</p>
        </div>
        <div class="controls-hint"><span class="mini-label">键盘</span><strong>先点游戏画面聚焦 · A / D 移动 · Space 跳跃 · Shift 切换走/跑 · P 暂停 · R 重开</strong></div>
        <div class="mobile-controls" aria-label="触屏控制">
          <div class="direction-pad">
            <button data-control="left" type="button" aria-label="向左移动">←</button>
            <button data-control="right" type="button" aria-label="向右移动">→</button>
          </div>
          <button data-control="jump" class="jump-button" type="button">跳跃</button>
          <button data-control="gait" id="gait-button" class="gait-button" type="button" aria-label="切换步态" aria-pressed="true">跑步</button>
          <button data-control="pause" class="pause-button" type="button" aria-label="暂停">Ⅱ</button>
        </div>
        <p class="non-production-note">角色模型和跑步骨架动画来自 UE 实际游戏资产；P1 机关、碰撞和复活规则仍是网页切片实现。</p>
      </aside>
    </section>
    <footer class="footer-note">局域网可玩切片 · <a href="/p2-probe.html">打开 P2 物理探针</a> · 目标支持 Android Chrome / iPhone Safari / PC Chrome / Edge</footer>
  </main>
`;

const canvas = root.querySelector<HTMLCanvasElement>('#game-canvas');
const canvasWrap = root.querySelector<HTMLElement>('#canvas-wrap');
const unsupported = root.querySelector<HTMLElement>('#unsupported');
const webglState = root.querySelector<HTMLElement>('#webgl-state');
const benchmarkButton = root.querySelector<HTMLButtonElement>('#benchmark-button');
const benchmarkStatus = root.querySelector<HTMLElement>('#benchmark-status');
const inputReadout = root.querySelector<HTMLElement>('#input-readout');
const browserLabel = root.querySelector<HTMLElement>('#browser-label');
const screenLabel = root.querySelector<HTMLElement>('#screen-label');
const fpsReadout = root.querySelector<HTMLElement>('#fps-readout');
const gaitButton = root.querySelector<HTMLButtonElement>('#gait-button');

if (!canvas || !canvasWrap || !unsupported || !webglState || !benchmarkButton || !benchmarkStatus || !inputReadout || !browserLabel || !screenLabel || !fpsReadout || !gaitButton) {
  throw new Error('G0 shell DOM is incomplete');
}

canvas.tabIndex = 0;

const userAgentData = (navigator as Navigator & { userAgentData?: { brands?: Array<{ brand: string }> } }).userAgentData;
browserLabel.textContent = `${userAgentData?.brands?.[0]?.brand ?? detectBrowser()} · ${navigator.userAgent.match(/(?:Chrome|CriOS|Edg|EdgiOS|Safari)\/[\d.]+/)?.[0] ?? '版本待识别'}`;
screenLabel.textContent = `${screen.width} × ${screen.height} · DPR ${window.devicePixelRatio.toFixed(2)}`;

let input: InputRouter | undefined;
let scene: P1SliceScene | undefined;
let latestSample: FrameSample | undefined;
let inputFeedbackUntil = 0;
const hasWebGL2 = Boolean(canvas.getContext('webgl2'));

if (!hasWebGL2) {
  unsupported.hidden = false;
  canvas.hidden = true;
  webglState.textContent = 'WebGL 2 不可用';
  webglState.closest('.connection-state')?.classList.add('state-error');
} else {
  webglState.textContent = 'WebGL 2 已就绪';
  webglState.closest('.connection-state')?.classList.add('state-ready');
  input = new InputRouter(root);
  inputReadout.textContent = '点击游戏画面以聚焦键盘输入';
  canvas.addEventListener('pointerdown', () => canvas.focus({ preventScroll: true }));
  canvas.addEventListener('focus', () => {
    inputReadout.textContent = '键盘已聚焦 · Space 跳跃 · A/D 移动';
  });
  scene = new P1SliceScene(canvas, canvasWrap, (message) => {
    benchmarkStatus.textContent = message;
  }, (sample) => {
    latestSample = sample;
    benchmarkButton.disabled = false;
    benchmarkButton.textContent = '重新采样 5 分钟';
    const result = {
      schemaVersion: 1,
      kind: 'g0-frame-time',
      userAgent: navigator.userAgent,
      screen: { width: screen.width, height: screen.height, dpr: window.devicePixelRatio },
      renderTarget: '1280x720 equivalent pixel count',
      sampleDurationSeconds: sample.elapsedSeconds,
      wallDurationSeconds: sample.wallDurationSeconds,
      frameCount: sample.sampleCount,
      frameTimeP50Ms: Number(sample.frameTimeP50Ms.toFixed(2)),
      frameTimeP95Ms: Number(sample.frameTimeP95Ms.toFixed(2)),
      capturedAt: new Date().toISOString(),
    };
    downloadJson('g0-frame-benchmark.json', result);
  });
  gaitButton.disabled = false;
  benchmarkButton.disabled = false;
  benchmarkButton.addEventListener('click', () => {
    benchmarkButton.disabled = true;
    benchmarkButton.textContent = '正在采样…';
    scene?.beginBenchmark();
  });

  canvas.addEventListener('webglcontextlost', (event) => {
    event.preventDefault();
    webglState.textContent = 'WebGL context 丢失';
    webglState.closest('.connection-state')?.classList.add('state-error');
    benchmarkStatus.textContent = '浏览器回收了 WebGL context；该设备不能通过 G0 稳定性门。';
  });
}

let lastHudAt = 0;
const updateHud = (now: number): void => {
  const command = input?.readFrame();
  if (command && scene) {
    scene.applyInput(command);
    gaitButton.textContent = scene.isRunningGait ? '跑步' : '走路';
    gaitButton.setAttribute('aria-pressed', String(scene.isRunningGait));
    const move = command.moveX === 0 ? '' : command.moveX < 0 ? '向左' : '向右';
    const actions = [
      move,
      command.jumpPressed ? 'Space 输入已读取' : '',
      command.gaitPressed ? `切换到 ${scene.isRunningGait ? 'Run300' : 'Walk150'}` : '',
      command.pausePressed ? '暂停' : '',
      command.restartPressed ? '重开' : '',
    ].filter(Boolean);
    if (actions.length > 0) {
      inputReadout.textContent = actions.join(' · ');
      inputFeedbackUntil = now + 1200;
    } else if (now >= inputFeedbackUntil) {
      inputReadout.textContent = document.activeElement === canvas
        ? `键盘已聚焦 · ${scene.isRunningGait ? 'Run300' : 'Walk150'} · Shift 切换步态`
        : '点击游戏画面以聚焦键盘输入';
    }
  }

  if (now - lastHudAt > 250) {
    const memory = (performance as Performance & { memory?: { usedJSHeapSize: number } }).memory;
    fpsReadout.textContent = latestSample
      ? `最近采样 p95 ${latestSample.frameTimeP95Ms.toFixed(1)} ms`
      : memory
        ? `JS heap ${(memory.usedJSHeapSize / 1024 / 1024).toFixed(0)} MB`
        : '帧时间采样未开始';
    lastHudAt = now;
  }
  requestAnimationFrame(updateHud);
};
requestAnimationFrame(updateHud);

const orientationLabel = root.querySelector<HTMLElement>('#orientation-label');
const updateOrientation = (): void => {
  if (!orientationLabel) return;
  orientationLabel.textContent = window.innerHeight > window.innerWidth ? '建议旋转设备横屏' : '横屏体验最佳';
};
window.addEventListener('resize', updateOrientation);
updateOrientation();

function detectBrowser(): string {
  const ua = navigator.userAgent;
  if (/Edg\//.test(ua)) return 'Edge';
  if (/Chrome|CriOS/.test(ua)) return 'Chrome';
  if (/Safari/.test(ua)) return 'Safari';
  return '未知浏览器';
}

function downloadJson(filename: string, value: unknown): void {
  const blob = new Blob([JSON.stringify(value, null, 2)], { type: 'application/json' });
  const url = URL.createObjectURL(blob);
  const anchor = document.createElement('a');
  anchor.href = url;
  anchor.download = filename;
  anchor.click();
  URL.revokeObjectURL(url);
}

window.addEventListener('pagehide', () => {
  input?.dispose();
  scene?.dispose();
});
