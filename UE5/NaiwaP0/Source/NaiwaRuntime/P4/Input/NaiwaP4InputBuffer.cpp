#include "NaiwaP4InputBuffer.h"

void FNaiwaP4InputBuffer::Reset()
{
	MoveAxis = FVector2D::ZeroVector;
	bMeleePressed = false;
	bMeleeHeld = false;
	bDashPressed = false;
	bBlockHeld = false;
	bUltimatePressed = false;
}

void FNaiwaP4InputBuffer::SetMoveAxis(FVector2D Axis)
{
	if (!FMath::IsFinite(Axis.X) || !FMath::IsFinite(Axis.Y))
	{
		MoveAxis = FVector2D::ZeroVector;
		return;
	}

	Axis.X = FMath::Clamp(Axis.X, -1.0, 1.0);
	Axis.Y = FMath::Clamp(Axis.Y, -1.0, 1.0);
	if (Axis.SizeSquared() > 1.0)
	{
		Axis.Normalize();
	}
	MoveAxis = Axis;
}

void FNaiwaP4InputBuffer::SetMeleeHeld(const bool bHeld)
{
	if (bHeld && !bMeleeHeld)
	{
		bMeleePressed = true;
	}
	bMeleeHeld = bHeld;
}

FNaiwaP4InputCommand FNaiwaP4InputBuffer::ConsumeForFixedStep()
{
	FNaiwaP4InputCommand Command;
	Command.MoveAxis = MoveAxis;
	Command.bMeleePressed = bMeleePressed;
	Command.bMeleeHeld = bMeleeHeld;
	Command.bDashPressed = bDashPressed;
	Command.bBlockHeld = bBlockHeld;
	Command.bUltimatePressed = bUltimatePressed;

	// Edge events are consumed once; held states remain for later fixed steps.
	bMeleePressed = false;
	bDashPressed = false;
	bUltimatePressed = false;
	return Command;
}
