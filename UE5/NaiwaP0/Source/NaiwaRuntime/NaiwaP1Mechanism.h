#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NaiwaP1Mechanism.generated.h"

/** Base contract for P1-only actors that must return to their initial state after an attempt. */
UCLASS(Abstract, Blueprintable)
class NAIWARUNTIME_API ANaiwaP1Mechanism : public AActor
{
	GENERATED_BODY()

public:
	virtual void ResetForAttempt() PURE_VIRTUAL(ANaiwaP1Mechanism::ResetForAttempt, );
};
