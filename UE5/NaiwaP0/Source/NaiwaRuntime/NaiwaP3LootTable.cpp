#include "NaiwaP3LootTable.h"

namespace
{
	struct FNaiwaP3LootWeight
	{
		ENaiwaP3LootType Type;
		int32 Weight;
	};

	constexpr FNaiwaP3LootWeight LootWeights[] = {
		{ENaiwaP3LootType::BombCapacity, 25},
		{ENaiwaP3LootType::BombRange, 25},
		{ENaiwaP3LootType::ChickenJump, 15},
		{ENaiwaP3LootType::SpeedBoost, 15},
		{ENaiwaP3LootType::Shield, 10},
		{ENaiwaP3LootType::Xray, 10},
		{ENaiwaP3LootType::ChaosReverse, 5}
	};
	constexpr int32 TotalLootWeight = 105;
}

ENaiwaP3LootType FNaiwaP3LootRandomStream::RollDrop()
{
	int32 Roll = RandomStream.RandRange(0, TotalLootWeight - 1);
	++DrawCount;
	for (const FNaiwaP3LootWeight& Entry : LootWeights)
	{
		if (Roll < Entry.Weight)
		{
			return Entry.Type;
		}
		Roll -= Entry.Weight;
	}
	return ENaiwaP3LootType::ChaosReverse;
}

const TCHAR* GetNaiwaP3LootDisplayName(const ENaiwaP3LootType Type)
{
	switch (Type)
	{
	case ENaiwaP3LootType::BombCapacity: return TEXT("炸弹上限 +1");
	case ENaiwaP3LootType::BombRange: return TEXT("威力 +1");
	case ENaiwaP3LootType::ChickenJump: return TEXT("奶鸡跳");
	case ENaiwaP3LootType::SpeedBoost: return TEXT("速度 +15%");
	case ENaiwaP3LootType::Shield: return TEXT("奶罩护盾");
	case ENaiwaP3LootType::Xray: return TEXT("透视 3 秒");
	case ENaiwaP3LootType::ChaosReverse: return TEXT("混乱奶反向操作");
	default: return TEXT("未知道具");
	}
}
