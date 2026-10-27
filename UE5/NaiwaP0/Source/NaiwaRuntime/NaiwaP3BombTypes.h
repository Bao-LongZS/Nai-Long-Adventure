#pragma once

#include "CoreMinimal.h"

// 奶鸡是跳跃道具，不属于炸弹；奶鼠分裂出的子弹使用 RatFragment。
enum class ENaiwaP3BombType : uint8
{
	Standard,
	SoyBigCross,
	TeaRadial,
	RatSplit,
	RatFragment
};

NAIWARUNTIME_API const TCHAR* GetNaiwaP3BombDisplayName(ENaiwaP3BombType Type);
NAIWARUNTIME_API FLinearColor GetNaiwaP3BombColor(ENaiwaP3BombType Type);
