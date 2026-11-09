#include "NaiwaP4ArenaSubsystem.h"

#include "NaiwaP4Determinism.h"
#include "NaiwaP4Settings.h"
#include "Engine/DataTable.h"
#include "Containers/StringConv.h"
#include "Misc/FileHelper.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	constexpr int32 PlayerEnergyFullHundredths = 10000;
	constexpr int32 PlayerProjectileDamage = 12;
	constexpr int32 GrayboxEnemyHP = 36;
	constexpr int32 GrayboxEnemyDamage = 8;
	constexpr double GrayboxEnemySpeed = 132.0;
	constexpr double GrayboxEnemyRange = 72.0;
	constexpr double GrayboxEnemyAttackIntervalSeconds = 1.2;
	constexpr double GrayboxEnemyTelegraphSeconds = 0.35;
	constexpr double PlayerCollisionRadius = 24.0;
	constexpr double GrayboxEnemyCollisionRadius = 30.0;
	constexpr double PlayerProjectileRadius = 8.0;
	constexpr double PlayerProjectileRange = 690.0;
	constexpr double PlayerProjectileSpeed = 760.0;
	constexpr double PlayerProjectileIntervalSeconds = 0.59;
	constexpr double PlayerMeleeRange = 160.0;
	constexpr double PlayerMeleeCooldownSeconds = 0.52;
	constexpr double PlayerMeleeWindupSeconds = 0.12;
	constexpr double PlayerMeleeCueSeconds = 0.30;
	constexpr double PlayerActionInputBufferSeconds = 0.28;
	constexpr double DamageKnockbackDistance = 32.0;
	constexpr int32 NaiTongBossMaxHP = 480;
	constexpr float NaiTongBossRadius = 78.0f;
	constexpr double NaiTongBossMoveSpeed = 92.0;
	constexpr double NaiTongBossAttackRange = 270.0;
	constexpr int32 NaiTongBossAttackDamage = 24;
	constexpr double NaiTongBossAttackCooldownSeconds = 0.82;
	constexpr double NaiTongBossChargeTelegraphSeconds = 0.90;
	constexpr double NaiTongBossChargeSpeed = 1200.0;
	constexpr double NaiTongBossSlamTelegraphSeconds = 1.05;

	bool IsDamageTarget(const FNaiwaP4EntityState& Entity, const bool bForPlayerProjectile)
	{
		if (!Entity.bAlive)
		{
			return false;
		}
		return bForPlayerProjectile
			? (Entity.Kind == ENaiwaP4EntityKind::Enemy || Entity.Kind == ENaiwaP4EntityKind::Boss)
			: Entity.Kind == ENaiwaP4EntityKind::Player;
	}

	double DistanceSquaredToSegment(const FVector2D& Point, const FVector2D& Start, const FVector2D& End)
	{
		const FVector2D Segment = End - Start;
		const double LengthSquared = Segment.SizeSquared();
		if (LengthSquared <= UE_SMALL_NUMBER)
		{
			return FVector2D::DistSquared(Point, Start);
		}

		const double T = FMath::Clamp(FVector2D::DotProduct(Point - Start, Segment) / LengthSquared, 0.0, 1.0);
		const FVector2D Closest = Start + Segment * T;
		return FVector2D::DistSquared(Point, Closest);
	}

	FVector2D GetImpactKnockbackDirection(const FNaiwaP4EntityState& Target,
		const FNaiwaP4EntityState* Source, const FVector2D& ImpactPosition)
	{
		FVector2D Direction = Source ? (Target.Position - Source->Position).GetSafeNormal()
			: (Target.Position - ImpactPosition).GetSafeNormal();
		if (!Direction.IsNearlyZero())
		{
			return Direction;
		}
		const FNaiwaP4EntityState& Fallback = Source ? *Source : Target;
		if (Fallback.Facing == ENaiwaP4Facing::Back)
		{
			return FVector2D(0.0, 1.0);
		}
		if (Fallback.Facing == ENaiwaP4Facing::Front)
		{
			return FVector2D(0.0, -1.0);
		}
		return FVector2D(Fallback.bFacingRight ? 1.0 : -1.0, 0.0);
	}

	bool ReadUtf8CsvRows(const FString& Path, TArray<TArray<FString>>& OutRows, FString& OutError)
	{
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *Path) || Bytes.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Could not read P4 combat CSV '%s'."), *Path);
			return false;
		}

		int32 ByteOffset = Bytes.Num() >= 3 && Bytes[0] == 0xef && Bytes[1] == 0xbb && Bytes[2] == 0xbf ? 3 : 0;
		const ANSICHAR* Utf8 = reinterpret_cast<const ANSICHAR*>(Bytes.GetData() + ByteOffset);
		FUTF8ToTCHAR Converted(Utf8, Bytes.Num() - ByteOffset);
		const FString CsvText(Converted.Length(), Converted.Get());
		TArray<FString> Lines;
		CsvText.ParseIntoArrayLines(Lines, true);
		OutRows.Reset();
		for (FString& Line : Lines)
		{
			Line.TrimStartAndEndInline();
			if (Line.IsEmpty())
			{
				continue;
			}
			TArray<FString>& Cells = OutRows.AddDefaulted_GetRef();
			Line.ParseIntoArray(Cells, TEXT(","), false);
			for (FString& Cell : Cells)
			{
				Cell.TrimStartAndEndInline();
			}
		}
		if (OutRows.IsEmpty())
		{
			OutError = FString::Printf(TEXT("P4 combat CSV '%s' has no rows."), *Path);
			return false;
		}
		return true;
	}

	bool CsvHeaderMatches(const TArray<FString>& Actual, const TArray<FString>& Expected)
	{
		if (Actual.Num() != Expected.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Expected.Num(); ++Index)
		{
			if (!Actual[Index].Equals(Expected[Index], ESearchCase::IgnoreCase))
			{
				return false;
			}
		}
		return true;
	}
}

void UNaiwaP4ArenaSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UNaiwaP4Settings* Settings = GetDefault<UNaiwaP4Settings>();
	if (Settings)
	{
		FixedStepSeconds = FMath::Clamp(Settings->FixedStepSeconds, 0.001, 0.05);
		MaxCatchUpSteps = FMath::Clamp(Settings->MaxCatchUpSteps, 1, 8);
		LogicalArenaWidth = FMath::Max(100.0, static_cast<double>(Settings->LogicalArenaWidth));
		LogicalArenaDepth = FMath::Max(50.0, static_cast<double>(Settings->LogicalArenaDepth));
		PlayerWalkSpeed = FMath::Max(0.0, static_cast<double>(Settings->WalkSpeed));
		PlayerDepthSpeedScale = FMath::Clamp(static_cast<double>(Settings->DepthSpeedScale), 0.0, 1.0);
		PlayerBlockMoveSpeedScale = FMath::Clamp(static_cast<double>(Settings->BlockMoveSpeedScale), 0.0, 1.0);
		PlayerMaxHP = FMath::Max(1, Settings->PlayerMaxHP);
	}
	State.Reset(0);
}

void UNaiwaP4ArenaSubsystem::Deinitialize()
{
	bSimulationActive = false;
	Super::Deinitialize();
}

void UNaiwaP4ArenaSubsystem::Tick(const float DeltaTime)
{
	if (!bSimulationActive || State.PendingUpgradeChoices > 0 || DeltaTime <= 0.0f || FixedStepSeconds <= 0.0)
	{
		return;
	}

	FixedAccumulatorSeconds += static_cast<double>(DeltaTime);
	int32 Steps = 0;
	// Bound work per frame without discarding simulation time; later ticks drain the backlog.
	while (FixedAccumulatorSeconds >= FixedStepSeconds && Steps < MaxCatchUpSteps
		&& State.PendingUpgradeChoices <= 0)
	{
		FixedAccumulatorSeconds = FMath::Max(0.0, FixedAccumulatorSeconds - FixedStepSeconds);
		AdvanceFixedStep();
		++Steps;
	}
}

TStatId UNaiwaP4ArenaSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UNaiwaP4ArenaSubsystem, STATGROUP_Tickables);
}

void UNaiwaP4ArenaSubsystem::StartSimulation(const uint32 MasterSeed)
{
	State.Reset(MasterSeed);
	FNaiwaP4EntityState Player;
	Player.EntityId = State.NextEntityId++;
	Player.Kind = ENaiwaP4EntityKind::Player;
	Player.Position = FVector2D(LogicalArenaWidth * 0.5, LogicalArenaDepth * 0.5);
	Player.HP = PlayerMaxHP;
	Player.MaxHP = PlayerMaxHP;
	Player.PlayerProjectileDamage = PlayerProjectileDamage;
	Player.CollisionRadius = PlayerCollisionRadius;
	Player.DashCharges = 2;
	Player.DashChargesMax = 2;
	Player.DashRecoveryDurationTicks = SecondsToTicks(0.24);
	Player.Facing = ENaiwaP4Facing::Side;
	State.Entities.Add(Player);
	PreviousPresentationPositions.Reset();
	PreviousPresentationPositions.Add(Player.EntityId, Player.Position);
	InputBuffer.Reset();
	LastConsumedPlayerCommand = FNaiwaP4InputCommand{};
	FixedAccumulatorSeconds = 0.0;
	bSimulationActive = true;
}

void UNaiwaP4ArenaSubsystem::SetArenaDepthForPreview(const double Depth)
{
	if (!bSimulationActive)
	{
		LogicalArenaDepth = FMath::Max(50.0, Depth);
	}
}

void UNaiwaP4ArenaSubsystem::StartGrayboxCombatPreview()
{
	if (!bSimulationActive)
	{
		StartSimulation(0x50345732u);
	}

	FNaiwaP4EntityState* Player = FindPlayerEntity();
	if (!Player)
	{
		return;
	}
	Player->EnergyHundredths = PlayerEnergyFullHundredths;

	const int32 PlayerEntityId = Player->EntityId;
	const FVector2D Center = Player->Position;
	const FVector2D SpawnPositions[] = {
		FVector2D(Center.X - 230.0, FMath::Clamp(Center.Y - 35.0, 0.0, LogicalArenaDepth)),
		FVector2D(Center.X + 230.0, FMath::Clamp(Center.Y + 35.0, 0.0, LogicalArenaDepth)),
	};
	for (const FVector2D& SpawnPosition : SpawnPositions)
	{
		FNaiwaP4EntityState Enemy;
		Enemy.EntityId = State.NextEntityId++;
		Enemy.Kind = ENaiwaP4EntityKind::Enemy;
		Enemy.Position = SpawnPosition;
		Enemy.EnemySpawnPosition = SpawnPosition;
		Enemy.HP = GrayboxEnemyHP;
		Enemy.MaxHP = GrayboxEnemyHP;
		Enemy.CollisionRadius = GrayboxEnemyCollisionRadius;
		Enemy.EnemyMoveSpeed = static_cast<float>(GrayboxEnemySpeed);
		Enemy.EnemyAttackRange = static_cast<float>(GrayboxEnemyRange);
		Enemy.EnemyDamage = GrayboxEnemyDamage;
		Enemy.bTrainingEnemy = true;
		Enemy.bFacingRight = SpawnPosition.X < Center.X;
		Enemy.Facing = ENaiwaP4Facing::Side;
		State.Entities.Add(Enemy);
		PreviousPresentationPositions.Add(Enemy.EntityId, Enemy.Position);
		EmitCombatEvent(TEXT("graybox_enemy_spawned"), Enemy.EntityId, PlayerEntityId, SpawnPosition);
	}
}

bool UNaiwaP4ArenaSubsystem::LoadCombatDataTables(const UDataTable* EnemyTable,
	const UDataTable* WaveTable, FString& OutError)
{
	if (!EnemyTable || EnemyTable->GetRowStruct() != FNaiwaP4EnemyRow::StaticStruct())
	{
		OutError = TEXT("Enemy DataTable is missing or uses the wrong row struct.");
		return false;
	}
	if (!WaveTable || WaveTable->GetRowStruct() != FNaiwaP4WaveSpawnRow::StaticStruct())
	{
		OutError = TEXT("Wave DataTable is missing or uses the wrong row struct.");
		return false;
	}

	TMap<FName, FNaiwaP4EnemyRow> CandidateEnemies;
	for (const FName RowName : EnemyTable->GetRowNames())
	{
		const FNaiwaP4EnemyRow* Row = EnemyTable->FindRow<FNaiwaP4EnemyRow>(RowName, TEXT("P4 W3 enemy table validation"), false);
		if (!Row || Row->MaxHP <= 0 || Row->Damage <= 0 || Row->MoveSpeed < 0.0f
			|| Row->AttackRange <= 0.0f || Row->AttackIntervalSeconds <= 0.0f
			|| Row->TelegraphSeconds <= 0.0f || Row->CollisionRadius <= 0.0f)
		{
			OutError = FString::Printf(TEXT("Enemy DataTable row '%s' has invalid combat values."), *RowName.ToString());
			return false;
		}
		CandidateEnemies.Add(RowName, *Row);
	}
	if (CandidateEnemies.IsEmpty())
	{
		OutError = TEXT("Enemy DataTable has no rows.");
		return false;
	}

	TArray<FNaiwaP4WaveSpawnEntry> CandidatePlan;
	int32 MaxWaveIndex = 0;
	int64 PlannedEnemyCount = 0;
	for (const FName RowName : WaveTable->GetRowNames())
	{
		const FNaiwaP4WaveSpawnRow* Row = WaveTable->FindRow<FNaiwaP4WaveSpawnRow>(RowName, TEXT("P4 W3 wave table validation"), false);
		if (!Row || Row->WaveIndex <= 0 || Row->Count <= 0 || Row->SpawnIntervalSeconds < 0.0f
			|| !CandidateEnemies.Contains(Row->EnemyTypeId))
		{
			OutError = FString::Printf(TEXT("Wave DataTable row '%s' is invalid or references a missing enemy type."), *RowName.ToString());
			return false;
		}
		FNaiwaP4WaveSpawnEntry& Entry = CandidatePlan.AddDefaulted_GetRef();
		Entry.WaveIndex = Row->WaveIndex;
		Entry.SpawnOrder = Row->SpawnOrder;
		Entry.RowId = RowName;
		Entry.EnemyTypeId = Row->EnemyTypeId;
		Entry.Count = Row->Count;
		Entry.SpawnIntervalTicks = SecondsToTicks(Row->SpawnIntervalSeconds);
		MaxWaveIndex = FMath::Max(MaxWaveIndex, Row->WaveIndex);
		PlannedEnemyCount += Row->Count;
	}
	if (CandidatePlan.IsEmpty() || PlannedEnemyCount <= 0 || PlannedEnemyCount > MAX_int32)
	{
		OutError = TEXT("Wave DataTable has no spawn rows or exceeds the supported enemy count.");
		return false;
	}

	CandidatePlan.Sort([](const FNaiwaP4WaveSpawnEntry& Left, const FNaiwaP4WaveSpawnEntry& Right)
	{
		if (Left.WaveIndex != Right.WaveIndex) return Left.WaveIndex < Right.WaveIndex;
		if (Left.SpawnOrder != Right.SpawnOrder) return Left.SpawnOrder < Right.SpawnOrder;
		if (Left.EnemyTypeId != Right.EnemyTypeId) return Left.EnemyTypeId.LexicalLess(Right.EnemyTypeId);
		return Left.RowId.LexicalLess(Right.RowId);
	});
	for (int32 WaveIndex = 1; WaveIndex <= MaxWaveIndex; ++WaveIndex)
	{
		if (!CandidatePlan.ContainsByPredicate([WaveIndex](const FNaiwaP4WaveSpawnEntry& Entry)
			{ return Entry.WaveIndex == WaveIndex; }))
		{
			OutError = FString::Printf(TEXT("Wave DataTable is missing wave %d; wave indices must be contiguous."), WaveIndex);
			return false;
		}
	}

	EnemyCatalog = MoveTemp(CandidateEnemies);
	WaveSpawnPlan = MoveTemp(CandidatePlan);
	OutError.Reset();
	UE_LOG(LogTemp, Display, TEXT("P4 W3 loaded %d enemy archetypes and %d wave spawn rows (%d planned enemies)."),
		EnemyCatalog.Num(), WaveSpawnPlan.Num(), static_cast<int32>(PlannedEnemyCount));
	return true;
}

bool UNaiwaP4ArenaSubsystem::StartWaveCombatPreview()
{
	if (WaveSpawnPlan.IsEmpty() || EnemyCatalog.IsEmpty())
	{
		return false;
	}
	if (!bSimulationActive)
	{
		StartSimulation(0x50345733u);
	}
	if (State.Entities.Num() != 1 || !FindPlayerEntity())
	{
		return false;
	}

	if (FNaiwaP4EntityState* Player = FindPlayerEntity())
	{
		Player->EnergyHundredths = PlayerEnergyFullHundredths;
	}
	State.TotalWaveCount = WaveSpawnPlan.Last().WaveIndex;
	for (const FNaiwaP4WaveSpawnEntry& Entry : WaveSpawnPlan)
	{
		State.TotalWaveEnemyCount += Entry.Count;
	}
	BeginWave(1);
	return true;
}

bool UNaiwaP4ArenaSubsystem::RestartWaveCombatPreview()
{
	if (WaveSpawnPlan.IsEmpty() || EnemyCatalog.IsEmpty())
	{
		return false;
	}

	const uint32 RestartSeed = State.MasterSeed;
	const TArray<FName> RunUpgrades = State.AcquiredUpgradeCardIds;
	StartSimulation(RestartSeed);
	if (!StartWaveCombatPreview())
	{
		return false;
	}

	// Reapply the saved card sequence without consuming RNG so enemy spawn points,
	// card offers, and combat hashes remain reproducible from the original seed.
	for (const FName CardId : RunUpgrades)
	{
		State.PendingUpgradeChoices = 1;
		State.CurrentUpgradeOfferCardIds = { CardId };
		FString Error;
		if (!ConfirmUpgradeChoice(CardId, Error))
		{
			UE_LOG(LogTemp, Warning, TEXT("P4 retry could not restore upgrade '%s': %s"), *CardId.ToString(), *Error);
			State.PendingUpgradeChoices = 0;
			State.CurrentUpgradeOfferCardIds.Reset();
		}
	}
	return true;
}

bool UNaiwaP4ArenaSubsystem::LoadCombatCsvFiles(const FString& EnemyCsvPath,
	const FString& WaveCsvPath, FString& OutError)
{
	TArray<TArray<FString>> EnemyRows;
	TArray<TArray<FString>> WaveRows;
	if (!ReadUtf8CsvRows(EnemyCsvPath, EnemyRows, OutError)
		|| !ReadUtf8CsvRows(WaveCsvPath, WaveRows, OutError))
	{
		return false;
	}

	const TArray<FString> ExpectedEnemyHeader = {
		TEXT("RowName"), TEXT("DisplayName"), TEXT("MaxHP"), TEXT("Damage"), TEXT("MoveSpeed"),
		TEXT("AttackRange"), TEXT("AttackIntervalSeconds"), TEXT("TelegraphSeconds"),
		TEXT("CollisionRadius"), TEXT("bFrontShield75")
	};
	const TArray<FString> ExpectedWaveHeader = {
		TEXT("RowName"), TEXT("WaveIndex"), TEXT("SpawnOrder"), TEXT("EnemyTypeId"),
		TEXT("Count"), TEXT("SpawnIntervalSeconds")
	};
	if (!CsvHeaderMatches(EnemyRows[0], ExpectedEnemyHeader)
		|| !CsvHeaderMatches(WaveRows[0], ExpectedWaveHeader))
	{
		OutError = TEXT("P4 W3 CSV columns do not match the enemy/wave row schemas.");
		return false;
	}

	UDataTable* EnemyTable = NewObject<UDataTable>(GetTransientPackageAsObject());
	UDataTable* WaveTable = NewObject<UDataTable>(GetTransientPackageAsObject());
	if (!EnemyTable || !WaveTable)
	{
		OutError = TEXT("Could not create transient P4 W3 DataTables from the CSV sources.");
		return false;
	}
	EnemyTable->RowStruct = FNaiwaP4EnemyRow::StaticStruct();
	WaveTable->RowStruct = FNaiwaP4WaveSpawnRow::StaticStruct();

	for (int32 Index = 1; Index < EnemyRows.Num(); ++Index)
	{
		const TArray<FString>& Cells = EnemyRows[Index];
		if (Cells.Num() != ExpectedEnemyHeader.Num() || Cells[0].IsEmpty())
		{
			OutError = FString::Printf(TEXT("P4 enemy CSV row %d has the wrong number of fields."), Index + 1);
			return false;
		}
		FNaiwaP4EnemyRow Row;
		Row.DisplayName = FText::FromString(Cells[1]);
		Row.MaxHP = FCString::Atoi(*Cells[2]);
		Row.Damage = FCString::Atoi(*Cells[3]);
		Row.MoveSpeed = FCString::Atof(*Cells[4]);
		Row.AttackRange = FCString::Atof(*Cells[5]);
		Row.AttackIntervalSeconds = FCString::Atof(*Cells[6]);
		Row.TelegraphSeconds = FCString::Atof(*Cells[7]);
		Row.CollisionRadius = FCString::Atof(*Cells[8]);
		Row.bFrontShield75 = FCString::ToBool(*Cells[9]);
		EnemyTable->AddRow(FName(*Cells[0]), Row);
	}
	for (int32 Index = 1; Index < WaveRows.Num(); ++Index)
	{
		const TArray<FString>& Cells = WaveRows[Index];
		if (Cells.Num() != ExpectedWaveHeader.Num() || Cells[0].IsEmpty())
		{
			OutError = FString::Printf(TEXT("P4 wave CSV row %d has the wrong number of fields."), Index + 1);
			return false;
		}
		FNaiwaP4WaveSpawnRow Row;
		Row.WaveIndex = FCString::Atoi(*Cells[1]);
		Row.SpawnOrder = FCString::Atoi(*Cells[2]);
		Row.EnemyTypeId = FName(*Cells[3]);
		Row.Count = FCString::Atoi(*Cells[4]);
		Row.SpawnIntervalSeconds = FCString::Atof(*Cells[5]);
		WaveTable->AddRow(FName(*Cells[0]), Row);
	}
	return LoadCombatDataTables(EnemyTable, WaveTable, OutError);
}

void UNaiwaP4ArenaSubsystem::StopSimulation()
{
	bSimulationActive = false;
	ClearPlayerInput();
	FixedAccumulatorSeconds = 0.0;
}

void UNaiwaP4ArenaSubsystem::ClearPlayerInput()
{
	InputBuffer.Reset();
	LastConsumedPlayerCommand = FNaiwaP4InputCommand{};
	if (FNaiwaP4EntityState* Player = FindPlayerEntity())
	{
		Player->MeleeInputBufferTicksRemaining = 0;
		Player->DashInputBufferTicksRemaining = 0;
		Player->UltimateInputBufferTicksRemaining = 0;
	}
}

void UNaiwaP4ArenaSubsystem::SetPlayerMoveAxis(const FVector2D Axis)
{
	if (bSimulationActive)
	{
		InputBuffer.SetMoveAxis(Axis);
	}
}

void UNaiwaP4ArenaSubsystem::SetPlayerMeleeHeld(const bool bHeld)
{
	if (bSimulationActive)
	{
		InputBuffer.SetMeleeHeld(bHeld);
	}
}

void UNaiwaP4ArenaSubsystem::QueuePlayerDashPressed()
{
	if (bSimulationActive)
	{
		InputBuffer.QueueDashPressed();
	}
}

void UNaiwaP4ArenaSubsystem::SetPlayerBlockHeld(const bool bHeld)
{
	if (bSimulationActive)
	{
		InputBuffer.SetBlockHeld(bHeld);
	}
}

void UNaiwaP4ArenaSubsystem::QueuePlayerUltimatePressed()
{
	if (bSimulationActive)
	{
		InputBuffer.QueueUltimatePressed();
	}
}

int64 UNaiwaP4ArenaSubsystem::GetPendingCatchUpSteps() const
{
	if (FixedStepSeconds <= 0.0 || FixedAccumulatorSeconds < FixedStepSeconds)
	{
		return 0;
	}

	const double PendingSteps = FMath::FloorToDouble(FixedAccumulatorSeconds / FixedStepSeconds);
	return PendingSteps >= static_cast<double>(MAX_int64)
		? MAX_int64
		: static_cast<int64>(PendingSteps);
}

FVector2D UNaiwaP4ArenaSubsystem::GetSmoothedPresentationPosition(
	const int32 EntityId, const FVector2D& CurrentPosition) const
{
	const FVector2D* PreviousPosition = PreviousPresentationPositions.Find(EntityId);
	if (!PreviousPosition || FixedStepSeconds <= 0.0)
	{
		return CurrentPosition;
	}

	const double Alpha = FMath::Clamp(FixedAccumulatorSeconds / FixedStepSeconds, 0.0, 1.0);
	const FVector2D PresentedPosition = *PreviousPosition + (CurrentPosition - *PreviousPosition) * Alpha;
	return FVector2D(
		FMath::Clamp(PresentedPosition.X, 0.0, LogicalArenaWidth),
		FMath::Clamp(PresentedPosition.Y, 0.0, LogicalArenaDepth));
}

bool UNaiwaP4ArenaSubsystem::LoadUpgradeDataTable(const UDataTable* DataTable, FString& OutError)
{
	return UpgradeCatalog.Load(DataTable, OutError);
}

bool UNaiwaP4ArenaSubsystem::LoadUpgradeCsvFile(const FString& UpgradeCsvPath, FString& OutError)
{
	TArray<TArray<FString>> Rows;
	if (!ReadUtf8CsvRows(UpgradeCsvPath, Rows, OutError))
	{
		return false;
	}
	const TArray<FString> ExpectedHeader = {
		TEXT("Name"), TEXT("Title"), TEXT("Description"), TEXT("TriggerRule"), TEXT("ComboRule"),
		TEXT("CategoryId"), TEXT("CategoryLabel"), TEXT("EffectKind"), TEXT("P0"), TEXT("P1"),
		TEXT("P2"), TEXT("bUnique"), TEXT("PrerequisiteCardIds"), TEXT("ExcludedCardIds"),
		TEXT("RedlineGroup"), TEXT("Rarity"), TEXT("IconSoftRef"), TEXT("CategoryMarkSoftRef")
	};
	if (!CsvHeaderMatches(Rows[0], ExpectedHeader))
	{
		OutError = TEXT("P4 upgrade CSV columns do not match the upgrade row schema.");
		return false;
	}

	UDataTable* UpgradeTable = NewObject<UDataTable>(GetTransientPackageAsObject());
	if (!UpgradeTable)
	{
		OutError = TEXT("Could not create transient P4 upgrade DataTable from CSV.");
		return false;
	}
	UpgradeTable->RowStruct = FNaiwaP4UpgradeRow::StaticStruct();
	for (int32 Index = 1; Index < Rows.Num(); ++Index)
	{
		const TArray<FString>& Cells = Rows[Index];
		if (Cells.Num() != ExpectedHeader.Num() || Cells[0].IsEmpty())
		{
			OutError = FString::Printf(TEXT("P4 upgrade CSV row %d has the wrong number of fields."), Index + 1);
			return false;
		}
		FNaiwaP4UpgradeRow Row;
		Row.Title = FText::FromString(Cells[1].TrimQuotes());
		Row.Description = FText::FromString(Cells[2].TrimQuotes());
		Row.TriggerRule = FText::FromString(Cells[3].TrimQuotes());
		Row.ComboRule = FText::FromString(Cells[4].TrimQuotes());
		Row.CategoryId = FName(*Cells[5]);
		Row.CategoryLabel = FText::FromString(Cells[6].TrimQuotes());
		Row.EffectKind = FName(*Cells[7]);
		Row.P0 = FCString::Atof(*Cells[8]);
		Row.P1 = FCString::Atof(*Cells[9]);
		Row.P2 = FCString::Atof(*Cells[10]);
		Row.bUnique = FCString::ToBool(*Cells[11]);
		Row.RedlineGroup = FName(*Cells[14]);
		Row.Rarity = FName(*Cells[15]);
		UpgradeTable->AddRow(FName(*Cells[0]), Row);
	}
	return LoadUpgradeDataTable(UpgradeTable, OutError);
}

TArray<FName> UNaiwaP4ArenaSubsystem::GetAvailableUpgradeCardIds() const
{
	TArray<FName> AvailableIds;
	for (const FName CardId : UpgradeCatalog.GetOrderedCardIds())
	{
		const FNaiwaP4UpgradeRow* Row = UpgradeCatalog.Find(CardId);
		if (!Row)
		{
			continue;
		}
		if (Row->PrerequisiteCardIds.ContainsByPredicate([this](const FName RequiredId)
			{ return !State.AcquiredUpgradeCardIds.Contains(RequiredId); }))
		{
			continue;
		}
		if (Row->ExcludedCardIds.ContainsByPredicate([this](const FName ExcludedId)
			{ return State.AcquiredUpgradeCardIds.Contains(ExcludedId); }))
		{
			continue;
		}
		AvailableIds.Add(CardId);
	}
	return AvailableIds;
}

int32 UNaiwaP4ArenaSubsystem::GetUpgradeCardRank(const FName CardId) const
{
	int32 Rank = 0;
	for (const FName AcquiredId : State.AcquiredUpgradeCardIds)
	{
		if (AcquiredId == CardId)
		{
			++Rank;
		}
	}
	return Rank;
}

void UNaiwaP4ArenaSubsystem::DrawUpgradeOffer(const bool bTryToChangePreviousOffer)
{
	const TArray<FName> AvailableIds = GetAvailableUpgradeCardIds();
	const TArray<FName> PreviousOffer = State.CurrentUpgradeOfferCardIds;
	State.CurrentUpgradeOfferCardIds.Reset();
	const int32 OfferSize = FMath::Min(3, AvailableIds.Num());
	auto HasSameCardSet = [](TArray<FName> Left, TArray<FName> Right)
	{
		const auto ByName = [](const FName A, const FName B) { return A.ToString() < B.ToString(); };
		Left.Sort(ByName);
		Right.Sort(ByName);
		return Left == Right;
	};
	if (OfferSize <= 0)
	{
		State.UpgradeRerollsRemaining = 0;
		++State.UpgradeOfferRevision;
		return;
	}

	const int32 Attempts = bTryToChangePreviousOffer && AvailableIds.Num() > OfferSize ? 12 : 1;
	for (int32 Attempt = 0; Attempt < Attempts; ++Attempt)
	{
		TArray<FName> Pool = AvailableIds;
		TArray<FName> Offer;
		while (Offer.Num() < OfferSize)
		{
			const int32 DrawIndex = static_cast<int32>(State.NextRandomUInt32() % static_cast<uint32>(Pool.Num()));
			Offer.Add(Pool[DrawIndex]);
			Pool.RemoveAt(DrawIndex, 1, EAllowShrinking::No);
		}
		State.CurrentUpgradeOfferCardIds = MoveTemp(Offer);
		if (!bTryToChangePreviousOffer || !HasSameCardSet(State.CurrentUpgradeOfferCardIds, PreviousOffer))
		{
			break;
		}
	}

	if (!bTryToChangePreviousOffer)
	{
		State.UpgradeRerollsRemaining = 1;
	}
	++State.UpgradeOfferRevision;
}

bool UNaiwaP4ArenaSubsystem::RerollUpgradeChoice(FString& OutError)
{
	if (State.PendingUpgradeChoices <= 0 || State.UpgradeRerollsRemaining <= 0)
	{
		OutError = TEXT("No reroll is available for the current level.");
		return false;
	}
	--State.UpgradeRerollsRemaining;
	DrawUpgradeOffer(true);
	OutError.Reset();
	return !State.CurrentUpgradeOfferCardIds.IsEmpty();
}

bool UNaiwaP4ArenaSubsystem::ConfirmUpgradeChoice(const FName CardId, FString& OutError)
{
	if (State.PendingUpgradeChoices <= 0)
	{
		OutError = TEXT("Defeat enemies to earn experience; an upgrade is available after leveling up.");
		return false;
	}
	if (!State.CurrentUpgradeOfferCardIds.Contains(CardId))
	{
		OutError = FString::Printf(TEXT("Upgrade '%s' is not in the current offer."), *CardId.ToString());
		return false;
	}
	FNaiwaP4EntityState* Player = FindPlayerEntity();
	const FNaiwaP4UpgradeRow* Row = UpgradeCatalog.Find(CardId);
	if (!Player || !Row)
	{
		OutError = TEXT("The player or selected upgrade row is unavailable.");
		return false;
	}

	const int32 NewRank = GetUpgradeCardRank(CardId) + 1;
	if (Row->EffectKind == TEXT("projectile_bounce"))
	{
		Player->ProjectileBouncesPerShot = NewRank * FMath::Max(1, FMath::RoundToInt(Row->P0));
		Player->ProjectileBounceDamageScale = FMath::Clamp(Row->P1 + 0.10f * (NewRank - 1), 0.1f, 0.9f);
	}
	else if (Row->EffectKind == TEXT("charge_capacity_recovery"))
	{
		const int32 BaseDashCapacity = FMath::Max(1, FMath::RoundToInt(Row->P0) - 1);
		Player->DashChargesMax = BaseDashCapacity + NewRank;
		Player->DashCharges = Player->DashChargesMax;
		Player->DashRechargeTicksRemaining = 0;
		Player->DashRecoveryDurationTicks = SecondsToTicks(FMath::Max(0.0f, Row->P1 + 0.10f * (NewRank - 1)));
	}
	else if (Row->EffectKind == TEXT("laugh_energy_on_hit"))
	{
		Player->bGainEnergyWhenHit = true;
		Player->EnergyOnHitHundredths = FMath::Max(0, FMath::RoundToInt(Row->P0 * NewRank * 100.0f));
	}
	else if (Row->EffectKind == TEXT("melee_damage"))
	{
		Player->MeleeDamageBonusPerRank = FMath::Max(0, FMath::RoundToInt(Row->P0));
		Player->MeleeDamage = 20 + Player->MeleeDamageBonusPerRank * NewRank;
	}
	else
	{
		OutError = FString::Printf(TEXT("Upgrade effect '%s' is not implemented yet."), *Row->EffectKind.ToString());
		return false;
	}

	State.AcquiredUpgradeCardIds.Add(CardId);
	--State.PendingUpgradeChoices;
	EmitCombatEvent(TEXT("upgrade_applied"), Player->EntityId, INDEX_NONE, Player->Position);
	if (State.PendingUpgradeChoices > 0)
	{
		DrawUpgradeOffer(false);
	}
	else
	{
		State.CurrentUpgradeOfferCardIds.Reset();
		State.UpgradeRerollsRemaining = 0;
		++State.UpgradeOfferRevision;
	}
	ClearPlayerInput();
	OutError.Reset();
	return true;
}

uint64 UNaiwaP4ArenaSubsystem::GetSimulationStateHash() const
{
	return FNaiwaP4Determinism::ComputeSimulationStateHashV7(State);
}

void UNaiwaP4ArenaSubsystem::AdvanceFixedStep()
{
	if (State.RunResult != ENaiwaP4RunResult::InProgress || State.PendingUpgradeChoices > 0)
	{
		return;
	}
	PreviousPresentationPositions.Reset();
	for (const FNaiwaP4EntityState& Entity : State.Entities)
	{
		PreviousPresentationPositions.Add(Entity.EntityId, Entity.Position);
	}

	LastConsumedPlayerCommand = InputBuffer.ConsumeForFixedStep();
	AdvancePlayerMovement(LastConsumedPlayerCommand);
	AdvancePlayerAutoFire();
	AdvanceEnemyBrains();
	AdvanceProjectiles();
	ResolvePendingDamage(LastConsumedPlayerCommand.bBlockHeld);
	TickCombatTimers();
	AdvanceWaveDirector();
	++State.CurrentTick;
}

void UNaiwaP4ArenaSubsystem::AdvancePlayerMovement(const FNaiwaP4InputCommand& Command)
{
	FNaiwaP4EntityState* Player = FindPlayerEntity();
	if (!Player || !Player->bAlive)
	{
		return;
	}
	const int32 InputBufferTicks = SecondsToTicks(PlayerActionInputBufferSeconds);
	if (Command.bMeleePressed)
	{
		Player->MeleeInputBufferTicksRemaining = InputBufferTicks;
	}
	else if (Player->MeleeInputBufferTicksRemaining > 0)
	{
		--Player->MeleeInputBufferTicksRemaining;
	}
	if (Command.bDashPressed)
	{
		Player->DashInputBufferTicksRemaining = InputBufferTicks;
	}
	else if (Player->DashInputBufferTicksRemaining > 0)
	{
		--Player->DashInputBufferTicksRemaining;
	}
	if (Command.bUltimatePressed)
	{
		Player->UltimateInputBufferTicksRemaining = InputBufferTicks;
	}
	else if (Player->UltimateInputBufferTicksRemaining > 0)
	{
		--Player->UltimateInputBufferTicksRemaining;
	}
	if (Player->MeleeHitTicksRemaining > 0)
	{
		--Player->MeleeHitTicksRemaining;
		if (Player->MeleeHitTicksRemaining == 0)
		{
			ResolvePlayerMeleeHit(*Player);
		}
	}

	Player->Velocity = FVector2D::ZeroVector;
	if (Player->DashInputBufferTicksRemaining > 0 && Player->DashCharges > 0
		&& Player->DashTicksRemaining <= 0 && Player->DashRecoveryTicksRemaining <= 0)
	{
		StartPlayerDash(*Player, Command.MoveAxis);
		Player->DashInputBufferTicksRemaining = 0;
	}

	if (Player->UltimateInputBufferTicksRemaining > 0 && Player->EnergyHundredths >= PlayerEnergyFullHundredths
		&& Player->DashTicksRemaining <= 0 && Player->DashRecoveryTicksRemaining <= 0)
	{
		UsePlayerUltimate(*Player);
		Player->UltimateInputBufferTicksRemaining = 0;
	}

	if (Player->DashTicksRemaining > 0)
	{
		const FVector2D StartPosition = Player->Position;
		const FVector2D Displacement = Player->DashDirection * 1120.0 * FixedStepSeconds;
		Player->Position.X = FMath::Clamp(Player->Position.X + Displacement.X, 0.0, LogicalArenaWidth);
		Player->Position.Y = FMath::Clamp(Player->Position.Y + Displacement.Y, 0.0, LogicalArenaDepth);
		Player->Velocity = (Player->Position - StartPosition) / FixedStepSeconds;
		ApplyDashDamage(*Player, StartPosition, Player->Position);
		--Player->DashTicksRemaining;
		if (Player->DashTicksRemaining == 0)
		{
			Player->DashRecoveryTicksRemaining = Player->DashRecoveryDurationTicks > 0
				? Player->DashRecoveryDurationTicks
				: SecondsToTicks(0.24);
		}
		Player->ActionCue = ENaiwaP4ActionCue::Dash;
		++Player->ActionTick;
		return;
	}

	if (Player->DashRecoveryTicksRemaining > 0)
	{
		Player->ActionCue = ENaiwaP4ActionCue::Dash;
		++Player->ActionTick;
		return;
	}

	const double SpeedScale = Command.bBlockHeld ? PlayerBlockMoveSpeedScale : 1.0;
	const FVector2D RequestedVelocity(
		Command.MoveAxis.X * PlayerWalkSpeed * SpeedScale,
		Command.MoveAxis.Y * PlayerWalkSpeed * PlayerDepthSpeedScale * SpeedScale);
	const FVector2D PreviousPosition = Player->Position;
	Player->Position.X = FMath::Clamp(
		Player->Position.X + RequestedVelocity.X * FixedStepSeconds, 0.0, LogicalArenaWidth);
	Player->Position.Y = FMath::Clamp(
		Player->Position.Y + RequestedVelocity.Y * FixedStepSeconds, 0.0, LogicalArenaDepth);
	Player->Velocity = (Player->Position - PreviousPosition) / FixedStepSeconds;

	if (!Command.MoveAxis.IsNearlyZero())
	{
		const double Horizontal = FMath::Abs(Command.MoveAxis.X);
		const double Vertical = FMath::Abs(Command.MoveAxis.Y);
		if (Horizontal >= Vertical)
		{
			Player->Facing = ENaiwaP4Facing::Side;
			Player->bFacingRight = Command.MoveAxis.X > 0.0;
		}
		else
		{
			Player->Facing = Command.MoveAxis.Y > 0.0
				? ENaiwaP4Facing::Back
				: ENaiwaP4Facing::Front;
		}
	}

	bool bMeleeStarted = false;
	if ((Player->MeleeInputBufferTicksRemaining > 0 || Command.bMeleeHeld)
		&& Player->AttackCooldownTicksRemaining <= 0)
	{
		Player->MeleeInputBufferTicksRemaining = 0;
		Player->AttackCooldownTicksRemaining = SecondsToTicks(PlayerMeleeCooldownSeconds);
		Player->MeleeHitTicksRemaining = SecondsToTicks(PlayerMeleeWindupSeconds);
		Player->MeleeContactTicksRemaining = 0;
		Player->AttackCueTicksRemaining = SecondsToTicks(PlayerMeleeCueSeconds);
		Player->ActionCue = ENaiwaP4ActionCue::Attack;
		Player->ActionTick = 0;
		EmitCombatEvent(TEXT("player_melee_started"), Player->EntityId, INDEX_NONE, Player->Position);
		bMeleeStarted = true;
	}

	if (Command.bBlockHeld)
	{
		Player->ActionCue = ENaiwaP4ActionCue::Defend;
	}
	else if (Player->HitCueTicksRemaining > 0)
	{
		Player->ActionCue = ENaiwaP4ActionCue::Hit;
	}
	else if (Player->LaughTicksRemaining > 0)
	{
		Player->ActionCue = ENaiwaP4ActionCue::Laugh;
	}
	else if (!bMeleeStarted && Player->AttackCueTicksRemaining <= 0)
	{
		Player->ActionCue = Command.MoveAxis.IsNearlyZero()
			? ENaiwaP4ActionCue::Idle
			: ENaiwaP4ActionCue::Move;
	}
	if (Player->ActionCue != ENaiwaP4ActionCue::Idle && Player->ActionTick < MAX_int32)
	{
		++Player->ActionTick;
	}
}

void UNaiwaP4ArenaSubsystem::AdvancePlayerAutoFire()
{
	FNaiwaP4EntityState* Player = FindPlayerEntity();
	if (!Player || !Player->bAlive || Player->ProjectileAutoFireCooldownTicksRemaining > 0)
	{
		return;
	}
	if (FNaiwaP4EntityState* Target = FindNearestEnemyTarget(Player->Position, PlayerProjectileRange))
	{
		Player->ProjectileAutoFireCooldownTicksRemaining = SecondsToTicks(PlayerProjectileIntervalSeconds);
		FirePlayerProjectile(Player->EntityId, Target->EntityId);
	}
}

void UNaiwaP4ArenaSubsystem::ResolvePlayerMeleeHit(FNaiwaP4EntityState& Player)
{
	Player.MeleeContactTicksRemaining = SecondsToTicks(0.08);
	FVector2D FacingDirection(0.0, -1.0);
	if (Player.Facing == ENaiwaP4Facing::Back)
	{
		FacingDirection = FVector2D(0.0, 1.0);
	}
	else if (Player.Facing == ENaiwaP4Facing::Side)
	{
		FacingDirection = FVector2D(Player.bFacingRight ? 1.0 : -1.0, 0.0);
	}

	int32 HitCount = 0;
	for (const FNaiwaP4EntityState& Target : State.Entities)
	{
		if (!IsDamageTarget(Target, true))
		{
			continue;
		}
		const FVector2D ToTarget = Target.Position - Player.Position;
		const double DistanceSquared = ToTarget.SizeSquared();
		const double MaxReach = PlayerMeleeRange + Target.CollisionRadius;
		if (DistanceSquared > FMath::Square(MaxReach))
		{
			continue;
		}
		const FVector2D DirectionToTarget = ToTarget.GetSafeNormal();
		if (!DirectionToTarget.IsNearlyZero()
			&& FVector2D::DotProduct(FacingDirection, DirectionToTarget) < 0.35)
		{
			continue;
		}
		QueueDamage(Player.EntityId, Target.EntityId, Player.MeleeDamage, Target.Position);
		EmitCombatEvent(TEXT("player_melee_hit"), Player.EntityId, Target.EntityId, Target.Position);
		++HitCount;
	}

	if (HitCount == 0)
	{
		EmitCombatEvent(TEXT("player_melee_miss"), Player.EntityId, INDEX_NONE, Player.Position);
	}
}

void UNaiwaP4ArenaSubsystem::AdvanceEnemyBrains()
{
	FNaiwaP4EntityState* Player = FindPlayerEntity();
	if (!Player || !Player->bAlive)
	{
		return;
	}

	for (FNaiwaP4EntityState& Enemy : State.Entities)
	{
		if ((!Enemy.bTrainingEnemy && !Enemy.bWaveEnemy && !Enemy.bWaveBoss) || !Enemy.bAlive)
		{
			continue;
		}
		if (Enemy.bWaveBoss)
		{
			if (Enemy.BossPhaseTransitionTicksRemaining > 0)
			{
				Enemy.Velocity = FVector2D::ZeroVector;
				Enemy.ActionCue = ENaiwaP4ActionCue::Attack;
				continue;
			}
			AdvanceBossBrain(Enemy, *Player);
			continue;
		}

	const FVector2D ToPlayer = Player->Position - Enemy.Position;
		const double DistanceSquared = ToPlayer.SizeSquared();
		const int32 AttackIntervalTicks = Enemy.EnemyAttackIntervalTicks > 0
			? Enemy.EnemyAttackIntervalTicks
			: SecondsToTicks(GrayboxEnemyAttackIntervalSeconds);
		const int32 TelegraphDurationTicks = Enemy.EnemyTelegraphDurationTicks > 0
			? Enemy.EnemyTelegraphDurationTicks
			: SecondsToTicks(GrayboxEnemyTelegraphSeconds);
		if (Enemy.EnemyTelegraphTicksRemaining > 0)
		{
			--Enemy.EnemyTelegraphTicksRemaining;
			Enemy.ActionCue = ENaiwaP4ActionCue::Attack;
			if (Enemy.EnemyTelegraphTicksRemaining == 0)
			{
				if (DistanceSquared <= FMath::Square(static_cast<double>(Enemy.EnemyAttackRange)))
				{
					QueueDamage(Enemy.EntityId, Player->EntityId, Enemy.EnemyDamage, Enemy.Position);
					EmitCombatEvent(Enemy.bWaveEnemy ? TEXT("wave_enemy_attack") : TEXT("training_enemy_attack"),
						Enemy.EntityId, Player->EntityId, Player->Position);
				}
				else
				{
					EmitCombatEvent(Enemy.bWaveEnemy ? TEXT("wave_enemy_attack_missed") : TEXT("training_enemy_attack_missed"),
						Enemy.EntityId, Player->EntityId, Enemy.Position);
				}
				Enemy.EnemyAttackCooldownTicksRemaining = AttackIntervalTicks;
			}
			continue;
		}

		if (DistanceSquared <= FMath::Square(static_cast<double>(Enemy.EnemyAttackRange)))
		{
			Enemy.Facing = ENaiwaP4Facing::Side;
			Enemy.bFacingRight = ToPlayer.X > 0.0;
			if (Enemy.EnemyAttackCooldownTicksRemaining <= 0)
			{
				Enemy.EnemyTelegraphTicksRemaining = TelegraphDurationTicks;
				Enemy.ActionCue = ENaiwaP4ActionCue::Attack;
				Enemy.ActionTick = 0;
				EmitCombatEvent(Enemy.bWaveEnemy ? TEXT("wave_enemy_telegraph") : TEXT("training_enemy_telegraph"),
					Enemy.EntityId, Player->EntityId, Player->Position);
			}
			else
			{
				Enemy.Velocity = FVector2D::ZeroVector;
				Enemy.ActionCue = ENaiwaP4ActionCue::Idle;
			}
			continue;
		}

		const FVector2D Direction = ToPlayer.GetSafeNormal();
		const FVector2D Velocity(
			Direction.X * Enemy.EnemyMoveSpeed,
			Direction.Y * Enemy.EnemyMoveSpeed * PlayerDepthSpeedScale);
		const FVector2D StartPosition = Enemy.Position;
		Enemy.Position.X = FMath::Clamp(Enemy.Position.X + Velocity.X * FixedStepSeconds, 0.0, LogicalArenaWidth);
		Enemy.Position.Y = FMath::Clamp(Enemy.Position.Y + Velocity.Y * FixedStepSeconds, 0.0, LogicalArenaDepth);
		Enemy.Velocity = (Enemy.Position - StartPosition) / FixedStepSeconds;
		Enemy.Facing = ENaiwaP4Facing::Side;
		Enemy.bFacingRight = ToPlayer.X > 0.0;
		Enemy.ActionCue = ENaiwaP4ActionCue::Move;
		++Enemy.ActionTick;
	}
}

void UNaiwaP4ArenaSubsystem::AdvanceBossBrain(FNaiwaP4EntityState& Boss,
	const FNaiwaP4EntityState& Player)
{
	const FVector2D ToPlayer = Player.Position - Boss.Position;
	const double DistanceSquared = ToPlayer.SizeSquared();
	if (Boss.BossChargeTicksRemaining > 0)
	{
		const FVector2D PreviousPosition = Boss.Position;
		const int32 DurationTicks = FMath::Max(1, Boss.BossChargeDurationTicks);
		const int32 ElapsedTicks = DurationTicks - Boss.BossChargeTicksRemaining + 1;
		const double Alpha = FMath::Clamp(static_cast<double>(ElapsedTicks) / DurationTicks, 0.0, 1.0);
		Boss.Position = FMath::Lerp(Boss.BossChargeStartPosition, Boss.BossChargeEndPosition, Alpha);
		Boss.Velocity = (Boss.Position - PreviousPosition) / FixedStepSeconds;
		Boss.ActionCue = ENaiwaP4ActionCue::Dash;
		if (Boss.ActionTick < MAX_int32)
		{
			++Boss.ActionTick;
		}
		if (!Boss.bBossChargeHitPlayer)
		{
			const double HitRadius = static_cast<double>(Boss.CollisionRadius) + (Boss.BossPhase >= 2 ? 34.0 : 20.0);
			if (DistanceSquaredToSegment(Player.Position, PreviousPosition, Boss.Position) <= FMath::Square(HitRadius))
			{
				Boss.bBossChargeHitPlayer = true;
				QueueDamage(Boss.EntityId, Player.EntityId,
					Boss.BossPhase >= 2 ? FMath::RoundToInt(NaiTongBossAttackDamage * 1.25f) : NaiTongBossAttackDamage,
					Boss.Position);
				EmitCombatEvent(TEXT("boss_charge_hit"), Boss.EntityId, Player.EntityId, Player.Position);
			}
		}
		--Boss.BossChargeTicksRemaining;
		if (Boss.BossChargeTicksRemaining == 0)
		{
			Boss.Velocity = FVector2D::ZeroVector;
			if (!Boss.bBossChargeHitPlayer)
			{
				EmitCombatEvent(TEXT("boss_charge_missed"), Boss.EntityId, Player.EntityId, Boss.Position);
			}
			Boss.EnemyAttackCooldownTicksRemaining = SecondsToTicks(
				Boss.BossPhase >= 2 ? NaiTongBossAttackCooldownSeconds * 0.78 : NaiTongBossAttackCooldownSeconds);
			Boss.BossAttack = ENaiwaP4BossAttack::LidSlam;
		}
		return;
	}
	if (Boss.EnemyTelegraphTicksRemaining > 0)
	{
		--Boss.EnemyTelegraphTicksRemaining;
		Boss.ActionCue = ENaiwaP4ActionCue::Attack;
		Boss.Velocity = FVector2D::ZeroVector;
		if (Boss.EnemyTelegraphTicksRemaining == 0)
		{
			if (Boss.BossAttack == ENaiwaP4BossAttack::Charge)
			{
				FVector2D Direction = (Boss.BossLockedTargetPosition - Boss.Position).GetSafeNormal();
				if (Direction.IsNearlyZero())
				{
					Direction = ToPlayer.GetSafeNormal();
				}
				const double ChargeDistance = Boss.BossPhase >= 2 ? 340.0 : 280.0;
				const FVector2D AttackEnd(
					FMath::Clamp(Boss.Position.X + Direction.X * ChargeDistance, 0.0, LogicalArenaWidth),
					FMath::Clamp(Boss.Position.Y + Direction.Y * ChargeDistance, 0.0, LogicalArenaDepth));
				Boss.BossChargeStartPosition = Boss.Position;
				Boss.BossChargeEndPosition = AttackEnd;
				Boss.BossChargeDurationTicks = SecondsToTicks(ChargeDistance / NaiTongBossChargeSpeed);
				Boss.BossChargeTicksRemaining = Boss.BossChargeDurationTicks;
				Boss.bBossChargeHitPlayer = false;
				Boss.Facing = ENaiwaP4Facing::Side;
				Boss.bFacingRight = Direction.X >= 0.0;
				Boss.ActionCue = ENaiwaP4ActionCue::Dash;
				Boss.ActionTick = 0;
				EmitCombatEvent(TEXT("boss_charge_started"), Boss.EntityId, Player.EntityId, Boss.Position);
				return;
			}
			else
			{
				const double SlamRadius = Boss.BossPhase >= 2 ? 248.0 : 205.0;
				if (DistanceSquared <= FMath::Square(SlamRadius))
				{
					QueueDamage(Boss.EntityId, Player.EntityId,
						Boss.BossPhase >= 2 ? FMath::RoundToInt(NaiTongBossAttackDamage * 1.25f) : NaiTongBossAttackDamage,
						Boss.Position);
					EmitCombatEvent(TEXT("boss_lid_slam_hit"), Boss.EntityId, Player.EntityId, Player.Position);
				}
				else
				{
					EmitCombatEvent(TEXT("boss_lid_slam_missed"), Boss.EntityId, Player.EntityId, Boss.Position);
				}
			}
			Boss.EnemyAttackCooldownTicksRemaining = SecondsToTicks(
				Boss.BossPhase >= 2 ? NaiTongBossAttackCooldownSeconds * 0.78 : NaiTongBossAttackCooldownSeconds);
			Boss.BossAttack = Boss.BossAttack == ENaiwaP4BossAttack::Charge
				? ENaiwaP4BossAttack::LidSlam : ENaiwaP4BossAttack::Charge;
		}
		return;
	}

	if (DistanceSquared <= FMath::Square(NaiTongBossAttackRange))
	{
		Boss.Velocity = FVector2D::ZeroVector;
		Boss.Facing = ENaiwaP4Facing::Side;
		Boss.bFacingRight = ToPlayer.X >= 0.0;
		if (Boss.EnemyAttackCooldownTicksRemaining <= 0)
		{
			Boss.BossLockedTargetPosition = Player.Position;
			Boss.EnemyTelegraphDurationTicks = SecondsToTicks(Boss.BossAttack == ENaiwaP4BossAttack::Charge
				? NaiTongBossChargeTelegraphSeconds : NaiTongBossSlamTelegraphSeconds);
			Boss.EnemyTelegraphTicksRemaining = Boss.EnemyTelegraphDurationTicks;
			Boss.ActionCue = ENaiwaP4ActionCue::Attack;
			Boss.ActionTick = 0;
			EmitCombatEvent(Boss.BossAttack == ENaiwaP4BossAttack::Charge
				? TEXT("boss_telegraph_charge") : TEXT("boss_telegraph_lid_slam"),
				Boss.EntityId, Player.EntityId, Player.Position);
		}
		else
		{
			Boss.ActionCue = ENaiwaP4ActionCue::Idle;
		}
		return;
	}

	const FVector2D Direction = ToPlayer.GetSafeNormal();
	const FVector2D Velocity(Direction.X * NaiTongBossMoveSpeed,
		Direction.Y * NaiTongBossMoveSpeed * PlayerDepthSpeedScale);
	const FVector2D StartPosition = Boss.Position;
	Boss.Position.X = FMath::Clamp(Boss.Position.X + Velocity.X * FixedStepSeconds, 0.0, LogicalArenaWidth);
	Boss.Position.Y = FMath::Clamp(Boss.Position.Y + Velocity.Y * FixedStepSeconds, 0.0, LogicalArenaDepth);
	Boss.Velocity = (Boss.Position - StartPosition) / FixedStepSeconds;
	Boss.Facing = ENaiwaP4Facing::Side;
	Boss.bFacingRight = ToPlayer.X >= 0.0;
	Boss.ActionCue = ENaiwaP4ActionCue::Move;
	++Boss.ActionTick;
}

void UNaiwaP4ArenaSubsystem::BeginWave(const int32 WaveIndex)
{
	State.CurrentWaveIndex = WaveIndex;
	State.WaveSpawnEntryIndex = 0;
	State.WaveSpawnedInEntry = 0;
	State.WaveSpawnTicksRemaining = 0;
	State.bWaveSpawningComplete = false;
	State.bWaveRunComplete = false;
	while (State.WaveSpawnEntryIndex < WaveSpawnPlan.Num()
		&& WaveSpawnPlan[State.WaveSpawnEntryIndex].WaveIndex < WaveIndex)
	{
		++State.WaveSpawnEntryIndex;
	}
	EmitCombatEvent(TEXT("wave_started"), INDEX_NONE, INDEX_NONE, FVector2D(WaveIndex, State.TotalWaveCount));
}

void UNaiwaP4ArenaSubsystem::SpawnWaveEnemy(const FNaiwaP4WaveSpawnEntry& Entry)
{
	const FNaiwaP4EnemyRow* EnemyData = EnemyCatalog.Find(Entry.EnemyTypeId);
	if (!EnemyData)
	{
		return;
	}

	const uint32 SideRandom = State.NextRandomUInt32();
	const uint32 DepthRandom = State.NextRandomUInt32();
	const double Side = (SideRandom & 1u) == 0u ? -1.0 : 1.0;
	const double Offset = 280.0 + static_cast<double>((SideRandom >> 1) % 210u);
	const double DepthFraction = static_cast<double>(DepthRandom % 10001u) / 10000.0;
	const FVector2D SpawnPosition(
		FMath::Clamp(LogicalArenaWidth * 0.5 + Side * Offset, 0.0, LogicalArenaWidth),
		DepthFraction * LogicalArenaDepth);

	FNaiwaP4EntityState& Enemy = State.Entities.AddDefaulted_GetRef();
	Enemy.EntityId = State.NextEntityId++;
	Enemy.Kind = ENaiwaP4EntityKind::Enemy;
	Enemy.Position = SpawnPosition;
	Enemy.EnemySpawnPosition = SpawnPosition;
	Enemy.HP = EnemyData->MaxHP;
	Enemy.MaxHP = EnemyData->MaxHP;
	Enemy.CollisionRadius = EnemyData->CollisionRadius;
	Enemy.EnemyMoveSpeed = EnemyData->MoveSpeed;
	Enemy.EnemyAttackRange = EnemyData->AttackRange;
	Enemy.EnemyDamage = EnemyData->Damage;
	Enemy.EnemyAttackCooldownTicksRemaining = 0;
	Enemy.EnemyTelegraphTicksRemaining = 0;
	Enemy.TrainingRespawnTicksRemaining = 0;
	Enemy.EnemyAttackIntervalTicks = SecondsToTicks(EnemyData->AttackIntervalSeconds);
	Enemy.EnemyTelegraphDurationTicks = SecondsToTicks(EnemyData->TelegraphSeconds);
	Enemy.EnemyTypeId = Entry.EnemyTypeId;
	Enemy.SpawnWaveIndex = Entry.WaveIndex;
	Enemy.bFrontShield75 = EnemyData->bFrontShield75;
	Enemy.bWaveEnemy = true;
	Enemy.bFacingRight = SpawnPosition.X < LogicalArenaWidth * 0.5;
	Enemy.Facing = ENaiwaP4Facing::Side;
	PreviousPresentationPositions.Add(Enemy.EntityId, Enemy.Position);
	EmitCombatEvent(TEXT("wave_enemy_spawned"), Enemy.EntityId, INDEX_NONE, SpawnPosition);
}

void UNaiwaP4ArenaSubsystem::SpawnWaveBoss()
{
	if (State.bBossSpawned)
	{
		return;
	}

	const FNaiwaP4EntityState* Player = FindPlayerEntity();
	const double Side = (State.NextRandomUInt32() & 1u) == 0u ? -1.0 : 1.0;
	const FVector2D SpawnPosition(
		FMath::Clamp(LogicalArenaWidth * 0.5 + Side * 330.0, 0.0, LogicalArenaWidth),
		LogicalArenaDepth * 0.5);
	FNaiwaP4EntityState& Boss = State.Entities.AddDefaulted_GetRef();
	Boss.EntityId = State.NextEntityId++;
	Boss.Kind = ENaiwaP4EntityKind::Boss;
	Boss.Position = SpawnPosition;
	Boss.EnemySpawnPosition = SpawnPosition;
	Boss.HP = NaiTongBossMaxHP;
	Boss.MaxHP = NaiTongBossMaxHP;
	Boss.CollisionRadius = NaiTongBossRadius;
	Boss.EnemyMoveSpeed = NaiTongBossMoveSpeed;
	Boss.EnemyAttackRange = NaiTongBossAttackRange;
	Boss.EnemyDamage = NaiTongBossAttackDamage;
	Boss.EnemyAttackCooldownTicksRemaining = SecondsToTicks(0.75);
	Boss.EnemyAttackIntervalTicks = SecondsToTicks(NaiTongBossAttackCooldownSeconds);
	Boss.EnemyTelegraphTicksRemaining = 0;
	Boss.EnemyTelegraphDurationTicks = SecondsToTicks(NaiTongBossChargeTelegraphSeconds);
	Boss.EnemyTypeId = TEXT("boss_nai_tong");
	Boss.SpawnWaveIndex = State.TotalWaveCount;
	Boss.bWaveBoss = true;
	Boss.BossAttack = ENaiwaP4BossAttack::Charge;
	Boss.BossPhase = 1;
	Boss.Facing = ENaiwaP4Facing::Side;
	Boss.bFacingRight = Player ? Player->Position.X > SpawnPosition.X : Side < 0.0;
	State.bBossSpawned = true;
	State.bBossDefeated = false;
	PreviousPresentationPositions.Add(Boss.EntityId, SpawnPosition);
	EmitCombatEvent(TEXT("boss_spawned"), Boss.EntityId, INDEX_NONE, SpawnPosition);
}

void UNaiwaP4ArenaSubsystem::AdvanceWaveDirector()
{
	if (State.TotalWaveCount <= 0 || State.CurrentWaveIndex <= 0 || State.bWaveRunComplete)
	{
		return;
	}
	const FNaiwaP4EntityState* Player = FindPlayerEntity();
	if (!Player || !Player->bAlive)
	{
		return;
	}

	if (!State.bWaveSpawningComplete)
	{
		if (State.WaveSpawnTicksRemaining > 0)
		{
			--State.WaveSpawnTicksRemaining;
			return;
		}
		while (State.WaveSpawnEntryIndex < WaveSpawnPlan.Num()
			&& WaveSpawnPlan[State.WaveSpawnEntryIndex].WaveIndex < State.CurrentWaveIndex)
		{
			++State.WaveSpawnEntryIndex;
		}
		if (State.WaveSpawnEntryIndex < WaveSpawnPlan.Num()
			&& WaveSpawnPlan[State.WaveSpawnEntryIndex].WaveIndex == State.CurrentWaveIndex)
		{
			const FNaiwaP4WaveSpawnEntry& Entry = WaveSpawnPlan[State.WaveSpawnEntryIndex];
			SpawnWaveEnemy(Entry);
			++State.WaveSpawnedInEntry;
			State.WaveSpawnTicksRemaining = Entry.SpawnIntervalTicks;
			if (State.WaveSpawnedInEntry >= Entry.Count)
			{
				++State.WaveSpawnEntryIndex;
				State.WaveSpawnedInEntry = 0;
			}
			if (State.WaveSpawnEntryIndex >= WaveSpawnPlan.Num()
				|| WaveSpawnPlan[State.WaveSpawnEntryIndex].WaveIndex != State.CurrentWaveIndex)
			{
				State.bWaveSpawningComplete = true;
			}
			return;
		}
		State.bWaveSpawningComplete = true;
	}

	const bool bHasLivingWaveEnemy = State.Entities.ContainsByPredicate([this](const FNaiwaP4EntityState& Entity)
	{
		return Entity.bWaveEnemy && Entity.SpawnWaveIndex == State.CurrentWaveIndex && Entity.bAlive;
	});
	if (bHasLivingWaveEnemy)
	{
		return;
	}

	if (State.CurrentWaveIndex >= State.TotalWaveCount)
	{
		if (!State.bBossSpawned)
		{
			EmitCombatEvent(TEXT("wave_cleared"), INDEX_NONE, INDEX_NONE,
				FVector2D(State.CurrentWaveIndex, State.TotalWaveCount));
			SpawnWaveBoss();
		}
		return;
	}
	EmitCombatEvent(TEXT("wave_cleared"), INDEX_NONE, INDEX_NONE,
		FVector2D(State.CurrentWaveIndex, State.TotalWaveCount));
	BeginWave(State.CurrentWaveIndex + 1);
}

void UNaiwaP4ArenaSubsystem::AdvanceProjectiles()
{
	for (FNaiwaP4EntityState& Projectile : State.Entities)
	{
		if (Projectile.Kind != ENaiwaP4EntityKind::Projectile || !Projectile.bAlive)
		{
			continue;
		}

		const FVector2D StartPosition = Projectile.Position;
		Projectile.Position += Projectile.Velocity * FixedStepSeconds;
		const FVector2D EndPosition = Projectile.Position;
		for (const FNaiwaP4EntityState& Target : State.Entities)
		{
			if (!IsDamageTarget(Target, !Projectile.bEnemyProjectile)
				|| Target.EntityId == Projectile.ProjectileOwnerEntityId
				|| Projectile.ProjectileHitEntityIds.Contains(Target.EntityId))
			{
				continue;
			}

			const double HitRadius = static_cast<double>(Target.CollisionRadius + Projectile.CollisionRadius);
			if (DistanceSquaredToSegment(Target.Position, StartPosition, EndPosition) <= FMath::Square(HitRadius))
			{
				QueueDamage(Projectile.ProjectileOwnerEntityId, Target.EntityId,
					Projectile.ProjectileDamage, EndPosition);
				EmitCombatEvent(TEXT("projectile_hit"), Projectile.ProjectileOwnerEntityId, Target.EntityId, EndPosition);
				Projectile.ProjectileHitEntityIds.Add(Target.EntityId);

				bool bBounced = false;
				if (!Projectile.bEnemyProjectile && Projectile.ProjectileBouncesRemaining > 0)
				{
					const FNaiwaP4EntityState* NextTarget = nullptr;
					double BestDistanceSquared = FMath::Square(690.0);
					for (const FNaiwaP4EntityState& Candidate : State.Entities)
					{
						if (!IsDamageTarget(Candidate, true)
							|| Candidate.EntityId == Projectile.ProjectileOwnerEntityId
							|| Projectile.ProjectileHitEntityIds.Contains(Candidate.EntityId))
						{
							continue;
						}
						const double DistanceSquared = FVector2D::DistSquared(EndPosition, Candidate.Position);
						if (DistanceSquared < BestDistanceSquared
							|| (FMath::IsNearlyEqual(DistanceSquared, BestDistanceSquared)
								&& (!NextTarget || Candidate.EntityId < NextTarget->EntityId)))
						{
							BestDistanceSquared = DistanceSquared;
							NextTarget = &Candidate;
						}
					}
					if (NextTarget)
					{
						const double ProjectileSpeed = Projectile.Velocity.Size();
						Projectile.Position = EndPosition;
						Projectile.Velocity = (NextTarget->Position - EndPosition).GetSafeNormal() * ProjectileSpeed;
						Projectile.ProjectileDamage = FMath::Max(1, FMath::RoundToInt(
							static_cast<float>(Projectile.ProjectileDamage) * Projectile.ProjectileBounceDamageScale));
						--Projectile.ProjectileBouncesRemaining;
						EmitCombatEvent(TEXT("projectile_bounced"), Projectile.ProjectileOwnerEntityId,
							NextTarget->EntityId, EndPosition);
						bBounced = true;
					}
				}
				if (!bBounced)
				{
					Projectile.bAlive = false;
				}
				break;
			}
		}

		--Projectile.ProjectileLifetimeTicksRemaining;
		if (Projectile.ProjectileLifetimeTicksRemaining <= 0)
		{
			Projectile.bAlive = false;
		}
	}
}

void UNaiwaP4ArenaSubsystem::ResolvePendingDamage(const bool bPlayerBlockHeld)
{
	State.PendingDamage.Sort([](const FNaiwaP4DamageEvent& Left, const FNaiwaP4DamageEvent& Right)
	{
		if (Left.TargetEntityId != Right.TargetEntityId)
		{
			return Left.TargetEntityId < Right.TargetEntityId;
		}
		if (Left.SourceEntityId != Right.SourceEntityId)
		{
			return Left.SourceEntityId < Right.SourceEntityId;
		}
		if (Left.ImpactPosition.X != Right.ImpactPosition.X)
		{
			return Left.ImpactPosition.X < Right.ImpactPosition.X;
		}
		if (Left.ImpactPosition.Y != Right.ImpactPosition.Y)
		{
			return Left.ImpactPosition.Y < Right.ImpactPosition.Y;
		}
		return Left.Damage < Right.Damage;
	});

	for (const FNaiwaP4DamageEvent& DamageEvent : State.PendingDamage)
	{
		FNaiwaP4EntityState* Target = State.Entities.FindByPredicate([&DamageEvent](const FNaiwaP4EntityState& Entity)
		{
			return Entity.EntityId == DamageEvent.TargetEntityId;
		});
		if (!Target || !Target->bAlive || DamageEvent.Damage <= 0)
		{
			continue;
		}
		if (Target->Kind == ENaiwaP4EntityKind::Player && Target->InvulnerabilityTicksRemaining > 0)
		{
			EmitCombatEvent(TEXT("damage_blocked_by_invulnerability"), DamageEvent.SourceEntityId,
				Target->EntityId, DamageEvent.ImpactPosition);
			continue;
		}
		if (Target->Kind == ENaiwaP4EntityKind::Boss && Target->BossPhaseTransitionTicksRemaining > 0)
		{
			EmitCombatEvent(TEXT("boss_phase_damage_ignored"), DamageEvent.SourceEntityId,
				Target->EntityId, DamageEvent.ImpactPosition);
			continue;
		}

		int32 AppliedDamage = DamageEvent.Damage;
		if (Target->Kind == ENaiwaP4EntityKind::Player && bPlayerBlockHeld)
		{
			AppliedDamage = FMath::Max(1, FMath::RoundToInt(static_cast<float>(AppliedDamage) * 0.70f));
		}
		if (Target->Kind == ENaiwaP4EntityKind::Enemy && Target->bFrontShield75)
		{
			const FNaiwaP4EntityState* Attacker = State.Entities.FindByPredicate([&DamageEvent](const FNaiwaP4EntityState& Entity)
			{
				return Entity.EntityId == DamageEvent.SourceEntityId;
			});
			const double AttackerOffsetX = Attacker ? Attacker->Position.X - Target->Position.X : 0.0;
			const bool bHitShieldFront = Attacker && Attacker->Kind == ENaiwaP4EntityKind::Player
				&& (Target->bFacingRight ? AttackerOffsetX >= 0.0 : AttackerOffsetX <= 0.0);
			if (bHitShieldFront)
			{
				AppliedDamage = FMath::Max(1, FMath::RoundToInt(static_cast<float>(AppliedDamage) * 0.25f));
			}
		}
		Target->HP = FMath::Max(0, Target->HP - AppliedDamage);
		Target->HitCueTicksRemaining = SecondsToTicks(0.12);
		Target->ActionCue = ENaiwaP4ActionCue::Hit;
		Target->ActionTick = 0;
		if (Target->Kind == ENaiwaP4EntityKind::Boss && Target->bWaveBoss
			&& Target->BossPhase == 1 && Target->HP <= FMath::CeilToInt(Target->MaxHP * 0.5f))
		{
			Target->BossPhase = 2;
			Target->BossPhaseTransitionTicksRemaining = SecondsToTicks(1.35);
			Target->EnemyTelegraphTicksRemaining = 0;
			Target->BossChargeTicksRemaining = 0;
			Target->BossChargeDurationTicks = 0;
			Target->bBossChargeHitPlayer = false;
			for (FNaiwaP4EntityState& Entity : State.Entities)
			{
				if (Entity.Kind == ENaiwaP4EntityKind::Projectile && Entity.bEnemyProjectile)
				{
					Entity.bAlive = false;
				}
			}
			EmitCombatEvent(TEXT("boss_phase_two"), Target->EntityId, INDEX_NONE, Target->Position);
		}

		if (Target->Kind == ENaiwaP4EntityKind::Player)
		{
			Target->InvulnerabilityTicksRemaining = FMath::Max(Target->InvulnerabilityTicksRemaining, SecondsToTicks(0.9));
			FNaiwaP4EntityState* Source = State.Entities.FindByPredicate([&DamageEvent](const FNaiwaP4EntityState& Entity)
			{
				return Entity.EntityId == DamageEvent.SourceEntityId;
			});
			const FVector2D KnockbackDirection = GetImpactKnockbackDirection(*Target, Source, DamageEvent.ImpactPosition);
			Target->Position.X = FMath::Clamp(Target->Position.X + KnockbackDirection.X * DamageKnockbackDistance, 0.0, LogicalArenaWidth);
			Target->Position.Y = FMath::Clamp(Target->Position.Y + KnockbackDirection.Y * DamageKnockbackDistance, 0.0, LogicalArenaDepth);
			if (Target->bGainEnergyWhenHit && Target->EnergyOnHitHundredths > 0)
			{
				Target->EnergyHundredths = FMath::Clamp(
					Target->EnergyHundredths + Target->EnergyOnHitHundredths, 0, PlayerEnergyFullHundredths);
			}
			EmitCombatEvent(TEXT("player_damaged"), DamageEvent.SourceEntityId, Target->EntityId, Target->Position);
		}
		else
		{
			FNaiwaP4EntityState* Source = State.Entities.FindByPredicate([&DamageEvent](const FNaiwaP4EntityState& Entity)
			{
				return Entity.EntityId == DamageEvent.SourceEntityId;
			});
			if (Source && Source->Kind == ENaiwaP4EntityKind::Player)
			{
				const int32 EnergyGain = AppliedDamage * (Target->Kind == ENaiwaP4EntityKind::Boss ? 15 : 8);
				Source->EnergyHundredths = FMath::Clamp(Source->EnergyHundredths + EnergyGain, 0, PlayerEnergyFullHundredths);
			}
			if (Target->HP == 0)
			{
				Target->bAlive = false;
				if (Target->bTrainingEnemy)
				{
					Target->TrainingRespawnTicksRemaining = SecondsToTicks(2.0);
				}
				if (Target->bWaveEnemy)
				{
					++State.DefeatedWaveEnemyCount;
					GrantKillExperience(DamageEvent.SourceEntityId, Target->EntityId);
				}
				if (Target->Kind == ENaiwaP4EntityKind::Boss && Target->bWaveBoss)
				{
					State.bBossDefeated = true;
					State.bWaveRunComplete = true;
					State.RunResult = ENaiwaP4RunResult::Victory;
					EmitCombatEvent(TEXT("boss_killed"), DamageEvent.SourceEntityId, Target->EntityId, Target->Position);
					EmitCombatEvent(TEXT("all_waves_cleared"), DamageEvent.SourceEntityId, Target->EntityId, Target->Position);
				}
				else
				{
					EmitCombatEvent(TEXT("enemy_killed"), DamageEvent.SourceEntityId, Target->EntityId, Target->Position);
				}
			}
			else
			{
				const FVector2D KnockbackDirection = GetImpactKnockbackDirection(*Target, Source, DamageEvent.ImpactPosition);
				Target->Position.X = FMath::Clamp(Target->Position.X + KnockbackDirection.X * DamageKnockbackDistance, 0.0, LogicalArenaWidth);
				Target->Position.Y = FMath::Clamp(Target->Position.Y + KnockbackDirection.Y * DamageKnockbackDistance, 0.0, LogicalArenaDepth);
			}
			EmitCombatEvent(TEXT("enemy_damaged"), DamageEvent.SourceEntityId, Target->EntityId, Target->Position);
		}

		if (Target->HP == 0 && Target->Kind == ENaiwaP4EntityKind::Player)
		{
			Target->bAlive = false;
			Target->ActionCue = ENaiwaP4ActionCue::Death;
			State.RunResult = ENaiwaP4RunResult::Defeat;
			EmitCombatEvent(TEXT("run_defeat"), DamageEvent.SourceEntityId, Target->EntityId, Target->Position);
			EmitCombatEvent(TEXT("player_died"), DamageEvent.SourceEntityId, Target->EntityId, Target->Position);
		}
	}
	State.PendingDamage.Reset();
}

void UNaiwaP4ArenaSubsystem::GrantKillExperience(const int32 SourceEntityId, const int32 KilledEntityId)
{
	FNaiwaP4EntityState* Player = FindPlayerEntity();
	if (!Player || !Player->bAlive || State.RunResult != ENaiwaP4RunResult::InProgress
		|| SourceEntityId != Player->EntityId)
	{
		return;
	}

	++State.PlayerExperience;
	while (State.PlayerExperience >= State.PlayerExperienceForNextLevel)
	{
		State.PlayerExperience -= State.PlayerExperienceForNextLevel;
		++State.PlayerLevel;
		const bool bFirstUnclaimedUpgrade = State.PendingUpgradeChoices == 0;
		++State.PendingUpgradeChoices;
		if (bFirstUnclaimedUpgrade)
		{
			DrawUpgradeOffer(false);
		}
		State.PlayerExperienceForNextLevel = 4 + FMath::Max(0, State.PlayerLevel - 1) * 2;
		EmitCombatEvent(TEXT("player_level_up"), Player->EntityId, KilledEntityId, Player->Position);
	}
	if (State.PendingUpgradeChoices > 0)
	{
		ClearPlayerInput();
	}
}

void UNaiwaP4ArenaSubsystem::TickCombatTimers()
{
	for (FNaiwaP4EntityState& Entity : State.Entities)
	{
		if (Entity.AttackCooldownTicksRemaining > 0) --Entity.AttackCooldownTicksRemaining;
		if (Entity.AttackCueTicksRemaining > 0) --Entity.AttackCueTicksRemaining;
		if (Entity.MeleeContactTicksRemaining > 0) --Entity.MeleeContactTicksRemaining;
		if (Entity.ProjectileAutoFireCooldownTicksRemaining > 0) --Entity.ProjectileAutoFireCooldownTicksRemaining;
		if (Entity.InvulnerabilityTicksRemaining > 0) --Entity.InvulnerabilityTicksRemaining;
		if (Entity.HitCueTicksRemaining > 0) --Entity.HitCueTicksRemaining;
		if (Entity.LaughTicksRemaining > 0) --Entity.LaughTicksRemaining;
		if (Entity.DashRecoveryTicksRemaining > 0) --Entity.DashRecoveryTicksRemaining;

		if (Entity.DashCharges < Entity.DashChargesMax)
		{
			if (Entity.DashRechargeTicksRemaining > 0)
			{
				--Entity.DashRechargeTicksRemaining;
			}
			if (Entity.DashRechargeTicksRemaining == 0)
			{
				++Entity.DashCharges;
				if (Entity.DashCharges < Entity.DashChargesMax)
				{
					Entity.DashRechargeTicksRemaining = SecondsToTicks(2.0);
				}
			}
		}

		if (Entity.EnemyAttackCooldownTicksRemaining > 0)
		{
			--Entity.EnemyAttackCooldownTicksRemaining;
		}
		if (Entity.BossPhaseTransitionTicksRemaining > 0)
		{
			--Entity.BossPhaseTransitionTicksRemaining;
		}
		if (!Entity.bAlive && Entity.bTrainingEnemy && Entity.TrainingRespawnTicksRemaining > 0)
		{
			--Entity.TrainingRespawnTicksRemaining;
			if (Entity.TrainingRespawnTicksRemaining == 0)
			{
				Entity.bAlive = true;
				Entity.HP = Entity.MaxHP;
				Entity.Position = Entity.EnemySpawnPosition;
				Entity.Velocity = FVector2D::ZeroVector;
				PreviousPresentationPositions.Add(Entity.EntityId, Entity.Position);
				Entity.EnemyAttackCooldownTicksRemaining = 0;
				EmitCombatEvent(TEXT("graybox_enemy_respawned"), Entity.EntityId, INDEX_NONE, Entity.Position);
			}
		}
	}

	for (FNaiwaP4EntityState& Entity : State.Entities)
	{
		if (Entity.ActionCue == ENaiwaP4ActionCue::Hit && Entity.HitCueTicksRemaining == 0 && Entity.bAlive)
		{
			Entity.ActionCue = ENaiwaP4ActionCue::Idle;
		}
		if (Entity.ActionCue == ENaiwaP4ActionCue::Laugh && Entity.LaughTicksRemaining == 0 && Entity.bAlive)
		{
			Entity.ActionCue = ENaiwaP4ActionCue::Idle;
		}
	}
}

void UNaiwaP4ArenaSubsystem::QueueDamage(const int32 SourceEntityId, const int32 TargetEntityId,
	const int32 Damage, const FVector2D ImpactPosition)
{
	if (Damage <= 0)
	{
		return;
	}
	FNaiwaP4DamageEvent& Event = State.PendingDamage.AddDefaulted_GetRef();
	Event.SourceEntityId = SourceEntityId;
	Event.TargetEntityId = TargetEntityId;
	Event.Damage = Damage;
	Event.ImpactPosition = ImpactPosition;
}

void UNaiwaP4ArenaSubsystem::EmitCombatEvent(const FName EventId, const int32 SourceEntityId,
	const int32 TargetEntityId, const FVector2D Position)
{
	FNaiwaP4Event& Event = State.Events.AddDefaulted_GetRef();
	Event.EventId = EventId;
	Event.SourceEntityId = SourceEntityId;
	Event.TargetEntityId = TargetEntityId;
	Event.Tick = State.CurrentTick;
	Event.Position = Position;
}

void UNaiwaP4ArenaSubsystem::FirePlayerProjectile(const int32 PlayerEntityId, const int32 TargetEntityId)
{
	const FNaiwaP4EntityState* Player = State.Entities.FindByPredicate([PlayerEntityId](const FNaiwaP4EntityState& Entity)
	{
		return Entity.EntityId == PlayerEntityId;
	});
	const FNaiwaP4EntityState* Target = State.Entities.FindByPredicate([TargetEntityId](const FNaiwaP4EntityState& Entity)
	{
		return Entity.EntityId == TargetEntityId;
	});
	if (!Player || !Target || !Player->bAlive || !Target->bAlive)
	{
		return;
	}

	FVector2D Direction = (Target->Position - Player->Position).GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = Player->bFacingRight ? FVector2D(1.0, 0.0) : FVector2D(-1.0, 0.0);
	}
	const double TargetDistance = (Target->Position - Player->Position).Size();
	const double DesiredMuzzleOffset = static_cast<double>(Player->CollisionRadius)
		+ PlayerProjectileRadius + 10.0;
	const double SafeMuzzleOffset = FMath::Min(DesiredMuzzleOffset,
		FMath::Max(0.0, TargetDistance - static_cast<double>(Target->CollisionRadius) - PlayerProjectileRadius));
	const FVector2D ProjectileOrigin = Player->Position + Direction * SafeMuzzleOffset;
	FNaiwaP4EntityState Projectile;
	Projectile.EntityId = State.NextEntityId++;
	Projectile.Kind = ENaiwaP4EntityKind::Projectile;
	Projectile.Position = ProjectileOrigin;
	Projectile.Velocity = Direction * PlayerProjectileSpeed;
	Projectile.HP = 1;
	Projectile.MaxHP = 1;
	Projectile.CollisionRadius = static_cast<float>(PlayerProjectileRadius);
	Projectile.ProjectileOwnerEntityId = PlayerEntityId;
	Projectile.ProjectileDamage = Player->PlayerProjectileDamage;
	Projectile.ProjectileLifetimeTicksRemaining = SecondsToTicks(PlayerProjectileRange / PlayerProjectileSpeed);
	Projectile.ProjectileBouncesRemaining = Player->ProjectileBouncesPerShot;
	Projectile.ProjectileBounceDamageScale = Player->ProjectileBounceDamageScale;
	State.Entities.Add(Projectile);
	EmitCombatEvent(TEXT("player_projectile_fired"), PlayerEntityId, TargetEntityId, ProjectileOrigin);
}

void UNaiwaP4ArenaSubsystem::StartPlayerDash(FNaiwaP4EntityState& Player, const FVector2D MoveAxis)
{
	if (Player.DashCharges <= 0 || Player.DashTicksRemaining > 0 || Player.DashRecoveryTicksRemaining > 0)
	{
		return;
	}
	FVector2D Direction = MoveAxis.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		if (Player.Facing == ENaiwaP4Facing::Back)
		{
			Direction = FVector2D(0.0, 1.0);
		}
		else if (Player.Facing == ENaiwaP4Facing::Front)
		{
			Direction = FVector2D(0.0, -1.0);
		}
		else
		{
			Direction = FVector2D(Player.bFacingRight ? 1.0 : -1.0, 0.0);
		}
	}

	--Player.DashCharges;
	if (Player.DashCharges < Player.DashChargesMax && Player.DashRechargeTicksRemaining <= 0)
	{
		Player.DashRechargeTicksRemaining = SecondsToTicks(2.0);
	}
	Player.DashDirection = Direction;
	Player.DashTicksRemaining = SecondsToTicks(0.24);
	Player.DashHitEntityIds.Reset();
	Player.InvulnerabilityTicksRemaining = FMath::Max(Player.InvulnerabilityTicksRemaining, SecondsToTicks(0.48));
	EmitCombatEvent(TEXT("player_dash_started"), Player.EntityId, INDEX_NONE, Player.Position);
}

void UNaiwaP4ArenaSubsystem::ApplyDashDamage(FNaiwaP4EntityState& Player,
	const FVector2D StartPosition, const FVector2D EndPosition)
{
	for (const FNaiwaP4EntityState& Enemy : State.Entities)
	{
		if ((Enemy.Kind != ENaiwaP4EntityKind::Enemy && Enemy.Kind != ENaiwaP4EntityKind::Boss)
			|| !Enemy.bAlive || Player.DashHitEntityIds.Contains(Enemy.EntityId))
		{
			continue;
		}
		const double HitRadius = static_cast<double>(Player.CollisionRadius + Enemy.CollisionRadius);
		if (DistanceSquaredToSegment(Enemy.Position, StartPosition, EndPosition) <= FMath::Square(HitRadius))
		{
			Player.DashHitEntityIds.Add(Enemy.EntityId);
			QueueDamage(Player.EntityId, Enemy.EntityId, 35, Player.Position);
			EmitCombatEvent(TEXT("player_dash_hit"), Player.EntityId, Enemy.EntityId, Enemy.Position);
		}
	}
}

void UNaiwaP4ArenaSubsystem::UsePlayerUltimate(FNaiwaP4EntityState& Player)
{
	if (Player.EnergyHundredths < PlayerEnergyFullHundredths)
	{
		return;
	}
	Player.EnergyHundredths = 0;
	Player.HP = FMath::Min(Player.MaxHP, Player.HP + FMath::RoundToInt(17.6f));
	Player.InvulnerabilityTicksRemaining = FMath::Max(Player.InvulnerabilityTicksRemaining, SecondsToTicks(1.2));
	Player.LaughTicksRemaining = SecondsToTicks(1.2);
	Player.ActionCue = ENaiwaP4ActionCue::Laugh;
	Player.ActionTick = 0;
	const FVector2D Origin = Player.Position;
	for (FNaiwaP4EntityState& Entity : State.Entities)
	{
		if ((Entity.Kind == ENaiwaP4EntityKind::Enemy || Entity.Kind == ENaiwaP4EntityKind::Boss)
			&& Entity.bAlive && FVector2D::DistSquared(Origin, Entity.Position) <= FMath::Square(400.0))
		{
			QueueDamage(Player.EntityId, Entity.EntityId, 65, Origin);
			EmitCombatEvent(TEXT("player_ultimate_hit"), Player.EntityId, Entity.EntityId, Entity.Position);
		}
		if (Entity.Kind == ENaiwaP4EntityKind::Projectile && Entity.bAlive && Entity.bEnemyProjectile)
		{
			Entity.bAlive = false;
			EmitCombatEvent(TEXT("player_ultimate_cleared_projectile"), Player.EntityId, Entity.EntityId, Entity.Position);
		}
	}
	EmitCombatEvent(TEXT("player_ultimate_used"), Player.EntityId, INDEX_NONE, Origin);
}

FNaiwaP4EntityState* UNaiwaP4ArenaSubsystem::FindPlayerEntity()
{
	return State.Entities.FindByPredicate([](const FNaiwaP4EntityState& Entity)
	{
		return Entity.Kind == ENaiwaP4EntityKind::Player;
	});
}

FNaiwaP4EntityState* UNaiwaP4ArenaSubsystem::FindNearestEnemyTarget(const FVector2D& Origin, const double MaxRange)
{
	FNaiwaP4EntityState* BestTarget = nullptr;
	double BestDistanceSquared = FMath::Square(MaxRange);
	for (FNaiwaP4EntityState& Entity : State.Entities)
	{
		if ((Entity.Kind != ENaiwaP4EntityKind::Enemy && Entity.Kind != ENaiwaP4EntityKind::Boss) || !Entity.bAlive)
		{
			continue;
		}
		const double DistanceSquared = FVector2D::DistSquared(Origin, Entity.Position);
		if (DistanceSquared < BestDistanceSquared
			|| (DistanceSquared == BestDistanceSquared && (!BestTarget || Entity.EntityId < BestTarget->EntityId)))
		{
			BestDistanceSquared = DistanceSquared;
			BestTarget = &Entity;
		}
	}
	return BestTarget;
}

int32 UNaiwaP4ArenaSubsystem::SecondsToTicks(const double Seconds) const
{
	if (Seconds <= 0.0 || FixedStepSeconds <= 0.0)
	{
		return 0;
	}
	return FMath::Max(1, FMath::CeilToInt(Seconds / FixedStepSeconds));
}
