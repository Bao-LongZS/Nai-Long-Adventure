#pragma once

#include "CoreMinimal.h"
#include "../Core/NaiwaP4Types.h"

/**
 * Converts asynchronous Enhanced Input callbacks into fixed-step commands.
 * Held values persist; one-shot presses survive until exactly one step consumes them.
 */
struct FNaiwaP4InputBuffer final
{
	void Reset();
	void SetMoveAxis(FVector2D Axis);
	void SetMeleeHeld(bool bHeld);
	void QueueDashPressed() { bDashPressed = true; }
	void SetBlockHeld(bool bHeld) { bBlockHeld = bHeld; }
	void QueueUltimatePressed() { bUltimatePressed = true; }

	FNaiwaP4InputCommand ConsumeForFixedStep();

private:
	FVector2D MoveAxis = FVector2D::ZeroVector;
	bool bMeleePressed = false;
	bool bMeleeHeld = false;
	bool bDashPressed = false;
	bool bBlockHeld = false;
	bool bUltimatePressed = false;
};
