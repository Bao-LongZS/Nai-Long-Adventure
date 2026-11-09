#include "NaiwaWebNativeDataExport.h"

#include "NaiwaP2PhysicsProfile.h"
#include "NaiwaP2SceneLibrary.h"
#include "Dom/JsonObject.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
    using namespace NaiwaP2;
    using namespace NaiwaP2::Physics;

    TArray<TSharedPtr<FJsonValue>> StringArray(const TArray<FString>& Values)
    {
        TArray<TSharedPtr<FJsonValue>> Items;
        Items.Reserve(Values.Num());
        for (const FString& Value : Values)
        {
            Items.Add(MakeShared<FJsonValueString>(Value));
        }
        return Items;
    }

    bool IsSceneryBox(const FString& Id)
    {
        static const TCHAR* const ExplicitScenery[] = {
            TEXT("A.col"), TEXT("A.plate"), TEXT("S.rope1"), TEXT("S.rope2"), TEXT("S.deck")
        };
        for (const TCHAR* Candidate : ExplicitScenery)
        {
            if (Id.Equals(Candidate, ESearchCase::CaseSensitive)) return true;
        }
        return false;
    }

    EBlockRole ClassifyBlockRole(const FBox2& Box)
    {
        if (IsSceneryBox(Box.Id)) return EBlockRole::Foundation;
        const double Width = Box.MaxX - Box.MinX;
        const double Height = Box.MaxZ - Box.MinZ;
        return Width >= Height ? EBlockRole::Board : EBlockRole::Column;
    }

    const TCHAR* RoleLabel(const EBlockRole Role)
    {
        switch (Role)
        {
        case EBlockRole::Foundation: return TEXT("Foundation");
        case EBlockRole::Column: return TEXT("Column");
        case EBlockRole::Board: return TEXT("Board");
        case EBlockRole::Goal: return TEXT("Goal");
        default: return TEXT("Unknown");
        }
    }

    TSharedPtr<FJsonObject> MakeMaterialJson(const FBlockMaterial& Material)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetNumberField(TEXT("densityKgPerM3"), Material.DensityKgPerM3);
        Json->SetNumberField(TEXT("friction"), Material.Friction);
        Json->SetNumberField(TEXT("staticFriction"), Material.StaticFriction);
        Json->SetNumberField(TEXT("restitution"), Material.Restitution);
        Json->SetNumberField(TEXT("linearDamping"), Material.LinearDamping);
        Json->SetNumberField(TEXT("angularDamping"), Material.AngularDamping);
        Json->SetBoolField(TEXT("simulate"), Material.bSimulate);
        Json->SetBoolField(TEXT("useCCD"), Material.bUseCCD);
        return Json;
    }

    TSharedPtr<FJsonObject> MakeProfileJson()
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetNumberField(TEXT("gravityMPerS2"), NaiwaP2::Physics::GravityMPerS2);
        Json->SetNumberField(TEXT("depthMeters"), DepthMeters);
        Json->SetNumberField(TEXT("spawnGapMeters"), SpawnGapMeters);
        Json->SetNumberField(TEXT("minBlockDimensionMeters"), MinBlockDimensionMeters);
        Json->SetNumberField(TEXT("maxSubstepDeltaTime"), MaxSubstepDeltaTime);
        Json->SetNumberField(TEXT("maxSubsteps"), MaxSubsteps);
        Json->SetNumberField(TEXT("maxDepenetrationVelocityCmPerS"), MaxDepenetrationVelocityCmPerS);
        Json->SetNumberField(TEXT("baseEggMassKg"), BaseEggMassKg);
        Json->SetNumberField(TEXT("eggRestitution"), EggRestitution);
        Json->SetNumberField(TEXT("eggFriction"), EggFriction);
        Json->SetNumberField(TEXT("projectileSubstepSeconds"), ProjectileSubstepSeconds);
        Json->SetNumberField(TEXT("projectileMaxSubstepsPerAdvance"), ProjectileMaxSubstepsPerAdvance);
        Json->SetNumberField(TEXT("projectileSettleSpeedCmPerS"), ProjectileSettleSpeedCmPerS);
        Json->SetNumberField(TEXT("projectileSettleSeconds"), ProjectileSettleSeconds);
        Json->SetNumberField(TEXT("projectileMaxFlightSeconds"), ProjectileMaxFlightSeconds);
        Json->SetNumberField(TEXT("projectileGroundCenterZCm"), ProjectileGroundCenterZCm);
        Json->SetNumberField(TEXT("projectileTangentRetention"), ProjectileTangentRetention);
        Json->SetNumberField(TEXT("projectileSpinCoupling"), ProjectileSpinCoupling);
        Json->SetNumberField(TEXT("projectileMaxSpinDegPerS"), ProjectileMaxSpinDegPerS);
        Json->SetNumberField(TEXT("projectileBounceSeparationCm"), ProjectileBounceSeparationCm);
        Json->SetNumberField(TEXT("projectileStopSpeedCmPerS"), ProjectileStopSpeedCmPerS);
        Json->SetNumberField(TEXT("stabilityTestSeconds"), StabilityTestSeconds);
        Json->SetNumberField(TEXT("stabilityMaxDriftCm"), StabilityMaxDriftCm);
        Json->SetBoolField(TEXT("applyWorldGravity"), bApplyWorldGravity);

        TSharedPtr<FJsonObject> Materials = MakeShared<FJsonObject>();
        Materials->SetObjectField(TEXT("Foundation"), MakeMaterialJson(GetBlockMaterial(EBlockRole::Foundation)));
        Materials->SetObjectField(TEXT("Column"), MakeMaterialJson(GetBlockMaterial(EBlockRole::Column)));
        Materials->SetObjectField(TEXT("Board"), MakeMaterialJson(GetBlockMaterial(EBlockRole::Board)));
        Materials->SetObjectField(TEXT("Goal"), MakeMaterialJson(GetBlockMaterial(EBlockRole::Goal)));
        Json->SetObjectField(TEXT("blockMaterials"), Materials);
        return Json;
    }
}

namespace NaiwaWebNative
{
    bool FP2SceneExport::Run(const FString& OutputPath,
                             const FString& SceneSourceHash,
                             const FString& PhysicsProfileHash,
                             const FString& ProjectileDriverHash,
                             const FString& GameModeHash,
                             const FString& ExporterHash,
                             const FString& RuntimeEntryHash)
    {
        const NaiwaP2::FScene Scene = NaiwaP2::FSceneLibrary::Make21();
        const NaiwaP2::FAmmoRule Ammo = NaiwaP2::FSceneLibrary::GetAmmoRule(Scene.LevelId);
        TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetNumberField(TEXT("schemaVersion"), 1);
        Root->SetStringField(TEXT("levelId"), Scene.LevelId);
        Root->SetNumberField(TEXT("stage"), 1);
        Root->SetNumberField(TEXT("maxX"), Scene.MaxX);
        Root->SetStringField(TEXT("coordinateSystem"), TEXT("X/Z side plane; meters; gravity toward -Z"));
        Root->SetStringField(TEXT("geometrySource"), TEXT("NaiwaP2::FSceneLibrary::Make21"));
        Root->SetStringField(TEXT("engineVersion"), FEngineVersion::Current().ToString());

        TArray<TSharedPtr<FJsonValue>> Boxes;
        Boxes.Reserve(Scene.Boxes.Num());
        for (const NaiwaP2::FBox2& Box : Scene.Boxes)
        {
            TSharedPtr<FJsonObject> BoxJson = MakeShared<FJsonObject>();
            BoxJson->SetStringField(TEXT("id"), Box.Id);
            BoxJson->SetNumberField(TEXT("minX"), Box.MinX);
            BoxJson->SetNumberField(TEXT("maxX"), Box.MaxX);
            BoxJson->SetNumberField(TEXT("minZ"), Box.MinZ);
            BoxJson->SetNumberField(TEXT("maxZ"), Box.MaxZ);
            BoxJson->SetNumberField(TEXT("requiredRadius"), Box.RequiredRadius);
            BoxJson->SetBoolField(TEXT("isGoal"), Box.bGoal);
            BoxJson->SetBoolField(TEXT("breakable"), Box.bBreakable);
            BoxJson->SetStringField(TEXT("role"), RoleLabel(ClassifyBlockRole(Box)));
            BoxJson->SetArrayField(TEXT("kills"), StringArray(Box.Kills));
            Boxes.Add(MakeShared<FJsonValueObject>(BoxJson));
        }
        Root->SetArrayField(TEXT("boxes"), Boxes);
        Root->SetArrayField(TEXT("goalIds"), StringArray(Scene.GoalIds));

        TArray<FString> EggNames;
        for (const NaiwaP2::EEggKind Egg : Ammo.EggList)
        {
            EggNames.Add(FString(GetEggKindLabel(Egg)).ToLower());
        }
        TSharedPtr<FJsonObject> AmmoJson = MakeShared<FJsonObject>();
        AmmoJson->SetArrayField(TEXT("eggList"), StringArray(EggNames));
        AmmoJson->SetNumberField(TEXT("totalShots"), Ammo.TotalShots);
        AmmoJson->SetBoolField(TEXT("repeatAllowed"), Ammo.bRepeatAllowed);
        AmmoJson->SetBoolField(TEXT("hasFreeRetry"), Ammo.bHasFreeRetry);
        Root->SetObjectField(TEXT("ammoRule"), AmmoJson);
        Root->SetObjectField(TEXT("physicsProfile"), MakeProfileJson());

        TSharedPtr<FJsonObject> Hashes = MakeShared<FJsonObject>();
        Hashes->SetStringField(TEXT("sceneLibrarySha256"), SceneSourceHash);
        Hashes->SetStringField(TEXT("physicsProfileSha256"), PhysicsProfileHash);
        Hashes->SetStringField(TEXT("projectileDriverSha256"), ProjectileDriverHash);
        Hashes->SetStringField(TEXT("gameModeSha256"), GameModeHash);
        Hashes->SetStringField(TEXT("exporterSha256"), ExporterHash);
        Hashes->SetStringField(TEXT("runtimeEntrySha256"), RuntimeEntryHash);
        Root->SetObjectField(TEXT("sourceHashes"), Hashes);

        FString Json;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
        FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
        const FString AbsolutePath = FPaths::ConvertRelativePathToFull(OutputPath);
        const bool bWritten = FFileHelper::SaveStringToFile(
            Json + LINE_TERMINATOR, *AbsolutePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        UE_LOG(LogTemp, Display, TEXT("Naiwa Web P2 export %s; boxes=%d; output=%s"),
               bWritten ? TEXT("PASS") : TEXT("FAIL"), Scene.Boxes.Num(), *AbsolutePath);
        return bWritten;
    }
}
