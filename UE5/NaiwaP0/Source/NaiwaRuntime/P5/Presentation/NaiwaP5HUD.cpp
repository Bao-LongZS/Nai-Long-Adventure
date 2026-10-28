#include "NaiwaP5HUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "../Core/NaiwaP5RunSubsystem.h"
#include "../Core/NaiwaP5Simulation.h"
#include "../Core/NaiwaP5TrackData.h"
#include "../../NaiwaMenuProgress.h"
#include "../Data/NaiwaP5SkinCatalog.h"
#include "NaiwaP5A0ArtSampleUtils.h"
#include "Engine/GameInstance.h"

void ANaiwaP5HUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GetWorld())
	{
		return;
	}
	const UNaiwaP5RunSubsystem* RunSubsystem = GetWorld()->GetSubsystem<UNaiwaP5RunSubsystem>();
	if (!RunSubsystem)
	{
		return;
	}
	const FNaiwaP5RunState& State = RunSubsystem->GetState();
	const FNaiwaP5RouteFixture& RouteFixture = RunSubsystem->GetActivePrototypeFixture();
	const double RouteLengthMeters = static_cast<double>(RouteFixture.RouteLengthMm) / 1000.0;
	const FNaiwaP5SkinRow* SkinRow = FNaiwaP5SkinCatalog::GetRows().FindByPredicate(
		[&State](const FNaiwaP5SkinRow& Row) { return Row.SkinId == State.EquippedSkinId; });
	const UNaiwaProgressSubsystem* Progress = GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UNaiwaProgressSubsystem>() : nullptr;
	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	if (!Font)
	{
		return;
	}

	const FLinearColor MainText(0.94f, 0.97f, 1.0f, 1.0f);
	const FLinearColor WarningText = State.bAvalancheWarning ?
		FLinearColor(1.0f, 0.20f, 0.16f, 1.0f) : FLinearColor(0.35f, 0.90f, 1.0f, 1.0f);
	const bool bA0ArtSample = IsNaiwaP5A0ArtSampleWorld(GetWorld());
	const float X = 40.0f;
	float Y = 36.0f;
	const float Scale = 1.25f;
	if (bA0ArtSample)
	{
		const float PanelWidth = FMath::Min(Canvas->ClipX - 48.0f, 900.0f);
		const float PanelHeight = FMath::Min(Canvas->ClipY - 36.0f, 520.0f);
		DrawRect(FLinearColor(0.035f, 0.075f, 0.12f, 0.78f), 24.0f, 20.0f,
			PanelWidth, PanelHeight);
	}
	const FString RouteTitle = bA0ArtSample
		? TEXT("P5 A0 ART SAMPLE  |  SPACE JUMP / HOLD  |  A / D FLIP")
		: (State.bM2PrototypeRoute
			? TEXT("P5 M2 GRAYBOX 1800M  |  SPACE: JUMP (HOLD FOR HEIGHT)  |  A / D: ROTATE")
			: TEXT("P5 M1 GRAYBOX 100M  |  SPACE: JUMP (HOLD FOR HEIGHT)  |  A / D: ROTATE"));
	DrawText(RouteTitle,
		MainText, X, Y, Font, Scale);
	Y += 34.0f;
	DrawText(FString::Printf(TEXT("R: HOLD 0.6s TO RESTART SAME ROUTE  |  ESC: PAUSE")),
		MainText, X, Y, Font, 1.0f);
	Y += 38.0f;
	DrawText(FString::Printf(TEXT("DISTANCE  %5.1f / %5.1f m   SPEED %4.1f m/s"),
		State.DistanceMeters, RouteLengthMeters, State.PlayerSpeedMetersPerSecond),
		MainText, X, Y, Font, 1.2f);
	Y += 30.0f;
	DrawText(FString::Printf(TEXT("AVALANCHE GAP  %5.1f m"), State.AvalancheGapMeters),
		WarningText, X, Y, Font, 1.2f);
	Y += 30.0f;
	DrawText(FString::Printf(TEXT("SCORE  %d   TICKETS  %d   COINS  %d   COMBO  x%.1f"),
		State.Score.GetTotalScore(), State.Score.GetTicketsEarned(), State.CollectedCoinIds.Num(),
		State.ComboMultiplier), MainText, X, Y, Font, 1.1f);
	Y += 38.0f;
	const TCHAR* ActionStatus = State.bFallen ? TEXT("FALLEN - RECOVERING") :
		(State.bAirborne ? TEXT("AIRBORNE - A / D ROTATE, HOLD SPACE FOR HEIGHT") :
		(State.bRiding ? TEXT("RIDING KANGAROO - JUMP TO CLEAR OBSTACLES") :
		(State.bJumpCharging ? TEXT("AIRBORNE - HOLD SPACE FOR MORE HEIGHT") : TEXT("READY - PRESS SPACE TO JUMP"))));
	DrawText(ActionStatus, State.bFallen ? WarningText : MainText, X, Y, Font, 1.0f);
	Y += 28.0f;
	if (State.bAirborne)
	{
		const int32 CompletedAirFlips = FMath::Min(2,
			FMath::FloorToInt(FMath::Abs(State.RotationDegrees) / 360.0));
		DrawText(FString::Printf(TEXT("AIR ROTATION  %4.0f deg   %d / 2 FLIPS"),
			State.RotationDegrees, CompletedAirFlips), MainText, X, Y, Font, 1.0f);
		Y += 26.0f;
	}
	if (State.TrickBoostSeconds > 0.0)
	{
		DrawText(FString::Printf(TEXT("TRICK BOOST  TIER %d   %.1fs   SPEED CAP %.0f m/s"),
			State.TrickBoostLevel, State.TrickBoostSeconds,
			State.TrickBoostLevel >= 2 ? NaiwaP5Rules::TrickBoostTier2MaxSpeedMetersPerSecond
				: NaiwaP5Rules::TrickBoostTier1MaxSpeedMetersPerSecond),
			FLinearColor(1.0f, 0.82f, 0.25f, 1.0f), X, Y, Font, 1.0f);
		Y += 26.0f;
	}
	DrawText(FString::Printf(TEXT("SKIN  %s   |  WATER BOOST %.1fs   |  PUSHBACK CD %.1fs"),
		SkinRow ? *SkinRow->DisplayName : TEXT("Base"), State.LandingSpeedBoostSeconds,
		State.AvalanchePushbackCooldownSeconds), MainText, X, Y, Font, 0.95f);
	Y += 26.0f;
	if (State.bObstacleWarning)
	{
		DrawText(FString::Printf(TEXT("OBSTACLE AHEAD  %4.1f m  (event %d)"),
			State.ObstacleWarningDistanceMeters, State.ObstacleWarningEventId),
			WarningText, X, Y, Font, 1.0f);
		Y += 26.0f;
	}
	if (State.bRiding)
	{
		DrawText(FString::Printf(TEXT("RIDE  %4.1f / 30.0 s   POUCH %s   DELIVERY %s"),
			State.RideElapsedSeconds, State.bRidePouchAvailable ? TEXT("READY") : TEXT("EMPTY"),
			State.bRideDeliveryCanceled ? TEXT("CANCELLED") : (State.bRideDeliveryAwarded ? TEXT("BOOSTED") : TEXT("IN PROGRESS"))),
			MainText, X, Y, Font, 0.95f);
		Y += 26.0f;
	}
	DrawText(RunSubsystem->GetLastAbilityEventText(), MainText, X, Y, Font, 0.9f);
	Y += 25.0f;

	const float BarWidth = 520.0f;
	const float BarHeight = 18.0f;
	const float GapAlpha = FMath::Clamp(State.AvalancheGapMeters / NaiwaP5Rules::AvalancheGapMaxMeters, 0.0, 1.0);
	DrawText(TEXT("AVALANCHE WALL"), WarningText, X, Y - 15.0f, Font, 0.78f);
	DrawText(TEXT("PLAYER"), MainText, X + BarWidth - 48.0f, Y - 15.0f, Font, 0.78f);
	DrawRect(FLinearColor(0.12f, 0.16f, 0.20f, 0.9f), X, Y, BarWidth, BarHeight);
	const float AvalancheFrontX = X + BarWidth * (1.0f - GapAlpha);
	DrawRect(State.bAvalancheWarning ? FLinearColor(0.95f, 0.13f, 0.12f, 1.0f) :
		FLinearColor(0.28f, 0.78f, 0.90f, 1.0f), AvalancheFrontX, Y,
		BarWidth * GapAlpha, BarHeight);
	DrawRect(FLinearColor(0.92f, 0.98f, 1.0f, 1.0f), AvalancheFrontX - 8.0f, Y - 5.0f, 16.0f, BarHeight + 10.0f);
	DrawRect(FLinearColor(0.25f, 0.92f, 1.0f, 1.0f), X + BarWidth - 6.0f, Y - 5.0f, 12.0f, BarHeight + 10.0f);
	Y += 34.0f;

	if (State.Phase == ENaiwaP5RunPhase::Paused)
	{
		DrawText(TEXT("PAUSED"), FLinearColor::White, X, Y, Font, 1.4f);
	}
	else if (State.Phase == ENaiwaP5RunPhase::Finished)
	{
		DrawText(TEXT("FINISH  |  HOLD R TO RETRY"), FLinearColor(0.40f, 1.0f, 0.58f, 1.0f), X, Y, Font, 1.4f);
	}
	else if (State.Phase == ENaiwaP5RunPhase::Swallowed)
	{
		DrawText(TEXT("AVALANCHE CAUGHT YOU  |  HOLD R TO RETRY"), FLinearColor(1.0f, 0.24f, 0.18f, 1.0f), X, Y, Font, 1.35f);
	}

	if (State.Phase == ENaiwaP5RunPhase::Finished || State.Phase == ENaiwaP5RunPhase::Swallowed)
	{
		Y += 34.0f;
		DrawText(FString::Printf(TEXT("SETTLEMENT  +%d TICKETS   |   WALLET %d"),
			State.Score.GetTicketsEarned(), Progress ? Progress->GetMilkTickets() : 0),
			MainText, X, Y, Font, 1.15f);
		Y += 30.0f;
		if (Progress && Progress->GetP5BestRecord().bHasRun)
		{
			const FNaiwaP5SaveRecord& Best = Progress->GetP5BestRecord();
			const FString BestTime = Best.bHasFinishedRun
				? FString::Printf(TEXT("%4.1f s"), Best.BestTimeSeconds) : TEXT("N/A");
			DrawText(FString::Printf(TEXT("BEST  %4.1f m   SCORE %d   TIME %s   FALLS %d"),
				static_cast<double>(Best.BestDistanceMm) / 1000.0, Best.BestScore,
				*BestTime, Best.BestFalls), MainText, X, Y, Font, 1.0f);
		}
	}
}
