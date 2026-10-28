#include "NaiwaP4Determinism.h"

#include "NaiwaP4ArenaState.h"
#include "Containers/StringConv.h"

namespace
{
	constexpr uint64 FnvOffsetBasis = 14695981039346656037ull;
	constexpr uint64 FnvPrime = 1099511628211ull;

	struct FStableHash64
	{
		uint64 Value = FnvOffsetBasis;

		void AddByte(const uint8 Byte)
		{
			Value ^= Byte;
			Value *= FnvPrime;
		}

		void AddUInt32(const uint32 Number)
		{
			for (uint32 Shift = 0; Shift < 32; Shift += 8)
			{
				AddByte(static_cast<uint8>((Number >> Shift) & 0xffu));
			}
		}

		void AddUInt64(const uint64 Number)
		{
			for (uint32 Shift = 0; Shift < 64; Shift += 8)
			{
				AddByte(static_cast<uint8>((Number >> Shift) & 0xffull));
			}
		}

		void AddInt32(const int32 Number)
		{
			AddUInt32(static_cast<uint32>(Number));
		}

		void AddInt64(const int64 Number)
		{
			AddUInt64(static_cast<uint64>(Number));
		}

		void AddBool(const bool bValue)
		{
			AddByte(bValue ? 1u : 0u);
		}

		void AddFloat(const float Number)
		{
			uint32 Bits = 0;
			static_assert(sizeof(Bits) == sizeof(Number), "Unexpected float width for P4 state hash");
			FMemory::Memcpy(&Bits, &Number, sizeof(Bits));
			AddUInt32(Bits);
		}

		void AddDouble(const double Number)
		{
			uint64 Bits = 0;
			static_assert(sizeof(Bits) == sizeof(Number), "Unexpected double width for P4 state hash");
			FMemory::Memcpy(&Bits, &Number, sizeof(Bits));
			AddUInt64(Bits);
		}

		void AddPosition(const FVector2D& Position)
		{
			AddDouble(Position.X);
			AddDouble(Position.Y);
		}

		void AddName(const FName Name)
		{
			const FString NameString = Name.ToString();
			FTCHARToUTF8 Utf8Name(*NameString);
			AddUInt32(static_cast<uint32>(Utf8Name.Length()));
			for (int32 Index = 0; Index < Utf8Name.Length(); ++Index)
			{
				AddByte(static_cast<uint8>(Utf8Name.Get()[Index]));
			}
		}
	};

	void AddEntity(FStableHash64& Hash, const FNaiwaP4EntityState& Entity)
	{
		Hash.AddInt32(Entity.EntityId);
		Hash.AddByte(static_cast<uint8>(Entity.Kind));
		Hash.AddPosition(Entity.Position);
		Hash.AddPosition(Entity.Velocity);
		Hash.AddFloat(Entity.HeightZ);
		Hash.AddInt32(Entity.HP);
		Hash.AddByte(static_cast<uint8>(Entity.Facing));
		Hash.AddBool(Entity.bFacingRight);
		Hash.AddInt32(Entity.ActionTick);
		Hash.AddBool(Entity.bAlive);
	}
}

uint64 FNaiwaP4Determinism::ComputeSimulationStateHashV1(const FNaiwaP4ArenaState& State)
{
	FStableHash64 Hash;
	Hash.AddInt64(State.CurrentTick);
	Hash.AddUInt32(State.MasterSeed);
	Hash.AddUInt32(State.RngState);
	Hash.AddInt32(State.NextEntityId);

	TArray<FNaiwaP4EntityState> SortedEntities = State.Entities;
	SortedEntities.Sort([](const FNaiwaP4EntityState& Left, const FNaiwaP4EntityState& Right)
	{
		return Left.EntityId < Right.EntityId;
	});
	for (int32 Index = 1; Index < SortedEntities.Num(); ++Index)
	{
		checkf(SortedEntities[Index - 1].EntityId != SortedEntities[Index].EntityId,
			TEXT("P4 deterministic state hash requires unique EntityId values; duplicate=%d"),
			SortedEntities[Index].EntityId);
	}
	Hash.AddUInt32(static_cast<uint32>(SortedEntities.Num()));
	for (const FNaiwaP4EntityState& Entity : SortedEntities)
	{
		AddEntity(Hash, Entity);
	}

	// Damage resolution is pending authoritative work, so preserve its queue order.
	Hash.AddUInt32(static_cast<uint32>(State.PendingDamage.Num()));
	for (const FNaiwaP4DamageEvent& Damage : State.PendingDamage)
	{
		Hash.AddInt32(Damage.SourceEntityId);
		Hash.AddInt32(Damage.TargetEntityId);
		Hash.AddInt32(Damage.Damage);
		Hash.AddPosition(Damage.ImpactPosition);
	}

	Hash.AddUInt32(static_cast<uint32>(State.Events.Num()));
	for (const FNaiwaP4Event& Event : State.Events)
	{
		Hash.AddName(Event.EventId);
		Hash.AddInt32(Event.SourceEntityId);
		Hash.AddInt32(Event.TargetEntityId);
		Hash.AddInt64(Event.Tick);
		Hash.AddPosition(Event.Position);
	}

	return Hash.Value;
}

FString FNaiwaP4Determinism::FormatHash(const uint64 Hash)
{
	return FString::Printf(TEXT("p4s1:%016llX"), static_cast<unsigned long long>(Hash));
}
