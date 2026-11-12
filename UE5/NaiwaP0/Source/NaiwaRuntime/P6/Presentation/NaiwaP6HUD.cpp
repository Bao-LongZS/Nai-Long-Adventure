#include "NaiwaP6HUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "P6/Core/NaiwaP6InvestigationSubsystem.h"
#include "P6/Data/NaiwaP6CaseDefinition.h"
#include "P6/Presentation/NaiwaP6InteractableActor.h"
#include "P6/Presentation/NaiwaP6PlayerPawn.h"

namespace
{
	const FLinearColor Panel(0.025f, 0.050f, 0.065f, 0.91f);
	const FLinearColor TextPrimary(0.92f, 0.95f, 0.93f, 1.0f);
	const FLinearColor TextMuted(0.64f, 0.72f, 0.72f, 1.0f);
	const FLinearColor Accent(0.30f, 0.86f, 0.75f, 1.0f);
}

void ANaiwaP6HUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GEngine || !GetWorld()) return;

	const UNaiwaP6InvestigationSubsystem* Investigation = GetWorld()->GetSubsystem<UNaiwaP6InvestigationSubsystem>();
	const ANaiwaP6PlayerPawn* Pawn = Cast<ANaiwaP6PlayerPawn>(GetOwningPawn());
	UFont* Font = GEngine->GetMediumFont();
	if (!Investigation || !Font) return;

	const float Width = Canvas->SizeX;
	const float Height = Canvas->SizeY;
	const float Scale = FMath::Clamp(FMath::Min(Width / 1440.0f, Height / 810.0f), 0.72f, 1.25f);
	const float Margin = 24.0f * Scale;
	const float HeaderHeight = 92.0f * Scale;
	DrawRect(Panel, Margin, Margin, Width - Margin * 2.0f, HeaderHeight);
	DrawRect(Accent, Margin, Margin, 5.0f * Scale, HeaderHeight);
	DrawText(TEXT("P6  /  5-1   禁笑令之夜"), TextPrimary, Margin + 18.0f * Scale,
		Margin + 12.0f * Scale, Font, 1.05f * Scale);
	FString ProgressText;
	if (Investigation->IsCaseConcluded())
	{
		ProgressText = FString::Printf(TEXT("本次结案   ·   证据 %d / 12"), Investigation->GetCollectedClueIds().Num());
	}
	else if (Investigation->IsCaseFailedAtDawn())
	{
		ProgressText = FString::Printf(TEXT("夜间结案暂停   ·   证据 %d / 12"), Investigation->GetCollectedClueIds().Num());
	}
	else if (Investigation->IsMorningReentry())
	{
		ProgressText = FString::Printf(TEXT("晨间复核   ·   证据 %d / 12"), Investigation->GetCollectedClueIds().Num());
	}
	else
	{
		ProgressText = FString::Printf(TEXT("大院调查 M4   ·   证据 %d / 12   ·   必看点 %d / 9"),
			Investigation->GetCollectedClueIds().Num(), Investigation->GetVisitedRequiredPointCount());
	}
	DrawText(ProgressText, TextMuted, Margin + 20.0f * Scale,
		Margin + 50.0f * Scale, Font, 0.70f * Scale);
	const FString NextHintText = Investigation->IsCaseConcluded()
		? TEXT("案件已结案；按 C 回顾三段结论和案情复盘。")
		: Investigation->GetNextRequiredHintText();
	DrawText(NextHintText, TextMuted,
		Margin + 20.0f * Scale, Margin + 70.0f * Scale, Font, 0.48f * Scale);
	const FString SkinStatus = FString::Printf(TEXT("%s%s  ·  Tab 余 %d 次"),
		*Investigation->GetCurrentSkinName(),
		Investigation->IsCurrentSkinBorrowed() ? TEXT("（试用）") : TEXT(""),
		Investigation->GetSkinSwitchesRemaining());
	DrawText(SkinStatus, Accent, Width - Margin - 420.0f * Scale,
		Margin + 12.0f * Scale, Font, 0.66f * Scale);
	const int32 ElapsedMinutes = Investigation->GetCaseElapsedMinutes();
	const FString CaseTime = FString::Printf(TEXT("%02d:%02d"), ElapsedMinutes / 60, ElapsedMinutes % 60);
	const TCHAR* BestRating = Investigation->GetBestCaseRating() >= 2 ? TEXT("完美") :
		(Investigation->GetBestCaseRating() == 1 ? TEXT("已结案") : TEXT("未结案"));
	FString CaseStatus;
	if (Investigation->IsCaseConcluded())
	{
		CaseStatus = FString::Printf(TEXT("%s · 最佳%s"),
			Investigation->GetCurrentCaseRating() >= 2 ? TEXT("本次完美结案") : TEXT("本次结案成立"), BestRating);
	}
	else if (Investigation->IsCaseFailedAtDawn())
	{
		CaseStatus = FString::Printf(TEXT("06:00 · 结案暂停 · 最佳%s"), BestRating);
	}
	else if (Investigation->IsMorningReentry())
	{
		CaseStatus = FString::Printf(TEXT("晨间复核 · 时间不再推进 · 最佳%s"), BestRating);
	}
	else
	{
		CaseStatus = FString::Printf(TEXT("时刻 %s / 06:00 · 累计失败 %d · 连败 %d · 最佳 %s"),
			*CaseTime, Investigation->GetWrongSubmissionCount(), Investigation->GetFailureStreakCount(), BestRating);
	}
	DrawText(CaseStatus, TextPrimary, Width - Margin - 560.0f * Scale,
		Margin + 48.0f * Scale, Font, 0.53f * Scale);

	if (Pawn)
	{
		if (ANaiwaP6InteractableActor* Nearby = Pawn->GetNearbyInteractable())
		{
			const float PromptWidth = 620.0f * Scale;
			const float PromptHeight = 50.0f * Scale;
			const float PromptX = (Width - PromptWidth) * 0.5f;
			const float PromptY = Height - Margin - PromptHeight - 54.0f * Scale;
			DrawRect(Panel, PromptX, PromptY, PromptWidth, PromptHeight);
			const bool bHasCurrentSkinLayer = Nearby->HasInfoLayer(Investigation->GetCurrentSkinId());
			const TCHAR* LayerHint = bHasCurrentSkinLayer ? TEXT("F 查看皮肤信息层") :
				(Nearby->HasInfoLayer() ? TEXT("Tab 换对应皮肤后按 F") : TEXT("此点没有信息层"));
			DrawText(FString::Printf(TEXT("%s   ·   空格 / J 检查   ·   %s"),
				*Nearby->GetDisplayName(), LayerHint), TextPrimary, PromptX + 16.0f * Scale,
				PromptY + 13.0f * Scale, Font, 0.78f * Scale);
		}

		const float FooterY = Height - Margin - 38.0f * Scale;
		DrawRect(Panel, Margin, FooterY, Width - Margin * 2.0f, 38.0f * Scale);
		const TCHAR* FooterText = Pawn->IsCaseBoardOpen()
			? (Investigation->IsCaseConcluded()
				? TEXT("案件已结案并揭晓真相     Q / E 回顾题目     C 返回院子     Esc 返回")
				: TEXT("Q / E 切换题目     1–4 选择     Enter 提交三段     C 返回院子     Esc 返回"))
			: TEXT("WASD / 方向键 移动     空格 / 左键 检查     Tab 换皮肤     F 信息层     C 线索板     Esc 返回");
		DrawText(FooterText,
			TextMuted, Margin + 14.0f * Scale, FooterY + 9.0f * Scale, Font, 0.66f * Scale);
		if (!Pawn->IsCaseBoardOpen() && !Pawn->GetLastFeedback().IsEmpty())
		{
			const float FeedbackY = Height - Margin - 174.0f * Scale;
			const float FeedbackWidth = Width - Margin * 2.0f;
			const float CharacterWidth = 11.0f * Scale;
			const int32 CharactersPerLine = FMath::Max(48,
				FMath::FloorToInt((FeedbackWidth - 28.0f * Scale) / CharacterWidth));
			FString FeedbackText = Pawn->GetLastFeedback();
			const int32 VisibleCharacterLimit = CharactersPerLine * 2;
			if (FeedbackText.Len() > VisibleCharacterLimit)
			{
				FeedbackText = FeedbackText.Left(VisibleCharacterLimit - 1) + TEXT("…");
			}
			DrawRect(Panel, Margin, FeedbackY, FeedbackWidth, 48.0f * Scale);
			for (int32 Line = 0; Line < 2; ++Line)
			{
				const int32 Start = Line * CharactersPerLine;
				if (Start >= FeedbackText.Len()) break;
				DrawText(FeedbackText.Mid(Start, CharactersPerLine), TextPrimary,
					Margin + 12.0f * Scale, FeedbackY + (7.0f + Line * 18.0f) * Scale,
					Font, 0.58f * Scale);
			}
		}

		if (Pawn->IsCaseBoardOpen())
		{
			const float BoardWidth = FMath::Min(1120.0f * Scale, Width - Margin * 2.0f);
			const float BoardHeight = FMath::Min(690.0f * Scale, Height - Margin * 2.0f);
			const float BoardX = (Width - BoardWidth) * 0.5f;
			const float BoardY = (Height - BoardHeight) * 0.5f;
			DrawRect(FLinearColor(0.015f, 0.027f, 0.035f, 0.97f), BoardX, BoardY, BoardWidth, BoardHeight);
			DrawRect(Accent, BoardX, BoardY, BoardWidth, 4.0f * Scale);
			DrawText(Investigation->IsCaseConcluded()
				? TEXT("线索板  /  案情复盘") : TEXT("线索板  /  三段结案"), TextPrimary,
				BoardX + 26.0f * Scale, BoardY + 20.0f * Scale, Font, 1.05f * Scale);

			if (Investigation->GetCollectedClueIds().IsEmpty())
			{
				DrawText(TEXT("还没有证据卡。先到院子里检查调查点。"), TextMuted,
					BoardX + 28.0f * Scale, BoardY + 68.0f * Scale, Font, 0.76f * Scale);
			}
			else
			{
				int32 CardIndex = 0;
				for (const FName ClueId : Investigation->GetCollectedClueIds())
				{
					const FNaiwaP6ClueDefinition* Clue = FNaiwaP6CaseDefinition::FindClue(ClueId);
					if (!Clue) continue;
					const int32 Columns = 4;
					const int32 Column = CardIndex % Columns;
					const int32 Row = CardIndex / Columns;
					const float Gap = 8.0f * Scale;
					const float CardWidth = (BoardWidth - 56.0f * Scale - Gap * (Columns - 1)) / Columns;
					const float CardHeight = 88.0f * Scale;
					const float CardX = BoardX + 28.0f * Scale + Column * (CardWidth + Gap);
					const float CardY = BoardY + 68.0f * Scale + Row * (CardHeight + Gap);
					DrawRect(FLinearColor(0.07f, 0.105f, 0.12f, 0.96f),
						CardX, CardY, CardWidth, CardHeight);
					const bool bSkinClue = Clue->ClueId.ToString().StartsWith(TEXT("S"));
					const bool bCrossClue = Clue->ClueId.ToString().StartsWith(TEXT("X"));
					const FLinearColor CardAccent = bCrossClue
						? FLinearColor(0.82f, 0.73f, 0.48f) : (bSkinClue ? Accent : TextMuted);
					DrawRect(CardAccent, CardX, CardY, 3.0f * Scale, CardHeight);
					DrawText(FString::Printf(TEXT("%s  %s"), *Clue->ClueId.ToString(), *Clue->Timestamp),
						CardAccent, CardX + 9.0f * Scale, CardY + 8.0f * Scale, Font, 0.47f * Scale);
					DrawText(Clue->Title.Left(13), TextPrimary,
						CardX + 9.0f * Scale, CardY + 28.0f * Scale, Font, 0.42f * Scale);
					DrawText(Clue->Source.Left(16), TextMuted,
						CardX + 9.0f * Scale, CardY + 43.0f * Scale, Font, 0.34f * Scale);
					FString CardBody = Clue->Body;
					if (CardBody.Len() > 48)
					{
						CardBody = CardBody.Left(47) + TEXT("…");
					}
					DrawText(CardBody.Left(24), TextMuted,
						CardX + 9.0f * Scale, CardY + 58.0f * Scale, Font, 0.31f * Scale);
					if (CardBody.Len() > 24)
					{
						DrawText(CardBody.Mid(24, 24), TextMuted,
							CardX + 9.0f * Scale, CardY + 71.0f * Scale, Font, 0.31f * Scale);
					}
					++CardIndex;
				}
			}

			const float QuestionY = BoardY + BoardHeight - 164.0f * Scale;
			if (Investigation->IsCaseConcluded())
			{
				const float RevealHeight = 72.0f * Scale;
				const float FeedbackY = QuestionY - 76.0f * Scale;
				const float RevealY = FeedbackY - RevealHeight - 8.0f * Scale;
				DrawRect(FLinearColor(0.08f, 0.12f, 0.14f, 1.0f),
					BoardX + 20.0f * Scale, RevealY, BoardWidth - 40.0f * Scale, RevealHeight);
				DrawText(TEXT("案情复盘  /  结案后揭晓"), Accent,
					BoardX + 32.0f * Scale, RevealY + 7.0f * Scale, Font, 0.50f * Scale);
				const float RevealTextY = RevealY + 28.0f * Scale;
				const float CharacterWidth = 10.0f * Scale;
				const int32 CharactersPerLine = FMath::Max(36,
					FMath::FloorToInt((BoardWidth - 72.0f * Scale) / CharacterWidth));
				FString RevealText = FNaiwaP6CaseDefinition::GetConclusionRevealText();
				const int32 VisibleCharacterLimit = CharactersPerLine * 2;
				if (RevealText.Len() > VisibleCharacterLimit)
				{
					RevealText = RevealText.Left(VisibleCharacterLimit - 1) + TEXT("…");
				}
				for (int32 Line = 0; Line < 2; ++Line)
				{
					const int32 Start = Line * CharactersPerLine;
					if (Start >= RevealText.Len()) break;
					DrawText(RevealText.Mid(Start, CharactersPerLine), TextPrimary,
						BoardX + 32.0f * Scale, RevealTextY + Line * 19.0f * Scale,
						Font, 0.45f * Scale);
				}
			}
			if (!Pawn->GetLastFeedback().IsEmpty())
			{
				const float FeedbackY = QuestionY - 76.0f * Scale;
				const float FeedbackHeight = 56.0f * Scale;
				DrawRect(Panel, BoardX + 20.0f * Scale, FeedbackY,
					BoardWidth - 40.0f * Scale, FeedbackHeight);
				const float CharacterWidth = 10.0f * Scale;
				const int32 CharactersPerLine = FMath::Max(36,
					FMath::FloorToInt((BoardWidth - 72.0f * Scale) / CharacterWidth));
				FString FeedbackText = Pawn->GetLastFeedback();
				const int32 VisibleCharacterLimit = CharactersPerLine * 2;
				if (FeedbackText.Len() > VisibleCharacterLimit)
				{
					FeedbackText = FeedbackText.Left(VisibleCharacterLimit - 1) + TEXT("…");
				}
				for (int32 Line = 0; Line < 2; ++Line)
				{
					const int32 Start = Line * CharactersPerLine;
					if (Start >= FeedbackText.Len()) break;
					DrawText(FeedbackText.Mid(Start, CharactersPerLine), TextPrimary,
						BoardX + 32.0f * Scale, FeedbackY + (8.0f + Line * 20.0f) * Scale,
						Font, 0.48f * Scale);
				}
			}
			DrawRect(FLinearColor(0.08f, 0.12f, 0.14f, 1.0f),
				BoardX + 20.0f * Scale, QuestionY - 12.0f * Scale, BoardWidth - 40.0f * Scale, 146.0f * Scale);
			const int32 QuestionIndex = Pawn->GetSelectedQuestionIndex();
			const int32 QuestionCount = FNaiwaP6CaseDefinition::GetQuestionCount();
			const FName SelectedAnswerId = Investigation->GetSelectedAnswerId(QuestionIndex);
			DrawText(FString::Printf(TEXT("第 %d / %d 段  ·  已选 %d / %d"),
				QuestionIndex + 1, QuestionCount, Investigation->GetSelectedQuestionAnswerCount(), QuestionCount),
				Accent, BoardX + 38.0f * Scale, QuestionY, Font, 0.52f * Scale);
			DrawText(FNaiwaP6CaseDefinition::GetQuestionText(QuestionIndex), TextPrimary,
				BoardX + 38.0f * Scale, QuestionY + 22.0f * Scale, Font, 0.68f * Scale);
			const TArray<FNaiwaP6AnswerOption>& Options = FNaiwaP6CaseDefinition::GetQuestionOptions(QuestionIndex);
			const float OptionY = QuestionY + 52.0f * Scale;
			for (int32 Index = 0; Index < Options.Num(); ++Index)
			{
				const FLinearColor OptionColor = SelectedAnswerId == Options[Index].AnswerId ? Accent : TextMuted;
				DrawText(FString::Printf(TEXT("%d  %s"), Index + 1, *Options[Index].Label), OptionColor,
					BoardX + 42.0f * Scale + (Index % 2) * BoardWidth * 0.46f,
					OptionY + (Index / 2) * 25.0f * Scale, Font, 0.56f * Scale);
			}
			FString BoardStatus;
			if (Investigation->IsCaseConcluded())
			{
				BoardStatus = Investigation->GetCurrentCaseRating() >= 2
					? TEXT("完美结案 · 证据保留") : TEXT("结案成立 · 证据保留");
			}
			else if (Investigation->IsCaseFailedAtDawn())
			{
				BoardStatus = TEXT("已到 06:00 · 仍可查看证据；离开并重进后晨间复核");
			}
			else if (Investigation->IsMorningReentry())
			{
				BoardStatus = TEXT("晨间复核 · 失败提交不再推进时间");
			}
			else
			{
				BoardStatus = TEXT("提交前只展示证据与选项，不标记答案正误。Q / E 浏览，1–4 选择，Enter 整组提交。");
			}
			DrawText(BoardStatus,
				TextMuted, BoardX + 28.0f * Scale, BoardY + BoardHeight - 24.0f * Scale,
				Font, 0.50f * Scale);
		}
	}
}
