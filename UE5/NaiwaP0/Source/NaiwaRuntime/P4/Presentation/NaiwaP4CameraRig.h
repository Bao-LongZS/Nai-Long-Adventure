#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NaiwaP4CameraRig.generated.h"

class UCameraComponent;

/** Fixed perspective camera for the P4 arena. It never follows or shakes. */
UCLASS()
class NAIWARUNTIME_API ANaiwaP4CameraRig final : public AActor
{
	GENERATED_BODY()

public:
	ANaiwaP4CameraRig();
	virtual void BeginPlay() override;
	void ApplyCalibration();

private:
	UPROPERTY(VisibleAnywhere, Category="P4|Camera")
	TObjectPtr<UCameraComponent> CameraComponent;
};
