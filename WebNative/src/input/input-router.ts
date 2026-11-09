import type { InputCommand } from '../core/commands';

type HeldControl = 'left' | 'right' | 'jump';

const CONTROL_KEYS: Record<string, HeldControl> = {
  ArrowLeft: 'left',
  a: 'left',
  A: 'left',
  ArrowRight: 'right',
  d: 'right',
  D: 'right',
  ' ': 'jump',
  Spacebar: 'jump',
  Space: 'jump',
};

const CONTROL_CODES: Record<string, HeldControl> = {
  ArrowLeft: 'left',
  KeyA: 'left',
  ArrowRight: 'right',
  KeyD: 'right',
  Space: 'jump',
};

function controlFor(event: KeyboardEvent): HeldControl | undefined {
  if (event.keyCode === 32) return 'jump';
  return CONTROL_CODES[event.code] ?? CONTROL_KEYS[event.key];
}

export class InputRouter {
  private readonly heldKeys = new Set<HeldControl>();
  private readonly heldPointers = new Map<number, HeldControl>();
  private jumpQueued = false;
  private gaitQueued = false;
  private pauseQueued = false;
  private restartQueued = false;
  private readonly cleanup: Array<() => void> = [];

  constructor(private readonly root: HTMLElement) {
    this.bindKeyboard();
    this.bindTouchControls();
  }

  readFrame(): InputCommand {
    const left = this.heldKeys.has('left') || this.hasPointer('left');
    const right = this.heldKeys.has('right') || this.hasPointer('right');
    const command: InputCommand = {
      moveX: left === right ? 0 : left ? -1 : 1,
      jumpPressed: this.jumpQueued,
      jumpHeld: this.heldKeys.has('jump') || this.hasPointer('jump'),
      gaitPressed: this.gaitQueued,
      pausePressed: this.pauseQueued,
      restartPressed: this.restartQueued,
    };
    this.jumpQueued = false;
    this.gaitQueued = false;
    this.pauseQueued = false;
    this.restartQueued = false;
    return command;
  }

  dispose(): void {
    for (const dispose of this.cleanup) dispose();
    this.cleanup.length = 0;
    this.heldKeys.clear();
    this.heldPointers.clear();
  }

  private hasPointer(control: HeldControl): boolean {
    return [...this.heldPointers.values()].includes(control);
  }

  private bindKeyboard(): void {
    const onKeyDown = (event: KeyboardEvent): void => {
      const control = controlFor(event);
      if (control) {
        event.preventDefault();
        if (control === 'jump' && !event.repeat) this.jumpQueued = true;
        this.heldKeys.add(control);
      } else if ((event.code === 'ShiftLeft' || event.code === 'ShiftRight') && !event.repeat) {
        event.preventDefault();
        this.gaitQueued = true;
      } else if ((event.code === 'KeyP' || event.key.toLowerCase() === 'p') && !event.repeat) {
        event.preventDefault();
        this.pauseQueued = true;
      } else if ((event.code === 'KeyR' || event.key.toLowerCase() === 'r') && !event.repeat) {
        event.preventDefault();
        this.restartQueued = true;
      }
    };
    const onKeyUp = (event: KeyboardEvent): void => {
      const control = controlFor(event);
      if (control) {
        event.preventDefault();
        this.heldKeys.delete(control);
      }
    };
    const onKeyPress = (event: KeyboardEvent): void => {
      if (controlFor(event) !== 'jump' || event.repeat) return;
      event.preventDefault();
      this.jumpQueued = true;
    };
    const onBlur = (): void => this.heldKeys.clear();

    window.addEventListener('keydown', onKeyDown, { capture: true, passive: false });
    window.addEventListener('keyup', onKeyUp, { capture: true, passive: false });
    window.addEventListener('keypress', onKeyPress, { capture: true, passive: false });
    window.addEventListener('blur', onBlur);
    this.cleanup.push(() => window.removeEventListener('keydown', onKeyDown, true));
    this.cleanup.push(() => window.removeEventListener('keyup', onKeyUp, true));
    this.cleanup.push(() => window.removeEventListener('keypress', onKeyPress, true));
    this.cleanup.push(() => window.removeEventListener('blur', onBlur));
  }

  private bindTouchControls(): void {
    const controls = this.root.querySelectorAll<HTMLElement>('[data-control]');
    for (const element of controls) {
      const control = element.dataset.control as HeldControl | 'gait' | 'pause' | undefined;
      if (!control) continue;

      const onPointerDown = (event: PointerEvent): void => {
        event.preventDefault();
        element.setPointerCapture(event.pointerId);
        element.classList.add('is-pressed');
        if (control === 'pause') {
          this.pauseQueued = true;
          return;
        }
        if (control === 'gait') {
          this.gaitQueued = true;
          return;
        }
        if (control === 'jump') this.jumpQueued = true;
        this.heldPointers.set(event.pointerId, control);
      };
      const release = (event: PointerEvent): void => {
        this.heldPointers.delete(event.pointerId);
        element.classList.remove('is-pressed');
      };

      element.addEventListener('pointerdown', onPointerDown, { passive: false });
      element.addEventListener('pointerup', release);
      element.addEventListener('pointercancel', release);
      element.addEventListener('lostpointercapture', release);
      this.cleanup.push(() => element.removeEventListener('pointerdown', onPointerDown));
      this.cleanup.push(() => element.removeEventListener('pointerup', release));
      this.cleanup.push(() => element.removeEventListener('pointercancel', release));
      this.cleanup.push(() => element.removeEventListener('lostpointercapture', release));
    }
  }
}
