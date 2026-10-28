#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NaiwaP4Settings.generated.h"

/** P4 scalar defaults. Values are frozen per run; changing config does not alter a live run. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="P4 Arena"))
class NAIWARUNTIME_API UNaiwaP4Settings final : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category="Simulation", meta=(ClampMin="0.001", UIMin="0.001", UIMax="0.05"))
	double FixedStepSeconds = 1.0 / 60.0;

	UPROPERTY(Config, EditAnywhere, Category="Simulation", meta=(ClampMin="1", ClampMax="8"))
	int32 MaxCatchUpSteps = 4;

	UPROPERTY(Config, EditAnywhere, Category="Arena", meta=(ClampMin="100.0"))
	float LogicalArenaWidth = 1100.0f;

	UPROPERTY(Config, EditAnywhere, Category="Arena", meta=(ClampMin="50.0"))
	float LogicalArenaDepth = 185.0f;

	UPROPERTY(Config, EditAnywhere, Category="Player", meta=(ClampMin="0.0"))
	float WalkSpeed = 270.0f;

	UPROPERTY(Config, EditAnywhere, Category="Player", meta=(ClampMin="0.0", ClampMax="1.0"))
	float DepthSpeedScale = 0.611f;

	UPROPERTY(Config, EditAnywhere, Category="Player", meta=(ClampMin="0.0", ClampMax="1.0"))
	float BlockMoveSpeedScale = 0.45f;

	UPROPERTY(Config, EditAnywhere, Category="Player", meta=(ClampMin="1"))
	int32 PlayerMaxHP = 120;

	UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="1.0", ClampMax="89.0"))
	float CameraPitchDegrees = 57.0f;

	UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="1.0", ClampMax="170.0"))
	float CameraHorizontalFOVDegrees = 37.0f;

	UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="100.0"))
	float CameraDistance = 1900.0f;
};
