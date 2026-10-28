#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "../Core/NaiwaP4ArenaState.h"
#include "../Core/NaiwaP4Determinism.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNaiwaP4DeterminismInvariantTest,
	"Naiwa.P4.Determinism.StateHashInvariants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNaiwaP4DeterminismInvariantTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FNaiwaP4ArenaState DefaultState;
	TestEqual(
		TEXT("Default state starts outside xorshift32's locked zero state"),
		DefaultState.RngState,
		FNaiwaP4ArenaState::ZeroSeedFallback);

	FNaiwaP4ArenaState ExplicitZeroSeedState;
	ExplicitZeroSeedState.Reset(0u);
	TestEqual(
		TEXT("Default and explicit zero-seed states produce the same first random value"),
		DefaultState.NextRandomUInt32(),
		ExplicitZeroSeedState.NextRandomUInt32());

	FNaiwaP4ArenaState OrderedState;
	OrderedState.Reset(0x12345678u);
	FNaiwaP4EntityState Player;
	Player.EntityId = 1;
	Player.Kind = ENaiwaP4EntityKind::Player;
	Player.Position = FVector2D(12.5, -8.0);
	FNaiwaP4EntityState Enemy;
	Enemy.EntityId = 2;
	Enemy.Kind = ENaiwaP4EntityKind::Enemy;
	Enemy.Position = FVector2D(-31.0, 4.25);
	OrderedState.Entities.Add(Player);
	OrderedState.Entities.Add(Enemy);

	FNaiwaP4ArenaState ReorderedState = OrderedState;
	ReorderedState.Entities.Swap(0, 1);
	const uint64 OrderedHash = FNaiwaP4Determinism::ComputeSimulationStateHashV1(OrderedState);
	TestEqual(
		TEXT("Entity storage order does not change the deterministic state hash"),
		OrderedHash,
		FNaiwaP4Determinism::ComputeSimulationStateHashV1(ReorderedState));

	ReorderedState.Entities[0].ActionCue = ENaiwaP4ActionCue::Laugh;
	TestEqual(
		TEXT("Presentation-only ActionCue does not change the simulation hash"),
		OrderedHash,
		FNaiwaP4Determinism::ComputeSimulationStateHashV1(ReorderedState));

	ReorderedState.Entities[0].Position.X += 1.0;
	TestNotEqual(
		TEXT("Authoritative entity position changes the simulation hash"),
		OrderedHash,
		FNaiwaP4Determinism::ComputeSimulationStateHashV1(ReorderedState));

	TestEqual(
		TEXT("State hash format carries the version prefix"),
		FNaiwaP4Determinism::FormatHash(OrderedHash).Left(5),
		FString(TEXT("p4s1:")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
