#pragma once

#include "CoreMinimal.h"

namespace NaiwaP2
{
    class FProbeTraj
    {
    public:
        static bool Run(const FString& BallisticsFixturePath,
                        const FString& DesignFixturePath,
                        const FString& BallisticsFixtureHash,
                        const FString& DesignFixtureHash,
                        const FString& ReportPath);
    };

    class FProbeShot
    {
    public:
        static bool Run(const FString& DesignFixturePath,
                        const FString& DesignFixtureHash,
                        const FString& ReportPath);
    };
}
