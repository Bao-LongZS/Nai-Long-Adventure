#pragma once

#include "NaiwaP2Types.h"

namespace NaiwaP2
{
    class FSceneLibrary
    {
    public:
        static FScene Make21();
        static FScene Make22(double HighPlatformApex = 7.50);
        static FScene Make23(int32 Stage = 1);
        static FScene Make(const FString& LevelId, int32 Stage = 1);
        static FAmmoRule GetAmmoRule(const FString& LevelId);
    };
}
