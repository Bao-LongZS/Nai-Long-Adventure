#include "NaiwaP6CaseDefinition.h"

namespace
{
	struct FNaiwaP6QuestionRow
	{
		FString Prompt;
		TArray<FNaiwaP6AnswerOption> Options;
		TArray<FName> RequiredClueIds;
		FName CorrectAnswerId = NAME_None;
	};

	const TArray<FNaiwaP6QuestionRow>& GetP6Questions()
	{
		static const TArray<FNaiwaP6QuestionRow> Questions = {
			{
				TEXT("谁撞倒了奖杯？"),
				{
					{ FName(TEXT("NaiDou")), TEXT("奶抖") },
					{ FName(TEXT("NaiDuck")), TEXT("奶鸭") },
					{ FName(TEXT("NaiPai")), TEXT("奶拍") },
					{ FName(TEXT("NaiBi")), TEXT("奶鼻") }
				},
				{ FName(TEXT("G5")) },
				FName(TEXT("NaiDuck"))
			},
			{
				TEXT("检测仪录到的“笑声”是什么？"),
				{
					{ FName(TEXT("NaiDou")), TEXT("奶抖") },
					{ FName(TEXT("NaiPai")), TEXT("奶拍") },
					{ FName(TEXT("Chip")), TEXT("奖杯内的感应播放芯片") },
					{ FName(TEXT("NaiBi")), TEXT("奶鼻") }
				},
				{ FName(TEXT("G1")), FName(TEXT("G3")) },
				FName(TEXT("Chip"))
			},
			{
				TEXT("三名嫌疑人谁在说谎？"),
				{
					{ FName(TEXT("NoOne")), TEXT("没有人因这次警报说谎") },
					{ FName(TEXT("NaiDou")), TEXT("奶抖") },
					{ FName(TEXT("NaiPai")), TEXT("奶拍") },
					{ FName(TEXT("NaiBi")), TEXT("奶鼻") }
				},
				{ FName(TEXT("G4")), FName(TEXT("G6")) },
				FName(TEXT("NoOne"))
			}
		};
		return Questions;
	}
}

const TArray<FNaiwaP6ClueDefinition>& FNaiwaP6CaseDefinition::GetM1Clues()
{
	static const TArray<FNaiwaP6ClueDefinition> Clues = {
		{ FName(TEXT("G1")), TEXT("奖杯铭牌与底座结构"),
			TEXT("奖杯底座内有感应触发器和声音播放模块。铭牌标注为低价塑料壳产品。"),
			TEXT("案发后检查"), TEXT("戏台奖杯") },
		{ FName(TEXT("G5")), TEXT("蹼印、啄痕与底座擦痕"),
			TEXT("三趾蹼印从假花旁延伸到奖杯底座；底座擦痕与假花的材质连续。"),
			TEXT("23:57 左右"), TEXT("戏台边缘") }
	};
	return Clues;
}

const TArray<FNaiwaP6InteractableDefinition>& FNaiwaP6CaseDefinition::GetM1Interactables()
{
	static const TArray<FNaiwaP6InteractableDefinition> Points = {
		{ FName(TEXT("Trophy")), TEXT("感应奖杯"), FVector(-320.0f, 520.0f, 70.0f),
			FVector(1.15f, 0.72f, 1.25f), FLinearColor(0.84f, 0.62f, 0.25f), FName(TEXT("G1")),
			true, TEXT(""),
			TEXT("检查戏台边的感应奖杯") },
		{ FName(TEXT("Flowers")), TEXT("戏台假花与地面痕迹"), FVector(1050.0f, 400.0f, 45.0f),
			FVector(0.82f, 0.82f, 0.68f), FLinearColor(0.72f, 0.44f, 0.51f), FName(TEXT("G5")),
			true, TEXT(""), TEXT("沿假花旁的三趾印检查底座擦痕") },
		{ FName(TEXT("Socks")), TEXT("屋檐下的袜子"), FVector(1770.0f, -810.0f, 55.0f),
			FVector(0.58f, 0.42f, 0.72f), FLinearColor(0.40f, 0.63f, 0.72f), NAME_None,
			false, TEXT("袜子左右脚的花纹不一样。洗衣绳拒绝就案情发表评论。"), TEXT("") }
	};
	return Points;
}

const TArray<FNaiwaP6ClueDefinition>& FNaiwaP6CaseDefinition::GetM2Clues()
{
	static const TArray<FNaiwaP6ClueDefinition> Clues = []()
	{
		TArray<FNaiwaP6ClueDefinition> Result;
		const TArray<FNaiwaP6ClueDefinition>& M1Clues = GetM1Clues();
		Result.Add(M1Clues[0]); // G1
		Result.Add({ FName(TEXT("G2")), TEXT("禁笑令与评比公告"),
			TEXT("公告要求静音并安排检测评比。三名值守者都有共同动机，但公告不能单独指认任何人。"),
			TEXT("背景资料"), TEXT("禁笑令 / 评比") });
		Result.Add({ FName(TEXT("G3")), TEXT("检测仪采样纸条"),
			TEXT("23:58:07，96 dB，分类为人类大笑。波形中段被踩糊；纸条不是完整录音。"),
			TEXT("23:58:07"), TEXT("检测仪纸条") });
		Result.Add({ FName(TEXT("G4")), TEXT("照片底片与元数据"),
			TEXT("23:57:15 奖杯已经倒地。亮斑与闪光同步，底片没有搬动或剪切痕迹。"),
			TEXT("23:57:15"), TEXT("奶拍底片") });
		FNaiwaP6ClueDefinition M2G5 = M1Clues[1];
		M2G5.Source = TEXT("假花 / 底座"); // Match the current A3 card index while preserving the M1 fixture.
		Result.Add(M2G5); // G5
		Result.Add({ FName(TEXT("G6")), TEXT("巡夜记录、口供与收摊票据"),
			TEXT("奶抖 00:03、奶拍 00:12、奶鼻 03:00 才分别发笑；奶鼻在 03:00 扶正奖杯。"),
			TEXT("23:55—03:00"), TEXT("巡夜记录 / 票据") });
		return Result;
	}();
	return Clues;
}

const TArray<FNaiwaP6InteractableDefinition>& FNaiwaP6CaseDefinition::GetM2Interactables()
{
	static const TArray<FNaiwaP6InteractableDefinition> Points = []()
	{
		TArray<FNaiwaP6InteractableDefinition> Result;
		const TArray<FNaiwaP6InteractableDefinition>& M1Points = GetM1Interactables();
		Result.Add(M1Points[0]); // Trophy / G1
		Result.Add({ FName(TEXT("Noticeboard")), TEXT("村委会公告栏"), FVector(-2700.0f, 900.0f, 74.0f),
			FVector(0.82f, 0.24f, 1.18f), FLinearColor(0.70f, 0.59f, 0.38f), FName(TEXT("G2")),
			true, TEXT("公告盖了三层‘禁笑令’章，末尾还有一张被奶渍糊住的评比签到。"), TEXT("查看院门边的禁笑令公告") });
		Result.Add({ FName(TEXT("Detector")), TEXT("静音检测仪与纸条"), FVector(2300.0f, 610.0f, 78.0f),
			FVector(0.74f, 0.68f, 1.12f), FLinearColor(0.37f, 0.54f, 0.57f), FName(TEXT("G3")),
			true, TEXT("纸条显示 96 dB 和‘人类大笑’，但波形被踩糊，机器只留下分类结果。"),
			TEXT("读一下静音检测仪的采样纸条") });
		Result.Add({ FName(TEXT("PhotoTable")), TEXT("奶拍的相机与底片"), FVector(1500.0f, 1080.0f, 65.0f),
			FVector(0.72f, 0.64f, 0.78f), FLinearColor(0.55f, 0.59f, 0.72f), FName(TEXT("G4")),
			true, TEXT("底片边上写着‘我拍到时它已经倒了’，闪光亮斑正好对着镜头。"),
			TEXT("检查门廊相机的底片") });
		Result.Add(M1Points[1]); // Flowers / G5
		Result.Add({ FName(TEXT("PatrolLedger")), TEXT("巡夜簿与摊位收摊票"), FVector(-2250.0f, -1000.0f, 68.0f),
			FVector(0.88f, 0.63f, 0.72f), FLinearColor(0.50f, 0.43f, 0.34f), FName(TEXT("G6")),
			true, TEXT("记录写着 00:03、00:12、03:00；三次笑都晚于仪器采样，奶鼻在收摊时才扶杯。"),
			TEXT("核对西侧门房里的巡夜簿") });
		Result.Add({ FName(TEXT("NaiDou")), TEXT("奶抖的口供"), FVector(-3150.0f, 0.50f, 90.0f),
			FVector(0.46f, 0.42f, 0.92f), FLinearColor(0.42f, 0.66f, 0.80f), NAME_None,
			true, TEXT("奶抖：00:03 我看见奶鸭滑进垃圾桶才笑；奖杯那时已倒，我没看见它怎么倒的。"),
			TEXT("听听戏台西侧巡夜人的口供") });
		Result.Add({ FName(TEXT("NaiPai")), TEXT("奶拍的口供"), FVector(3100.0f, 760.0f, 90.0f),
			FVector(0.46f, 0.42f, 0.92f), FLinearColor(0.80f, 0.59f, 0.52f), NAME_None,
			true, TEXT("奶拍：23:57 拍照时奖杯已经倒了；00:12 回看过曝底片，我才笑出声。"),
			TEXT("询问门廊的奶拍") });
		Result.Add({ FName(TEXT("NaiBi")), TEXT("奶鼻的口供"), FVector(2950.0f, -1480.0f, 90.0f),
			FVector(0.46f, 0.42f, 0.92f), FLinearColor(0.74f, 0.72f, 0.48f), NAME_None,
			true, TEXT("奶鼻：03:00 收摊时我扶正奖杯，留下手印。我说它闻起来像奶，自己笑了。"),
			TEXT("询问井边收摊的奶鼻") });
		Result.Add(M1Points[2]); // Socks / joke
		Result.Add({ FName(TEXT("TrashCan")), TEXT("院角的大垃圾桶"), FVector(-1280.0f, -1430.0f, 50.0f),
			FVector(0.70f, 0.68f, 0.94f), FLinearColor(0.39f, 0.47f, 0.42f), NAME_None,
			false, TEXT("桶盖里卡着一片假花。垃圾桶表示它只收垃圾，不接收口供。"), TEXT("") });
		Result.Add({ FName(TEXT("RoofTiles")), TEXT("戏台屋檐瓦片"), FVector(420.0f, -2270.0f, 90.0f),
			FVector(0.82f, 0.62f, 0.68f), FLinearColor(0.54f, 0.35f, 0.30f), NAME_None,
			false, TEXT("瓦片上贴着一张倒过来的‘禁止大笑’告示，屋顶拒绝作证。"), TEXT("") });
		Result.Add({ FName(TEXT("WellBucket")), TEXT("井边的水桶"), FVector(2520.0f, -2240.0f, 46.0f),
			FVector(0.62f, 0.62f, 0.80f), FLinearColor(0.43f, 0.58f, 0.63f), NAME_None,
			false, TEXT("水桶里浮着一朵假花。它上浮的速度暂时不构成任何证据。"), TEXT("") });
		Result.Add({ FName(TEXT("Clothesline")), TEXT("后院晾衣绳"), FVector(-850.0f, 2220.0f, 112.0f),
			FVector(0.92f, 0.24f, 0.42f), FLinearColor(0.67f, 0.48f, 0.60f), NAME_None,
			false, TEXT("三条围巾的风向各不相同。它们都说自己只是来晾干的。"), TEXT("") });
		return Result;
	}();
	return Points;
}

const TArray<FNaiwaP6ClueDefinition>& FNaiwaP6CaseDefinition::GetM3Clues()
{
	static const TArray<FNaiwaP6ClueDefinition> Clues = []()
	{
		TArray<FNaiwaP6ClueDefinition> Result = GetM2Clues();
		Result.Add({ FName(TEXT("S1")), TEXT("固定时刻视线重建"),
			TEXT("固定时刻视线重建：23:58:07 的巡夜记录显示奶抖不在奖杯旁；示意线表示当时路线与戏台遮挡，不是实时位置或案发录像。"),
			TEXT("23:58:07"), TEXT("视线 / 遮挡") });
		Result.Add({ FName(TEXT("S2")), TEXT("低位撞痕与破损顺序"),
			TEXT("底座约 30 cm 处有外撞缺口；缺口宽度与假花、底座擦痕的破损先后可互相对照，撞痕本身不指认人物。"),
			TEXT("案发后检查"), TEXT("奖杯底座") });
		Result.Add({ FName(TEXT("S3")), TEXT("奖杯短距离滑移轨迹"),
			TEXT("底座从戏台边缘沿地面短距离滑到撞击点；没有抛掷轨迹，最终落点与芯片触发记录可对照。"),
			TEXT("23:57 左右"), TEXT("戏台地面") });
		Result.Add({ FName(TEXT("S4")), TEXT("井边水痕与冲淡脚印"),
			TEXT("水流从戏台方向流向井边，三趾印由清晰转淡。水痕形成于案发之后，时间需与巡夜和收摊记录核对；清理痕迹不等于犯罪。"),
			TEXT("调查期间"), TEXT("地面 / 井边") });
		Result.Add({ FName(TEXT("X1")), TEXT("视线 × 破损"),
			TEXT("采样时没有人在能触发奖杯的位置；低位外撞与现场破损顺序吻合，具体接触来源需结合蹼印与啄痕判断。"),
			TEXT("组合重建"), TEXT("戏台：S1 + S2") });
		Result.Add({ FName(TEXT("X2")), TEXT("滑移 × 结构"),
			TEXT("奖杯落地触发底座芯片；播放循环约三分钟，与检测仪的采样时刻相符。"),
			TEXT("组合重建"), TEXT("奖杯：S3 + G1") });
		return Result;
	}();
	return Clues;
}

const TArray<FNaiwaP6InfoLayerDefinition>& FNaiwaP6CaseDefinition::GetM3InfoLayers()
{
	static const TArray<FNaiwaP6InfoLayerDefinition> Layers = []()
	{
		const FName Trophy(TEXT("Trophy"));
		const auto MakeMark = [](const FVector& OffsetCm, const FVector& MeshScale, const float Yaw = 0.0f)
		{
			FNaiwaP6InfoLayerMark Mark;
			Mark.LocalOffsetCm = OffsetCm;
			Mark.MeshScale = MeshScale;
			Mark.Rotation = FRotator(0.0f, Yaw, 0.0f);
			return Mark;
		};
		TArray<FNaiwaP6InfoLayerDefinition> Result;

		FNaiwaP6InfoLayerDefinition Sight;
		Sight.ClueId = FName(TEXT("S1"));
		Sight.SkinId = FName(TEXT("NaiwaSkin_NaiCha"));
		Sight.PointId = Trophy;
		Sight.Color = FLinearColor(0.82f, 0.92f, 0.78f);
		Sight.Marks = { MakeMark(FVector(-105, 12, 65), FVector(2.0f, 0.055f, 0.035f), 4.0f),
			MakeMark(FVector(-58, 1, 65), FVector(1.35f, 0.045f, 0.03f), 18.0f),
			MakeMark(FVector(-12, -10, 65), FVector(1.2f, 0.045f, 0.03f), -14.0f),
			MakeMark(FVector(-45, 14, 100), FVector(0.08f, 0.10f, 0.72f)) };
		Result.Add(Sight);

		FNaiwaP6InfoLayerDefinition Impact;
		Impact.ClueId = FName(TEXT("S2"));
		Impact.SkinId = FName(TEXT("NaiwaSkin_NaiSha"));
		Impact.PointId = Trophy;
		Impact.Color = FLinearColor(0.96f, 0.69f, 0.31f);
		Impact.Marks = { MakeMark(FVector(-32, 51, -38), FVector(0.55f, 0.055f, 0.055f), -24.0f),
			MakeMark(FVector(26, 45, -40), FVector(0.42f, 0.05f, 0.05f), 17.0f),
			MakeMark(FVector(-6, 40, -45), FVector(0.10f, 0.07f, 0.10f)) };
		Result.Add(Impact);

		FNaiwaP6InfoLayerDefinition Slide;
		Slide.ClueId = FName(TEXT("S3"));
		Slide.SkinId = FName(TEXT("NaiwaSkin_NaiHao"));
		Slide.PointId = Trophy;
		Slide.Color = FLinearColor(0.94f, 0.82f, 0.55f);
		Slide.Marks = { MakeMark(FVector(-185, 43, -52), FVector(0.72f, 0.045f, 0.028f), 6.0f),
			MakeMark(FVector(-117, 43, -52), FVector(0.66f, 0.045f, 0.028f), -8.0f),
			MakeMark(FVector(-53, 42, -52), FVector(0.54f, 0.045f, 0.028f), -10.0f),
			MakeMark(FVector(-12, 42, -52), FVector(0.12f, 0.12f, 0.035f)) };
		Result.Add(Slide);

		FNaiwaP6InfoLayerDefinition Water;
		Water.ClueId = FName(TEXT("S4"));
		Water.SkinId = FName(TEXT("NaiwaSkin_NaiYu"));
		Water.PointId = Trophy;
		Water.Color = FLinearColor(0.42f, 0.82f, 0.88f);
		Water.Marks = {
			MakeMark(FVector(95, -72, -52), FVector(0.80f, 0.14f, 0.022f), -40.0f),
			MakeMark(FVector(154, -116, -52), FVector(0.70f, 0.12f, 0.022f), -38.0f),
			MakeMark(FVector(210, -158, -52), FVector(0.40f, 0.09f, 0.02f), -32.0f),
			MakeMark(FVector(84, -64, -52), FVector(0.09f, 0.075f, 0.018f)),
			MakeMark(FVector(74, -56, -52), FVector(0.04f, 0.06f, 0.014f)),
			MakeMark(FVector(86, -53, -52), FVector(0.04f, 0.06f, 0.014f)),
			MakeMark(FVector(96, -62, -52), FVector(0.04f, 0.06f, 0.014f)),
			MakeMark(FVector(142, -107, -52), FVector(0.06f, 0.05f, 0.016f)),
			MakeMark(FVector(136, -100, -52), FVector(0.03f, 0.045f, 0.012f)),
			MakeMark(FVector(145, -98, -52), FVector(0.03f, 0.045f, 0.012f)),
			MakeMark(FVector(153, -105, -52), FVector(0.03f, 0.045f, 0.012f)) };
		Result.Add(Water);
		return Result;
	}();
	return Layers;
}

const TArray<FNaiwaP6CrossClueDefinition>& FNaiwaP6CaseDefinition::GetM3CrossClues()
{
	static const TArray<FNaiwaP6CrossClueDefinition> CrossClues = {
		{ FName(TEXT("X1")), FName(TEXT("Trophy")), { FName(TEXT("S1")), FName(TEXT("S2")) } },
		{ FName(TEXT("X2")), FName(TEXT("Trophy")), { FName(TEXT("S3")), FName(TEXT("G1")) } }
	};
	return CrossClues;
}

const TArray<FNaiwaP6AnswerOption>& FNaiwaP6CaseDefinition::GetFirstQuestionOptions()
{
	return GetQuestionOptions(0);
}

const FString& FNaiwaP6CaseDefinition::GetFirstQuestionText()
{
	return GetQuestionText(0);
}

int32 FNaiwaP6CaseDefinition::GetQuestionCount()
{
	return GetP6Questions().Num();
}

const FString& FNaiwaP6CaseDefinition::GetQuestionText(const int32 QuestionIndex)
{
	static const FString EmptyQuestion;
	const TArray<FNaiwaP6QuestionRow>& Questions = GetP6Questions();
	return Questions.IsValidIndex(QuestionIndex) ? Questions[QuestionIndex].Prompt : EmptyQuestion;
}

const TArray<FNaiwaP6AnswerOption>& FNaiwaP6CaseDefinition::GetQuestionOptions(const int32 QuestionIndex)
{
	static const TArray<FNaiwaP6AnswerOption> EmptyOptions;
	const TArray<FNaiwaP6QuestionRow>& Questions = GetP6Questions();
	return Questions.IsValidIndex(QuestionIndex) ? Questions[QuestionIndex].Options : EmptyOptions;
}

const FString& FNaiwaP6CaseDefinition::GetConclusionRevealText()
{
	static const FString RevealText = TEXT("奶鸭撞倒了感应奖杯。奖杯里的播放芯片循环播放“哈哈哈”，检测仪将录音误判为现场笑声。三名村民都在别的时间笑过，没人因这次警报说谎。");
	return RevealText;
}

const TArray<FName>& FNaiwaP6CaseDefinition::GetQuestionRequiredClueIds(const int32 QuestionIndex)
{
	static const TArray<FName> EmptyClueIds;
	const TArray<FNaiwaP6QuestionRow>& Questions = GetP6Questions();
	return Questions.IsValidIndex(QuestionIndex) ? Questions[QuestionIndex].RequiredClueIds : EmptyClueIds;
}

int32 FNaiwaP6CaseDefinition::ScoreAnswers(const TArray<FName>& AnswerIds)
{
	const TArray<FNaiwaP6QuestionRow>& Questions = GetP6Questions();
	if (AnswerIds.Num() != Questions.Num())
	{
		return INDEX_NONE;
	}

	int32 CorrectAnswerCount = 0;
	for (int32 Index = 0; Index < Questions.Num(); ++Index)
	{
		const FName AnswerId = AnswerIds[Index];
		if (!Questions[Index].Options.ContainsByPredicate(
			[AnswerId](const FNaiwaP6AnswerOption& Option) { return Option.AnswerId == AnswerId; }))
		{
			return INDEX_NONE;
		}
		if (AnswerIds[Index] == Questions[Index].CorrectAnswerId)
		{
			++CorrectAnswerCount;
		}
	}
	return CorrectAnswerCount;
}

const FNaiwaP6ClueDefinition* FNaiwaP6CaseDefinition::FindClue(const FName ClueId)
{
	return GetM3Clues().FindByPredicate([ClueId](const FNaiwaP6ClueDefinition& Row)
		{ return Row.ClueId == ClueId; });
}

const FNaiwaP6InteractableDefinition* FNaiwaP6CaseDefinition::FindInteractable(const FName PointId)
{
	return GetM2Interactables().FindByPredicate([PointId](const FNaiwaP6InteractableDefinition& Row)
		{ return Row.PointId == PointId; });
}

const FNaiwaP6InfoLayerDefinition* FNaiwaP6CaseDefinition::FindInfoLayer(const FName ClueId)
{
	return GetM3InfoLayers().FindByPredicate([ClueId](const FNaiwaP6InfoLayerDefinition& Row)
		{ return Row.ClueId == ClueId; });
}

const FNaiwaP6InfoLayerDefinition* FNaiwaP6CaseDefinition::FindInfoLayer(
	const FName PointId, const FName SkinId)
{
	return GetM3InfoLayers().FindByPredicate([PointId, SkinId](const FNaiwaP6InfoLayerDefinition& Row)
		{ return Row.PointId == PointId && Row.SkinId == SkinId; });
}
