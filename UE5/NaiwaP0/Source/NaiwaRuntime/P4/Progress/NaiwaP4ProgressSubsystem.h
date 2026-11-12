#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NaiwaP4ProgressSubsystem.generated.h"

/** P4 Beta6 progress only. This save object uses its own slot and never touches menu progress. */
UCLASS()
class NAIWARUNTIME_API UNaiwaP4SaveGame final : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentSchemaVersion = 1;
	static constexpr int32 Beta6ChapterCount = 6;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="P4|Progress")
	int32 SchemaVersion = CurrentSchemaVersion;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="P4|Progress")
	int32 HighestUnlockedChapterIndex = 1;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="P4|Progress")
	int32 ActiveChapterIndex = 1;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="P4|Progress")
	int32 LastClearedChapterIndex = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="P4|Progress")
	int64 RunSeed = 0;

	/** Entry seed stays fixed for retry and resume, so the chapter can replay deterministically. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="P4|Progress")
	int64 ActiveChapterSeed = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="P4|Progress")
	int32 ActiveChapterRetryCount = 0;

	/** Ordered sequence is intentional: repeatable fallback cards may appear more than once. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="P4|Progress")
	TArray<FName> AcquiredUpgradeCardIds;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="P4|Progress")
	float RunElapsedSeconds = 0.0f;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="P4|Progress")
	int32 RunHighestCombo = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="P4|Progress")
	bool bHasActiveRun = false;

	/** Reaching Chapter 6 completes this temporary Beta scope, not the ten-chapter story. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="P4|Progress")
	bool bBeta6StageComplete = false;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="P4|Progress")
	float BestBeta6RunSeconds = 0.0f;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="P4|Progress")
	int32 BestBeta6HighestCombo = 0;
};

/** Owns sequential Beta6 progression, retries and persistence in the P4-specific save slot. */
UCLASS()
class NAIWARUNTIME_API UNaiwaP4ProgressSubsystem final : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category="P4|Progress")
	bool IsChapterUnlocked(int32 ChapterIndex) const;

	UFUNCTION(BlueprintPure, Category="P4|Progress")
	int32 GetActiveChapterIndex() const;

	UFUNCTION(BlueprintPure, Category="P4|Progress")
	bool HasActiveRun() const;

	UFUNCTION(BlueprintPure, Category="P4|Progress")
	bool HasReachedBeta6StageEnd() const;

	UFUNCTION(BlueprintPure, Category="P4|Progress")
	TArray<FName> GetAcquiredUpgradeCardIds() const;

	UFUNCTION(BlueprintPure, Category="P4|Progress")
	float GetRunElapsedSeconds() const;

	UFUNCTION(BlueprintPure, Category="P4|Progress")
	int32 GetRunHighestCombo() const;

	/** Starts at Chapter 1 and clears run-local state while preserving Beta6 best records. */
	UFUNCTION(BlueprintCallable, Category="P4|Progress")
	bool StartNewBeta6Run(int64 InRunSeed, FString& OutError);

	/** Resumes the saved chapter from its entry seed; partial fixed-step combat state is not restored. */
	UFUNCTION(BlueprintCallable, Category="P4|Progress")
	bool ResumeCurrentChapter(int32& OutChapterIndex, int64& OutChapterSeed, FString& OutError) const;

	/** Restarts the current chapter at the same entry seed and keeps all acquired run upgrades. */
	UFUNCTION(BlueprintCallable, Category="P4|Progress")
	bool RetryCurrentChapter(int64& OutChapterSeed, FString& OutError);

	/** Appends one card to the ordered run history. Duplicate IDs are allowed by card rules. */
	UFUNCTION(BlueprintCallable, Category="P4|Progress")
	bool RecordAcquiredUpgrade(FName CardId, FString& OutError);

	/** Clears the active chapter and advances sequentially, or marks the Chapter 6 scope endpoint. */
	UFUNCTION(BlueprintCallable, Category="P4|Progress")
	bool CompleteCurrentChapter(int32 ChapterIndex, float CumulativeRunSeconds,
		int32 HighestCombo, FString& OutError);

private:
	static uint32 DeriveChapterSeed(uint32 InRunSeed, int32 ChapterIndex);
	bool Persist(FString& OutError) const;
	void CreateDefaultSave();

	UPROPERTY(Transient)
	TObjectPtr<UNaiwaP4SaveGame> SaveData;

	/** A newer save schema is kept readable but is not overwritten by this build. */
	bool bSaveWritable = true;

	static constexpr int32 SaveUserIndex = 0;
	static const TCHAR* SaveSlotName;
};
