#include "NaiwaP2Probe.h"

#include "NaiwaP2Ballistics.h"
#include "NaiwaP2SceneLibrary.h"
#include "Dom/JsonObject.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace NaiwaP2
{
    namespace
    {
        const FString ExpectedBallisticsFixtureHash = TEXT("4a30e8392f1d7734382b2242e02f2480ed83200cf9b61c39c9a5d83f2cb4bffe");
        const FString ExpectedDesignFixtureHash = TEXT("226df1f4588255893fa4a045cc1474445b0b634cce88033de5afa779738bda84");
        const FString ExpectedDesignRulesetVersion = TEXT("2026-10-19-click-v2");
        const FString ExpectedDesignRuleHash = TEXT("6a920a72481669da6018b7de7d674bd27b840a55c92d56bf5cc8219927fd2538");

        TSharedPtr<FJsonObject> MakeCheck(const FString& Id, const bool bPass,
                                          const TSharedRef<FJsonValue>& Actual,
                                          const TSharedRef<FJsonValue>& Expected,
                                          const double Tolerance)
        {
            TSharedPtr<FJsonObject> Check = MakeShared<FJsonObject>();
            Check->SetStringField(TEXT("id"), Id);
            Check->SetBoolField(TEXT("pass"), bPass);
            Check->SetField(TEXT("actual"), Actual);
            Check->SetField(TEXT("expected"), Expected);
            Check->SetNumberField(TEXT("tol"), Tolerance);
            return Check;
        }

        void AddCheck(TArray<TSharedPtr<FJsonValue>>& Checks,
                      const TSharedPtr<FJsonObject>& Check,
                      FString& FirstFail)
        {
            if (!Check->GetBoolField(TEXT("pass")) && FirstFail.IsEmpty())
            {
                FirstFail = Check->GetStringField(TEXT("id"));
            }
            Checks.Add(MakeShared<FJsonValueObject>(Check));
        }

        TSharedRef<FJsonValue> NumberValue(const double Value)
        {
            return MakeShared<FJsonValueNumber>(Value);
        }

        TSharedRef<FJsonValue> StringArrayValue(const TArray<FString>& Values)
        {
            TArray<TSharedPtr<FJsonValue>> JsonValues;
            for (const FString& Value : Values)
            {
                JsonValues.Add(MakeShared<FJsonValueString>(Value));
            }
            return MakeShared<FJsonValueArray>(MoveTemp(JsonValues));
        }

    }

    bool FProbeTraj::Run(const FString& BallisticsFixturePath,
                         const FString& DesignFixturePath,
                         const FString& BallisticsFixtureHash,
                         const FString& DesignFixtureHash,
                         const FString& ReportPath)
    {
        TArray<uint8> FixtureBytes;
        TArray<uint8> DesignFixtureBytes;
        FString FixtureText;
        FString DesignFixtureText;
        FString FirstFail;
        TArray<TSharedPtr<FJsonValue>> Checks;

        const bool bFixtureLoaded = FFileHelper::LoadFileToArray(FixtureBytes, *BallisticsFixturePath) &&
                                    FFileHelper::LoadFileToString(FixtureText, *BallisticsFixturePath);
        const bool bDesignLoaded = FFileHelper::LoadFileToArray(DesignFixtureBytes, *DesignFixturePath) &&
                                   FFileHelper::LoadFileToString(DesignFixtureText, *DesignFixturePath);

        TSharedPtr<FJsonObject> Fixture;
        if (bFixtureLoaded)
        {
            const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FixtureText);
            FJsonSerializer::Deserialize(Reader, Fixture);
        }
        TSharedPtr<FJsonObject> DesignFixture;
        if (bDesignLoaded)
        {
            const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(DesignFixtureText);
            FJsonSerializer::Deserialize(Reader, DesignFixture);
        }

        const bool bFixtureValid = Fixture.IsValid() &&
                                   Fixture->GetStringField(TEXT("schema")) == TEXT("NaiwaP2BallisticsFixtureV1") &&
                                   Fixture->HasField(TEXT("vectors"));
        if (!bFixtureLoaded || !bDesignLoaded || !bFixtureValid)
        {
            FirstFail = !bFixtureLoaded ? TEXT("fixture_load") :
                        (!bDesignLoaded ? TEXT("design_fixture_load") : TEXT("fixture_schema"));
        }
        const bool bFixtureHashesValid = BallisticsFixtureHash.Equals(ExpectedBallisticsFixtureHash, ESearchCase::IgnoreCase) &&
                                         DesignFixtureHash.Equals(ExpectedDesignFixtureHash, ESearchCase::IgnoreCase);
        AddCheck(Checks, MakeCheck(TEXT("fixture.sha256_baselines"), bFixtureHashesValid,
                                   MakeShared<FJsonValueString>(BallisticsFixtureHash + TEXT(";") + DesignFixtureHash),
                                   MakeShared<FJsonValueString>(ExpectedBallisticsFixtureHash + TEXT(";") + ExpectedDesignFixtureHash), 0.0), FirstFail);
        const FString DesignVersion = DesignFixture.IsValid() ? DesignFixture->GetStringField(TEXT("version")) : TEXT("missing");
        const FString DesignRuleHash = DesignFixture.IsValid() ? DesignFixture->GetStringField(TEXT("rule_hash")) : TEXT("missing");
        const bool bDesignFixtureBaselineValid = DesignFixture.IsValid() &&
                                                 DesignVersion == ExpectedDesignRulesetVersion &&
                                                 DesignRuleHash.Equals(ExpectedDesignRuleHash, ESearchCase::IgnoreCase) &&
                                                 DesignFixtureHash.Equals(ExpectedDesignFixtureHash, ESearchCase::IgnoreCase);
        AddCheck(Checks, MakeCheck(TEXT("design_fixture.baseline"), bDesignFixtureBaselineValid,
                                   MakeShared<FJsonValueString>(DesignVersion + TEXT(";") + DesignRuleHash + TEXT(";") + DesignFixtureHash),
                                   MakeShared<FJsonValueString>(ExpectedDesignRulesetVersion + TEXT(";") + ExpectedDesignRuleHash + TEXT(";") + ExpectedDesignFixtureHash), 0.0), FirstFail);

        if (bFixtureValid)
        {
            const TSharedPtr<FJsonObject> Tolerances = Fixture->GetObjectField(TEXT("tolerances"));
            const double PerStepTolerance = Tolerances->GetNumberField(TEXT("per_step"));
            const double LandingTolerance = Tolerances->GetNumberField(TEXT("landing_x_m"));
            const double TimeTolerance = Tolerances->GetNumberField(TEXT("flight_time_s"));
            const TArray<TSharedPtr<FJsonValue>>& Vectors = Fixture->GetArrayField(TEXT("vectors"));

            AddCheck(Checks, MakeCheck(TEXT("fixture.vector_count"), Vectors.Num() == 15,
                                       NumberValue(Vectors.Num()), NumberValue(15), 0.0), FirstFail);

            const double FixtureDt = Fixture->GetNumberField(TEXT("dt_s"));
            const double FixtureGravity = Fixture->GetNumberField(TEXT("gravity_m_s2"));
            const double FixtureSpeed = Fixture->GetNumberField(TEXT("launch_speed_m_s"));
            const double FixturePouchZ = Fixture->GetNumberField(TEXT("pouch_z_m"));
            const double FixtureGroundZ = Fixture->GetNumberField(TEXT("ground_center_z_m"));

            AddCheck(Checks, MakeCheck(TEXT("constants.dt_s"), FMath::IsNearlyEqual(FixtureDt, FixedStepSeconds, 1.0e-12),
                                       NumberValue(FixedStepSeconds), NumberValue(FixtureDt), 1.0e-12), FirstFail);
            AddCheck(Checks, MakeCheck(TEXT("constants.gravity_m_s2"), FMath::IsNearlyEqual(FixtureGravity, GravityMPerS2, 1.0e-12),
                                       NumberValue(GravityMPerS2), NumberValue(FixtureGravity), 1.0e-12), FirstFail);
            AddCheck(Checks, MakeCheck(TEXT("constants.launch_speed_m_s"), FMath::IsNearlyEqual(FixtureSpeed, LaunchSpeedMPerS, 1.0e-12),
                                       NumberValue(LaunchSpeedMPerS), NumberValue(FixtureSpeed), 1.0e-12), FirstFail);
            AddCheck(Checks, MakeCheck(TEXT("constants.pouch_z_m"), FMath::IsNearlyEqual(FixturePouchZ, PouchHeightM, 1.0e-12),
                                       NumberValue(PouchHeightM), NumberValue(FixturePouchZ), 1.0e-12), FirstFail);
            AddCheck(Checks, MakeCheck(TEXT("constants.ground_center_z_m"), FMath::IsNearlyEqual(FixtureGroundZ, GroundCenterZM, 1.0e-12),
                                       NumberValue(GroundCenterZM), NumberValue(FixtureGroundZ), 1.0e-12), FirstFail);

            for (const TSharedPtr<FJsonValue>& VectorValue : Vectors)
            {
                const TSharedPtr<FJsonObject> Vector = VectorValue->AsObject();
                if (!Vector.IsValid())
                {
                    AddCheck(Checks, MakeCheck(TEXT("vector.invalid"), false,
                                               MakeShared<FJsonValueString>(TEXT("invalid vector object")),
                                               MakeShared<FJsonValueString>(TEXT("object")), 0.0), FirstFail);
                    continue;
                }

                const FString Id = Vector->GetStringField(TEXT("id"));
                const double Angle = Vector->GetNumberField(TEXT("angle_deg"));
                const double Power = Vector->GetNumberField(TEXT("power"));
                const FEggSpec BaseEgg = GetEggSpec(EEggKind::Base);
                const FShotState Launch = FBallistics::MakeLaunchState(Angle, Power, BaseEgg);
                const FTrajectoryResult Actual = FBallistics::TraceEmpty(Launch, FixedStepSeconds,
                                                                         GravityMPerS2, GroundCenterZM, 10000);
                const TArray<TSharedPtr<FJsonValue>>& ExpectedStates = Vector->GetArrayField(TEXT("states"));

                bool bPerStepPass = Actual.bCompleted && Actual.States.Num() == ExpectedStates.Num();
                double MaxAbsError = 0.0;
                FString FirstMismatch;
                const int32 SharedStateCount = FMath::Min(Actual.States.Num(), ExpectedStates.Num());
                for (int32 Index = 0; Index < SharedStateCount; ++Index)
                {
                    const FTrajectoryState& ActualState = Actual.States[Index];
                    const TSharedPtr<FJsonObject> ExpectedState = ExpectedStates[Index]->AsObject();
                    if (!ExpectedState.IsValid())
                    {
                        bPerStepPass = false;
                        FirstMismatch = FString::Printf(TEXT("tick=%d invalid expected state"), Index);
                        break;
                    }

                    const double ExpectedValues[] = {
                        ExpectedState->GetNumberField(TEXT("x_m")),
                        ExpectedState->GetNumberField(TEXT("z_m")),
                        ExpectedState->GetNumberField(TEXT("vx_m_s")),
                        ExpectedState->GetNumberField(TEXT("vz_m_s"))
                    };
                    const double ActualValues[] = {ActualState.X, ActualState.Z,
                                                   ActualState.VelocityX, ActualState.VelocityZ};
                    const TCHAR* Names[] = {TEXT("x_m"), TEXT("z_m"), TEXT("vx_m_s"), TEXT("vz_m_s")};
                    const int32 ExpectedTick = static_cast<int32>(ExpectedState->GetNumberField(TEXT("tick")));
                    if (ActualState.Tick != ExpectedTick)
                    {
                        bPerStepPass = false;
                        FirstMismatch = FString::Printf(TEXT("tick index %d actual=%d expected=%d"),
                                                        Index, ActualState.Tick, ExpectedTick);
                        break;
                    }
                    for (int32 Component = 0; Component < UE_ARRAY_COUNT(ActualValues); ++Component)
                    {
                        const double Error = FMath::Abs(ActualValues[Component] - ExpectedValues[Component]);
                        MaxAbsError = FMath::Max(MaxAbsError, Error);
                        if (Error > PerStepTolerance)
                        {
                            bPerStepPass = false;
                            if (FirstMismatch.IsEmpty())
                            {
                                FirstMismatch = FString::Printf(TEXT("tick=%d field=%s actual=%.17g expected=%.17g error=%.3g"),
                                                                Index, Names[Component], ActualValues[Component],
                                                                ExpectedValues[Component], Error);
                            }
                        }
                    }
                }

                TSharedPtr<FJsonObject> PerStepActual = MakeShared<FJsonObject>();
                PerStepActual->SetNumberField(TEXT("state_count"), Actual.States.Num());
                PerStepActual->SetNumberField(TEXT("max_abs_error"), MaxAbsError);
                PerStepActual->SetStringField(TEXT("first_mismatch"), FirstMismatch);
                PerStepActual->SetBoolField(TEXT("completed"), Actual.bCompleted);
                TSharedPtr<FJsonObject> PerStepExpected = MakeShared<FJsonObject>();
                PerStepExpected->SetNumberField(TEXT("state_count"), ExpectedStates.Num());
                PerStepExpected->SetNumberField(TEXT("per_step_tolerance"), PerStepTolerance);
                AddCheck(Checks, MakeCheck(Id + TEXT(".per_step"), bPerStepPass,
                                           MakeShared<FJsonValueObject>(PerStepActual.ToSharedRef()),
                                           MakeShared<FJsonValueObject>(PerStepExpected.ToSharedRef()), PerStepTolerance), FirstFail);

                const FTrajectoryState* FinalState = Actual.GetFinalState();
                const double ExpectedLandingX = Vector->GetNumberField(TEXT("expected_end_x_m"));
                const double ActualLandingX = FinalState ? FinalState->X : TNumericLimits<double>::Lowest();
                AddCheck(Checks, MakeCheck(Id + TEXT(".landing_x"), FinalState &&
                                           FMath::Abs(ActualLandingX - ExpectedLandingX) <= LandingTolerance,
                                           NumberValue(ActualLandingX), NumberValue(ExpectedLandingX), LandingTolerance), FirstFail);

                const double ExpectedTime = Vector->GetNumberField(TEXT("expected_flight_time_s"));
                const double ActualTime = FinalState ? FinalState->Tick * FixedStepSeconds : -1.0;
                AddCheck(Checks, MakeCheck(Id + TEXT(".flight_time"), FinalState &&
                                           FMath::Abs(ActualTime - ExpectedTime) <= TimeTolerance,
                                           NumberValue(ActualTime), NumberValue(ExpectedTime), TimeTolerance), FirstFail);
            }
        }

        TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
        const bool bPass = FirstFail.IsEmpty() && bFixtureHashesValid;
        Report->SetStringField(TEXT("verdict"), bPass ? TEXT("PASS") : TEXT("FAIL"));
        Report->SetStringField(TEXT("ruleset_version"), Fixture.IsValid() ? Fixture->GetStringField(TEXT("ruleset_version")) : TEXT("unknown"));
        Report->SetStringField(TEXT("rule_hash"), Fixture.IsValid() ? Fixture->GetStringField(TEXT("rule_hash")) : TEXT(""));
        Report->SetStringField(TEXT("design_ruleset_version"), DesignVersion);
        Report->SetStringField(TEXT("design_rule_hash"), DesignRuleHash);
        Report->SetStringField(TEXT("design_fixture_hash"), DesignFixtureHash.ToLower());
        Report->SetStringField(TEXT("ballistics_fixture_hash"), BallisticsFixtureHash.ToLower());
        Report->SetStringField(TEXT("fixture_hash_source"), TEXT("PowerShell Get-FileHash SHA256, checked before and after execution"));
        Report->SetStringField(TEXT("engine_build"), FEngineVersion::Current().ToString());
        Report->SetStringField(TEXT("first_fail"), FirstFail);
        Report->SetArrayField(TEXT("checks"), Checks);

        FString Json;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
        FJsonSerializer::Serialize(Report.ToSharedRef(), Writer);
        const FString AbsoluteReportPath = FPaths::ConvertRelativePathToFull(ReportPath);
        const bool bReportWritten = FFileHelper::SaveStringToFile(Json + LINE_TERMINATOR, *AbsoluteReportPath,
                                                                 FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        UE_LOG(LogTemp, Display, TEXT("NaiwaP2 ProbeTraj %s; checks=%d; first_fail=%s; report=%s"),
               bPass && bReportWritten ? TEXT("PASS") : TEXT("FAIL"), Checks.Num(), *FirstFail,
               *AbsoluteReportPath);
        return bPass && bReportWritten;
    }

    bool FProbeShot::Run(const FString& DesignFixturePath,
                         const FString& DesignFixtureHash,
                         const FString& ReportPath)
    {
        TArray<uint8> FixtureBytes;
        FString FixtureText;
        FString FirstFail;
        TArray<TSharedPtr<FJsonValue>> Checks;
        const bool bFixtureLoaded = FFileHelper::LoadFileToArray(FixtureBytes, *DesignFixturePath) &&
                                    FFileHelper::LoadFileToString(FixtureText, *DesignFixturePath);

        TSharedPtr<FJsonObject> Fixture;
        if (bFixtureLoaded)
        {
            const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FixtureText);
            FJsonSerializer::Deserialize(Reader, Fixture);
        }
        const FString RulesetVersion = Fixture.IsValid() ? Fixture->GetStringField(TEXT("version")) : TEXT("missing");
        const FString RuleHash = Fixture.IsValid() ? Fixture->GetStringField(TEXT("rule_hash")) : TEXT("missing");
        const bool bBaselineValid = bFixtureLoaded && Fixture.IsValid() &&
                                    RulesetVersion == ExpectedDesignRulesetVersion &&
                                    RuleHash.Equals(ExpectedDesignRuleHash, ESearchCase::IgnoreCase) &&
                                    DesignFixtureHash.Equals(ExpectedDesignFixtureHash, ESearchCase::IgnoreCase);
        if (!bFixtureLoaded)
        {
            FirstFail = TEXT("design_fixture_load");
        }
        const FString ActualBaseline = RulesetVersion + TEXT(";") + RuleHash + TEXT(";") + DesignFixtureHash;
        const FString ExpectedBaseline = ExpectedDesignRulesetVersion + TEXT(";") + ExpectedDesignRuleHash + TEXT(";") + ExpectedDesignFixtureHash;
        AddCheck(Checks, MakeCheck(TEXT("design_fixture.baseline"), bBaselineValid,
                                   MakeShared<FJsonValueString>(ActualBaseline),
                                   MakeShared<FJsonValueString>(ExpectedBaseline), 0.0), FirstFail);

        if (bBaselineValid)
        {
            const TSharedPtr<FJsonObject> Levels = Fixture->GetObjectField(TEXT("levels"));
            const TSharedPtr<FJsonObject> Level21 = Levels->GetObjectField(TEXT("2-1"));
            const TArray<TSharedPtr<FJsonValue>>& Chains = Level21->GetArrayField(TEXT("chains"));
            const TSharedPtr<FJsonObject> Chain = Chains.IsEmpty() ? nullptr : Chains[0]->AsObject();
            const TArray<TSharedPtr<FJsonValue>>* ExpectedShots = Chain.IsValid()
                ? &Chain->GetArrayField(TEXT("shots")) : nullptr;
            AddCheck(Checks, MakeCheck(TEXT("2-1.standard_shot_count"), ExpectedShots && ExpectedShots->Num() == 2,
                                       NumberValue(ExpectedShots ? ExpectedShots->Num() : 0), NumberValue(2), 0.0), FirstFail);

            const FScene Scene = FSceneLibrary::Make21();
            for (int32 Index = 0; ExpectedShots && Index < ExpectedShots->Num(); ++Index)
            {
                const TSharedPtr<FJsonObject> Shot = (*ExpectedShots)[Index]->AsObject();
                if (!Shot.IsValid() || Shot->GetStringField(TEXT("egg")) != TEXT("base"))
                {
                    AddCheck(Checks, MakeCheck(FString::Printf(TEXT("2-1.shot_%d.base_egg"), Index + 1), false,
                                               MakeShared<FJsonValueString>(TEXT("invalid or non-base fixture shot")),
                                               MakeShared<FJsonValueString>(TEXT("base")), 0.0), FirstFail);
                    continue;
                }

                const double Angle = Shot->GetNumberField(TEXT("ang"));
                const double Power = Shot->GetNumberField(TEXT("pow"));
                const FShotResult Actual = FBallistics::Shoot(Scene, Angle, Power, GetEggSpec(EEggKind::Base));
                TArray<FString> ActualHits;
                for (const FShotHit& Hit : Actual.Hits)
                {
                    ActualHits.Add(Hit.BoxId);
                }

                TArray<FString> ExpectedHits;
                for (const TSharedPtr<FJsonValue>& Value : Shot->GetArrayField(TEXT("hits")))
                {
                    ExpectedHits.Add(Value->AsString());
                }
                const FString ShotId = FString::Printf(TEXT("2-1.standard_shot_%d"), Index + 1);
                AddCheck(Checks, MakeCheck(ShotId + TEXT(".raw_hits"), Actual.bCompleted && ActualHits == ExpectedHits,
                                           StringArrayValue(ActualHits), StringArrayValue(ExpectedHits), 0.0), FirstFail);

                TArray<FString> ExpectedKilled;
                for (const TSharedPtr<FJsonValue>& Value : Shot->GetArrayField(TEXT("raw_killed")))
                {
                    ExpectedKilled.Add(Value->AsString());
                }
                AddCheck(Checks, MakeCheck(ShotId + TEXT(".raw_killed"), Actual.bCompleted && Actual.RawKilled == ExpectedKilled,
                                           StringArrayValue(Actual.RawKilled), StringArrayValue(ExpectedKilled), 0.0), FirstFail);
            }

            const TSharedPtr<FJsonObject> Level22 = Levels->GetObjectField(TEXT("2-2"));
            const TArray<TSharedPtr<FJsonValue>>& Chain22 = Level22->GetArrayField(TEXT("chains"));
            const TSharedPtr<FJsonObject> Standard22 = Chain22.IsEmpty() ? nullptr : Chain22[0]->AsObject();
            const TArray<TSharedPtr<FJsonValue>>* ExpectedShots22 = Standard22
                ? &Standard22->GetArrayField(TEXT("shots")) : nullptr;
            AddCheck(Checks, MakeCheck(TEXT("2-2.standard_shot_count"),
                                       ExpectedShots22 && ExpectedShots22->Num() == 3,
                                       NumberValue(ExpectedShots22 ? ExpectedShots22->Num() : 0),
                                       NumberValue(3), 0.0), FirstFail);

            FScene Scene22 = FSceneLibrary::Make22();
            const TArray<EEggKind> ExpectedEggs22 = {EEggKind::Mouse, EEggKind::Chick, EEggKind::Tea};
            for (int32 Index = 0; ExpectedShots22 && Index < ExpectedShots22->Num(); ++Index)
            {
                const TSharedPtr<FJsonObject> Shot = (*ExpectedShots22)[Index]->AsObject();
                if (!Shot.IsValid() || Index >= ExpectedEggs22.Num())
                {
                    AddCheck(Checks, MakeCheck(FString::Printf(TEXT("2-2.standard_shot_%d.fixture"), Index + 1),
                                               false, MakeShared<FJsonValueString>(TEXT("invalid fixture shot")),
                                               MakeShared<FJsonValueString>(TEXT("valid shot")), 0.0), FirstFail);
                    continue;
                }

                const FString ShotId = FString::Printf(TEXT("2-2.standard_shot_%d"), Index + 1);
                const FString ExpectedEggName = Shot->GetStringField(TEXT("egg"));
                const EEggKind EggKind = ExpectedEggs22[Index];
                const bool bEggMatches = ExpectedEggName ==
                    (EggKind == EEggKind::Mouse ? TEXT("mouse") :
                     EggKind == EEggKind::Chick ? TEXT("chick") : TEXT("tea"));
                AddCheck(Checks, MakeCheck(ShotId + TEXT(".egg"), bEggMatches,
                    MakeShared<FJsonValueString>(ExpectedEggName),
                    MakeShared<FJsonValueString>(Index == 0 ? TEXT("mouse") : Index == 1 ? TEXT("chick") : TEXT("tea")), 0.0), FirstFail);

                const double Angle = Shot->GetNumberField(TEXT("ang"));
                const double Power = Shot->GetNumberField(TEXT("pow"));
                const int32 ClickTick = Shot->HasTypedField<EJson::Number>(TEXT("click_tick"))
                    ? static_cast<int32>(Shot->GetNumberField(TEXT("click_tick"))) : INDEX_NONE;
                const FShotResult Actual = FBallistics::Shoot(
                    Scene22, Angle, Power, GetEggSpec(EggKind), 10000, ClickTick);
                TArray<FString> ActualHits;
                for (const FShotHit& Hit : Actual.Hits)
                {
                    ActualHits.Add(Hit.BoxId);
                }
                TArray<FString> ExpectedHits22;
                for (const TSharedPtr<FJsonValue>& Value : Shot->GetArrayField(TEXT("hits")))
                {
                    ExpectedHits22.Add(Value->AsString());
                }
                AddCheck(Checks, MakeCheck(ShotId + TEXT(".raw_hits"),
                    Actual.bCompleted && ActualHits == ExpectedHits22,
                    StringArrayValue(ActualHits), StringArrayValue(ExpectedHits22), 0.0), FirstFail);

                TArray<FString> ExpectedKilled22;
                for (const TSharedPtr<FJsonValue>& Value : Shot->GetArrayField(TEXT("raw_killed")))
                {
                    ExpectedKilled22.Add(Value->AsString());
                }
                AddCheck(Checks, MakeCheck(ShotId + TEXT(".raw_killed"),
                    Actual.bCompleted && Actual.RawKilled == ExpectedKilled22,
                    StringArrayValue(Actual.RawKilled), StringArrayValue(ExpectedKilled22), 0.0), FirstFail);
                if (Index < 2)
                {
                    AddCheck(Checks, MakeCheck(ShotId + TEXT(".skill_tick"),
                        Actual.bSkillTriggered && Actual.SkillTick == ClickTick,
                        NumberValue(Actual.SkillTick), NumberValue(ClickTick), 0.0), FirstFail);
                }
                for (const FString& GoalId : Actual.RawKilled)
                {
                    Scene22.Boxes.RemoveAll([&GoalId](const FBox2& Box)
                    {
                        return Box.bGoal && Box.Id == GoalId;
                    });
                    Scene22.GoalIds.Remove(GoalId);
                }
            }

            const FShotResult MouseNoClick = FBallistics::Shoot(
                FSceneLibrary::Make22(), 14.0, 0.90, GetEggSpec(EEggKind::Mouse));
            AddCheck(Checks, MakeCheck(TEXT("2-2.mouse.no_click_does_not_split"),
                !MouseNoClick.bSkillTriggered && MouseNoClick.SplitPaths.IsEmpty(),
                NumberValue(MouseNoClick.SplitPaths.Num()), NumberValue(0), 0.0), FirstFail);
            const FShotResult ChickNoClick = FBallistics::Shoot(
                FSceneLibrary::Make22(), 74.0, 0.94, GetEggSpec(EEggKind::Chick));
            AddCheck(Checks, MakeCheck(TEXT("2-2.chick.no_click_does_not_boost"),
                !ChickNoClick.bSkillTriggered,
                NumberValue(ChickNoClick.bSkillTriggered ? 1 : 0), NumberValue(0), 0.0), FirstFail);

            const FShotResult MouseLateClick = FBallistics::Shoot(
                FSceneLibrary::Make22(), 14.0, 0.90, GetEggSpec(EEggKind::Mouse), 10000, 10000);
            AddCheck(Checks, MakeCheck(TEXT("2-2.mouse.late_click_does_not_split"),
                MouseLateClick.bCompleted && !MouseLateClick.bSkillTriggered && MouseLateClick.SplitPaths.IsEmpty(),
                NumberValue(MouseLateClick.SplitPaths.Num()), NumberValue(0), 0.0), FirstFail);

            const FShotResult ChickLateClick = FBallistics::Shoot(
                FSceneLibrary::Make22(), 74.0, 0.94, GetEggSpec(EEggKind::Chick), 10000, 10000);
            AddCheck(Checks, MakeCheck(TEXT("2-2.chick.late_click_does_not_boost"),
                ChickLateClick.bCompleted && !ChickLateClick.bSkillTriggered,
                NumberValue(ChickLateClick.bSkillTriggered ? 1 : 0), NumberValue(0), 0.0), FirstFail);
        }

        const bool bPass = FirstFail.IsEmpty();
        TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
        Report->SetStringField(TEXT("verdict"), bPass ? TEXT("PASS") : TEXT("FAIL"));
        Report->SetStringField(TEXT("ruleset_version"), RulesetVersion);
        Report->SetStringField(TEXT("rule_hash"), RuleHash);
        Report->SetStringField(TEXT("design_fixture_hash"), DesignFixtureHash.ToLower());
        Report->SetStringField(TEXT("engine_build"), FEngineVersion::Current().ToString());
        Report->SetStringField(TEXT("first_fail"), FirstFail);
        Report->SetArrayField(TEXT("checks"), Checks);

        FString Json;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
        FJsonSerializer::Serialize(Report.ToSharedRef(), Writer);
        const FString AbsoluteReportPath = FPaths::ConvertRelativePathToFull(ReportPath);
        const bool bReportWritten = FFileHelper::SaveStringToFile(Json + LINE_TERMINATOR, *AbsoluteReportPath,
                                                                 FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        UE_LOG(LogTemp, Display, TEXT("NaiwaP2 ProbeShot %s; checks=%d; first_fail=%s; report=%s"),
               bPass && bReportWritten ? TEXT("PASS") : TEXT("FAIL"), Checks.Num(), *FirstFail,
               *AbsoluteReportPath);
        return bPass && bReportWritten;
    }
}
