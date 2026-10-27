#include "NaiwaP3M1HUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "NaiwaP3GridSubsystem.h"

namespace
{
	FLinearColor FromSrgb(const uint8 R, const uint8 G, const uint8 B, const uint8 A = 255)
	{
		return FLinearColor::FromSRGBColor(FColor(R, G, B, A));
	}

	const FLinearColor PanelFill(0.028f, 0.041f, 0.055f, 0.94f);
	const FLinearColor PanelSoft(0.045f, 0.065f, 0.080f, 0.94f);
	const FLinearColor TextPrimary = FromSrgb(226, 232, 239);
	const FLinearColor TextSecondary = FromSrgb(154, 169, 184);
	const FLinearColor ZoneA = FromSrgb(104, 169, 157);
	const FLinearColor ZoneB = FromSrgb(208, 180, 119);
	const FLinearColor ZoneC = FromSrgb(255, 160, 40);
	const FLinearColor ZoneD = FromSrgb(255, 116, 78);
}

void ANaiwaP3M1HUD::DrawPanel(const float X, const float Y, const float Width,
	const float Height, const FLinearColor& Accent)
{
	DrawRect(PanelFill, X, Y, Width, Height);
	DrawRect(Accent, X, Y, 4.0f, Height);
	DrawRect(PanelSoft, X + 10.0f, Y + 10.0f, Width - 20.0f, 1.0f);
}

void ANaiwaP3M1HUD::DrawLabel(const FString& Text, const float X, const float Y,
	const float Scale, const FLinearColor& Color)
{
	if (GEngine)
	{
		DrawText(Text, Color, X, Y, GEngine->GetMediumFont(), Scale);
	}
}

void ANaiwaP3M1HUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GEngine) return;

	const float Width = Canvas->SizeX;
	const float Height = Canvas->SizeY;
	const UNaiwaP3GridSubsystem* Grid = GetWorld() ? GetWorld()->GetSubsystem<UNaiwaP3GridSubsystem>() : nullptr;
	const float Scale = FMath::Clamp(FMath::Min(Width / 1280.0f, Height / 720.0f), 0.72f, 1.30f);
	const float Margin = 22.0f * Scale;
	const float PanelWidth = 330.0f * Scale;
	const float PanelX = Width - Margin - PanelWidth;

	DrawRect(PanelFill, Margin, Margin, Width - Margin * 2.0f, 66.0f * Scale);
	DrawRect(ZoneA, Margin, Margin, 5.0f * Scale, 66.0f * Scale);
	DrawLabel(TEXT("夜巡 / P3 手感测试"), Margin + 18.0f * Scale,
		Margin + 8.0f * Scale, 1.20f * Scale, TextPrimary);
	DrawLabel(TEXT("15×11 测试场  |  网格移动  |  无物理模拟"),
		Margin + 20.0f * Scale, Margin + 38.0f * Scale, 0.78f * Scale, TextSecondary);

	const float GuideY = Margin + 82.0f * Scale;
	DrawPanel(PanelX, GuideY, PanelWidth, 274.0f * Scale, ZoneA);
	DrawLabel(TEXT("区域说明"), PanelX + 20.0f * Scale, GuideY + 16.0f * Scale,
		0.92f * Scale, TextPrimary);
	DrawLabel(TEXT("A   开阔区"), PanelX + 20.0f * Scale, GuideY + 47.0f * Scale,
		0.84f * Scale, ZoneA);
	DrawLabel(TEXT("练习移动、停止与转向。"), PanelX + 20.0f * Scale,
		GuideY + 69.0f * Scale, 0.68f * Scale, TextSecondary);
	DrawLabel(TEXT("B   木箱区"), PanelX + 20.0f * Scale, GuideY + 99.0f * Scale,
		0.84f * Scale, ZoneB);
	DrawLabel(TEXT("木箱阻挡火焰，被击中的箱子会摧毁。"), PanelX + 20.0f * Scale,
		GuideY + 121.0f * Scale, 0.62f * Scale, TextSecondary);
	DrawLabel(TEXT("C   连锁通道"), PanelX + 20.0f * Scale, GuideY + 151.0f * Scale,
		0.84f * Scale, ZoneC);
	const FString ChainHint = Grid ? FString::Printf(TEXT("按 T 演示四枚炸弹，连锁间隔 %.1f 秒。"),
		Grid->GetChainDelaySeconds()) : TEXT("按 T 演示四枚炸弹连续引爆。");
	DrawLabel(ChainHint, PanelX + 20.0f * Scale,
		GuideY + 173.0f * Scale, 0.60f * Scale, TextSecondary);
	DrawLabel(TEXT("D   放弹闪避"), PanelX + 20.0f * Scale, GuideY + 203.0f * Scale,
		0.84f * Scale, ZoneD);
	DrawLabel(TEXT("放下炸弹后，及时跑出十字爆炸区。"), PanelX + 20.0f * Scale,
		GuideY + 225.0f * Scale, 0.62f * Scale, TextSecondary);

	const float StatusY = GuideY + 284.0f * Scale;
	DrawPanel(PanelX, StatusY, PanelWidth, 126.0f * Scale, ZoneB);
	DrawLabel(TEXT("实时读数"), PanelX + 20.0f * Scale, StatusY + 14.0f * Scale,
		0.88f * Scale, TextPrimary);
	if (Grid)
	{
		const FIntPoint PlayerCell = Grid->GetPlayerCell();
		const FIntPoint EnemyCell = Grid->GetEnemyCell();
		const float Elapsed = static_cast<float>(Grid->GetCurrentTick()) * 0.1f;
		DrawLabel(FString::Printf(TEXT("时间 %05.1f 秒  在场炸弹 %d 枚"), Elapsed,
			Grid->GetActiveBombCount()), PanelX + 20.0f * Scale, StatusY + 55.0f * Scale,
			0.68f * Scale, TextPrimary);
		DrawLabel(FString::Printf(TEXT("玩家坐标 %02d / %02d"), PlayerCell.X, PlayerCell.Y),
			PanelX + 20.0f * Scale, StatusY + 76.0f * Scale, 0.66f * Scale, ZoneA);
		DrawLabel(FString::Printf(TEXT("奶察坐标 %02d / %02d"), EnemyCell.X, EnemyCell.Y),
			PanelX + 20.0f * Scale, StatusY + 96.0f * Scale, 0.66f * Scale, ZoneB);
		DrawLabel(FString::Printf(TEXT("爆炸命中 玩家 %d  /  奶察 %d"), Grid->GetPlayerHitCount(), Grid->GetEnemyHitCount()),
			PanelX + 20.0f * Scale, StatusY + 115.0f * Scale, 0.60f * Scale, TextSecondary);
	}

	const float MetricsY = StatusY + 136.0f * Scale;
	DrawPanel(PanelX, MetricsY, PanelWidth, 120.0f * Scale, ZoneC);
	DrawLabel(TEXT("当前参数"), PanelX + 20.0f * Scale, MetricsY + 13.0f * Scale,
		0.82f * Scale, TextPrimary);
	if (Grid)
	{
		const int32 MaximumCrossCells = 1 + 4 * Grid->GetBombRange();
		DrawLabel(FString::Printf(TEXT("玩家 %.1f 格/秒  奶察 %.1f 格/秒"),
			Grid->GetPlayerCellsPerSecond(), Grid->GetPatrolCellsPerSecond()), PanelX + 20.0f * Scale,
			MetricsY + 39.0f * Scale, 0.60f * Scale, TextSecondary);
		DrawLabel(FString::Printf(TEXT("当前 %s  容量 %d 枚  标准范围 %d 格"),
			Grid->GetSelectedBombDisplayName(), Grid->GetBombCapacity(), MaximumCrossCells), PanelX + 20.0f * Scale,
			MetricsY + 60.0f * Scale, 0.58f * Scale, TextSecondary);
		DrawLabel(FString::Printf(TEXT("放弹冷却 %.1f 秒  引爆 %.1f 秒"),
			Grid->GetBombPlacementIntervalSeconds(), Grid->GetBombFuseSeconds()),
			PanelX + 20.0f * Scale, MetricsY + 81.0f * Scale, 0.58f * Scale, TextSecondary);
		DrawLabel(FString::Printf(TEXT("连锁 %.1f 秒  火焰 %.1f 秒"),
			Grid->GetChainDelaySeconds(), Grid->GetFlameDurationSeconds()),
			PanelX + 20.0f * Scale, MetricsY + 102.0f * Scale, 0.58f * Scale, TextSecondary);
	}

	const float FooterHeight = 52.0f * Scale;
	const float FooterY = Height - Margin - FooterHeight;
	DrawRect(PanelFill, Margin, FooterY, Width - Margin * 2.0f, FooterHeight);
	DrawLabel(TEXT("WASD / 方向键：移动"), Margin + 14.0f * Scale,
		FooterY + 7.0f * Scale, 0.70f * Scale, TextPrimary);
	DrawLabel(TEXT("空格 / J：放炸弹"), Margin + 190.0f * Scale,
		FooterY + 7.0f * Scale, 0.70f * Scale, ZoneC);
	DrawLabel(TEXT("1-4：切换炸弹"), Margin + 360.0f * Scale,
		FooterY + 7.0f * Scale, 0.70f * Scale, ZoneA);
	DrawLabel(TEXT("T：连锁演示"), Margin + 505.0f * Scale,
		FooterY + 7.0f * Scale, 0.70f * Scale, ZoneB);
	DrawLabel(TEXT("R：重置"), Margin + 650.0f * Scale,
		FooterY + 7.0f * Scale, 0.70f * Scale, ZoneB);
	DrawLabel(TEXT("无胜负结算，可自由测试"), Margin + 14.0f * Scale,
		FooterY + 29.0f * Scale, 0.62f * Scale, TextSecondary);
}
