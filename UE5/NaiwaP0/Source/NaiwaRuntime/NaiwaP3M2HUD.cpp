#include "NaiwaP3M2HUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "NaiwaP3BombTypes.h"
#include "NaiwaP3M2GameMode.h"
#include "NaiwaP3M2RuntimeSubsystem.h"

namespace
{
	const FLinearColor PanelFill(0.025f, 0.037f, 0.050f, 0.95f);
	const FLinearColor PanelSoft(0.055f, 0.072f, 0.086f, 0.94f);
	const FLinearColor TextPrimary = FLinearColor::FromSRGBColor(FColor(235, 239, 241));
const FLinearColor TextSecondary = FLinearColor::FromSRGBColor(FColor(199, 211, 218));
	const FLinearColor Teal = FLinearColor::FromSRGBColor(FColor(104, 196, 177));
	const FLinearColor Gold = FLinearColor::FromSRGBColor(FColor(244, 190, 89));
	const FLinearColor Cyan = FLinearColor::FromSRGBColor(FColor(87, 221, 225));
	const FLinearColor Violet = FLinearColor::FromSRGBColor(FColor(189, 148, 222));
	const FLinearColor Red = FLinearColor::FromSRGBColor(FColor(231, 105, 91));

	void DrawCircleOutline(UCanvas* Canvas, const FVector2D& Center, const float Radius,
		const FLinearColor& Color, const float Thickness)
	{
		if (!Canvas) return;
		constexpr int32 SegmentCount = 12;
		for (int32 Index = 0; Index < SegmentCount; ++Index)
		{
			const float A0 = 2.0f * PI * static_cast<float>(Index) / SegmentCount;
			const float A1 = 2.0f * PI * static_cast<float>(Index + 1) / SegmentCount;
			Canvas->K2_DrawLine(Center + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * Radius,
				Center + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * Radius, Thickness, Color);
		}
	}

	void DrawBombGlyph(UCanvas* Canvas, const ENaiwaP3BombType Type, const FVector2D& Center,
		const float Radius, const FLinearColor& Accent)
	{
		if (!Canvas) return;
		DrawCircleOutline(Canvas, Center, Radius, Accent, FMath::Max(1.5f, Radius * 0.12f));
		Canvas->K2_DrawLine(Center + FVector2D(0.0f, -Radius), Center + FVector2D(-Radius * 0.12f, -Radius * 1.42f),
			FMath::Max(1.5f, Radius * 0.12f), Accent);
		switch (Type)
		{
		case ENaiwaP3BombType::SoyBigCross:
			Canvas->K2_DrawLine(Center + FVector2D(-Radius * 0.72f, 0.0f), Center + FVector2D(Radius * 0.72f, 0.0f),
				FMath::Max(2.0f, Radius * 0.22f), Accent);
			Canvas->K2_DrawLine(Center + FVector2D(0.0f, -Radius * 0.72f), Center + FVector2D(0.0f, Radius * 0.72f),
				FMath::Max(2.0f, Radius * 0.22f), Accent);
			break;
		case ENaiwaP3BombType::TeaRadial:
			DrawCircleOutline(Canvas, Center, Radius * 0.42f, Accent, FMath::Max(1.5f, Radius * 0.12f));
			for (int32 Index = 0; Index < 8; ++Index)
			{
				const float A = 2.0f * PI * Index / 8.0f;
				Canvas->K2_DrawLine(Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Radius * 0.56f,
					Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Radius * 0.82f,
					FMath::Max(1.5f, Radius * 0.10f), Accent);
			}
			break;
		case ENaiwaP3BombType::RatSplit:
			Canvas->K2_DrawLine(Center, Center + FVector2D(-Radius * 0.55f, Radius * 0.50f),
				FMath::Max(2.0f, Radius * 0.16f), Accent);
			Canvas->K2_DrawLine(Center, Center + FVector2D(Radius * 0.55f, Radius * 0.50f),
				FMath::Max(2.0f, Radius * 0.16f), Accent);
			Canvas->K2_DrawLine(Center, Center + FVector2D(0.0f, -Radius * 0.55f),
				FMath::Max(2.0f, Radius * 0.16f), Accent);
			break;
		case ENaiwaP3BombType::Standard:
		default:
			Canvas->K2_DrawLine(Center + FVector2D(-Radius * 0.48f, 0.0f), Center + FVector2D(Radius * 0.48f, 0.0f),
				FMath::Max(2.0f, Radius * 0.18f), Accent);
			Canvas->K2_DrawLine(Center + FVector2D(0.0f, -Radius * 0.48f), Center + FVector2D(0.0f, Radius * 0.48f),
				FMath::Max(2.0f, Radius * 0.18f), Accent);
			break;
		}
	}

}

void ANaiwaP3M2HUD::DrawBombLoadout(const float X, const float Y, const float Width,
	const float Scale, const ENaiwaP3BombType Selected, const bool bTeachingLevel)
{
	static const ENaiwaP3BombType Types[] = { ENaiwaP3BombType::Standard,
		ENaiwaP3BombType::SoyBigCross, ENaiwaP3BombType::TeaRadial, ENaiwaP3BombType::RatSplit };
	static const TCHAR* Labels[] = { TEXT("标准"), TEXT("大十字"), TEXT("奶茶"), TEXT("分裂") };
	const int32 CardCount = bTeachingLevel ? 1 : UE_ARRAY_COUNT(Types);
	const float Gap = bTeachingLevel ? 0.0f : 5.0f * Scale;
	const float CardWidth = (Width - (CardCount - 1) * Gap) / CardCount;
	const float CardHeight = 48.0f * Scale;
	static const TCHAR* IconNames[] = {
		TEXT("SPR_Bomb_Standard_A"), TEXT("SPR_Bomb_SoyBigCross_A"),
		TEXT("SPR_Bomb_TeaRadial_A"), TEXT("SPR_Bomb_RatSplit_A")
	};
	if (BombIcons.Num() != UE_ARRAY_COUNT(IconNames))
	{
		BombIcons.SetNum(UE_ARRAY_COUNT(IconNames));
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(IconNames); ++Index)
		{
			const FString AssetPath = FString::Printf(TEXT("/Game/Naiwa/P3/Sprites/%s.%s"),
				IconNames[Index], IconNames[Index]);
			BombIcons[Index] = LoadObject<UTexture2D>(nullptr, *AssetPath);
		}
	}
	for (int32 Index = 0; Index < CardCount; ++Index)
	{
		const float CardX = X + Index * (CardWidth + Gap);
		const bool bSelected = Types[Index] == Selected;
		const FLinearColor Accent = bSelected ? Gold : TextSecondary;
		DrawRect(bSelected ? FLinearColor(0.12f, 0.13f, 0.15f, 1.0f) : PanelSoft,
			CardX, Y, CardWidth, CardHeight);
		DrawRect(Accent, CardX, Y, CardWidth, 2.0f * Scale);
		DrawRect(Accent, CardX, Y + CardHeight - 2.0f * Scale, CardWidth, 2.0f * Scale);
		DrawRect(Accent, CardX, Y, 2.0f * Scale, CardHeight);
		DrawRect(Accent, CardX + CardWidth - 2.0f * Scale, Y, 2.0f * Scale, CardHeight);
		if (Canvas && BombIcons.IsValidIndex(Index) && BombIcons[Index])
		{
			Canvas->K2_DrawTexture(BombIcons[Index], FVector2D(CardX + 2.0f * Scale, Y + 2.0f * Scale),
				FVector2D(33.0f * Scale, 44.0f * Scale), FVector2D::ZeroVector, FVector2D(1.0f, 1.0f),
				FLinearColor::White, BLEND_Translucent, 0.0f, FVector2D::ZeroVector);
		}
		if (Canvas && GEngine)
		{
			const float FontScale = FMath::Max(0.53f, 0.62f * Scale);
			Canvas->K2_DrawText(GEngine->GetMediumFont(), FString::Printf(TEXT("%d %s"), Index + 1, Labels[Index]),
				FVector2D(CardX + 35.0f * Scale, Y + 17.0f * Scale), FVector2D(FontScale), Accent);
		}
	}
}

void ANaiwaP3M2HUD::DrawMinimap(const FNaiwaP3M2LevelSession& Session,
	const UNaiwaP3M2RuntimeSubsystem& Runtime, const float X, const float Y,
	const float MaxWidth, const float MaxHeight, const float Scale)
{
	const FNaiwaP3M2SeededLevel& Level = Session.GetLevel();
	const FNaiwaP3FogOfWar& Fog = Session.GetFog();
	const float CellSize = FMath::Min(MaxWidth / Level.GridWidth, MaxHeight / Level.GridHeight);
	const float BoardWidth = CellSize * Level.GridWidth;
	const float BoardHeight = CellSize * Level.GridHeight;
	const float PanelWidth = BoardWidth + 16.0f * Scale;
	const float PanelHeight = BoardHeight + 43.0f * Scale;
	DrawRect(PanelFill, X, Y, PanelWidth, PanelHeight);
	DrawRect(Teal, X, Y, 3.0f * Scale, PanelHeight);
	DrawLabel(TEXT("小地图"), X + 12.0f * Scale, Y + 7.0f * Scale, 0.74f * Scale, TextPrimary);
	const float BoardX = X + (PanelWidth - BoardWidth) * 0.5f;
	const float BoardY = Y + 31.0f * Scale;

	for (int32 Row = 0; Row < Level.GridHeight; ++Row)
	{
		for (int32 Column = 0; Column < Level.GridWidth; ++Column)
		{
			const FIntPoint Cell(Column, Row);
			const ENaiwaP3FogCellState FogState = Fog.GetCellState(Cell);
			FLinearColor Color(0.025f, 0.04f, 0.055f, 1.0f);
			if (FogState != ENaiwaP3FogCellState::Unseen)
			{
				switch (Level.GetCell(Cell))
				{
				case ENaiwaP3M2Cell::HardBrick: Color = FLinearColor(0.25f, 0.34f, 0.42f, 1.0f); break;
				case ENaiwaP3M2Cell::SoftBrick: Color = FLinearColor(0.39f, 0.24f, 0.14f, 1.0f); break;
				case ENaiwaP3M2Cell::ItemBrick: Color = FLinearColor(0.61f, 0.37f, 0.13f, 1.0f); break;
				case ENaiwaP3M2Cell::Floor:
				default: Color = ((Column + Row) % 2 == 0) ? FLinearColor(0.13f, 0.20f, 0.24f, 1.0f) : FLinearColor(0.16f, 0.24f, 0.28f, 1.0f); break;
				}
				if (FogState == ENaiwaP3FogCellState::Remembered) Color *= 0.48f;
			}
			DrawRect(Color, BoardX + Column * CellSize, BoardY + Row * CellSize,
				FMath::Max(1.0f, CellSize - 0.6f * Scale), FMath::Max(1.0f, CellSize - 0.6f * Scale));
		}
	}

	const auto DrawMarker = [this, BoardX, BoardY, CellSize, Scale](const FIntPoint& Cell,
		const FLinearColor& Color, const float SizeScale)
	{
		const float Size = FMath::Max(2.0f * Scale, CellSize * SizeScale);
		DrawRect(Color, BoardX + Cell.X * CellSize + (CellSize - Size) * 0.5f,
			BoardY + Cell.Y * CellSize + (CellSize - Size) * 0.5f, Size, Size);
	};
	for (const FNaiwaP3M2MechanicPlacement& Mechanic : Level.Mechanics)
	{
		if (!Fog.IsVisible(Mechanic.Cell) && !Fog.IsRemembered(Mechanic.Cell)) continue;
		DrawMarker(Mechanic.Cell, Mechanic.Kind == ENaiwaP3M2MechanicKind::Portal ? Cyan : Violet, 0.52f);
	}
	for (const FIntPoint& Egg : Level.GoldenEggCells)
	{
		if (Fog.IsVisible(Egg)) DrawMarker(Egg, Gold, 0.66f);
	}
	for (int32 Index = 0; Index < Runtime.GetEnemyCells().Num(); ++Index)
	{
		const FIntPoint Cell = Runtime.GetEnemyCells()[Index];
		if (Runtime.IsEnemyAlive(Index) && Fog.IsVisible(Cell)) DrawMarker(Cell, Red, 0.70f);
	}
	for (const FNaiwaP3M2BombForecastInput& Bomb : Runtime.GetActiveBombForecasts())
	{
		if (Fog.IsVisible(Bomb.Cell)) DrawMarker(Bomb.Cell, FLinearColor(1.0f, 0.40f, 0.12f, 1.0f), 0.64f);
	}
	if (Fog.IsVisible(Level.ExitCell) || Fog.IsRemembered(Level.ExitCell)) DrawMarker(Level.ExitCell, Gold, 0.66f);
	DrawMarker(Runtime.GetPlayerCell(), Cyan, 0.82f);
}

void ANaiwaP3M2HUD::DrawLabel(const FString& Text, const float X, const float Y,
	const float Scale, const FLinearColor& Color)
{
	if (GEngine)
	{
		DrawText(Text, Color, X, Y, GEngine->GetMediumFont(), Scale * 1.12f);
	}
}

void ANaiwaP3M2HUD::DrawPanel(const float X, const float Y, const float Width,
	const float Height, const FLinearColor& Accent)
{
	DrawRect(PanelFill, X, Y, Width, Height);
	DrawRect(Accent, X, Y, 4.0f, Height);
	DrawRect(PanelSoft, X + 14.0f, Y + 43.0f, Width - 28.0f, 1.0f);
}

void ANaiwaP3M2HUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GEngine || !GetWorld())
	{
		return;
	}
	const UNaiwaP3M2RuntimeSubsystem* Runtime = GetWorld()->GetSubsystem<UNaiwaP3M2RuntimeSubsystem>();
	const FNaiwaP3M2LevelSession* Session = Runtime ? Runtime->GetSession() : nullptr;
	if (!Runtime || !Session)
	{
		return;
	}

	const float Width = Canvas->SizeX;
	const float Height = Canvas->SizeY;
	const float Scale = FMath::Clamp(FMath::Min(Width / 1440.0f, Height / 810.0f), 0.68f, 1.30f);
	const bool bTeachingLevel = Runtime->GetLevelId() == 31;
	const float FlashAlpha = Runtime->GetExplosionFlashFraction() * 0.24f;
	if (FlashAlpha > 0.0f)
	{
		DrawRect(FLinearColor(1.0f, 0.78f, 0.52f, FlashAlpha), 0.0f, 0.0f, Width, Height);
	}
	const float Margin = 22.0f * Scale;
	const float PanelWidth = 356.0f * Scale;
	const float PanelX = Width - Margin - PanelWidth;
	const float HeaderHeight = 76.0f * Scale;

	DrawRect(PanelFill, Margin, Margin, Width - Margin * 2.0f, HeaderHeight);
	DrawRect(Teal, Margin, Margin, 5.0f * Scale, HeaderHeight);
	DrawLabel(bTeachingLevel ? TEXT("奶龙历险 · 3-1 第一夜") : TEXT("奶龙历险 · 3-2 奶察总局"), Margin + 18.0f * Scale,
		Margin + 8.0f * Scale, 1.16f * Scale, TextPrimary);
	DrawLabel(bTeachingLevel ? TEXT("固定教学地图 · 全图可见") : FString::Printf(TEXT("正式布局  %d  ·  编号  %s  ·  战争迷雾"),
		Runtime->GetStartAttemptCount(), *Session->GetLevel().LayoutHash),
		Margin + 20.0f * Scale, Margin + 43.0f * Scale, 0.78f * Scale, TextSecondary);

	const float PanelY = Margin + HeaderHeight + 14.0f * Scale;
	DrawMinimap(*Session, *Runtime, Margin, PanelY, 248.0f * Scale, 190.0f * Scale, Scale);
	DrawPanel(PanelX, PanelY, PanelWidth, 430.0f * Scale, Gold);
	DrawLabel(TEXT("本局状态"), PanelX + 18.0f * Scale, PanelY + 13.0f * Scale,
		0.98f * Scale, TextPrimary);
	const float ElapsedSeconds = static_cast<float>(Runtime->GetCurrentTick()) * 0.1f;
	DrawLabel(FString::Printf(TEXT("用时  %05.1f 秒"), ElapsedSeconds),
		PanelX + 18.0f * Scale, PanelY + 58.0f * Scale, 0.82f * Scale, TextSecondary);
	DrawLabel(FString::Printf(TEXT("金蛋  %d / %d"), Runtime->GetCollectedEggCount(),
		Runtime->GetCollectedEggCount() + Runtime->GetRemainingEggCount()),
		PanelX + 18.0f * Scale, PanelY + 88.0f * Scale, 0.92f * Scale, Gold);
	DrawLabel(FString::Printf(TEXT("生命  %d"), Runtime->GetPlayerLives()),
		PanelX + 18.0f * Scale, PanelY + 126.0f * Scale, 0.86f * Scale,
		Runtime->GetPlayerLives() > 1 ? Teal : Red);
	DrawLabel(FString::Printf(TEXT("剩余敌人  %d"), Runtime->GetAliveEnemyCount()),
		PanelX + 158.0f * Scale, PanelY + 128.0f * Scale, 0.74f * Scale, TextSecondary);
	if (!bTeachingLevel)
	{
		const int32 NaiChaDistance = Runtime->GetNearestNaiChaDistance();
		const FString NaiChaStatus = NaiChaDistance == INDEX_NONE ? FString(TEXT("奶察未现")) :
			FString::Printf(TEXT("奶察 %d 格"), NaiChaDistance);
		DrawLabel(NaiChaStatus,
			PanelX + 158.0f * Scale, PanelY + 151.0f * Scale, 0.78f * Scale,
			NaiChaDistance != INDEX_NONE && NaiChaDistance <= 4 ? Red : TextSecondary);
	}
	DrawLabel(FString::Printf(TEXT("当前位置  %02d，%02d"), Runtime->GetPlayerCell().X,
		Runtime->GetPlayerCell().Y), PanelX + 18.0f * Scale, PanelY + 181.0f * Scale,
		0.80f * Scale, TextPrimary);
	DrawLabel(FString::Printf(TEXT("当前炸弹  %s"), GetNaiwaP3BombDisplayName(Runtime->GetSelectedBombType())),
		PanelX + 18.0f * Scale, PanelY + 215.0f * Scale, 0.78f * Scale, Violet);
	DrawBombLoadout(PanelX + 18.0f * Scale, PanelY + 240.0f * Scale,
		PanelWidth - 36.0f * Scale, Scale, Runtime->GetSelectedBombType(), bTeachingLevel);
	const FString BombDetail = Runtime->GetSelectedBombType() == ENaiwaP3BombType::Standard ?
		FString::Printf(TEXT("基础威力 固定五格十字  ·  容量 %d"), Runtime->GetBombCapacity()) :
		FString::Printf(TEXT("道具威力 %d 格  ·  容量 %d"), Runtime->GetBombRange(), Runtime->GetBombCapacity());
	DrawLabel(BombDetail, PanelX + 18.0f * Scale, PanelY + 296.0f * Scale,
		0.78f * Scale, TextSecondary);
	const FString SpeedLabel = Runtime->IsPlayerSlowed() ?
		FString::Printf(TEXT("移动速度  %.0f%%  ·  减速中"), Runtime->GetPlayerSpeedMultiplier() * 100.0f) :
		FString::Printf(TEXT("移动速度  %.0f%%"), Runtime->GetPlayerSpeedMultiplier() * 100.0f);
	DrawLabel(SpeedLabel, PanelX + 18.0f * Scale, PanelY + 328.0f * Scale,
		0.80f * Scale, Runtime->IsPlayerSlowed() ? Red : TextSecondary);
	DrawLabel(FString::Printf(TEXT("探索区域  %d / %d 格"), Session->GetFog().GetExploredCellCount(),
		Session->GetLevel().GridWidth * Session->GetLevel().GridHeight),
		PanelX + 18.0f * Scale, PanelY + 359.0f * Scale, 0.78f * Scale, TextSecondary);
	const float BombCooldown = Runtime->GetBombPlacementCooldownSeconds();
	DrawLabel(BombCooldown > 0.0f ?
		FString::Printf(TEXT("放弹冷却  %.1f 秒  ·  间隔 %.1f 秒"), BombCooldown, Runtime->GetBombPlacementIntervalSeconds()) :
		FString::Printf(TEXT("放弹间隔  %.1f 秒"), Runtime->GetBombPlacementIntervalSeconds()),
		PanelX + 18.0f * Scale, PanelY + 390.0f * Scale, 0.76f * Scale,
		BombCooldown > 0.0f ? Gold : TextSecondary);
	const FString ProgressStatus = bTeachingLevel ? TEXT("教学提示 · 炸开木箱，取得道具") :
		(Runtime->IsRageEscapeActive() ?
		FString::Printf(TEXT("狂暴回程  已持续 %.1f 秒"), Runtime->GetRageElapsedSeconds()) :
		TEXT("狂暴回程  集齐金蛋后触发"));
	DrawLabel(ProgressStatus, PanelX + 18.0f * Scale, PanelY + 414.0f * Scale,
		0.76f * Scale, Runtime->IsRageEscapeActive() ? Red : TextSecondary);
	const float NaiYuLockSeconds = Runtime->GetNaiYuLockSeconds();
	if (NaiYuLockSeconds >= 0.0f)
	{
		DrawLabel(FString::Printf(TEXT("奶玉锁定！%.1f 秒后冲击"), NaiYuLockSeconds),
			PanelX + 18.0f * Scale, PanelY + 414.0f * Scale, 0.80f * Scale, Red);
	}
	else if (Runtime->IsPlayerSlowed())
	{
		DrawLabel(TEXT("奶玉命中 · 移动减速中"), PanelX + 18.0f * Scale,
			PanelY + 414.0f * Scale, 0.80f * Scale, Red);
	}

	const float GuideY = PanelY + 442.0f * Scale;
	DrawPanel(PanelX, GuideY, PanelWidth, 256.0f * Scale, Teal);
	DrawLabel(TEXT("目标与操作"), PanelX + 18.0f * Scale, GuideY + 13.0f * Scale,
		0.98f * Scale, TextPrimary);
	DrawLabel(bTeachingLevel ? TEXT("收集 3 枚金蛋，再前往出口撤离。") : TEXT("收集全部金蛋，再前往出口撤离。"), PanelX + 18.0f * Scale,
		GuideY + 56.0f * Scale, 0.80f * Scale, TextSecondary);
	DrawLabel(TEXT("WASD / 方向键  移动"), PanelX + 18.0f * Scale,
		GuideY + 81.0f * Scale, 0.80f * Scale, TextPrimary);
	DrawLabel(TEXT("空格 / J  放置炸弹"), PanelX + 18.0f * Scale,
		GuideY + 106.0f * Scale, 0.80f * Scale, TextPrimary);
	DrawLabel(TEXT("1—4  切换炸弹类型"), PanelX + 18.0f * Scale,
		GuideY + 131.0f * Scale, 0.80f * Scale, TextPrimary);
	DrawLabel(TEXT("左 Shift  奶鸡跳（取得道具后）"), PanelX + 18.0f * Scale,
		GuideY + 156.0f * Scale, 0.78f * Scale, TextPrimary);
	DrawLabel(TEXT("青门 C / 紫门 D  ·  触碰传送"), PanelX + 18.0f * Scale,
		GuideY + 181.0f * Scale, 0.78f * Scale, Cyan);
	DrawLabel(TEXT("踩板开闸  ·  尖刺周期弹出  ·  裂地塌陷"), PanelX + 18.0f * Scale,
		GuideY + 206.0f * Scale, 0.74f * Scale, TextSecondary);
	DrawLabel(TEXT("R  重开本局（复现相同布局）"), PanelX + 18.0f * Scale,
		GuideY + 231.0f * Scale, 0.76f * Scale, Gold);

	const float DamageFeedback = Runtime->GetDamageFeedbackFraction();
	if (DamageFeedback > 0.0f)
	{
		FLinearColor Border = Runtime->WasLastDamageShielded() ? Cyan : Red;
		Border.A = 0.25f + DamageFeedback * 0.45f;
		const float BorderWidth = 8.0f * Scale;
		DrawRect(Border, 0.0f, 0.0f, Width, BorderWidth);
		DrawRect(Border, 0.0f, Height - BorderWidth, Width, BorderWidth);
		DrawRect(Border, 0.0f, 0.0f, BorderWidth, Height);
		DrawRect(Border, Width - BorderWidth, 0.0f, BorderWidth, Height);
		const float CardWidth = 430.0f * Scale;
		const float CardHeight = 76.0f * Scale;
		const float CardX = (Width - CardWidth) * 0.5f;
		const float CardY = Height * 0.13f;
		DrawRect(FLinearColor(0.04f, 0.05f, 0.06f, 0.96f), CardX, CardY, CardWidth, CardHeight);
		DrawRect(Border, CardX, CardY, 6.0f * Scale, CardHeight);
		DrawLabel(Runtime->GetDamageFeedbackMessage(), CardX + 24.0f * Scale,
			CardY + 23.0f * Scale, 1.0f * Scale, Border);
	}
	const float EventSeconds = Runtime->GetTransientEventSeconds();
	if (EventSeconds > 0.0f && !Runtime->GetTransientEventMessage().IsEmpty())
	{
		const float EventAlpha = FMath::Clamp(EventSeconds * 2.0f, 0.0f, 1.0f);
		const float CardWidth = FMath::Min(560.0f * Scale, Width - 2.0f * Margin);
		const float CardHeight = 62.0f * Scale;
		const float CardX = (Width - CardWidth) * 0.5f;
		const float CardY = Height * 0.22f;
		DrawRect(FLinearColor(0.035f, 0.048f, 0.06f, 0.94f * EventAlpha), CardX, CardY, CardWidth, CardHeight);
		DrawRect(Gold, CardX, CardY, 5.0f * Scale, CardHeight);
		DrawLabel(Runtime->GetTransientEventMessage(), CardX + 22.0f * Scale,
			CardY + 19.0f * Scale, 0.92f * Scale, TextPrimary);
	}

	FString StateLabel;
	FLinearColor StateColor = Teal;
	switch (Runtime->GetRunState())
	{
	case ENaiwaP3M2RunState::Won:
		StateLabel = TEXT("成功撤离");
		StateColor = Gold;
		break;
	case ENaiwaP3M2RunState::Defeated:
		StateLabel = TEXT("生命耗尽");
		StateColor = Red;
		break;
	case ENaiwaP3M2RunState::Playing:
	default:
		StateLabel = TEXT("探索中");
		break;
	}
	if (Runtime->GetRunState() != ENaiwaP3M2RunState::Playing)
	{
		const float CardWidth = 430.0f * Scale;
		const float CardHeight = 138.0f * Scale;
		const float CardX = (Width - CardWidth) * 0.5f;
		const float CardY = (Height - CardHeight) * 0.5f;
		DrawRect(PanelFill, CardX, CardY, CardWidth, CardHeight);
		DrawRect(StateColor, CardX, CardY, 5.0f * Scale, CardHeight);
		DrawLabel(StateLabel, CardX + 26.0f * Scale, CardY + 20.0f * Scale,
			1.16f * Scale, StateColor);
		DrawLabel(FString::Printf(TEXT("用时 %.1f 秒  ·  金蛋 %d 枚"), ElapsedSeconds,
			Runtime->GetCollectedEggCount()), CardX + 28.0f * Scale,
			CardY + 57.0f * Scale, 0.72f * Scale, TextPrimary);
		FString Followup;
		if (Runtime->GetRunState() == ENaiwaP3M2RunState::Defeated)
		{
			Followup = TEXT("按 R 重开本局");
		}
		else if (Runtime->GetLevelId() == 31)
		{
			const ANaiwaP3M2GameMode* Mode = Cast<ANaiwaP3M2GameMode>(GetWorld()->GetAuthGameMode());
			Followup = Mode && Mode->IsCampaignRun() ? TEXT("即将进入 3-2") : TEXT("通关已记录");
		}
		else
		{
			Followup = TEXT("P3 章节完成 · 即将返回关卡选择");
		}
		DrawLabel(Followup, CardX + 28.0f * Scale, CardY + 99.0f * Scale,
			0.68f * Scale, TextSecondary);
	}
}
