import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import type * as RapierTypes from '@dimforge/rapier3d-compat';
import './p2-probe.css';

type RapierApi = typeof import('@dimforge/rapier3d-compat');

interface P2Box {
  id: string;
  minX: number;
  maxX: number;
  minZ: number;
  maxZ: number;
  isGoal: boolean;
  breakable: boolean;
  role: 'Board' | 'Column';
}

interface BlockMaterial {
  densityKgPerM3: number;
  friction: number;
  restitution: number;
  linearDamping: number;
  angularDamping: number;
  simulate: boolean;
}

interface P2Snapshot {
  schemaVersion: number;
  levelId: string;
  maxX: number;
  coordinateSystem: string;
  boxes: P2Box[];
  physicsProfile: {
    gravityMPerS2: number;
    depthMeters: number;
    spawnGapMeters: number;
    minBlockDimensionMeters: number;
    maxSubstepDeltaTime: number;
    maxSubsteps: number;
    stabilityTestSeconds: number;
    stabilityMaxDriftCm: number;
    baseEggMassKg: number;
    eggRestitution: number;
    eggFriction: number;
    projectileSubstepSeconds: number;
    projectileMaxFlightSeconds: number;
    projectileTangentRetention: number;
    projectileBounceSeparationCm: number;
    blockMaterials: Record<string, BlockMaterial>;
  };
  sourceHashes: Record<string, string>;
}

interface BlockView {
  id: string;
  body: RapierTypes.RigidBody;
  mesh: THREE.Mesh;
  initialX: number;
  initialY: number;
  initialYaw: number;
}

interface TraceEvent {
  timeSeconds: number;
  kind: string;
  [key: string]: string | number | boolean | undefined;
}

interface ProbeRecord {
  schemaVersion: 1;
  kind: 'p2-web-physics-probe';
  engine: string;
  levelId: string;
  caseId: string;
  sourceHashes: Record<string, string>;
  profile: {
    fixedStepSeconds: number;
    gravityMPerS2: number;
    depthMeters: number;
    projectileMassKg: number;
    projectileRadiusMeters: number;
  };
  result: Record<string, unknown>;
  events: TraceEvent[];
  completedAt: string;
}

const FIXED_DT = 1 / 120;
const PROJECTILE_RADIUS_M = 0.44;
const PROJECTILE_START = { x: 0, y: 1.8, z: 0 };
const IDENTITY_ROTATION = { x: 0, y: 0, z: 0, w: 1 };
const PROBE_BUTTONS = ['stability', 'hit', 'miss', 'download'] as const;

type ProbeButton = typeof PROBE_BUTTONS[number];

const root = document.querySelector<HTMLElement>('#probe-app');
if (!root) throw new Error('Missing #probe-app root element');

root.innerHTML = `
  <main class="probe-shell">
    <header class="probe-header">
      <div class="probe-brand"><span class="probe-mark" aria-hidden="true">N</span><div><small>NAI LONG ADVENTURE · WEB NATIVE</small><h1>P2 物理探针</h1></div></div>
      <div id="rapier-state" class="probe-state"><span class="probe-dot"></span><span>正在启动 Rapier</span></div>
    </header>
    <section class="probe-layout">
      <div class="probe-canvas-card">
        <div class="probe-canvas-title"><strong>2-1 正式结构快照 · Rapier 3D WASM</strong><span id="probe-clock">等待数据</span></div>
        <div id="probe-canvas-wrap" class="probe-canvas-wrap">
          <canvas id="p2-canvas" aria-label="P2 2-1 结构物理探针画面"></canvas>
          <div id="probe-hint" class="probe-canvas-hint">正在加载 2-1 结构和物理参数…</div>
        </div>
        <div class="probe-footer"><a href="/">返回 P1 试玩切片</a><span>物理模拟以固定 1/120 秒步长推进</span></div>
      </div>
      <aside class="probe-sidebar">
        <div><span class="probe-kicker">G0 · P2 PHYSICS</span><h2>塔楼稳定与弹丸扫掠</h2></div>
        <p class="probe-copy">结构尺寸、密度、摩擦、弹性、重力和子步来自 UE 只读导出的 P2 2-1 快照。发射时使用 UE 基础蛋网格与材质，碰撞仍为 UE 的 44 cm 球体；弹丸按固定步形状扫掠，命中后把反向动量冲量交给 Rapier 刚体。</p>
        <div class="probe-actions">
          <button class="probe-button primary" data-probe="stability" type="button" disabled>运行 5 秒静置稳定性</button>
          <button class="probe-button" data-probe="hit" type="button" disabled>发射命中用例 · 0°</button>
          <button class="probe-button" data-probe="miss" type="button" disabled>发射未命中用例 · 45°</button>
          <button class="probe-button" data-probe="download" type="button" disabled>下载事件轨迹 JSON</button>
        </div>
        <div class="probe-result"><span class="probe-kicker">最近结果</span><div id="probe-result" class="probe-value">等待运行探针</div></div>
        <div class="probe-metrics">
          <div class="probe-metric"><span class="probe-kicker">最大结构位移</span><strong id="drift-metric">—</strong></div>
          <div class="probe-metric"><span class="probe-kicker">最近命中</span><strong id="hit-metric">—</strong></div>
        </div>
        <div id="probe-log" class="probe-log" role="log" aria-live="polite"></div>
        <p class="probe-note">Rapier 结果是网页原型证据，不等于 Chaos 逐浮点复现。手机浏览器与 UE 对照仍须单独实测。</p>
      </aside>
    </section>
  </main>
`;

const canvas = requireElement<HTMLCanvasElement>('#p2-canvas');
const canvasWrap = requireElement<HTMLElement>('#probe-canvas-wrap');
const rapierState = requireElement<HTMLElement>('#rapier-state');
const hint = requireElement<HTMLElement>('#probe-hint');
const clockLabel = requireElement<HTMLElement>('#probe-clock');
const resultLabel = requireElement<HTMLElement>('#probe-result');
const driftMetric = requireElement<HTMLElement>('#drift-metric');
const hitMetric = requireElement<HTMLElement>('#hit-metric');
const log = requireElement<HTMLElement>('#probe-log');
const buttons = new Map<ProbeButton, HTMLButtonElement>();
for (const button of root.querySelectorAll<HTMLButtonElement>('[data-probe]')) {
  const action = button.dataset.probe as ProbeButton | undefined;
  if (action && PROBE_BUTTONS.includes(action)) buttons.set(action, button);
}

const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, powerPreference: 'high-performance' });
renderer.outputColorSpace = THREE.SRGBColorSpace;
renderer.setClearColor('#bdd7e9');
const scene = new THREE.Scene();
scene.background = new THREE.Color('#bdd7e9');
scene.add(new THREE.HemisphereLight('#f4fbff', '#334260', 2.0));
const keyLight = new THREE.DirectionalLight('#fff2cf', 2.4);
keyLight.position.set(-4, 9, 12);
scene.add(keyLight);
const camera = new THREE.OrthographicCamera(-10, 10, 6, -6, 0.1, 100);
camera.position.set(9, 3, 26);
camera.lookAt(9, 3, 0);
const geometryGroup = new THREE.Group();
scene.add(geometryGroup);
const projectileVisual = new THREE.Group();
projectileVisual.name = 'UE P2 base egg visual';
projectileVisual.visible = false;
scene.add(projectileVisual);

let snapshot: P2Snapshot | undefined;
let rapierApi: RapierApi | undefined;
let projectileVisualPromise: Promise<void> | undefined;
let physics: RapierTypes.World | undefined;
let eventQueue: RapierTypes.EventQueue | undefined;
let resizeObserver: ResizeObserver | undefined;
let animationFrame = 0;
let lastFrameAt = 0;
let accumulator = 0;
let simulationTime = 0;
let simulationMode: 'idle' | 'stability' | 'shot' = 'idle';
let activeScenario: 'hit' | 'miss' | undefined;
let activeTarget = 'P.col';
let projectilePosition = { ...PROJECTILE_START };
let projectileVelocity = { x: 0, y: 0, z: 0 };
let projectileShape: RapierTypes.Ball | undefined;
let shotHitIds: string[] = [];
let traceEvents: TraceEvent[] = [];
let colliderIds = new Map<number, string>();
let colliderBodies = new Map<number, RapierTypes.RigidBody | null>();
let blocks: BlockView[] = [];
let completedRecords: ProbeRecord[] = [];
let disposed = false;

const setStatus = (text: string, state: 'loading' | 'ready' | 'error' = 'loading'): void => {
  rapierState.classList.toggle('ready', state === 'ready');
  rapierState.classList.toggle('error', state === 'error');
  rapierState.lastElementChild!.textContent = text;
};

function requireElement<T extends HTMLElement>(selector: string): T {
  const element = document.querySelector<T>(selector);
  if (!element) throw new Error(`Missing probe DOM element: ${selector}`);
  return element;
}

function pushLog(message: string): void {
  const line = document.createElement('div');
  line.textContent = `[${simulationTime.toFixed(2)}s] ${message}`;
  log.prepend(line);
  while (log.childElementCount > 80) log.lastElementChild?.remove();
}

function materialFor(box: P2Box): { role: string; material: BlockMaterial; dynamic: boolean } {
  const foundation = box.id.endsWith('.base') || box.id.endsWith('.wall');
  if (box.isGoal) return { role: 'Goal', material: snapshot!.physicsProfile.blockMaterials.Goal, dynamic: false };
  const role = foundation ? 'Foundation' : box.role;
  const material = snapshot!.physicsProfile.blockMaterials[role];
  return { role, material, dynamic: !foundation && material.simulate };
}

function colorFor(role: string): string {
  switch (role) {
    case 'Foundation': return '#596b82';
    case 'Column': return '#ad895c';
    case 'Board': return '#7a9a81';
    case 'Goal': return '#edc66f';
    default: return '#78889c';
  }
}

function createBoxMesh(box: P2Box, width: number, height: number, depth: number, role: string): THREE.Mesh {
  const material = new THREE.MeshStandardMaterial({ color: colorFor(role), roughness: 0.82, metalness: 0.01 });
  const mesh = new THREE.Mesh(new THREE.BoxGeometry(width, height, depth), material);
  mesh.name = box.id;
  geometryGroup.add(mesh);
  return mesh;
}

function clearWorld(): void {
  if (eventQueue) eventQueue.free();
  physics?.free();
  physics = undefined;
  eventQueue = undefined;
  blocks = [];
  colliderIds = new Map();
  colliderBodies = new Map();
  for (const child of [...geometryGroup.children]) {
    geometryGroup.remove(child);
    if (child instanceof THREE.Mesh) {
      child.geometry.dispose();
      const materials = Array.isArray(child.material) ? child.material : [child.material];
      for (const material of materials) material.dispose();
    }
  }
  projectileVisual.visible = false;
  projectileShape = undefined;
}

function createCollider(boxId: string, collider: RapierTypes.Collider, body: RapierTypes.RigidBody | null): void {
  colliderIds.set(collider.handle, boxId);
  colliderBodies.set(collider.handle, body);
}

function buildWorld(): void {
  if (!snapshot) throw new Error('P2 level data is not ready');
  const R = requireRapier();
  clearWorld();
  const profile = snapshot.physicsProfile;
  physics = new R.World({ x: 0, y: -profile.gravityMPerS2, z: 0 });
  physics.timestep = FIXED_DT;
  eventQueue = new R.EventQueue(true);
  traceEvents = [];
  shotHitIds = [];
  activeScenario = undefined;
  simulationTime = 0;
  accumulator = 0;

  // The runtime's level plane is supported by an invisible world floor at z=0.
  const floorBody = physics.createRigidBody(R.RigidBodyDesc.fixed().setTranslation(snapshot.maxX / 2, -0.12, 0));
  const floorCollider = physics.createCollider(
    R.ColliderDesc.cuboid(snapshot.maxX / 2 + 4, 0.12, profile.depthMeters / 2)
      .setFriction(profile.blockMaterials.Foundation.friction)
      .setActiveEvents(R.ActiveEvents.COLLISION_EVENTS),
    floorBody,
  );
  createCollider('WorldGround', floorCollider, floorBody);
  const floorMesh = new THREE.Mesh(
    new THREE.BoxGeometry(snapshot.maxX + 8, 0.24, profile.depthMeters),
    new THREE.MeshStandardMaterial({ color: '#4d6374', roughness: 0.9 }),
  );
  floorMesh.position.set(snapshot.maxX / 2, -0.12, 0);
  geometryGroup.add(floorMesh);

  for (const box of snapshot.boxes) {
    const rawWidth = box.maxX - box.minX;
    const rawHeight = box.maxZ - box.minZ;
    const canApplyGap = rawWidth > profile.minBlockDimensionMeters + profile.spawnGapMeters * 2
      && rawHeight > profile.minBlockDimensionMeters + profile.spawnGapMeters * 2;
    const gap = canApplyGap ? profile.spawnGapMeters : 0;
    const width = Math.max(profile.minBlockDimensionMeters, rawWidth - gap);
    const height = Math.max(profile.minBlockDimensionMeters, rawHeight - gap);
    const centerX = (box.minX + box.maxX) / 2;
    const centerY = (box.minZ + box.maxZ) / 2;
    const { role, material, dynamic } = materialFor(box);
    const bodyDesc = dynamic
      ? R.RigidBodyDesc.dynamic()
        .setTranslation(centerX, centerY, 0)
        .setLinearDamping(material.linearDamping)
        .setAngularDamping(material.angularDamping)
      : R.RigidBodyDesc.fixed().setTranslation(centerX, centerY, 0);
    const body = physics.createRigidBody(bodyDesc);
    if (dynamic) {
      // UE's XZ-plane bodies translate only in X/Z and rotate only around Y.
      // Here the equivalent 2D axes are Rapier X/Y with rotation around Z.
      body.setEnabledTranslations(true, true, false, true);
      body.setEnabledRotations(false, false, true, true);
    }
    const colliderDesc = R.ColliderDesc.cuboid(width / 2, height / 2, profile.depthMeters / 2)
      .setFriction(material.friction)
      .setRestitution(material.restitution)
      .setFrictionCombineRule(R.CoefficientCombineRule.Max)
      .setRestitutionCombineRule(R.CoefficientCombineRule.Max)
      .setActiveEvents(R.ActiveEvents.COLLISION_EVENTS);
    if (dynamic) colliderDesc.setDensity(material.densityKgPerM3);
    const collider = physics.createCollider(colliderDesc, body);
    createCollider(box.id, collider, body);
    const mesh = createBoxMesh(box, width, height, profile.depthMeters, role);
    const rotation = body.rotation();
    blocks.push({
      id: box.id,
      body,
      mesh,
      initialX: centerX,
      initialY: centerY,
      initialYaw: Math.atan2(2 * (rotation.w * rotation.z + rotation.x * rotation.y), 1 - 2 * (rotation.y ** 2 + rotation.z ** 2)),
    });
  }

  camera.position.set(snapshot.maxX / 2, 2.85, 28);
  camera.lookAt(snapshot.maxX / 2, 2.85, 0);
  resize();
}

async function startStabilityProbe(): Promise<void> {
  if (!snapshot) return;
  setButtonsDisabled(true);
  resultLabel.textContent = '正在加载 Rapier 并创建物理世界…';
  try {
    await Promise.all([ensurePhysics(), ensureProjectileVisual()]);
    buildWorld();
  } catch (error) {
    handleProbeError(error);
    return;
  }
  simulationMode = 'stability';
  activeScenario = undefined;
  resultLabel.textContent = '正在以 1/120 秒步长模拟 5 秒静置…';
  driftMetric.textContent = '采集中';
  hitMetric.textContent = '—';
  clockLabel.textContent = '稳定性探针';
  setButtonsDisabled(true);
  setStatus('正在运行 5 秒塔楼稳定性探针');
  pushLog('stability start · profile-defined tower · 600 fixed steps');
}

async function startShotProbe(kind: 'hit' | 'miss'): Promise<void> {
  if (!snapshot) return;
  setButtonsDisabled(true);
  resultLabel.textContent = '正在加载 Rapier 并创建物理世界…';
  try {
    await ensurePhysics();
    buildWorld();
  } catch (error) {
    handleProbeError(error);
    return;
  }
  activeScenario = kind;
  activeTarget = 'P.col';
  simulationMode = 'shot';
  const angle = kind === 'hit' ? 0 : 45;
  const radians = angle * Math.PI / 180;
  projectileShape = new (requireRapier().Ball)(PROJECTILE_RADIUS_M);
  projectilePosition = { ...PROJECTILE_START };
  projectileVelocity = { x: 27 * Math.cos(radians), y: 27 * Math.sin(radians), z: 0 };
  projectileVisual.position.set(projectilePosition.x, projectilePosition.y, projectilePosition.z);
  projectileVisual.visible = true;
  resultLabel.textContent = kind === 'hit'
    ? '预期命中 P.col；等待扫掠接触和刚体冲量…'
    : '预期错过 P.col；等待弹道结算…';
  hitMetric.textContent = '飞行中';
  driftMetric.textContent = '—';
  clockLabel.textContent = `${angle}° ${kind === 'hit' ? '命中' : '未命中'}`;
  setButtonsDisabled(true);
  setStatus(`正在运行 ${angle}° 弹丸扫掠探针`);
  pushLog(`shot start · angle=${angle}° · speed=27 m/s · target=${activeTarget}`);
}

function setButtonsDisabled(disabled: boolean): void {
  for (const [action, button] of buttons) button.disabled = disabled || (action === 'download' && completedRecords.length === 0);
}

async function ensurePhysics(): Promise<void> {
  if (!rapierApi) {
    hint.textContent = '正在下载并初始化 Rapier WASM…';
    const module = await import('@dimforge/rapier3d-compat');
    await module.init();
    rapierApi = module;
  }
}

function requireRapier(): RapierApi {
  if (!rapierApi) throw new Error('Rapier WASM 尚未初始化');
  return rapierApi;
}

function ensureProjectileVisual(): Promise<void> {
  if (projectileVisual.children.length > 0) return Promise.resolve();
  if (!projectileVisualPromise) {
    projectileVisualPromise = new GLTFLoader().loadAsync('/assets/naiwa-egg-base.glb')
      .then((gltf) => {
        gltf.scene.name = 'UE P2 approved base egg';
        projectileVisual.add(gltf.scene);
        pushLog('loaded approved UE P2 base egg · 85 × 115 × 82.025 cm · 44 cm sphere collision');
      })
      .catch((error: unknown) => {
        projectileVisualPromise = undefined;
        throw error;
      });
  }
  return projectileVisualPromise;
}

function handleProbeError(error: unknown): void {
  const message = error instanceof Error ? error.message : String(error);
  resultLabel.textContent = `探针运行失败：${message}`;
  hint.textContent = message;
  setStatus('探针运行失败', 'error');
  setButtonsDisabled(false);
}

function bodyYaw(rotation: RapierTypes.Rotation): number {
  return Math.atan2(2 * (rotation.w * rotation.z + rotation.x * rotation.y), 1 - 2 * (rotation.y ** 2 + rotation.z ** 2));
}

function drainPhysicsEvents(): void {
  if (!eventQueue) return;
  eventQueue.drainCollisionEvents((handle1, handle2, started) => {
    if (!started || traceEvents.length >= 320) return;
    const a = colliderIds.get(handle1) ?? `collider-${handle1}`;
    const b = colliderIds.get(handle2) ?? `collider-${handle2}`;
    traceEvents.push({ timeSeconds: Number(simulationTime.toFixed(5)), kind: 'rapier-contact-start', bodyA: a, bodyB: b });
  });
}

function stepShot(dt: number): void {
  if (!physics || !snapshot || !eventQueue || !activeScenario || !projectileShape) return;
  const profile = snapshot.physicsProfile;
  const incoming = { ...projectileVelocity, y: projectileVelocity.y - profile.gravityMPerS2 * dt };
  const hit = physics.castShape(
    projectilePosition,
    IDENTITY_ROTATION,
    incoming,
    projectileShape,
    0.0001,
    dt,
    true,
    undefined,
    undefined,
    undefined,
    undefined,
    (collider) => collider.parent() !== undefined,
  );

  if (hit) {
    const toi = Math.max(0, Math.min(dt, hit.time_of_impact));
    const normalLength = Math.hypot(hit.normal1.x, hit.normal1.y, hit.normal1.z) || 1;
    const normal = { x: hit.normal1.x / normalLength, y: hit.normal1.y / normalLength, z: hit.normal1.z / normalLength };
    const normalSpeed = incoming.x * normal.x + incoming.y * normal.y + incoming.z * normal.z;
    const tangent = {
      x: incoming.x - normal.x * normalSpeed,
      y: incoming.y - normal.y * normalSpeed,
      z: incoming.z - normal.z * normalSpeed,
    };
    const bounced = normalSpeed < 0;
    const outgoing = bounced
      ? {
        x: tangent.x * profile.projectileTangentRetention - normal.x * normalSpeed * profile.eggRestitution,
        y: tangent.y * profile.projectileTangentRetention - normal.y * normalSpeed * profile.eggRestitution,
        z: tangent.z * profile.projectileTangentRetention - normal.z * normalSpeed * profile.eggRestitution,
      }
      : incoming;
    const id = colliderIds.get(hit.collider.handle) ?? 'unknown';
    const body = colliderBodies.get(hit.collider.handle);
    const impulse = body?.isDynamic() && bounced
      ? {
        x: (incoming.x - outgoing.x) * profile.baseEggMassKg,
        y: (incoming.y - outgoing.y) * profile.baseEggMassKg,
        z: (incoming.z - outgoing.z) * profile.baseEggMassKg,
      }
      : { x: 0, y: 0, z: 0 };
    if (body?.isDynamic() && bounced) body.applyImpulseAtPoint(impulse, hit.witness1, true);
    projectilePosition = {
      x: projectilePosition.x + incoming.x * toi + normal.x * profile.projectileBounceSeparationCm / 100,
      y: projectilePosition.y + incoming.y * toi + normal.y * profile.projectileBounceSeparationCm / 100,
      z: projectilePosition.z + incoming.z * toi + normal.z * profile.projectileBounceSeparationCm / 100,
    };
    projectileVelocity = outgoing;
    shotHitIds.push(id);
    traceEvents.push({
      timeSeconds: Number((simulationTime + toi).toFixed(5)),
      kind: 'projectile-sweep-impact',
      bodyId: id,
      x: Number(hit.witness1.x.toFixed(4)),
      y: Number(hit.witness1.y.toFixed(4)),
      normalX: Number(normal.x.toFixed(4)),
      normalY: Number(normal.y.toFixed(4)),
      incomingSpeedMPerS: Number(Math.hypot(incoming.x, incoming.y, incoming.z).toFixed(4)),
      outgoingSpeedMPerS: Number(Math.hypot(outgoing.x, outgoing.y, outgoing.z).toFixed(4)),
      impulseXKgMPerS: Number(impulse.x.toFixed(4)),
      impulseYKgMPerS: Number(impulse.y.toFixed(4)),
      dynamicTarget: Boolean(body?.isDynamic()),
      bounced,
    });
    hitMetric.textContent = `${id} · ${bounced ? '反弹' : '擦碰'}`;
    pushLog(`projectile hit ${id} · impulse ${Math.hypot(impulse.x, impulse.y).toFixed(1)} kg·m/s`);
  } else {
    projectilePosition.x += incoming.x * dt;
    projectilePosition.y += incoming.y * dt;
    projectilePosition.z += incoming.z * dt;
    projectileVelocity = incoming;
  }
  projectileVisual.position.set(projectilePosition.x, projectilePosition.y, projectilePosition.z);
  simulationTime += dt;
}

function stepSimulation(): void {
  if (!physics || !eventQueue || !snapshot) return;
  if (simulationMode === 'shot') stepShot(FIXED_DT);
  else simulationTime += FIXED_DT;
  physics.step(eventQueue);
  drainPhysicsEvents();
  if (simulationMode === 'stability' && simulationTime >= snapshot.physicsProfile.stabilityTestSeconds) finishStabilityProbe();
  if (simulationMode === 'shot' && (simulationTime >= snapshot.physicsProfile.projectileMaxFlightSeconds
    || projectilePosition.x > snapshot.maxX + 3 || projectilePosition.y < -2)) finishShotProbe();
}

function makeRecord(caseId: string, result: Record<string, unknown>): ProbeRecord {
  if (!snapshot) throw new Error('Missing P2 snapshot');
  const R = requireRapier();
  return {
    schemaVersion: 1,
    kind: 'p2-web-physics-probe',
    engine: `Rapier ${R.version()}`,
    levelId: snapshot.levelId,
    caseId,
    sourceHashes: snapshot.sourceHashes,
    profile: {
      fixedStepSeconds: FIXED_DT,
      gravityMPerS2: snapshot.physicsProfile.gravityMPerS2,
      depthMeters: snapshot.physicsProfile.depthMeters,
      projectileMassKg: snapshot.physicsProfile.baseEggMassKg,
      projectileRadiusMeters: PROJECTILE_RADIUS_M,
    },
    result,
    events: [...traceEvents],
    completedAt: new Date().toISOString(),
  };
}

function finishStabilityProbe(): void {
  if (!snapshot) return;
  simulationMode = 'idle';
  const drift = blocks.map((block) => {
    const position = block.body.translation();
    const yaw = bodyYaw(block.body.rotation());
    return {
      id: block.id,
      displacementCm: Math.hypot(position.x - block.initialX, position.y - block.initialY) * 100,
      yawDegrees: Math.abs(THREE.MathUtils.radToDeg(yaw - block.initialYaw)),
    };
  });
  const maxDriftCm = Math.max(0, ...drift.map((entry) => entry.displacementCm));
  const maxYawDegrees = Math.max(0, ...drift.map((entry) => entry.yawDegrees));
  const passed = maxDriftCm <= snapshot.physicsProfile.stabilityMaxDriftCm;
  completedRecords.push(makeRecord('stability-5s', {
    simulatedSeconds: Number(simulationTime.toFixed(4)),
    fixedStepCount: Math.round(simulationTime / FIXED_DT),
    maxDriftCm: Number(maxDriftCm.toFixed(4)),
    maxYawDegrees: Number(maxYawDegrees.toFixed(4)),
    allowedDriftCm: snapshot.physicsProfile.stabilityMaxDriftCm,
    passed,
    bodies: drift.map((entry) => ({ ...entry, displacementCm: Number(entry.displacementCm.toFixed(4)), yawDegrees: Number(entry.yawDegrees.toFixed(4)) })),
  }));
  driftMetric.textContent = `${maxDriftCm.toFixed(2)} cm`;
  hitMetric.textContent = '—';
  resultLabel.textContent = `${passed ? '达到位移门槛' : '超出位移门槛'} · ${maxDriftCm.toFixed(2)} cm / ${snapshot.physicsProfile.stabilityMaxDriftCm} cm · 最大转角 ${maxYawDegrees.toFixed(2)}°`;
  clockLabel.textContent = '静置结果';
  setStatus(passed ? '静置位移门通过（尚需 UE/实机对照）' : '静置位移门未通过，已记录结果', passed ? 'ready' : 'error');
  pushLog(`stability end · max drift=${maxDriftCm.toFixed(3)} cm · max yaw=${maxYawDegrees.toFixed(3)}° · ${passed ? 'PASS' : 'FAIL'}`);
  setButtonsDisabled(false);
}

function finishShotProbe(): void {
  if (!snapshot || !activeScenario) return;
  const kind = activeScenario;
  simulationMode = 'idle';
  const targetHit = shotHitIds.includes(activeTarget);
  const expectedHit = kind === 'hit';
  const passed = targetHit === expectedHit;
  const caseId = kind === 'hit' ? 'sweep-hit-pcol-0deg' : 'sweep-miss-pcol-45deg';
  completedRecords.push(makeRecord(caseId, {
    angleDegrees: kind === 'hit' ? 0 : 45,
    power: 1,
    start: PROJECTILE_START,
    targetId: activeTarget,
    hitIds: [...shotHitIds],
    targetHit,
    expectedTargetHit: expectedHit,
    passed,
    simulatedSeconds: Number(simulationTime.toFixed(4)),
  }));
  hitMetric.textContent = targetHit ? `${activeTarget} 命中` : `${activeTarget} 未命中`;
  resultLabel.textContent = `${passed ? '用例符合预期' : '用例与预期不符'} · ${shotHitIds.length ? `接触 ${shotHitIds.join(' → ')}` : '未发生接触'} · ${simulationTime.toFixed(2)} s`;
  clockLabel.textContent = targetHit ? '命中结果' : '未命中结果';
  setStatus(passed ? '弹丸用例符合预期（尚需 Chaos 对照）' : '弹丸用例与预期不符', passed ? 'ready' : 'error');
  pushLog(`shot end · target=${activeTarget} hit=${targetHit} · contacts=${shotHitIds.join(',') || 'none'} · ${passed ? 'PASS' : 'FAIL'}`);
  projectileVisual.visible = false;
  activeScenario = undefined;
  setButtonsDisabled(false);
}

function downloadTrace(): void {
  const R = requireRapier();
  const result = {
    schemaVersion: 1,
    kind: 'p2-web-physics-probe-session',
    engine: `Rapier ${R.version()}`,
    levelId: snapshot?.levelId,
    sourceHashes: snapshot?.sourceHashes,
    fixedStepSeconds: FIXED_DT,
    records: completedRecords,
  };
  const url = URL.createObjectURL(new Blob([JSON.stringify(result, null, 2)], { type: 'application/json' }));
  const anchor = document.createElement('a');
  anchor.href = url;
  anchor.download = 'p2-web-physics-probe.json';
  anchor.click();
  URL.revokeObjectURL(url);
}

function resize(): void {
  const width = Math.max(canvasWrap.clientWidth, 1);
  const height = Math.max(canvasWrap.clientHeight, 1);
  renderer.setPixelRatio(Math.min(Math.sqrt((1280 * 720) / (width * height)), 2));
  renderer.setSize(width, height, false);
  const aspect = width / height;
  const halfWidth = (snapshot?.maxX ?? 18) / 2 + 1.5;
  const halfHeight = Math.max(3.2, halfWidth / aspect);
  camera.left = -halfHeight * aspect;
  camera.right = halfHeight * aspect;
  camera.top = halfHeight;
  camera.bottom = -halfHeight;
  camera.updateProjectionMatrix();
}

function render(now: number): void {
  if (disposed) return;
  const dt = lastFrameAt === 0 ? 0 : Math.min((now - lastFrameAt) / 1000, 0.1);
  lastFrameAt = now;
  if (simulationMode !== 'idle') {
    accumulator += dt;
    let steps = 0;
    while (accumulator >= FIXED_DT && steps < 8) {
      stepSimulation();
      accumulator -= FIXED_DT;
      steps += 1;
    }
  }
  for (const block of blocks) {
    const position = block.body.translation();
    const rotation = block.body.rotation();
    block.mesh.position.set(position.x, position.y, position.z);
    block.mesh.quaternion.set(rotation.x, rotation.y, rotation.z, rotation.w);
  }
  renderer.render(scene, camera);
  animationFrame = requestAnimationFrame(render);
}

for (const [action, button] of buttons) {
  button.addEventListener('click', () => {
    if (action === 'stability') void startStabilityProbe();
    else if (action === 'hit') void startShotProbe('hit');
    else if (action === 'miss') void startShotProbe('miss');
    else if (action === 'download') downloadTrace();
  });
}

resizeObserver = new ResizeObserver(resize);
resizeObserver.observe(canvasWrap);
window.addEventListener('pagehide', () => {
  disposed = true;
  cancelAnimationFrame(animationFrame);
  resizeObserver?.disconnect();
  clearWorld();
  projectileVisual.traverse((object) => {
    if (!(object instanceof THREE.Mesh)) return;
    object.geometry.dispose();
    const materials = Array.isArray(object.material) ? object.material : [object.material];
    for (const material of materials) material.dispose();
  });
  projectileVisual.clear();
  renderer.dispose();
});
animationFrame = requestAnimationFrame(render);

async function initialize(): Promise<void> {
  try {
    const response = await fetch('/data/p2_21_scene.json', { cache: 'no-cache' });
    if (!response.ok) throw new Error(`P2 snapshot HTTP ${response.status}`);
    snapshot = await response.json() as P2Snapshot;
    if (snapshot.schemaVersion !== 1 || snapshot.levelId !== '2-1' || !Array.isArray(snapshot.boxes)) {
      throw new Error('P2 2-1 快照格式不受支持');
    }
    buildPreview();
    for (const button of buttons.values()) button.disabled = false;
    buttons.get('download')!.disabled = true;
    hint.textContent = `${snapshot.boxes.length} 个 UE 场景盒 · 按下探针后加载物理运行时`;
    resultLabel.textContent = 'P2 2-1 快照已就绪 · UE 源哈希已绑定';
    clockLabel.textContent = '就绪';
    setStatus('P2 2-1 快照已加载 · Rapier 按需加载', 'ready');
    pushLog(`loaded ${snapshot.boxes.length} UE scene boxes · Rapier WASM deferred until first probe`);
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    hint.textContent = `探针启动失败：${message}`;
    resultLabel.textContent = message;
    setStatus('探针启动失败', 'error');
  }
}

function buildPreview(): void {
  if (!snapshot) return;
  clearWorld();
  const floor = new THREE.Mesh(
    new THREE.BoxGeometry(snapshot.maxX + 8, 0.24, snapshot.physicsProfile.depthMeters),
    new THREE.MeshStandardMaterial({ color: '#4d6374', roughness: 0.9 }),
  );
  floor.position.set(snapshot.maxX / 2, -0.12, 0);
  geometryGroup.add(floor);
  for (const box of snapshot.boxes) {
    const width = box.maxX - box.minX;
    const height = box.maxZ - box.minZ;
    const { role } = materialFor(box);
    const mesh = createBoxMesh(box, width, height, snapshot.physicsProfile.depthMeters, role);
    mesh.position.set((box.minX + box.maxX) / 2, (box.minZ + box.maxZ) / 2, 0);
  }
  resize();
}

void initialize();
