import * as THREE from 'three';
import { GLTFLoader, type GLTF } from 'three/addons/loaders/GLTFLoader.js';
import type { InputCommand } from '../core/commands';

export interface FrameSample {
  frameTimeP50Ms: number;
  frameTimeP95Ms: number;
  elapsedSeconds: number;
  sampleCount: number;
  wallDurationSeconds: number;
}

interface BoundsSnapshot {
  originCm?: [number, number, number];
  extentCm?: [number, number, number];
}

interface ComponentSnapshot {
  name: string;
  class: string;
  locationCm: [number, number, number];
  rotationDeg: [number, number, number];
  bounds?: BoundsSnapshot;
  staticMesh?: string;
}

interface ActorSnapshot {
  id: string;
  label: string;
  class: string;
  tags: string[];
  locationCm: [number, number, number];
  properties: Record<string, unknown>;
  components: ComponentSnapshot[];
}

interface P1MapSnapshot {
  schemaVersion: number;
  levelId: string;
  killZCm: number;
  actors: ActorSnapshot[];
  configCheckpoints: Array<{ id: string; x: number; top: number }>;
}

interface Rect {
  id: string;
  minX: number;
  maxX: number;
  minZ: number;
  maxZ: number;
  kind: 'terrain' | 'bounce' | 'shuttle';
  mesh?: THREE.Mesh;
}

interface Trigger {
  actor: ActorSnapshot;
  centerX: number;
  centerZ: number;
  halfX: number;
  halfZ: number;
}

const FIXED_DT = 1 / 120;
const PLAYER_RADIUS = 0.42;
const PLAYER_HALF_HEIGHT = 0.90;
const WALK_SPEED = 1.5;
const RUN_SPEED = 3.0;
// Movement values mirror NaiwaCharacter.cpp in meters and seconds.
const MAX_ACCELERATION = 18;
const BRAKING_DECELERATION = 22;
const AIR_CONTROL = 0.4;
const GRAVITY = -9.8 * 1.6;
const JUMP_SPEED = 6.5;
const JUMP_HOLD_SECONDS = 0.2;
const BOUNCE_SPEED = 10;
const JUMP_BUFFER_SECONDS = 0.16;
const COYOTE_SECONDS = 0.1;
const UE_MESH_RIGHT_YAW = -Math.PI / 2;

export class P1SliceScene {
  readonly renderer: THREE.WebGLRenderer;
  private readonly scene = new THREE.Scene();
  private readonly camera = new THREE.OrthographicCamera(-8, 8, 4.5, -4.5, 0.1, 100);
  private readonly resizeObserver: ResizeObserver;
  private readonly samples: number[] = [];
  private readonly world = new THREE.Group();
  private readonly player = new THREE.Group();
  private readonly playerFacing = new THREE.Group();
  private readonly playerFallback: THREE.Mesh;
  private readonly gltfLoader = new GLTFLoader();
  private playerMixer: THREE.AnimationMixer | undefined;
  private playerRunAction: THREE.AnimationAction | undefined;
  private readonly triggerViz = new THREE.Group();
  private readonly solids: Rect[] = [];
  private readonly hazards: Trigger[] = [];
  private readonly pickups: Trigger[] = [];
  private readonly checkpoints: Trigger[] = [];
  private snapshot: P1MapSnapshot | undefined;
  private shuttle: Rect | undefined;
  private shuttleX = -4.2;
  private shuttleTargetX = -4.2;
  private shuttleStarted = false;
  private leftSwitchActivated = false;
  private moveAxis: -1 | 0 | 1 = 0;
  private runningGait = true;
  private jumpHeld = false;
  private jumpHoldSeconds = 0;
  private jumpBufferSeconds = 0;
  private coyoteSeconds = 0;
  private paused = false;
  private onGround = false;
  private dyingSeconds = 0;
  private accumulated = 0;
  private lastFrameTime = 0;
  private benchmarkStartedAt = 0;
  private benchmarkActive = false;
  private elapsed = 0;
  private animationFrame = 0;
  private disposed = false;
  private playerX = -14.5;
  private playerZ = 2.12;
  private velocityX = 0;
  private velocityZ = 0;
  private checkpointX = -14.5;
  private checkpointZ = 2.12;
  private collected = new Set<string>();
  private revealedHazards = false;
  private levelReady = false;
  private readonly solidMaterial = new THREE.MeshStandardMaterial({ color: '#557567', roughness: 0.9 });

  constructor(
    canvas: HTMLCanvasElement,
    private readonly container: HTMLElement,
    private readonly onStatus: (message: string) => void,
    private readonly onBenchmarkComplete: (sample: FrameSample) => void,
  ) {
    this.renderer = new THREE.WebGLRenderer({ canvas, antialias: true, powerPreference: 'high-performance' });
    this.renderer.setClearColor('#a9c8e3');
    this.renderer.outputColorSpace = THREE.SRGBColorSpace;
    this.renderer.setPixelRatio(1);
    this.scene.background = new THREE.Color('#b9d5e8');

    const hemiLight = new THREE.HemisphereLight('#f4fbff', '#334260', 2.2);
    this.scene.add(hemiLight);
    const keyLight = new THREE.DirectionalLight('#fff2cf', 2.7);
    keyLight.position.set(-4, -8, 14);
    this.scene.add(keyLight);
    this.scene.add(this.world, this.triggerViz);

    this.playerFallback = new THREE.Mesh(
      new THREE.CapsuleGeometry(PLAYER_RADIUS, PLAYER_HALF_HEIGHT * 2 - PLAYER_RADIUS * 2, 5, 12),
      new THREE.MeshStandardMaterial({ color: '#ffc857', roughness: 0.72 }),
    );
    this.playerFallback.rotation.x = Math.PI / 2;
    this.player.position.set(this.playerX, 0, this.playerZ);
    this.player.add(this.playerFallback, this.playerFacing);
    this.scene.add(this.player);
    this.camera.up.set(0, 0, 1);

    this.resizeObserver = new ResizeObserver(() => this.resize());
    this.resizeObserver.observe(container);
    this.resize();
    this.animationFrame = requestAnimationFrame(this.renderFrame);
    void this.loadSnapshot();
    void this.loadGameplayCharacter();
  }

  beginBenchmark(): void {
    this.samples.length = 0;
    this.benchmarkStartedAt = performance.now();
    this.benchmarkActive = true;
    this.onStatus('正在采集 5 分钟帧时间；保持页面前台，不要切到其他应用。');
  }

  get isRunningGait(): boolean {
    return this.runningGait;
  }

  applyInput(command: InputCommand): void {
    if (command.pausePressed && this.levelReady) {
      this.paused = !this.paused;
      this.onStatus(this.paused ? '已暂停，按 P 或暂停键继续。' : '继续游戏。');
    }
    if (command.restartPressed && this.levelReady) {
      this.restart();
      return;
    }
    if (command.gaitPressed && this.levelReady && !this.paused && this.dyingSeconds <= 0) {
      this.runningGait = !this.runningGait;
      this.onStatus(this.runningGait ? '已切换到 Run300（3.0 m/s）。' : '已切换到 Walk150（1.5 m/s）。');
    }
    this.moveAxis = this.paused || this.dyingSeconds > 0 ? 0 : command.moveX;
    this.jumpHeld = !this.paused && this.dyingSeconds <= 0 && command.jumpHeld;
    if (command.jumpPressed && !this.paused && this.dyingSeconds <= 0) {
      this.jumpBufferSeconds = JUMP_BUFFER_SECONDS;
      this.onStatus('已收到 Space：正在触发跳跃（起跳缓冲 160 ms）。');
    }
  }

  dispose(): void {
    this.disposed = true;
    cancelAnimationFrame(this.animationFrame);
    this.resizeObserver.disconnect();
    this.renderer.dispose();
    this.scene.traverse((object) => {
      if (object instanceof THREE.Mesh) {
        object.geometry.dispose();
        const materials = Array.isArray(object.material) ? object.material : [object.material];
        for (const material of materials) material.dispose();
      }
    });
  }

  private async loadSnapshot(): Promise<void> {
    this.onStatus('正在加载 P1 v7 真实地图数据…');
    try {
      const response = await fetch('/data/p1_v7_map.json', { cache: 'no-cache' });
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      this.snapshot = await response.json() as P1MapSnapshot;
      if (this.snapshot.schemaVersion !== 1 || !Array.isArray(this.snapshot.actors)) {
        throw new Error('不支持的 P1 地图数据版本');
      }
      this.buildLevel(this.snapshot);
      this.levelReady = true;
      this.onStatus('P1 开场至 CP01 切片已就绪：跳过红色危险，触发摆渡台后乘台过沟。');
    } catch (error) {
      this.onStatus(`P1 地图加载失败：${error instanceof Error ? error.message : String(error)}`);
    }
  }

  private async loadGameplayCharacter(): Promise<void> {
    const hint = this.container.querySelector<HTMLElement>('.scene-hint');
    try {
      const [character, run] = await Promise.all([
        this.loadGltf('/assets/naiwa-character-e2.glb', (ratio) => {
          if (hint) hint.textContent = `正在加载 UE 正式奶龙角色… ${ratio}%`;
        }),
        this.loadGltf('/assets/naiwa-run.glb'),
      ]);
      if (this.disposed) return;

      const characterAxes = new THREE.Group();
      // The UE-exported GLB root already contains the Y-up to Z-up conversion.
      characterAxes.add(character.scene);
      characterAxes.updateMatrixWorld(true);
      const originalBounds = new THREE.Box3().setFromObject(characterAxes);
      const originalHeight = Math.max(originalBounds.max.z - originalBounds.min.z, 0.01);
      characterAxes.scale.setScalar(1.72 / originalHeight);
      characterAxes.updateMatrixWorld(true);
      const scaledBounds = new THREE.Box3().setFromObject(characterAxes);
      characterAxes.position.z += -PLAYER_HALF_HEIGHT - scaledBounds.min.z;
      this.playerFacing.add(characterAxes);

      this.playerMixer = new THREE.AnimationMixer(character.scene);
      const runClip = run.animations[0];
      if (runClip) {
        this.playerRunAction = this.playerMixer.clipAction(runClip);
        this.playerRunAction.setLoop(THREE.LoopRepeat, Infinity);
        this.playerRunAction.setEffectiveWeight(0);
        this.playerRunAction.play();
      }
      this.playerFallback.visible = false;
      if (hint) hint.textContent = 'UE 正式奶龙角色 · 共享骨架跑步动画 · P1 v7 关卡';
    } catch (error) {
      if (hint) hint.textContent = `正式角色加载失败：${error instanceof Error ? error.message : String(error)}`;
    }
  }

  private async loadGltf(
    url: string,
    onProgress?: (percent: number) => void,
  ): Promise<GLTF> {
    let stage = '下载';
    try {
      const response = await fetch(url, { cache: 'force-cache' });
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      const expectedBytes = Number(response.headers.get('content-length')) || 0;
      const reader = response.body?.getReader();
      if (!reader) throw new Error('浏览器未提供流式下载');
      const chunks: Uint8Array[] = [];
      let receivedBytes = 0;
      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        if (value) {
          chunks.push(value);
          receivedBytes += value.byteLength;
          if (onProgress && expectedBytes > 0) {
            onProgress(Math.min(99, Math.round(receivedBytes / expectedBytes * 100)));
          }
        }
      }
      const encoded = new Uint8Array(receivedBytes);
      let offset = 0;
      for (const chunk of chunks) {
        encoded.set(chunk, offset);
        offset += chunk.byteLength;
      }
      const binary = encoded.buffer as ArrayBuffer;
      const header = new Uint8Array(binary, 0, Math.min(binary.byteLength, 4));
      if (!(header.length === 4 && header[0] === 0x67 && header[1] === 0x6c && header[2] === 0x54 && header[3] === 0x46)) {
        throw new Error(`服务器返回的不是 GLB（二进制头 ${Array.from(header).join(',')}）`);
      }
      stage = '解析 glTF';
      const gltf = await this.parseGltf(binary);
      onProgress?.(100);
      return gltf;
    } catch (error) {
      const message = error instanceof Error ? `${error.name}: ${error.message}` : String(error);
      throw new Error(`${url} ${stage}失败 · ${message}`);
    }
  }

  private parseGltf(binary: ArrayBuffer): Promise<GLTF> {
    return new Promise<GLTF>((resolve, reject) => {
      this.gltfLoader.parse(binary, '/', resolve, reject);
    });
  }

  private buildLevel(data: P1MapSnapshot): void {
    this.world.clear();
    this.triggerViz.clear();
    this.solids.length = 0;
    this.hazards.length = 0;
    this.pickups.length = 0;
    this.checkpoints.length = 0;
    this.shuttle = undefined;

    const playerStart = data.actors.find((actor) => actor.class.endsWith('.PlayerStart'));
    if (playerStart) {
      this.playerX = playerStart.locationCm[0] / 100;
      this.playerZ = playerStart.locationCm[2] / 100;
      this.checkpointX = this.playerX;
      this.checkpointZ = this.playerZ;
    }

    for (const actor of data.actors) {
      const isTrigger = actor.class.endsWith('.NaiwaP1Trigger');
      const triggerKind = String(actor.properties.trigger_kind ?? '').toLowerCase();
      if (isTrigger) {
        const box = actor.components.find((component) => component.name === 'TriggerBox')?.bounds;
        const extent = box?.extentCm ?? actor.properties.box_extent as [number, number, number] | undefined;
        const center = box?.originCm ?? actor.locationCm;
        if (extent) {
          const trigger: Trigger = {
            actor,
            centerX: center[0] / 100,
            centerZ: center[2] / 100,
            halfX: extent[0] / 100,
            halfZ: extent[2] / 100,
          };
          if (triggerKind.includes('hazard')) this.hazards.push(trigger);
          if (triggerKind.includes('pickup')) this.pickups.push(trigger);
          if (triggerKind.includes('checkpoint')) this.checkpoints.push(trigger);
        }
      }

      for (const component of actor.components) {
        if (!component.staticMesh || !component.bounds?.originCm || !component.bounds.extentCm) continue;
        const center = component.bounds.originCm;
        const extent = component.bounds.extentCm;
        const x = center[0] / 100;
        const z = center[2] / 100;
        if (x < -22 || x > 32 || z < -6 || z > 16) continue;
        const isHazardActor = isTrigger && triggerKind.includes('hazard');
        const isPickupActor = isTrigger && triggerKind.includes('pickup');
        if (isHazardActor && !this.revealedHazards) continue;
        if (isPickupActor && component.name !== 'DisplayMesh') continue;
        if (component.name.startsWith('SideSpike') || component.name === 'CollectedDisplayMesh') continue;

        const isShuttle = actor.class.endsWith('.NaiwaP1SwitchPlatform') && component.name === 'PlatformDisplay';
        const color = this.colorFor(actor);
        const geometry = component.staticMesh.endsWith('.Sphere')
          ? new THREE.SphereGeometry(1, 18, 12)
          : component.staticMesh.endsWith('.Cone')
            ? new THREE.ConeGeometry(1, 2, 12)
            : new THREE.BoxGeometry(1, 1, 1);
        const mesh = new THREE.Mesh(geometry, new THREE.MeshStandardMaterial({ color, roughness: 0.84 }));
        mesh.name = `${actor.label}/${component.name}`;
        mesh.position.set(x, center[1] / 100, z);
        mesh.scale.set(extent[0] / 50, extent[1] / 50, extent[2] / 50);
        this.world.add(mesh);

        if (isShuttle) {
          this.shuttle = {
            id: actor.id,
            minX: x - extent[0] / 100,
            maxX: x + extent[0] / 100,
            minZ: z - extent[2] / 100,
            maxZ: z + extent[2] / 100,
            kind: 'shuttle',
            mesh,
          };
          this.shuttleX = x;
          this.shuttleTargetX = x;
        }
      }

      if (actor.label.includes('Terrain')) {
        const component = actor.components.find((value) => value.staticMesh && value.bounds?.originCm && value.bounds.extentCm);
        if (component?.bounds?.originCm && component.bounds.extentCm) {
          this.solids.push(this.rectFromComponent(actor.id, component, 'terrain'));
        }
      }
      if (actor.class.endsWith('.NaiwaP1BouncePad')) {
        const component = actor.components.find((value) => value.name === 'PadCollision');
        if (component?.bounds?.originCm && component.bounds.extentCm) {
          this.solids.push(this.rectFromComponent(actor.id, component, 'bounce'));
        }
      }
    }

    if (this.shuttle) this.solids.push(this.shuttle);
    this.player.position.set(this.playerX, 0, this.playerZ);
  }

  private rectFromComponent(id: string, component: ComponentSnapshot, kind: Rect['kind']): Rect {
    const center = component.bounds!.originCm!;
    const extent = component.bounds!.extentCm!;
    return {
      id,
      minX: (center[0] - extent[0]) / 100,
      maxX: (center[0] + extent[0]) / 100,
      minZ: (center[2] - extent[2]) / 100,
      maxZ: (center[2] + extent[2]) / 100,
      kind,
    };
  }

  private colorFor(actor: ActorSnapshot): string {
    const label = actor.label.toLowerCase();
    if (label.includes('hazard')) return '#ed765f';
    if (label.includes('pickup')) return '#f6ca70';
    if (label.includes('checkpoint') || label.includes('goal')) return '#4ac7a4';
    if (label.includes('bounce')) return '#80b9ef';
    if (label.includes('switch')) return '#52a4bd';
    if (label.includes('cue')) return '#d99963';
    return this.solidMaterial.color.getStyle();
  }

  private restart(): void {
    this.playerX = -14.5;
    this.playerZ = 2.12;
    this.checkpointX = this.playerX;
    this.checkpointZ = this.playerZ;
    this.velocityX = 0;
    this.velocityZ = 0;
    this.moveAxis = 0;
    this.jumpHeld = false;
    this.jumpHoldSeconds = 0;
    this.jumpBufferSeconds = 0;
    this.coyoteSeconds = 0;
    this.collected.clear();
    this.revealedHazards = false;
    this.paused = false;
    this.dyingSeconds = 0;
    this.shuttleStarted = false;
    this.leftSwitchActivated = false;
    if (this.snapshot) this.buildLevel(this.snapshot);
    this.onStatus('从 P1 开场重来。');
  }

  private readonly renderFrame = (now: number): void => {
    if (this.disposed) return;
    const frameTimeMs = this.lastFrameTime === 0 ? 0 : now - this.lastFrameTime;
    this.lastFrameTime = now;
    this.elapsed += Math.min(frameTimeMs / 1000, 0.05);
    if (this.benchmarkActive && frameTimeMs > 0) this.samples.push(frameTimeMs);
    if (this.levelReady && frameTimeMs > 0 && !this.paused) {
      this.accumulated += Math.min(frameTimeMs / 1000, 0.1);
      while (this.accumulated >= FIXED_DT) {
        this.step(FIXED_DT);
        this.accumulated -= FIXED_DT;
      }
    }
    const visualDelta = Math.min(frameTimeMs / 1000, 0.05);
    this.playerMixer?.update(visualDelta);
    this.playerRunAction?.setEffectiveWeight(this.moveAxis === 0 ? 0 : 1);
    this.playerRunAction?.setEffectiveTimeScale(this.runningGait ? 1 : WALK_SPEED / RUN_SPEED);
    this.playerFacing.rotation.z = UE_MESH_RIGHT_YAW + (this.moveAxis < 0 ? Math.PI : 0);
    this.player.position.set(this.playerX, 0, this.playerZ);
    this.camera.position.set(this.playerX, -24, this.playerZ + 3.2);
    this.camera.lookAt(this.playerX, 0, this.playerZ + 0.45);
    this.renderer.render(this.scene, this.camera);

    if (this.benchmarkActive && now - this.benchmarkStartedAt >= 300_000) {
      this.benchmarkActive = false;
      const ordered = [...this.samples].sort((a, b) => a - b);
      const p50 = percentile(ordered, 0.5);
      const p95 = percentile(ordered, 0.95);
      this.onBenchmarkComplete({
        frameTimeP50Ms: p50,
        frameTimeP95Ms: p95,
        elapsedSeconds: Math.round((now - this.benchmarkStartedAt) / 1000),
        sampleCount: this.samples.length,
        wallDurationSeconds: Number(((now - this.benchmarkStartedAt) / 1000).toFixed(1)),
      });
      this.onStatus(`采样完成：${this.samples.length} 帧，p50 ${p50.toFixed(1)} ms，p95 ${p95.toFixed(1)} ms。`);
    }
    this.animationFrame = requestAnimationFrame(this.renderFrame);
  };

  private step(dt: number): void {
    this.jumpBufferSeconds = Math.max(0, this.jumpBufferSeconds - dt);
    this.coyoteSeconds = this.onGround
      ? COYOTE_SECONDS
      : Math.max(0, this.coyoteSeconds - dt);

    if (this.dyingSeconds > 0) {
      this.dyingSeconds -= dt;
      if (this.dyingSeconds <= 0) this.respawn();
      return;
    }

    const wasOnShuttle = this.shuttle && this.playerZ - PLAYER_HALF_HEIGHT <= this.shuttle.maxZ + 0.04
      && this.playerZ - PLAYER_HALF_HEIGHT >= this.shuttle.minZ - 0.1
      && this.playerX >= this.shuttle.minX - PLAYER_RADIUS
      && this.playerX <= this.shuttle.maxX + PLAYER_RADIUS;
    if (this.shuttle && this.shuttleTargetX !== this.shuttleX) {
      const direction = Math.sign(this.shuttleTargetX - this.shuttleX);
      const delta = direction * Math.min(Math.abs(this.shuttleTargetX - this.shuttleX), 3 * dt);
      this.shuttleX += delta;
      this.shuttle.minX += delta;
      this.shuttle.maxX += delta;
      if (wasOnShuttle) this.playerX += delta;
      if (this.shuttle.mesh) this.shuttle.mesh.position.x += delta;
      if (Math.abs(this.shuttleTargetX - this.shuttleX) < 0.001) {
        this.shuttleX = this.shuttleTargetX;
        this.onStatus('摆渡台已到达对岸，可以跳到右侧平台。');
      }
    }

    const wasGrounded = this.onGround;
    if (this.jumpBufferSeconds > 0 && (this.onGround || this.coyoteSeconds > 0)) {
      this.velocityZ = JUMP_SPEED;
      this.onGround = false;
      this.jumpHoldSeconds = JUMP_HOLD_SECONDS;
      this.jumpBufferSeconds = 0;
      this.coyoteSeconds = 0;
    }
    const targetVelocityX = this.moveAxis * (this.runningGait ? RUN_SPEED : WALK_SPEED);
    const horizontalAcceleration = this.moveAxis === 0
      ? BRAKING_DECELERATION
      : MAX_ACCELERATION * (wasGrounded ? 1 : AIR_CONTROL);
    this.velocityX = moveToward(this.velocityX, targetVelocityX, horizontalAcceleration * dt);
    const previousFeet = this.playerZ - PLAYER_HALF_HEIGHT;
    const previousX = this.playerX;
    this.playerX += this.velocityX * dt;

    const leftSwitch = this.snapshot?.actors.find((actor) => actor.class.endsWith('.NaiwaP1SwitchPlatform'))
      ?.components.find((component) => component.name === 'LeftSwitchTrigger');
    const switchBounds = leftSwitch?.bounds;
    if (switchBounds?.originCm && switchBounds.extentCm) {
      const switchX = switchBounds.originCm[0] / 100;
      const switchZ = switchBounds.originCm[2] / 100;
      if (Math.abs(this.playerX - switchX) < switchBounds.extentCm[0] / 100 + PLAYER_RADIUS
        && Math.abs(this.playerZ - switchZ) < switchBounds.extentCm[2] / 100 + PLAYER_HALF_HEIGHT) {
        if (!this.leftSwitchActivated) this.onStatus('摆渡台开关已触发；跳上平台乘台过沟。');
        this.leftSwitchActivated = true;
      }
    }

    if (this.jumpHoldSeconds > 0 && this.jumpHeld) {
      this.jumpHoldSeconds = Math.max(0, this.jumpHoldSeconds - dt);
    } else {
      this.jumpHoldSeconds = 0;
      this.velocityZ += GRAVITY * dt;
    }
    this.playerZ += this.velocityZ * dt;
    this.onGround = false;
    const currentFeet = this.playerZ - PLAYER_HALF_HEIGHT;
    for (const solid of this.solids) {
      if (this.playerX + PLAYER_RADIUS < solid.minX || this.playerX - PLAYER_RADIUS > solid.maxX) continue;
      const standing = Math.abs(currentFeet - solid.maxZ) < 0.06;
      const crossing = previousFeet >= solid.maxZ - 0.02 && currentFeet <= solid.maxZ && this.velocityZ <= 0;
      if (standing || crossing) {
        this.playerZ = solid.maxZ + PLAYER_HALF_HEIGHT;
        this.velocityZ = 0;
        this.jumpHoldSeconds = 0;
        this.onGround = true;
        if (solid.kind === 'bounce' && !wasOnShuttle) {
          this.velocityZ = BOUNCE_SPEED;
          this.onGround = false;
          this.onStatus('弹跳板触发！');
        }
        if (solid.kind === 'shuttle' && this.leftSwitchActivated && !this.shuttleStarted) {
          this.shuttleStarted = true;
          this.shuttleTargetX = 4.2;
          this.onStatus('已登上摆渡台，正在驶向对岸。');
        }
        break;
      }
    }

    this.resolvePickups();
    this.resolveCheckpoint();
    if (this.resolveHazard() || this.playerZ < (this.snapshot?.killZCm ?? -1000) / 100) this.die();

    // Keep a brief visual hint if horizontal movement is blocked by the map's route bounds.
    if (previousX !== this.playerX && Math.abs(this.playerX - previousX) > 0.2) this.player.rotation.z = 0;
  }

  private resolvePickups(): void {
    for (const pickup of this.pickups) {
      const id = String(pickup.actor.properties.trigger_id ?? pickup.actor.id);
      if (this.collected.has(id)) continue;
      if (Math.abs(this.playerX - pickup.centerX) < pickup.halfX + PLAYER_RADIUS
        && Math.abs(this.playerZ - pickup.centerZ) < pickup.halfZ + PLAYER_HALF_HEIGHT) {
        this.collected.add(id);
        for (const child of this.world.children) {
          if (child instanceof THREE.Mesh && child.name.startsWith(`${pickup.actor.label}/`)) child.visible = false;
        }
        this.onStatus(`已收集 ${this.collected.size} 个拾取物。`);
      }
    }
  }

  private resolveCheckpoint(): void {
    for (const checkpoint of this.checkpoints) {
      if (this.playerX < checkpoint.centerX - checkpoint.halfX - PLAYER_RADIUS
        || this.playerX > checkpoint.centerX + checkpoint.halfX + PLAYER_RADIUS) continue;
      if (Math.abs(this.playerZ - checkpoint.centerZ) > checkpoint.halfZ + PLAYER_HALF_HEIGHT) continue;
      const config = this.snapshot?.configCheckpoints.find((item) => item.id === checkpoint.actor.properties.trigger_id);
      this.checkpointX = checkpoint.centerX;
      this.checkpointZ = config ? config.top / 100 + PLAYER_HALF_HEIGHT : checkpoint.centerZ;
      this.onStatus('CP01 已激活。触碰危险后会在安全位置复活。');
      this.checkpoints.splice(this.checkpoints.indexOf(checkpoint), 1);
      return;
    }
  }

  private resolveHazard(): boolean {
    return this.hazards.some((hazard) =>
      Math.abs(this.playerX - hazard.centerX) < hazard.halfX + PLAYER_RADIUS
      && Math.abs(this.playerZ - hazard.centerZ) < hazard.halfZ + PLAYER_HALF_HEIGHT,
    );
  }

  private die(): void {
    if (this.dyingSeconds > 0) return;
    this.dyingSeconds = 0.85;
    this.velocityX = 0;
    this.velocityZ = 0;
    this.moveAxis = 0;
    this.jumpHeld = false;
    this.jumpHoldSeconds = 0;
    this.jumpBufferSeconds = 0;
    this.revealedHazards = true;
    if (this.snapshot) this.rebuildHazardVisuals(this.snapshot);
    this.onStatus('碰到危险：短暂倒地后在最近检查点复活。');
  }

  private rebuildHazardVisuals(data: P1MapSnapshot): void {
    for (const actor of data.actors.filter((value) => value.class.endsWith('.NaiwaP1Trigger')
      && String(value.properties.trigger_kind ?? '').toLowerCase().includes('hazard'))) {
      for (const component of actor.components.filter((value) => value.staticMesh && value.bounds?.originCm && value.bounds.extentCm
        && value.name !== 'CollectedDisplayMesh' && !value.name.startsWith('SideSpike'))) {
        const center = component.bounds!.originCm!;
        const extent = component.bounds!.extentCm!;
        const mesh = new THREE.Mesh(
          new THREE.BoxGeometry(extent[0] / 50, extent[1] / 50, extent[2] / 50),
          new THREE.MeshStandardMaterial({ color: '#ed765f', transparent: true, opacity: 0.85 }),
        );
        mesh.position.set(center[0] / 100, center[1] / 100, center[2] / 100);
        this.world.add(mesh);
      }
    }
  }

  private respawn(): void {
    this.playerX = this.checkpointX;
    this.playerZ = this.checkpointZ;
    this.velocityX = 0;
    this.velocityZ = 0;
    this.moveAxis = 0;
    this.jumpHeld = false;
    this.jumpHoldSeconds = 0;
    this.jumpBufferSeconds = 0;
    this.coyoteSeconds = 0;
    this.onGround = false;
    this.onStatus(this.checkpointX > 0 ? '已在 CP01 复活，继续前进。' : '已在开场复活；危险位置已显示，可以再次尝试跳跃。');
  }

  private resize(): void {
    const width = Math.max(this.container.clientWidth, 1);
    const height = Math.max(this.container.clientHeight, 1);
    const cssPixels = width * height;
    const targetPixels = 1280 * 720;
    const renderScale = Math.sqrt(targetPixels / cssPixels);
    this.renderer.setPixelRatio(Math.min(Math.max(renderScale, 0.5), 2.5));
    this.renderer.setSize(width, height, false);
    const aspect = width / height;
    const halfHeight = 5.1;
    this.camera.left = -halfHeight * aspect;
    this.camera.right = halfHeight * aspect;
    this.camera.top = halfHeight;
    this.camera.bottom = -halfHeight;
    this.camera.updateProjectionMatrix();
  }
}

function percentile(sortedValues: number[], ratio: number): number {
  if (sortedValues.length === 0) return 0;
  return sortedValues[Math.min(Math.ceil(sortedValues.length * ratio) - 1, sortedValues.length - 1)];
}

function moveToward(current: number, target: number, maxDelta: number): number {
  if (Math.abs(target - current) <= maxDelta) return target;
  return current + Math.sign(target - current) * maxDelta;
}
