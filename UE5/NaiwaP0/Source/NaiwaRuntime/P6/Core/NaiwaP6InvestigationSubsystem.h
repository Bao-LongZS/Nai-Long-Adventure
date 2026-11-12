#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NaiwaP6InvestigationSubsystem.generated.h"

/** Session authority for collected evidence, visited points, and the current case flow. */
UCLASS()
class NAIWARUNTIME_API UNaiwaP6InvestigationSubsystem final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	bool CollectClue(FName ClueId, FName SourcePointId = NAME_None);
	bool HasClue(FName ClueId) const;
	const TArray<FName>& GetCollectedClueIds() const { return CollectedClueIds; }
	FName GetCurrentSkinId() const { return CurrentSkinId; }
	FString GetCurrentSkinName() const;
	int32 GetSkinSwitchesRemaining() const { return FMath::Max(0, MaxSkinSwitches - SkinSwitchCount); }
	bool IsCurrentSkinBorrowed() const;
	bool CycleSkin(FString& OutFeedback);
	void RecordPointVisited(FName PointId);
	bool HasVisitedPoint(FName PointId) const;
	int32 GetVisitedRequiredPointCount() const;
	FString GetNextRequiredHintText() const;
	bool IsFirstQuestionSolved() const { return bFirstQuestionSolved; }
	bool SubmitFirstQuestion(FName AnswerId, FString& OutFeedback);
	FName GetSelectedAnswerId(int32 QuestionIndex) const;
	bool SetSelectedAnswer(int32 QuestionIndex, FName AnswerId, FString& OutFeedback);
	bool SubmitCase(FString& OutFeedback);
	int32 GetSelectedQuestionAnswerCount() const;
	int32 GetWrongSubmissionCount() const { return WrongSubmissionCount; }
	int32 GetFailureStreakCount() const { return FailureStreakCount; }
	int32 GetCaseElapsedMinutes() const { return CaseElapsedMinutes; }
	int32 GetBestCaseRating() const { return BestCaseRating; }
	int32 GetCurrentCaseRating() const { return CurrentCaseRating; }
	bool IsMorningReentry() const { return bMorningReentry; }
	bool IsCaseConcluded() const { return bCaseConcluded; }
	bool IsCaseFailedAtDawn() const { return bCaseFailedAtDawn; }

private:
	void PersistClue(FName ClueId);
	void TryCollectCrossClues(FName SourcePointId);
	void InitializeCurrentSkin();

	TArray<FName> CollectedClueIds;
	TSet<FName> VisitedPointIds;
	FName CurrentSkinId = FName(TEXT("NaiwaSkin_Base"));
	int32 SkinSwitchCount = 0;
	TArray<FName> SelectedAnswerIds;
	/** Stable for this world session so repeated clear calls cannot duplicate rewards. */
	FGuid CaseRunRewardId;
	int32 WrongSubmissionCount = 0;
	int32 FailureStreakCount = 0;
	int32 CaseElapsedMinutes = 20;
	int32 BestCaseRating = 0;
	int32 CurrentCaseRating = 0;
	bool bFirstQuestionSolved = false;
	bool bProbeMode = false;
	bool bMorningReentry = false;
	bool bCaseConcluded = false;
	bool bCaseFailedAtDawn = false;
	static constexpr int32 MaxSkinSwitches = 2;
	static constexpr int32 FailedSubmissionMinutes = 40;
	static constexpr int32 DawnMinutes = 360;
};
