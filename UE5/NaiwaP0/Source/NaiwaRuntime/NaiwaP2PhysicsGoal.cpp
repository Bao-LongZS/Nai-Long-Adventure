#include "NaiwaP2PhysicsGoal.h"

#include "Components/CapsuleComponent.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"

ANaiwaP2PhysicsGoal::ANaiwaP2PhysicsGoal()
{
    PrimaryActorTick.bCanEverTick = false;

    CollisionCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionBody"));
    SetRootComponent(CollisionCapsule);
    CollisionCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
    VisualMesh->SetupAttachment(CollisionCapsule);
    VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    VisualMesh->SetMobility(EComponentMobility::Movable);

    GeoEggVisualMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeoEggVisualMesh"));
    GeoEggVisualMesh->SetupAttachment(CollisionCapsule);
    GeoEggVisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GeoEggVisualMesh->SetMobility(EComponentMobility::Movable);
    GeoEggVisualMesh->SetVisibility(false, true);
}

void ANaiwaP2PhysicsGoal::InitializeVisual(UStaticMesh* Mesh, UMaterialInterface* Material,
    const FVector& VisualScale, const float CollisionRadiusCm, const float CollisionHalfHeightCm)
{
    CollisionCapsule->SetCapsuleSize(CollisionRadiusCm, CollisionHalfHeightCm, false);
    VisualMesh->SetStaticMesh(Mesh);
    VisualMesh->SetRelativeScale3D(VisualScale);
    if (Material)
    {
        VisualMesh->SetMaterial(0, Material);
    }
}

void ANaiwaP2PhysicsGoal::InitializeGeoEggVisual(UMaterialInterface* Material,
    const FVector& VisualScale)
{
    constexpr int32 LatitudeSegments = 24;
    constexpr int32 LongitudeSegments = 32;
    constexpr float RadiusCm = 50.0f;

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FProcMeshTangent> Tangents;
    const int32 Stride = LongitudeSegments + 1;
    Vertices.Reserve((LatitudeSegments + 1) * Stride);
    UVs.Reserve((LatitudeSegments + 1) * Stride);
    Tangents.Reserve((LatitudeSegments + 1) * Stride);
    Triangles.Reserve(LatitudeSegments * LongitudeSegments * 6);

    // A latitude-varying radius makes the top narrower and the lower half fuller.
    // The source extent is 100 cm, so the caller can size it in meters like the old sphere.
    for (int32 Latitude = 0; Latitude <= LatitudeSegments; ++Latitude)
    {
        const float Theta = PI * static_cast<float>(Latitude) / LatitudeSegments;
        const float CosTheta = FMath::Cos(Theta);
        const float SinTheta = FMath::Sin(Theta);
        const float RingRadius = RadiusCm * SinTheta * (1.0f - 0.18f * CosTheta);
        const float Z = RadiusCm * CosTheta;

        for (int32 Longitude = 0; Longitude <= LongitudeSegments; ++Longitude)
        {
            const float Phi = 2.0f * PI * static_cast<float>(Longitude) / LongitudeSegments;
            const float CosPhi = FMath::Cos(Phi);
            const float SinPhi = FMath::Sin(Phi);
            Vertices.Add(FVector(RingRadius * CosPhi, RingRadius * SinPhi, Z));
            UVs.Add(FVector2D(static_cast<float>(Longitude) / LongitudeSegments,
                static_cast<float>(Latitude) / LatitudeSegments));
            Tangents.Add(FProcMeshTangent(FVector(-SinPhi, CosPhi, 0.0f), false));
        }
    }

    for (int32 Latitude = 0; Latitude < LatitudeSegments; ++Latitude)
    {
        for (int32 Longitude = 0; Longitude < LongitudeSegments; ++Longitude)
        {
            const int32 A = Latitude * Stride + Longitude;
            const int32 B = (Latitude + 1) * Stride + Longitude;
            const int32 C = B + 1;
            const int32 D = A + 1;
            Triangles.Add(A);
            Triangles.Add(B);
            Triangles.Add(C);
            Triangles.Add(A);
            Triangles.Add(C);
            Triangles.Add(D);
        }
    }

    Normals.Init(FVector::ZeroVector, Vertices.Num());
    for (int32 Index = 0; Index < Triangles.Num(); Index += 3)
    {
        const int32 A = Triangles[Index];
        const int32 B = Triangles[Index + 1];
        const int32 C = Triangles[Index + 2];
        const FVector FaceNormal = FVector::CrossProduct(
            Vertices[B] - Vertices[A], Vertices[C] - Vertices[A]);
        Normals[A] += FaceNormal;
        Normals[B] += FaceNormal;
        Normals[C] += FaceNormal;
    }
    for (FVector& Normal : Normals)
    {
        Normal = Normal.GetSafeNormal();
    }

    GeoEggVisualMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs,
        TArray<FColor>(), Tangents, false);
    if (Material)
    {
        GeoEggVisualMesh->SetMaterial(0, Material);
    }
    GeoEggVisualMesh->SetRelativeScale3D(FVector(VisualScale.X * 0.90, VisualScale.Y, VisualScale.Z));
    GeoEggVisualMesh->SetVisibility(true, true);
    VisualMesh->SetVisibility(false, true);
}
