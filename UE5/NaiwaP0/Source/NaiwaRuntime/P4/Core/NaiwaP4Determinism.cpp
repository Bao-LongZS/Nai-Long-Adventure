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

	void AddEntityV2(FStableHash64& Hash, const FNaiwaP4EntityState& Entity)
	{
		AddEntity(Hash, Entity);
		Hash.AddInt32(Entity.MaxHP);
		Hash.AddFloat(Entity.CollisionRadius);
		Hash.AddInt32(Entity.EnergyHundredths);
		Hash.AddInt32(Entity.AttackCooldownTicksRemaining);
		Hash.AddInt32(Entity.AttackCueTicksRemaining);
		Hash.AddInt32(Entity.InvulnerabilityTicksRemaining);
		Hash.AddInt32(Entity.HitCueTicksRemaining);
		Hash.AddInt32(Entity.LaughTicksRemaining);
		Hash.AddInt32(Entity.DashCharges);
		Hash.AddInt32(Entity.DashChargesMax);
		Hash.AddInt32(Entity.DashRechargeTicksRemaining);
		Hash.AddInt32(Entity.DashTicksRemaining);
		Hash.AddInt32(Entity.DashRecoveryTicksRemaining);
		Hash.AddPosition(Entity.DashDirection);
		TArray<int32> SortedDashHitEntityIds = Entity.DashHitEntityIds;
		SortedDashHitEntityIds.Sort();
		Hash.AddUInt32(static_cast<uint32>(SortedDashHitEntityIds.Num()));
		for (const int32 HitEntityId : SortedDashHitEntityIds)
		{
			Hash.AddInt32(HitEntityId);
		}
		Hash.AddInt32(Entity.ProjectileOwnerEntityId);
		Hash.AddInt32(Entity.ProjectileDamage);
		Hash.AddInt32(Entity.ProjectileLifetimeTicksRemaining);
		Hash.AddBool(Entity.bEnemyProjectile);
		Hash.AddBool(Entity.bTrainingEnemy);
		Hash.AddPosition(Entity.EnemySpawnPosition);
		Hash.AddFloat(Entity.EnemyMoveSpeed);
		Hash.AddFloat(Entity.EnemyAttackRange);
		Hash.AddInt32(Entity.EnemyDamage);
		Hash.AddInt32(Entity.EnemyAttackCooldownTicksRemaining);
		Hash.AddInt32(Entity.EnemyTelegraphTicksRemaining);
		Hash.AddInt32(Entity.TrainingRespawnTicksRemaining);
	}

	void AddEntityV3(FStableHash64& Hash, const FNaiwaP4EntityState& Entity)
	{
		AddEntityV2(Hash, Entity);
		Hash.AddInt32(Entity.EnemyAttackIntervalTicks);
		Hash.AddInt32(Entity.EnemyTelegraphDurationTicks);
		Hash.AddBool(Entity.bWaveEnemy);
		Hash.AddName(Entity.EnemyTypeId);
		Hash.AddInt32(Entity.SpawnWaveIndex);
		Hash.AddBool(Entity.bFrontShield75);
	}

	void AddEntityV4(FStableHash64& Hash, const FNaiwaP4EntityState& Entity)
	{
		AddEntityV3(Hash, Entity);
		Hash.AddInt32(Entity.DashRecoveryDurationTicks);
		Hash.AddInt32(Entity.ProjectileBouncesPerShot);
		Hash.AddFloat(Entity.ProjectileBounceDamageScale);
		Hash.AddBool(Entity.bGainEnergyWhenHit);
		Hash.AddInt32(Entity.EnergyOnHitHundredths);
		Hash.AddInt32(Entity.ProjectileBouncesRemaining);
		TArray<int32> SortedProjectileHitEntityIds = Entity.ProjectileHitEntityIds;
		SortedProjectileHitEntityIds.Sort();
		Hash.AddUInt32(static_cast<uint32>(SortedProjectileHitEntityIds.Num()));
		for (const int32 HitEntityId : SortedProjectileHitEntityIds)
		{
			Hash.AddInt32(HitEntityId);
		}
	}

	void AddEntityV5(FStableHash64& Hash, const FNaiwaP4EntityState& Entity)
	{
		AddEntityV4(Hash, Entity);
		Hash.AddInt32(Entity.MeleeHitTicksRemaining);
		Hash.AddInt32(Entity.MeleeContactTicksRemaining);
		Hash.AddInt32(Entity.ProjectileAutoFireCooldownTicksRemaining);
		Hash.AddInt32(Entity.MeleeDamage);
		Hash.AddInt32(Entity.MeleeDamageBonusPerRank);
		Hash.AddInt32(Entity.PlayerProjectileDamage);
	}

	void AddEntityV6(FStableHash64& Hash, const FNaiwaP4EntityState& Entity)
	{
		AddEntityV5(Hash, Entity);
		Hash.AddBool(Entity.bWaveBoss);
		Hash.AddByte(static_cast<uint8>(Entity.BossAttack));
		Hash.AddPosition(Entity.BossLockedTargetPosition);
		Hash.AddInt32(Entity.BossPhase);
		Hash.AddInt32(Entity.BossPhaseTransitionTicksRemaining);
	}

	void AddEntityV7(FStableHash64& Hash, const FNaiwaP4EntityState& Entity)
	{
		AddEntityV6(Hash, Entity);
		Hash.AddInt32(Entity.MeleeInputBufferTicksRemaining);
		Hash.AddInt32(Entity.DashInputBufferTicksRemaining);
		Hash.AddInt32(Entity.UltimateInputBufferTicksRemaining);
		Hash.AddPosition(Entity.BossChargeStartPosition);
		Hash.AddPosition(Entity.BossChargeEndPosition);
		Hash.AddInt32(Entity.BossChargeTicksRemaining);
		Hash.AddInt32(Entity.BossChargeDurationTicks);
		Hash.AddBool(Entity.bBossChargeHitPlayer);
	}

	uint64 ComputeStateHash(const FNaiwaP4ArenaState& State, const bool bIncludeCombatState,
		const bool bIncludeWaveState, const bool bIncludeProgressionState = false,
		const bool bIncludeUpgradeOfferState = false, const bool bIncludeBossState = false,
		const bool bIncludeBufferedActionState = false)
	{
		FStableHash64 Hash;
		Hash.AddInt64(State.CurrentTick);
		Hash.AddUInt32(State.MasterSeed);
		Hash.AddUInt32(State.RngState);
		Hash.AddInt32(State.NextEntityId);
		if (bIncludeWaveState)
		{
			Hash.AddInt32(State.CurrentWaveIndex);
			Hash.AddInt32(State.TotalWaveCount);
			Hash.AddInt32(State.WaveSpawnEntryIndex);
			Hash.AddInt32(State.WaveSpawnedInEntry);
			Hash.AddInt32(State.WaveSpawnTicksRemaining);
			Hash.AddInt32(State.TotalWaveEnemyCount);
			Hash.AddInt32(State.DefeatedWaveEnemyCount);
			Hash.AddBool(State.bWaveSpawningComplete);
			Hash.AddBool(State.bWaveRunComplete);
		}
		if (bIncludeBossState)
		{
			Hash.AddBool(State.bBossSpawned);
			Hash.AddBool(State.bBossDefeated);
			Hash.AddByte(static_cast<uint8>(State.RunResult));
		}
		if (bIncludeProgressionState)
		{
			Hash.AddInt32(State.PlayerLevel);
			Hash.AddInt32(State.PlayerExperience);
			Hash.AddInt32(State.PlayerExperienceForNextLevel);
			Hash.AddInt32(State.PendingUpgradeChoices);
			Hash.AddUInt32(static_cast<uint32>(State.AcquiredUpgradeCardIds.Num()));
			for (const FName CardId : State.AcquiredUpgradeCardIds)
			{
				Hash.AddName(CardId);
			}
		}
		if (bIncludeUpgradeOfferState)
		{
			Hash.AddInt32(State.UpgradeRerollsRemaining);
			Hash.AddInt32(State.UpgradeOfferRevision);
			Hash.AddUInt32(static_cast<uint32>(State.CurrentUpgradeOfferCardIds.Num()));
			for (const FName CardId : State.CurrentUpgradeOfferCardIds)
			{
				Hash.AddName(CardId);
			}
		}

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
			if (bIncludeBufferedActionState)
			{
				AddEntityV7(Hash, Entity);
			}
			else if (bIncludeBossState)
			{
				AddEntityV6(Hash, Entity);
			}
			else if (bIncludeUpgradeOfferState)
			{
				AddEntityV5(Hash, Entity);
			}
			else if (bIncludeProgressionState)
			{
				AddEntityV4(Hash, Entity);
			}
			else if (bIncludeCombatState && bIncludeWaveState)
			{
				AddEntityV3(Hash, Entity);
			}
			else if (bIncludeCombatState)
			{
				AddEntityV2(Hash, Entity);
			}
			else
			{
				AddEntity(Hash, Entity);
			}
		}

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
}

uint64 FNaiwaP4Determinism::ComputeSimulationStateHashV1(const FNaiwaP4ArenaState& State)
{
	return ComputeStateHash(State, false, false);
}

uint64 FNaiwaP4Determinism::ComputeSimulationStateHashV2(const FNaiwaP4ArenaState& State)
{
	return ComputeStateHash(State, true, false);
}

uint64 FNaiwaP4Determinism::ComputeSimulationStateHashV3(const FNaiwaP4ArenaState& State)
{
	return ComputeStateHash(State, true, true);
}

uint64 FNaiwaP4Determinism::ComputeSimulationStateHashV4(const FNaiwaP4ArenaState& State)
{
	return ComputeStateHash(State, true, true, true);
}

uint64 FNaiwaP4Determinism::ComputeSimulationStateHashV5(const FNaiwaP4ArenaState& State)
{
	return ComputeStateHash(State, true, true, true, true);
}

uint64 FNaiwaP4Determinism::ComputeSimulationStateHashV6(const FNaiwaP4ArenaState& State)
{
	return ComputeStateHash(State, true, true, true, true, true);
}

uint64 FNaiwaP4Determinism::ComputeSimulationStateHashV7(const FNaiwaP4ArenaState& State)
{
	return ComputeStateHash(State, true, true, true, true, true, true);
}

FString FNaiwaP4Determinism::FormatHash(const uint64 Hash)
{
	return FString::Printf(TEXT("p4s1:%016llX"), static_cast<unsigned long long>(Hash));
}

FString FNaiwaP4Determinism::FormatHashV2(const uint64 Hash)
{
	return FString::Printf(TEXT("p4s2:%016llX"), static_cast<unsigned long long>(Hash));
}

FString FNaiwaP4Determinism::FormatHashV3(const uint64 Hash)
{
	return FString::Printf(TEXT("p4s3:%016llX"), static_cast<unsigned long long>(Hash));
}

FString FNaiwaP4Determinism::FormatHashV4(const uint64 Hash)
{
	return FString::Printf(TEXT("p4s4:%016llX"), static_cast<unsigned long long>(Hash));
}

FString FNaiwaP4Determinism::FormatHashV5(const uint64 Hash)
{
	return FString::Printf(TEXT("p4s5:%016llX"), static_cast<unsigned long long>(Hash));
}

FString FNaiwaP4Determinism::FormatHashV6(const uint64 Hash)
{
	return FString::Printf(TEXT("p4s6:%016llX"), static_cast<unsigned long long>(Hash));
}

FString FNaiwaP4Determinism::FormatHashV7(const uint64 Hash)
{
	return FString::Printf(TEXT("p4s7:%016llX"), static_cast<unsigned long long>(Hash));
}
