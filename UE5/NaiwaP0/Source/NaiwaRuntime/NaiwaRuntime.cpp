#include "Modules/ModuleManager.h"

#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Parse.h"
#include "NaiwaP2Probe.h"
#include "NaiwaWebNativeDataExport.h"

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
            FString WebP2OutputPath;
            if (FParse::Value(CommandLine, TEXT("NaiwaWebExportP2Scene="), WebP2OutputPath))
            {
                FString SceneSourceHash;
                FString PhysicsProfileHash;
                FString ProjectileDriverHash;
                FString GameModeHash;
                FString ExporterHash;
                FString RuntimeEntryHash;
                const bool bHasHashes =
                    FParse::Value(CommandLine, TEXT("NaiwaWebP2SceneSourceSha256="), SceneSourceHash) &&
                    FParse::Value(CommandLine, TEXT("NaiwaWebP2ProfileSourceSha256="), PhysicsProfileHash) &&
                    FParse::Value(CommandLine, TEXT("NaiwaWebP2ProjectileSourceSha256="), ProjectileDriverHash) &&
                    FParse::Value(CommandLine, TEXT("NaiwaWebP2GameModeSourceSha256="), GameModeHash) &&
                    FParse::Value(CommandLine, TEXT("NaiwaWebExporterSha256="), ExporterHash) &&
                    FParse::Value(CommandLine, TEXT("NaiwaWebRuntimeEntrySha256="), RuntimeEntryHash);
                const bool bExported = bHasHashes && NaiwaWebNative::FP2SceneExport::Run(
                    WebP2OutputPath, SceneSourceHash, PhysicsProfileHash, ProjectileDriverHash, GameModeHash,
                    ExporterHash, RuntimeEntryHash);
                if (!bHasHashes)
                {
                    UE_LOG(LogTemp, Error, TEXT("Naiwa Web P2 export missing source SHA-256 arguments"));
                }
                FPlatformMisc::RequestExitWithStatus(false, bExported ? 0 : (bHasHashes ? 1 : 2),
                    TEXT("Naiwa Web P2 data export completed"));
                return;
            }

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
