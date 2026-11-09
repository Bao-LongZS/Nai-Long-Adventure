#pragma once

#include "CoreMinimal.h"

namespace NaiwaWebNative
{
    /** Writes a read-only snapshot of the current P2 scene data. */
    class FP2SceneExport
    {
    public:
        static bool Run(const FString& OutputPath,
                        const FString& SceneSourceHash,
                        const FString& PhysicsProfileHash,
                        const FString& ProjectileDriverHash,
                        const FString& GameModeHash,
                        const FString& ExporterHash,
                        const FString& RuntimeEntryHash);
    };
}
