export interface InputCommand {
  moveX: -1 | 0 | 1;
  jumpPressed: boolean;
  jumpHeld: boolean;
  gaitPressed: boolean;
  pausePressed: boolean;
  restartPressed: boolean;
}

export const EMPTY_INPUT: InputCommand = {
  moveX: 0,
  jumpPressed: false,
  jumpHeld: false,
  gaitPressed: false,
  pausePressed: false,
  restartPressed: false,
};
