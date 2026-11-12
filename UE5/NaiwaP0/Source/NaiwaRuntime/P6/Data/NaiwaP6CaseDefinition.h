#pragma once

#include "CoreMinimal.h"
#include "P6/Core/NaiwaP6CaseTypes.h"

/** Versioned native case fixture shared by P6 M1–M4. */
class FNaiwaP6CaseDefinition
{
public:
	static const TArray<FNaiwaP6ClueDefinition>& GetM1Clues();
	static const TArray<FNaiwaP6InteractableDefinition>& GetM1Interactables();
	static const TArray<FNaiwaP6ClueDefinition>& GetM2Clues();
	static const TArray<FNaiwaP6ClueDefinition>& GetM3Clues();
	static const TArray<FNaiwaP6InteractableDefinition>& GetM2Interactables();
	static const TArray<FNaiwaP6InfoLayerDefinition>& GetM3InfoLayers();
	static const TArray<FNaiwaP6CrossClueDefinition>& GetM3CrossClues();
	static int32 GetQuestionCount();
	static const FString& GetQuestionText(int32 QuestionIndex);
	static const TArray<FNaiwaP6AnswerOption>& GetQuestionOptions(int32 QuestionIndex);
	static const TArray<FName>& GetQuestionRequiredClueIds(int32 QuestionIndex);
	/** Truth recap for the post-conclusion board; keep it hidden until the session concludes. */
	static const FString& GetConclusionRevealText();
	static int32 ScoreAnswers(const TArray<FName>& AnswerIds);
	static const TArray<FNaiwaP6AnswerOption>& GetFirstQuestionOptions();
	static const FString& GetFirstQuestionText();
	static const FNaiwaP6ClueDefinition* FindClue(FName ClueId);
	static const FNaiwaP6InteractableDefinition* FindInteractable(FName PointId);
	static const FNaiwaP6InfoLayerDefinition* FindInfoLayer(FName ClueId);
	static const FNaiwaP6InfoLayerDefinition* FindInfoLayer(FName PointId, FName SkinId);
	static constexpr int32 DefinitionVersion = 5;
};
