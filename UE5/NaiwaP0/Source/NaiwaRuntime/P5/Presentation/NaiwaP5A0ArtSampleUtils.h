#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"

/** True only for the isolated P5 A0 art sample maps. */
inline bool IsNaiwaP5A0ArtSampleWorld(const UWorld* World)
{
	return World && World->GetMapName().Contains(TEXT("L_NaiwaP5_A0ArtSample"));
}

