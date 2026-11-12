#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "NaiwaP6PlayerPawn.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInstanceDynamic;
class UFloatingPawnMovement;
class ANaiwaP6InteractableActor;

UCLASS()
class NAIWARUNTIME_API ANaiwaP6PlayerPawn final : public APawn
{
	GENERATED_BODY()

public:
	ANaiwaP6PlayerPawn();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	bool IsCaseBoardOpen() const { return bCaseBoardOpen; }
	int32 GetSelectedQuestionIndex() const { return SelectedQuestionIndex; }
	FString GetLastFeedback() const { return LastFeedback; }
	ANaiwaP6InteractableActor* GetNearbyInteractable() const;

private:
	void MoveUpPressed();
	void MoveUpReleased();
	void MoveDownPressed();
	void MoveDownReleased();
	void MoveLeftPressed();
	void MoveLeftReleased();
	void MoveRightPressed();
	void MoveRightReleased();
	void InteractPressed();
	void InteractAtCursor();
	void ToggleClueBoard();
	void ToggleInfoLayer();
	void SwitchSkin();
	void PreviousQuestion();
	void NextQuestion();
	void SelectAnswer(int32 OptionIndex);
	void ApplyCurrentSkinAppearance();
	void SelectAnswer1();
	void SelectAnswer2();
	void SelectAnswer3();
	void SelectAnswer4();
	void SubmitAnswer();
	void ReturnToLevelSelect();
	void SetMovementKey(bool& bKeyState, bool bPressed);
	void UpdateFeedback(const FString& Text);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UFloatingPawnMovement> FloatingMovement;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;

	bool bMoveUp = false;
	bool bMoveDown = false;
	bool bMoveLeft = false;
	bool bMoveRight = false;
	bool bCaseBoardOpen = false;
	int32 SelectedQuestionIndex = 0;
	FString LastFeedback;
};
