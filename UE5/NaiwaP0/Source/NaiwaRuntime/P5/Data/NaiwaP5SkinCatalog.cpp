#include "NaiwaP5SkinCatalog.h"

const TArray<FNaiwaP5SkinRow>& FNaiwaP5SkinCatalog::GetRows()
{
	static const TArray<FNaiwaP5SkinRow> Rows = {
		{ FName(TEXT("NaiwaSkin_Base")), NAME_None, 0, 0.0000f, 1.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, false, false, TEXT("奶蛙基础款"), TEXT("无加成；基准玩法") },
		{ FName(TEXT("NaiwaSkin_NaiYu")), FName(TEXT("WaterFilm")), 40, -10.0000f, 1.2500f, 1.5000f, 0.0000f, 0.0000f, 0.0000f, false, false, TEXT("奶雨"), TEXT("成功落地后 1.5 秒速度 ×1.25，落地角度修正 10°") },
		{ FName(TEXT("NaiwaSkin_NaiCha")), FName(TEXT("ObstacleWarning")), 60, 0.0000f, 1.0000f, 0.0000f, 40.0000f, 0.0000f, 0.0000f, false, false, TEXT("奶察"), TEXT("提前 40 米标记下一个障碍") },
		{ FName(TEXT("NaiwaSkin_NaiHao")), FName(TEXT("AvalanchePushback")), 80, 0.0000f, 1.0000f, 0.0000f, 0.0000f, 6.0000f, 8.0000f, false, false, TEXT("奶豪"), TEXT("每 8 秒一次，落地时自动增加 6 米雪崩距离") },
		{ FName(TEXT("NaiwaSkin_NaiSha")), FName(TEXT("TreeImpactGuard")), 140, 0.0000f, 1.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, true, true, TEXT("奶杀"), TEXT("撞树不扣即时距离、不摔倒；仍清除连击") }
	};
	return Rows;
}
