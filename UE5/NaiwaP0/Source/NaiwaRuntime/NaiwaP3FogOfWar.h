#pragma once

#include "CoreMinimal.h"
#include "NaiwaP3M2SeededMap.h"

enum class ENaiwaP3FogCellState : uint8
{
	Unseen,
	Remembered,
	Visible
};

// M2 战争迷雾的数据层。记忆只记录地形可见历史，敌人与道具仍由调用方按当前可见性筛选。
class NAIWARUNTIME_API FNaiwaP3FogOfWar final
{
public:
	void Initialize(const FNaiwaP3M2SeededLevel& Level, bool bInEnabled = true);
	void UpdateVisibility(const FNaiwaP3M2SeededLevel& Level,
		const FIntPoint& ViewerCell, bool bRageEscape = false);
	void RevealAll();

	ENaiwaP3FogCellState GetCellState(const FIntPoint& Cell) const;
	bool IsVisible(const FIntPoint& Cell) const;
	bool IsRemembered(const FIntPoint& Cell) const;
	int32 GetExploredCellCount() const;
	int32 GetVisibleCellCount() const;
	bool IsEnabled() const { return bEnabled; }

private:
	static bool HasLineOfSight(const FNaiwaP3M2SeededLevel& Level,
		const FIntPoint& Start, const FIntPoint& Target);
	int32 CellIndex(const FIntPoint& Cell) const;

	int32 Width = 0;
	int32 Height = 0;
	TArray<uint8> VisibleCells;
	TArray<uint8> ExploredCells;
	bool bEnabled = true;
};
