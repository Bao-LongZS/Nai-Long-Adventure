#include "NaiwaP3FogOfWar.h"

namespace
{
	constexpr int32 NormalVisionRadius = 12;
	constexpr int32 RageVisionRadius = 16;
}

void FNaiwaP3FogOfWar::Initialize(const FNaiwaP3M2SeededLevel& Level, const bool bInEnabled)
{
	Width = Level.GridWidth;
	Height = Level.GridHeight;
	VisibleCells.Init(0, Width * Height);
	ExploredCells.Init(0, Width * Height);
	bEnabled = bInEnabled;
	if (!bEnabled)
	{
		RevealAll();
	}
}

void FNaiwaP3FogOfWar::UpdateVisibility(const FNaiwaP3M2SeededLevel& Level,
	const FIntPoint& ViewerCell, const bool bRageEscape)
{
	if (Width <= 0 || Height <= 0)
	{
		Initialize(Level, true);
	}
	VisibleCells.Init(0, Width * Height);
	if (!bEnabled)
	{
		VisibleCells.Init(1, Width * Height);
		ExploredCells.Init(1, Width * Height);
		return;
	}

	const int32 Radius = bRageEscape ? RageVisionRadius : NormalVisionRadius;
	const int32 RadiusSquared = Radius * Radius;
	for (int32 Y = FMath::Max(0, ViewerCell.Y - Radius);
		Y <= FMath::Min(Height - 1, ViewerCell.Y + Radius); ++Y)
	{
		for (int32 X = FMath::Max(0, ViewerCell.X - Radius);
			X <= FMath::Min(Width - 1, ViewerCell.X + Radius); ++X)
		{
			const FIntPoint Target(X, Y);
			if (FMath::Square(X - ViewerCell.X) + FMath::Square(Y - ViewerCell.Y) > RadiusSquared ||
				!Level.IsInside(Target) || !HasLineOfSight(Level, ViewerCell, Target))
			{
				continue;
			}
			const int32 Index = CellIndex(Target);
			VisibleCells[Index] = 1;
			ExploredCells[Index] = 1;
		}
	}
}

void FNaiwaP3FogOfWar::RevealAll()
{
	VisibleCells.Init(1, Width * Height);
	ExploredCells.Init(1, Width * Height);
}

ENaiwaP3FogCellState FNaiwaP3FogOfWar::GetCellState(const FIntPoint& Cell) const
{
	if (Cell.X < 0 || Cell.Y < 0 || Cell.X >= Width || Cell.Y >= Height)
	{
		return ENaiwaP3FogCellState::Unseen;
	}
	const int32 Index = CellIndex(Cell);
	if (VisibleCells.IsValidIndex(Index) && VisibleCells[Index])
	{
		return ENaiwaP3FogCellState::Visible;
	}
	if (ExploredCells.IsValidIndex(Index) && ExploredCells[Index])
	{
		return ENaiwaP3FogCellState::Remembered;
	}
	return ENaiwaP3FogCellState::Unseen;
}

bool FNaiwaP3FogOfWar::IsVisible(const FIntPoint& Cell) const
{
	return GetCellState(Cell) == ENaiwaP3FogCellState::Visible;
}

bool FNaiwaP3FogOfWar::IsRemembered(const FIntPoint& Cell) const
{
	return GetCellState(Cell) != ENaiwaP3FogCellState::Unseen;
}

int32 FNaiwaP3FogOfWar::GetExploredCellCount() const
{
	int32 Count = 0;
	for (const uint8 bExplored : ExploredCells)
	{
		Count += bExplored != 0 ? 1 : 0;
	}
	return Count;
}

int32 FNaiwaP3FogOfWar::GetVisibleCellCount() const
{
	int32 Count = 0;
	for (const uint8 bVisible : VisibleCells)
	{
		Count += bVisible != 0 ? 1 : 0;
	}
	return Count;
}

bool FNaiwaP3FogOfWar::HasLineOfSight(const FNaiwaP3M2SeededLevel& Level,
	const FIntPoint& Start, const FIntPoint& Target)
{
	if (!Level.IsInside(Start) || !Level.IsInside(Target))
	{
		return false;
	}
	if (Start == Target)
	{
		return true;
	}

	int32 X = Start.X;
	int32 Y = Start.Y;
	const int32 DeltaX = FMath::Abs(Target.X - Start.X);
	const int32 StepX = Start.X < Target.X ? 1 : -1;
	const int32 DeltaY = -FMath::Abs(Target.Y - Start.Y);
	const int32 StepY = Start.Y < Target.Y ? 1 : -1;
	int32 Error = DeltaX + DeltaY;

	while (X != Target.X || Y != Target.Y)
	{
		const int32 TwiceError = 2 * Error;
		if (TwiceError >= DeltaY)
		{
			Error += DeltaY;
			X += StepX;
		}
		if (TwiceError <= DeltaX)
		{
			Error += DeltaX;
			Y += StepY;
		}
		if (X == Target.X && Y == Target.Y)
		{
			return true;
		}
		const ENaiwaP3M2Cell Tile = Level.GetCell(FIntPoint(X, Y));
		if (Tile == ENaiwaP3M2Cell::HardBrick || Tile == ENaiwaP3M2Cell::SoftBrick ||
			Tile == ENaiwaP3M2Cell::ItemBrick)
		{
			return false;
		}
	}
	return true;
}

int32 FNaiwaP3FogOfWar::CellIndex(const FIntPoint& Cell) const
{
	return Cell.Y * Width + Cell.X;
}
