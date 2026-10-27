#include "NaiwaP3BombTypes.h"

const TCHAR* GetNaiwaP3BombDisplayName(const ENaiwaP3BombType Type)
{
	switch (Type)
	{
	case ENaiwaP3BombType::SoyBigCross: return TEXT("豆奶大十字");
	case ENaiwaP3BombType::TeaRadial: return TEXT("奶茶圆爆");
	case ENaiwaP3BombType::RatSplit: return TEXT("奶鼠分裂弹");
	case ENaiwaP3BombType::RatFragment: return TEXT("奶鼠子弹");
	case ENaiwaP3BombType::Standard:
	default: return TEXT("标准十字弹");
	}
}

FLinearColor GetNaiwaP3BombColor(const ENaiwaP3BombType Type)
{
	switch (Type)
	{
	case ENaiwaP3BombType::SoyBigCross:
		return FLinearColor::FromSRGBColor(FColor(202, 157, 78));
	case ENaiwaP3BombType::TeaRadial:
		return FLinearColor::FromSRGBColor(FColor(174, 115, 186));
	case ENaiwaP3BombType::RatSplit:
	case ENaiwaP3BombType::RatFragment:
		return FLinearColor::FromSRGBColor(FColor(105, 143, 202));
	case ENaiwaP3BombType::Standard:
	default:
		return FLinearColor::FromSRGBColor(FColor(35, 42, 49));
	}
}
