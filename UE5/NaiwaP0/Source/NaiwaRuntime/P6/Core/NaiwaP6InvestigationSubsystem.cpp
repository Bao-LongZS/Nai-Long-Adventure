#include "NaiwaP6InvestigationSubsystem.h"

#include "NaiwaMenuProgress.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "P6/Data/NaiwaP6CaseDefinition.h"
#include "P5/Data/NaiwaP5SkinCatalog.h"

void UNaiwaP6InvestigationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bProbeMode = FParse::Param(FCommandLine::Get(), TEXT("NaiwaP6M1Probe")) ||
		FParse::Param(FCommandLine::Get(), TEXT("NaiwaP6M2Probe")) ||
		FParse::Param(FCommandLine::Get(), TEXT("NaiwaP6M3Probe")) ||
		FParse::Param(FCommandLine::Get(), TEXT("NaiwaP6M4Probe"));
	InitializeCurrentSkin();
	SelectedAnswerIds.Init(NAME_None, FNaiwaP6CaseDefinition::GetQuestionCount());
	CaseRunRewardId = FGuid::NewGuid();
	if (bProbeMode)
	{
		FString M4Scenario;
		if (FParse::Value(FCommandLine::Get(), TEXT("NaiwaP6M4Scenario="), M4Scenario) &&
			M4Scenario.Equals(TEXT("Morning"), ESearchCase::IgnoreCase))
		{
			// This transient fixture exercises morning reentry without reading or writing
			// the player's persisted case counters.
			WrongSubmissionCount = 8;
			FailureStreakCount = 2;
			CaseElapsedMinutes = DawnMinutes;
			BestCaseRating = 1;
			bMorningReentry = true;
		}
		return;
	}
	if (const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (const UNaiwaProgressSubsystem* Progress = GameInstance->GetSubsystem<UNaiwaProgressSubsystem>())
		{
			WrongSubmissionCount = Progress->GetP6WrongSubmissionCount();
			FailureStreakCount = Progress->GetP6FailureStreakCount();
			CaseElapsedMinutes = Progress->GetP6CaseElapsedMinutes();
			BestCaseRating = Progress->GetP6BestCaseRating();
			bMorningReentry = CaseElapsedMinutes >= DawnMinutes;
			for (const FName ClueId : Progress->GetP6CollectedClueIds())
			{
				if (FNaiwaP6CaseDefinition::FindClue(ClueId))
				{
					CollectedClueIds.AddUnique(ClueId);
				}
			}
		}
	}
	TSet<FName> CrossCluePoints;
	for (const FNaiwaP6CrossClueDefinition& Recipe : FNaiwaP6CaseDefinition::GetM3CrossClues())
	{
		CrossCluePoints.Add(Recipe.PointId);
	}
	for (const FName PointId : CrossCluePoints)
	{
		TryCollectCrossClues(PointId);
	}
}

void UNaiwaP6InvestigationSubsystem::InitializeCurrentSkin()
{
	CurrentSkinId = FName(TEXT("NaiwaSkin_Base"));
	if (const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (const UNaiwaProgressSubsystem* Progress = GameInstance->GetSubsystem<UNaiwaProgressSubsystem>())
		{
			const FName SelectedSkinId = Progress->GetSelectedP5SkinId();
			const bool bHasSelectedSkin = FNaiwaP5SkinCatalog::GetRows().ContainsByPredicate(
				[SelectedSkinId](const FNaiwaP5SkinRow& Row) { return Row.SkinId == SelectedSkinId; });
			const FNaiwaP6InfoLayerDefinition* SelectedLayer = FNaiwaP6CaseDefinition::GetM3InfoLayers()
				.FindByPredicate([SelectedSkinId](const FNaiwaP6InfoLayerDefinition& Layer)
					{ return Layer.SkinId == SelectedSkinId; });
			const bool bSelectedLayerUnseen = SelectedLayer &&
				!Progress->GetP6CollectedClueIds().Contains(SelectedLayer->ClueId);
			const FName BaseSkinId(TEXT("NaiwaSkin_Base"));
			const bool bHasCollectedAnySkinClue = FNaiwaP6CaseDefinition::GetM3InfoLayers().ContainsByPredicate(
				[Progress](const FNaiwaP6InfoLayerDefinition& Layer)
					{ return Progress->GetP6CollectedClueIds().Contains(Layer.ClueId); });
			if (bHasSelectedSkin && SelectedSkinId == BaseSkinId && !bHasCollectedAnySkinClue)
			{
				// Keep the no-purchase baseline available on a fresh save. Once a skin layer
				// has been collected, re-entry can resume with another unseen layer.
				CurrentSkinId = BaseSkinId;
				return;
			}
			if (bHasSelectedSkin && SelectedSkinId != BaseSkinId && bSelectedLayerUnseen)
			{
				CurrentSkinId = SelectedSkinId;
				return;
			}

			// If the player has not selected a purchased variant, start with the first
			// still-unseen layer. Re-entering P6 then continues with the next missing skin.
			for (const FNaiwaP6InfoLayerDefinition& Layer : FNaiwaP6CaseDefinition::GetM3InfoLayers())
			{
				if (!Progress->GetP6CollectedClueIds().Contains(Layer.ClueId))
				{
					CurrentSkinId = Layer.SkinId;
					return;
				}
			}
			if (bHasSelectedSkin)
			{
				CurrentSkinId = SelectedSkinId;
			}
		}
	}
}

FString UNaiwaP6InvestigationSubsystem::GetCurrentSkinName() const
{
	if (const FNaiwaP5SkinRow* Row = FNaiwaP5SkinCatalog::GetRows().FindByPredicate(
		[this](const FNaiwaP5SkinRow& Skin) { return Skin.SkinId == CurrentSkinId; }))
	{
		return Row->DisplayName;
	}
	return TEXT("奶蛙基础款");
}

bool UNaiwaP6InvestigationSubsystem::IsCurrentSkinBorrowed() const
{
	if (CurrentSkinId == FName(TEXT("NaiwaSkin_Base")))
	{
		return false;
	}
	if (const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (const UNaiwaProgressSubsystem* Progress = GameInstance->GetSubsystem<UNaiwaProgressSubsystem>())
		{
			return !Progress->IsP5SkinOwned(CurrentSkinId);
		}
	}
	return true;
}

bool UNaiwaP6InvestigationSubsystem::CycleSkin(FString& OutFeedback)
{
	if (SkinSwitchCount >= MaxSkinSwitches)
	{
		OutFeedback = TEXT("本局两次皮肤切换已用完；重新进入 5-1 可继续试用其他信息层。");
		return false;
	}

	const TArray<FNaiwaP5SkinRow>& Rows = FNaiwaP5SkinCatalog::GetRows();
	if (Rows.Num() < 2)
	{
		OutFeedback = TEXT("皮肤目录暂不可用。");
		return false;
	}
	int32 CurrentIndex = Rows.IndexOfByPredicate(
		[this](const FNaiwaP5SkinRow& Row) { return Row.SkinId == CurrentSkinId; });
	if (CurrentIndex == INDEX_NONE)
	{
		CurrentIndex = 0;
	}
	CurrentIndex = (CurrentIndex + 1) % Rows.Num();
	CurrentSkinId = Rows[CurrentIndex].SkinId;
	++SkinSwitchCount;
	OutFeedback = FString::Printf(TEXT("当前皮肤：%s%s；本局还可切换 %d 次。"),
		*Rows[CurrentIndex].DisplayName,
		IsCurrentSkinBorrowed() ? TEXT("（本局借用）") : TEXT(""), GetSkinSwitchesRemaining());
	return true;
}

bool UNaiwaP6InvestigationSubsystem::CollectClue(const FName ClueId, const FName SourcePointId)
{
	if (!FNaiwaP6CaseDefinition::FindClue(ClueId) || CollectedClueIds.Contains(ClueId))
	{
		return false;
	}

	CollectedClueIds.Add(ClueId);
	PersistClue(ClueId);
	TryCollectCrossClues(SourcePointId);
	return true;
}

void UNaiwaP6InvestigationSubsystem::PersistClue(const FName ClueId)
{
	if (!bProbeMode)
	{
		if (const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		{
			if (UNaiwaProgressSubsystem* Progress = GameInstance->GetSubsystem<UNaiwaProgressSubsystem>())
			{
				Progress->RecordP6ClueCollected(ClueId);
			}
		}
	}
}

void UNaiwaP6InvestigationSubsystem::TryCollectCrossClues(const FName SourcePointId)
{
	if (SourcePointId.IsNone())
	{
		return;
	}
	for (const FNaiwaP6CrossClueDefinition& Recipe : FNaiwaP6CaseDefinition::GetM3CrossClues())
	{
		if (Recipe.PointId != SourcePointId || Recipe.RequiredClueIds.IsEmpty() || HasClue(Recipe.ClueId))
		{
			continue;
		}
		const bool bHasMissingInput = Recipe.RequiredClueIds.ContainsByPredicate(
			[this](const FName RequiredClueId) { return !HasClue(RequiredClueId); });
		if (!bHasMissingInput && FNaiwaP6CaseDefinition::FindClue(Recipe.ClueId))
		{
			CollectedClueIds.Add(Recipe.ClueId);
			PersistClue(Recipe.ClueId);
		}
	}
}

bool UNaiwaP6InvestigationSubsystem::HasClue(const FName ClueId) const
{
	return CollectedClueIds.Contains(ClueId);
}

void UNaiwaP6InvestigationSubsystem::RecordPointVisited(const FName PointId)
{
	if (FNaiwaP6CaseDefinition::FindInteractable(PointId))
	{
		VisitedPointIds.Add(PointId);
	}
}

bool UNaiwaP6InvestigationSubsystem::HasVisitedPoint(const FName PointId) const
{
	return VisitedPointIds.Contains(PointId);
}

int32 UNaiwaP6InvestigationSubsystem::GetVisitedRequiredPointCount() const
{
	int32 Count = 0;
	for (const FNaiwaP6InteractableDefinition& Definition : FNaiwaP6CaseDefinition::GetM2Interactables())
	{
		if (Definition.bM2Required && HasVisitedPoint(Definition.PointId))
		{
			++Count;
		}
	}
	return Count;
}

FString UNaiwaP6InvestigationSubsystem::GetNextRequiredHintText() const
{
	if (bCaseConcluded)
	{
		return TEXT("本次结论已记录；案件证据保留在 C 线索板中。重进后可再次复核。");
	}
	if (bCaseFailedAtDawn)
	{
		return TEXT("已到 06:00，夜间结案流程暂停；仍可查看或收集证据。离开并重新进入 5-1 后可晨间复核。");
	}
	if (bMorningReentry)
	{
		return TEXT("晨间复核：已保存的证据仍在；按 C 可直接重新提交，无须重走夜间路线。");
	}
	for (const FNaiwaP6InteractableDefinition& Definition : FNaiwaP6CaseDefinition::GetM2Interactables())
	{
		if (Definition.bM2Required && !HasVisitedPoint(Definition.PointId))
		{
			return FString::Printf(TEXT("下一步：%s"), *Definition.HintText);
		}
	}
	return TEXT("必看点已检查完；可回顾线索板，院内笑点均可跳过。" );
}

bool UNaiwaP6InvestigationSubsystem::SubmitFirstQuestion(const FName AnswerId, FString& OutFeedback)
{
	if (bFirstQuestionSolved)
	{
		OutFeedback = TEXT("第一段结论已记录。你收集的证据仍保留在线索板中。");
		return true;
	}
	if (!HasClue(FName(TEXT("G5"))))
	{
		OutFeedback = TEXT("证据还不够。先检查戏台边的脚印和擦痕，再来提交。");
		return false;
	}

	// Keep the hidden key inside the authority layer. HUD and scene actors only receive
	// question text, options, and collected clue cards.
	if (AnswerId == FName(TEXT("NaiDuck")))
	{
		bFirstQuestionSolved = true;
		OutFeedback = TEXT("这段结论与现场痕迹吻合。证据卡已保留。");
		return true;
	}

	OutFeedback = TEXT("这段结论暂时对不上现场痕迹。线索仍保留，可以继续调查后再提交。");
	return false;
}

FName UNaiwaP6InvestigationSubsystem::GetSelectedAnswerId(const int32 QuestionIndex) const
{
	return SelectedAnswerIds.IsValidIndex(QuestionIndex) ? SelectedAnswerIds[QuestionIndex] : NAME_None;
}

bool UNaiwaP6InvestigationSubsystem::SetSelectedAnswer(
	const int32 QuestionIndex, const FName AnswerId, FString& OutFeedback)
{
	if (bCaseConcluded)
	{
		OutFeedback = TEXT("本次结论已记录；证据卡仍可查看。重新进入 5-1 可再次复核。");
		return false;
	}
	if (bCaseFailedAtDawn)
	{
		OutFeedback = TEXT("已到 06:00，夜间结案流程暂停；仍可查看证据。离开并重新进入 5-1 后可晨间复核。");
		return false;
	}
	if (!SelectedAnswerIds.IsValidIndex(QuestionIndex))
	{
		OutFeedback = TEXT("题目序号无效，请在结案题目间切换后再选择。");
		return false;
	}

	const TArray<FNaiwaP6AnswerOption>& Options = FNaiwaP6CaseDefinition::GetQuestionOptions(QuestionIndex);
	if (!Options.ContainsByPredicate([AnswerId](const FNaiwaP6AnswerOption& Option)
		{ return Option.AnswerId == AnswerId; }))
	{
		OutFeedback = TEXT("这个选项不属于当前题目。");
		return false;
	}

	SelectedAnswerIds[QuestionIndex] = AnswerId;
	OutFeedback = FString::Printf(TEXT("已记录第 %d / %d 段选择；提交前不会显示答案判断。"),
		QuestionIndex + 1, FNaiwaP6CaseDefinition::GetQuestionCount());
	return true;
}

int32 UNaiwaP6InvestigationSubsystem::GetSelectedQuestionAnswerCount() const
{
	int32 Count = 0;
	for (const FName AnswerId : SelectedAnswerIds)
	{
		if (!AnswerId.IsNone())
		{
			++Count;
		}
	}
	return Count;
}

bool UNaiwaP6InvestigationSubsystem::SubmitCase(FString& OutFeedback)
{
	if (bCaseConcluded)
	{
		OutFeedback = TEXT("本次结论已记录；所有证据仍保留在线索板中。");
		return true;
	}
	if (bCaseFailedAtDawn)
	{
		OutFeedback = TEXT("已到 06:00，夜间结案流程暂停；仍可查看证据。离开并重新进入 5-1 后可晨间复核。");
		return false;
	}
	if (GetSelectedQuestionAnswerCount() != FNaiwaP6CaseDefinition::GetQuestionCount())
	{
		OutFeedback = FString::Printf(TEXT("还差 %d 段选择；用 Q / E 浏览题目，再按 Enter 一次提交。"),
			FNaiwaP6CaseDefinition::GetQuestionCount() - GetSelectedQuestionAnswerCount());
		return false;
	}

	// The board submits all three answers as one case decision, so each segment's
	// supporting clue set must be present before the score is evaluated.
	TArray<FName> MissingClueIds;
	for (int32 QuestionIndex = 0; QuestionIndex < FNaiwaP6CaseDefinition::GetQuestionCount(); ++QuestionIndex)
	{
		for (const FName RequiredClueId : FNaiwaP6CaseDefinition::GetQuestionRequiredClueIds(QuestionIndex))
		{
			if (!HasClue(RequiredClueId))
			{
				MissingClueIds.AddUnique(RequiredClueId);
			}
		}
	}
	if (!MissingClueIds.IsEmpty())
	{
		TArray<FString> MissingClueNames;
		MissingClueNames.Reserve(MissingClueIds.Num());
		for (const FName ClueId : MissingClueIds)
		{
			MissingClueNames.Add(ClueId.ToString());
		}
		OutFeedback = FString::Printf(TEXT("关键证据尚未收齐（缺少 %s）。请先检查后再提交整组结论。"),
			*FString::Join(MissingClueNames, TEXT("、")));
		return false;
	}

	const int32 CorrectAnswerCount = FNaiwaP6CaseDefinition::ScoreAnswers(SelectedAnswerIds);
	if (CorrectAnswerCount == INDEX_NONE)
	{
		OutFeedback = TEXT("当前选择与题目选项不匹配；请检查每段选择后再提交。");
		return false;
	}
	if (CorrectAnswerCount >= 2)
	{
		const int32 Rating = CorrectAnswerCount == FNaiwaP6CaseDefinition::GetQuestionCount() ? 2 : 1;
		const bool bBestRatingImproved = Rating > BestCaseRating;
		int32 TicketsGranted = 0;
		bCaseConcluded = true;
		CurrentCaseRating = Rating;
		FailureStreakCount = 0;
		BestCaseRating = FMath::Max(BestCaseRating, Rating);
		if (!bProbeMode)
		{
			if (const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
			{
				if (UNaiwaProgressSubsystem* Progress = GameInstance->GetSubsystem<UNaiwaProgressSubsystem>())
				{
					TicketsGranted = Progress->RecordP6CaseClear(Rating, CaseRunRewardId);
				}
			}
		}
		const FString RewardFeedback = TicketsGranted > 0
			? FString::Printf(TEXT("本次结案获得 %d 张奶票。"), TicketsGranted)
			: TEXT("本次结案没有新增奶票奖励。");
		const FString RatingFeedback = bBestRatingImproved
			? TEXT("最佳评级已提升。")
			: TEXT("最佳评级保持不变。");
		OutFeedback = CorrectAnswerCount == FNaiwaP6CaseDefinition::GetQuestionCount()
			? FString::Printf(TEXT("三段结论全部与证据吻合，案件已结案。证据卡保留；%s%s"), *RatingFeedback, *RewardFeedback)
			: FString::Printf(TEXT("证据链已足以结案，仍有细节可再核对。案件已结案，证据卡保留；%s%s"),
				*RatingFeedback, *RewardFeedback);
		return true;
	}

	WrongSubmissionCount = FMath::Clamp(WrongSubmissionCount, 0, MAX_int32 - 1) + 1;
	FailureStreakCount = FMath::Clamp(FailureStreakCount, 0, MAX_int32 - 1) + 1;
	const int32 MinutesAdvanced = bMorningReentry ? 0 : FailedSubmissionMinutes;
	const int32 PreviousElapsedMinutes = CaseElapsedMinutes;
	CaseElapsedMinutes = FMath::Clamp(CaseElapsedMinutes + MinutesAdvanced, 20, DawnMinutes);
	bCaseFailedAtDawn = PreviousElapsedMinutes < DawnMinutes && CaseElapsedMinutes >= DawnMinutes;
	if (!bProbeMode)
	{
		if (const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		{
			if (UNaiwaProgressSubsystem* Progress = GameInstance->GetSubsystem<UNaiwaProgressSubsystem>())
			{
				Progress->RecordP6FailedSubmission(MinutesAdvanced);
			}
		}
	}

	OutFeedback = TEXT("这组结论暂时不足以结案。证据卡与已选内容保留，可调整后重新提交。");
	if (FailureStreakCount >= 3)
	{
		OutFeedback += TEXT("建议回看 G5 的接触痕迹，以及 G1、G3、G4、G6 的时间与装置记录。");
	}
	if (bCaseFailedAtDawn)
	{
		OutFeedback += TEXT("现在已到 06:00，夜间结案流程暂停；仍可查看证据，离开并重新进入 5-1 后可晨间复核。");
	}
	return false;
}
