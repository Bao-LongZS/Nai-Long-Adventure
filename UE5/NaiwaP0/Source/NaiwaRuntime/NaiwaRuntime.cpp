#include "Modules/ModuleManager.h"

#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Parse.h"
#include "NaiwaP2Probe.h"

namespace
{
    class FNaiwaRuntimeModule final : public FDefaultGameModuleImpl
    {
    public:
        virtual void StartupModule() override
        {
            PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(
                this, &FNaiwaRuntimeModule::OnPostEngineInit);
        }

        virtual void ShutdownModule() override
        {
            if (PostEngineInitHandle.IsValid())
            {
                FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
                PostEngineInitHandle.Reset();
            }
        }

    private:
        void OnPostEngineInit()
        {
            const TCHAR* CommandLine = FCommandLine::Get();
            const bool bRunProbeTraj = FParse::Param(CommandLine, TEXT("NaiwaP2ProbeTraj"));
            const bool bRunProbeShot = FParse::Param(CommandLine, TEXT("NaiwaP2ProbeShot"));
            if (!bRunProbeTraj && !bRunProbeShot)
            {
                return;
            }

            FString ReportPath;
            FString DesignFixturePath;
            FString DesignFixtureHash;
            bool bHasPaths = false;
            bool bPass = false;
            if (bRunProbeTraj)
            {
                FString FixturePath;
                FString FixtureHash;
                bHasPaths = FParse::Value(CommandLine, TEXT("NaiwaP2Fixture="), FixturePath) &&
                            FParse::Value(CommandLine, TEXT("NaiwaP2FixtureHash="), FixtureHash) &&
                            FParse::Value(CommandLine, TEXT("NaiwaP2DesignFixture="), DesignFixturePath) &&
                            FParse::Value(CommandLine, TEXT("NaiwaP2DesignFixtureHash="), DesignFixtureHash) &&
                            FParse::Value(CommandLine, TEXT("NaiwaP2Report="), ReportPath);
                bPass = bHasPaths && NaiwaP2::FProbeTraj::Run(FixturePath, DesignFixturePath,
                                                               FixtureHash, DesignFixtureHash, ReportPath);
            }
            else
            {
                bHasPaths = FParse::Value(CommandLine, TEXT("NaiwaP2DesignFixture="), DesignFixturePath) &&
                            FParse::Value(CommandLine, TEXT("NaiwaP2DesignFixtureHash="), DesignFixtureHash) &&
                            FParse::Value(CommandLine, TEXT("NaiwaP2Report="), ReportPath);
                bPass = bHasPaths && NaiwaP2::FProbeShot::Run(DesignFixturePath, DesignFixtureHash, ReportPath);
            }
            if (!bHasPaths)
            {
                UE_LOG(LogTemp, Error, TEXT("NaiwaP2 probe missing fixture/report command-line paths"));
            }
            FPlatformMisc::RequestExitWithStatus(false, bPass ? 0 : (bHasPaths ? 1 : 2), TEXT("NaiwaP2 probe completed"));
        }

        FDelegateHandle PostEngineInitHandle;
    };
}

IMPLEMENT_PRIMARY_GAME_MODULE(FNaiwaRuntimeModule, NaiwaRuntime, "Nai_Long");
