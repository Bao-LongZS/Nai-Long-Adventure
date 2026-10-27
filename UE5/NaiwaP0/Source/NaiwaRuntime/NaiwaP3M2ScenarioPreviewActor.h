#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NaiwaP3M2ScenarioPreviewActor.generated.h"

class ANaiwaP3M1MapActor;
class ANaiwaP3M2WorldPresentation;
class USceneComponent;

UCLASS()
class NAIWARUNTIME_API ANaiwaP3M2ScenarioPreviewActor final : public AActor
{
	GENERATED_BODY()

public:
	ANaiwaP3M2ScenarioPreviewActor();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "奶龙历险|M2 测试关")
	void BuildEditorPreview();

	UPROPERTY(EditAnywhere, Category = "奶龙历险|正式关卡")
	int32 PreviewLevelId = 32;

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(Transient)
	TObjectPtr<ANaiwaP3M1MapActor> MapPreview;

	UPROPERTY(Transient)
	TObjectPtr<ANaiwaP3M2WorldPresentation> WorldPreview;
};
