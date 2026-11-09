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
	if (bA0ArtSample)
	{
		// Keep the scene open: only distance/score at upper left, avalanche at upper right,
		// and a compact control hint at the bottom. Detailed state stays in the log.
		const float UiScale = FMath::Clamp(Canvas->ClipX / 1920.0f, 0.80f, 1.0f);
		const float Margin = 24.0f * UiScale;
		const FLinearColor PanelFill(0.025f, 0.060f, 0.11f, 0.78f);
		const FLinearColor PanelEdge(0.60f, 0.79f, 0.91f, 0.24f);
		const float LeftWidth = 410.0f * UiScale;
		const float PanelHeight = 132.0f * UiScale;
		const float RightWidth = 330.0f * UiScale;
		const float LeftX = Margin;
		const float TopY = Margin;
		const float RightX = Canvas->ClipX - Margin - RightWidth;
		const float RightY = Margin;
		const float Pad = 16.0f * UiScale;

		DrawRect(PanelFill, LeftX, TopY, LeftWidth, PanelHeight);
		DrawRect(PanelEdge, LeftX, TopY, LeftWidth, 2.0f * UiScale);
		DrawText(TEXT("P5  /  DOWNHILL"), MainText, LeftX + Pad, TopY + 10.0f * UiScale,
			Font, 0.82f * UiScale);
		DrawText(FString::Printf(TEXT("%.0f  /  %.0f m"), State.DistanceMeters, RouteLengthMeters),
			MainText, LeftX + Pad, TopY + 34.0f * UiScale, Font, 1.48f * UiScale);
		DrawText(FString::Printf(TEXT("SCORE %d     COINS %d     COMBO x%.1f"),
			State.Score.GetTotalScore(), State.CollectedCoinIds.Num(), State.ComboMultiplier),
			MainText, LeftX + Pad, TopY + 78.0f * UiScale, Font, 0.78f * UiScale);

		FString ActionStatus = TEXT("SPACE  JUMP");
		FLinearColor ActionColor = MainText;
		if (State.bFallen)
		{
			ActionStatus = TEXT("FALLEN  /  PRESS SPACE TO QUEUE JUMP");
			ActionColor = WarningText;
		}
		else if (State.bRiding)
		{
			ActionStatus = TEXT("KANGAROO RIDE  /  JUMP TO CLEAR");
		}
		else if (State.bAirborne)
		{
			const int32 Flips = FMath::Min(2, FMath::FloorToInt(FMath::Abs(State.RotationDegrees) / 360.0));
			ActionStatus = FString::Printf(TEXT("AIR  %d DEG  /  %d OF 2 FLIPS"),
				FMath::RoundToInt(State.RotationDegrees), Flips);
		}
		DrawText(ActionStatus, ActionColor, LeftX + Pad, TopY + 103.0f * UiScale,
			Font, 0.76f * UiScale);

		DrawRect(PanelFill, RightX, RightY, RightWidth, PanelHeight);
		DrawRect(PanelEdge, RightX, RightY, RightWidth, 2.0f * UiScale);
		DrawText(TEXT("AVALANCHE GAP"), WarningText, RightX + Pad, RightY + 10.0f * UiScale,
			Font, 0.82f * UiScale);
		DrawText(FString::Printf(TEXT("%.1f m"), State.AvalancheGapMeters),
			WarningText, RightX + Pad, RightY + 35.0f * UiScale, Font, 1.48f * UiScale);
		const float GapBarX = RightX + Pad;
		const float GapBarY = RightY + 89.0f * UiScale;
		const float GapBarWidth = RightWidth - 2.0f * Pad;
		const float GapBarHeight = 10.0f * UiScale;
		const float GapAlpha = FMath::Clamp(State.AvalancheGapMeters /
			NaiwaP5Rules::AvalancheGapMaxMeters, 0.0, 1.0);
		DrawRect(FLinearColor(0.12f, 0.16f, 0.20f, 0.95f), GapBarX, GapBarY, GapBarWidth, GapBarHeight);
		DrawRect(State.bAvalancheWarning ? FLinearColor(0.95f, 0.13f, 0.12f, 1.0f) :
			FLinearColor(0.30f, 0.78f, 0.90f, 1.0f), GapBarX, GapBarY,
			GapBarWidth * GapAlpha, GapBarHeight);
		DrawText(TEXT("WALL"), WarningText, GapBarX, GapBarY + 13.0f * UiScale, Font, 0.62f * UiScale);
		DrawText(TEXT("YOU"), MainText, GapBarX + GapBarWidth - 32.0f * UiScale,
			GapBarY + 13.0f * UiScale, Font, 0.62f * UiScale);

		const float HintHeight = 30.0f * UiScale;
		const float HintY = Canvas->ClipY - Margin - HintHeight;
		DrawRect(PanelFill, Margin, HintY, 390.0f * UiScale, HintHeight);
		DrawText(TEXT("SPACE  JUMP  /  A D  FLIP  /  R  RESTART"), MainText,
			Margin + 12.0f * UiScale, HintY + 7.0f * UiScale, Font, 0.72f * UiScale);

		if (State.Phase == ENaiwaP5RunPhase::Finished || State.Phase == ENaiwaP5RunPhase::Swallowed)
		{
			const float CardWidth = 440.0f * UiScale;
			const float CardHeight = 104.0f * UiScale;
			const float CardX = (Canvas->ClipX - CardWidth) * 0.5f;
			const float CardY = (Canvas->ClipY - CardHeight) * 0.5f;
			DrawRect(PanelFill, CardX, CardY, CardWidth, CardHeight);
			DrawText(State.Phase == ENaiwaP5RunPhase::Finished ? TEXT("FINISH") : TEXT("AVALANCHE CAUGHT"),
				State.Phase == ENaiwaP5RunPhase::Finished ? FLinearColor(0.40f, 1.0f, 0.58f, 1.0f) : WarningText,
				CardX + 18.0f * UiScale, CardY + 16.0f * UiScale, Font, 1.1f * UiScale);
			DrawText(FString::Printf(TEXT("+%d TICKETS   /   HOLD R TO RETRY"),
				State.Score.GetTicketsEarned()), MainText, CardX + 18.0f * UiScale,
				CardY + 58.0f * UiScale, Font, 0.9f * UiScale);
		}
		return;
	}
	const float X = 40.0f;
	float Y = 36.0f;
	const float Scale = 1.25f;
	const FString RouteTitle = State.bM2PrototypeRoute
			? TEXT("P5 M2 GRAYBOX 1800M  |  SPACE: JUMP  |  A / D: ROTATE")
			: TEXT("P5 M1 GRAYBOX 100M  |  SPACE: JUMP  |  A / D: ROTATE");
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
		(State.bAirborne ? TEXT("AIRBORNE - A / D ROTATE") :
		(State.bRiding ? TEXT("RIDING KANGAROO - JUMP TO CLEAR OBSTACLES") :
		TEXT("READY - PRESS SPACE TO JUMP")));
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
