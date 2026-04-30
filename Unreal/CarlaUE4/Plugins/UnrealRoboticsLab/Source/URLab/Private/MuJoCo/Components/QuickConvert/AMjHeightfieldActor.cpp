// Copyright (c) 2026 Jonathan Embley-Riches. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// --- LEGAL DISCLAIMER ---
// UnrealRoboticsLab is an independent software plugin. It is NOT affiliated with, 
// endorsed by, or sponsored by Epic Games, Inc. "Unreal" and "Unreal Engine" are 
// trademarks or registered trademarks of Epic Games, Inc. in the US and elsewhere.
//
// This plugin incorporates third-party software: MuJoCo (Apache 2.0), 
// CoACD (MIT), and libzmq (MPL 2.0). See ThirdPartyNotices.txt for details.

#include "MuJoCo/Components/QuickConvert/AMjHeightfieldActor.h"

#include "DrawDebugHelpers.h"
#include "MuJoCo/Components/QuickConvert/MjQuickConvertComponent.h"
#include "MuJoCo/Core/MjArticulation.h"
#include "Utils/URLabLogging.h"

AMjHeightfieldActor::AMjHeightfieldActor()
{
    PrimaryActorTick.bCanEverTick = false;

    // Root bounding box — user scales/moves this to cover the terrain region
    BoundsBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundsBox"));
    BoundsBox->SetBoxExtent(FVector(1000.f, 1000.f, 500.f));
    BoundsBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    BoundsBox->SetCollisionResponseToAllChannels(ECR_Overlap);
    BoundsBox->SetLineThickness(2.0f);
    SetRootComponent(BoundsBox);

    // Line batch for the editor grid visualizer — no collision, no tick
    GridLines = CreateDefaultSubobject<ULineBatchComponent>(TEXT("GridLines"));
    GridLines->SetupAttachment(BoundsBox);
    GridLines->bCalculateAccurateBounds = false;
}

// -----------------------------------------------------------------------------
// Helper: Raycast
// -----------------------------------------------------------------------------

float AMjHeightfieldActor::SampleHeightAt(const FVector2D& WorldXY, const FBox& Bounds) const
{
    const float RayStartZ = Bounds.Max.Z;
    const float RayEndZ   = Bounds.Min.Z;

    // Resolve whitelist once (soft ptrs → raw ptrs)
    TSet<AActor*> WhitelistSet;
    bool bUseWhitelist = false;
    for (const auto& SoftPtr : TraceWhitelist)
    {
        if (AActor* Actor = SoftPtr.Get())
        {
            WhitelistSet.Add(Actor);
            bUseWhitelist = true;
        }
    }

    FCollisionQueryParams CollisionParams;
    CollisionParams.AddIgnoredActor(this); // Always ignore self
    CollisionParams.bTraceComplex = true;  // Trace against actual mesh geometry, not simplified collision

    FHitResult Hit;
    bool bHit = GetWorld()->LineTraceSingleByChannel(
        Hit,
        FVector(WorldXY.X, WorldXY.Y, RayStartZ),
        FVector(WorldXY.X, WorldXY.Y, RayEndZ),
        ElevationTraceChannel,
        CollisionParams
    );

    // Iteratively trace until we hit something valid, or miss entirely
    while (bHit)
    {
        AActor* HitActor = Hit.GetActor();
        if (HitActor)
        {
            bool bIgnore = false;

            if (bUseWhitelist)
            {
                // Whitelist mode: only accept hits on whitelisted actors
                bIgnore = !WhitelistSet.Contains(HitActor);
            }
            else
            {
                // Default mode: ignore MuJoCo actors
                bIgnore = HitActor->FindComponentByClass<UMjQuickConvertComponent>() ||
                          HitActor->IsA<AMjArticulation>() ||
                          HitActor->IsA<AMjHeightfieldActor>();
            }

            if (bIgnore)
            {
                CollisionParams.AddIgnoredActor(HitActor);
                bHit = GetWorld()->LineTraceSingleByChannel(
                    Hit,
                    Hit.ImpactPoint - FVector(0, 0, 1.0f),
                    FVector(WorldXY.X, WorldXY.Y, RayEndZ),
                    ElevationTraceChannel,
                    CollisionParams
                );
                continue;
            }
        }
        // Valid hit!
        break;
    }

    return bHit ? Hit.ImpactPoint.Z : Bounds.Min.Z;
}

// -----------------------------------------------------------------------------
// Editor Visualizer
// -----------------------------------------------------------------------------

void AMjHeightfieldActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RebuildGridVisualizer();
}

void AMjHeightfieldActor::RebuildGridVisualizer()
{
    if (!GridLines)
        return;

    // Clear old lines
    GridLines->Flush();

    if (!bShowGrid || Resolution < 2)
        return;

    if (!GetWorld()) return; // Required for raycasting

    // Safety cap: don't draw an enormous number of lines and tank editor perf
    const int32 DrawResolution = FMath::Min(Resolution, 128);

    // Get world-space bounds from the box component
    FBoxSphereBounds BoxBounds = BoundsBox->CalcBounds(BoundsBox->GetComponentTransform());
    FBox Bounds = BoxBounds.GetBox();

    const float StepX = (Bounds.Max.X - Bounds.Min.X) / FMath::Max(DrawResolution - 1, 1);
    const float StepY = (Bounds.Max.Y - Bounds.Min.Y) / FMath::Max(DrawResolution - 1, 1);

    const FColor LineColorSRGB = GridColor.ToFColorSRGB();

    // 1. Pre-sample the heights into a 2D array
    TArray<FVector> Points;
    Points.SetNum(DrawResolution * DrawResolution);

    for (int32 Row = 0; Row < DrawResolution; ++Row)
    {
        const float Y = Bounds.Min.Y + Row * StepY;
        for (int32 Col = 0; Col < DrawResolution; ++Col)
        {
            const float X = Bounds.Min.X + Col * StepX;
            const float Z = SampleHeightAt(FVector2D(X, Y), Bounds);
            Points[Row * DrawResolution + Col] = FVector(X, Y, Z);
        }
    }

    // 2. Draw lines connecting adjacent points to form a projected grid
    for (int32 Row = 0; Row < DrawResolution; ++Row)
    {
        for (int32 Col = 0; Col < DrawResolution; ++Col)
        {
            const int32 CurrIdx = Row * DrawResolution + Col;
            const FVector& CurrPt = Points[CurrIdx];

            // Draw line to the right neighbour
            if (Col + 1 < DrawResolution)
            {
                const FVector& RightPt = Points[Row * DrawResolution + (Col + 1)];
                GridLines->DrawLine(CurrPt, RightPt, LineColorSRGB, 0, 1.0f, -1.0f);
            }

            // Draw line to the bottom neighbour
            if (Row + 1 < DrawResolution)
            {
                const FVector& DownPt = Points[(Row + 1) * DrawResolution + Col];
                GridLines->DrawLine(CurrPt, DownPt, LineColorSRGB, 0, 1.0f, -1.0f);
            }
        }
    }

    // If the resolution was capped, draw a warning cross in the centre
    if (Resolution > 128)
    {
        float Z = Bounds.Max.Z + 10.f;
        FVector Centre(
            (Bounds.Min.X + Bounds.Max.X) * 0.5f,
            (Bounds.Min.Y + Bounds.Max.Y) * 0.5f,
            Z
        );
        GridLines->DrawLine(Centre + FVector(-200, 0, 0), Centre + FVector(200, 0, 0), FColor::Red, 0, 4.0f, -1.0f);
        GridLines->DrawLine(Centre + FVector(0, -200, 0), Centre + FVector(0, 200, 0), FColor::Red, 0, 4.0f, -1.0f);
    }
}

// -----------------------------------------------------------------------------
// MuJoCo Integration
// -----------------------------------------------------------------------------

void AMjHeightfieldActor::Setup(mjSpec* Spec, mjVFS* /*VFS*/)
{
    if (!Spec || !GetWorld())
    {
        UE_LOG(LogURLabGenerator, Warning, TEXT("[MjHeightfieldActor] Setup called with null Spec or World."));
        return;
    }

    // 1. Derive world-space bounds from the box component
    FBox Bounds = BoundsBox->CalcBounds(BoundsBox->GetComponentTransform()).GetBox();
    const float BoundsWidth  = Bounds.Max.X - Bounds.Min.X;  // UE X (cm)
    const float BoundsHeight = Bounds.Max.Y - Bounds.Min.Y;  // UE Y (cm)

    if (BoundsWidth <= 0.f || BoundsHeight <= 0.f)
    {
        UE_LOG(LogURLabGenerator, Warning, TEXT("[MjHeightfieldActor] '%s': Bounding box has zero or negative size — skipping."), *GetName());
        return;
    }

    const int32 NRows = Resolution;
    const int32 NCols = Resolution;

    // 2. Try loading from cache (skips expensive raycasting)
    TArray<float> NormHeights;
    float MinH = 0.0f;
    float ElevRange = 1.0f;
    FString CacheKey = ComputeCacheKey();
    bool bUsedCache = false;

    if (!bForceRecache)
    {
        FBox CachedBounds;
        if (LoadCache(NormHeights, MinH, ElevRange, CachedBounds, CacheKey))
        {
            bUsedCache = true;
            UE_LOG(LogURLabGenerator, Log, TEXT("[MjHeightfieldActor] '%s': Using cached heightfield (%dx%d)."), *GetName(), NCols, NRows);
        }
    }

    if (!bUsedCache)
    {
        // 3. Raycast NxN grid downward to sample terrain heights
        const float StepX = BoundsWidth  / FMath::Max(NCols - 1, 1);
        const float StepY = BoundsHeight / FMath::Max(NRows - 1, 1);

        TArray<float> RawHeights;
        RawHeights.Reserve(NRows * NCols);

        float MaxH = -FLT_MAX;
        MinH = FLT_MAX;

        for (int32 Row = NRows - 1; Row >= 0; --Row)
        {
            for (int32 Col = 0; Col < NCols; ++Col)
            {
                const float WorldX = Bounds.Min.X + Col * StepX;
                const float WorldY = Bounds.Min.Y + Row * StepY;

                const float H = SampleHeightAt(FVector2D(WorldX, WorldY), Bounds);

                MaxH = FMath::Max(MaxH, H);
                MinH = FMath::Min(MinH, H);
                RawHeights.Add(H);
            }
        }

        UE_LOG(LogURLabGenerator, Log, TEXT("[MjHeightfieldActor] '%s': Sampled %dx%d grid. MinH=%.1f MaxH=%.1f (UE cm)"),
            *GetName(), NCols, NRows, MinH, MaxH);

        ElevRange = FMath::Max(MaxH - MinH, 1.0f);

        NormHeights.Reserve(RawHeights.Num());
        for (float H : RawHeights)
        {
            NormHeights.Add((H - MinH) / ElevRange);
        }

        // Save cache for next session
        SaveCache(NormHeights, MinH, ElevRange, Bounds, CacheKey);
        bForceRecache = false;
    }

    // 5. Register the mjsHField asset in the spec
    mjsHField* HF = mjs_addHField(Spec);
    if (!HF)
    {
        UE_LOG(LogURLabGenerator, Error, TEXT("[MjHeightfieldActor] '%s': mjs_addHField returned null."), *GetName());
        return;
    }

    mjs_setName(HF->element, TCHAR_TO_UTF8(*HFieldName));
    HF->nrow = NRows;
    HF->ncol = NCols;

    // size[4]: { half_x_m, half_y_m, max_elevation_m, base_m }
    // Divide by 200 = cm → m then halve (half-size)
    HF->size[0] = BoundsWidth  / 200.0;
    HF->size[1] = BoundsHeight / 200.0;
    HF->size[2] = ElevRange    / 100.0;                  // Full elevation range in metres
    HF->size[3] = HF->size[2]  * BaseThickness;          // Base thickness

    // Populate userdata with the normalised float elevation values.
    mjs_setFloat(HF->userdata, NormHeights.GetData(), NormHeights.Num());

    UE_LOG(LogURLabGenerator, Log, TEXT("[MjHeightfieldActor] '%s': Registered hfield '%s' nrow=%d ncol=%d size=[%.3f, %.3f, %.3f, %.3f]"),
        *GetName(), *HFieldName, HF->nrow, HF->ncol,
        HF->size[0], HF->size[1], HF->size[2], HF->size[3]);

    // 6. Create a static geom on the world body referencing this heightfield
    mjsBody* WorldBody = mjs_findBody(Spec, "world");
    if (!WorldBody)
    {
        UE_LOG(LogURLabGenerator, Error, TEXT("[MjHeightfieldActor] '%s': Could not find 'world' body in spec."), *GetName());
        return;
    }

    mjsGeom* Geom = mjs_addGeom(WorldBody, nullptr);
    if (!Geom)
    {
        UE_LOG(LogURLabGenerator, Error, TEXT("[MjHeightfieldActor] '%s': mjs_addGeom returned null."), *GetName());
        return;
    }

    // Assign a unique name prefixed with actor name
    FString GeomName = GetName() + TEXT("_") + HFieldName + TEXT("_geom");
    mjs_setName(Geom->element, TCHAR_TO_UTF8(*GeomName));

    Geom->type = mjGEOM_HFIELD;
    mjs_setString(Geom->hfieldname, TCHAR_TO_UTF8(*HFieldName));

    Geom->pos[0] =  (Bounds.Min.X + Bounds.Max.X) / 200.0;
    Geom->pos[1] = -(Bounds.Min.Y + Bounds.Max.Y) / 200.0;
    Geom->pos[2] =   MinH / 100.0;

    UE_LOG(LogURLabGenerator, Log, TEXT("[MjHeightfieldActor] '%s': Placed hfield geom at MuJoCo pos [%.3f, %.3f, %.3f]"),
        *GetName(), Geom->pos[0], Geom->pos[1], Geom->pos[2]);
}

void AMjHeightfieldActor::PostSetup(mjModel* /*Model*/, mjData* /*Data*/)
{
    UE_LOG(LogURLabGenerator, Log, TEXT("[MjHeightfieldActor] '%s': PostSetup complete (static terrain)."), *GetName());
}

// -----------------------------------------------------------------------------
// Heightfield Cache
// -----------------------------------------------------------------------------

FString AMjHeightfieldActor::GetCacheFilePath() const
{
    return FPaths::ConvertRelativePathToFull(
        FString::Printf(TEXT("%s/URLab/HeightfieldCache/%s_%s.hfcache"),
            *FPaths::ProjectSavedDir(), *HFieldName, *GetName()));
}

FString AMjHeightfieldActor::ComputeCacheKey() const
{
    FBoxSphereBounds BoxBounds = BoundsBox->CalcBounds(BoundsBox->GetComponentTransform());
    FBox Bounds = BoxBounds.GetBox();

    return FString::Printf(TEXT("res=%d pos=%.1f,%.1f,%.1f,%.1f,%.1f,%.1f base=%.3f"),
        Resolution,
        Bounds.Min.X, Bounds.Min.Y, Bounds.Min.Z,
        Bounds.Max.X, Bounds.Max.Y, Bounds.Max.Z,
        BaseThickness);
}

bool AMjHeightfieldActor::SaveCache(const TArray<float>& NormHeights, float MinH, float ElevRange,
    const FBox& Bounds, const FString& CacheKey) const
{
    FString Path = GetCacheFilePath();
    FString Dir = FPaths::GetPath(Path);
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    PlatformFile.CreateDirectoryTree(*Dir);

    FBufferArchive Archive;
    // Header
    int32 Version = 1;
    int32 NRows = Resolution;
    int32 NCols = Resolution;
    Archive << Version << NRows << NCols;
    Archive << MinH << ElevRange;

    // Cache key for invalidation
    FString Key = CacheKey;
    Archive << Key;

    // Bounds
    FVector BMin = Bounds.Min;
    FVector BMax = Bounds.Max;
    Archive << BMin << BMax;

    // Height data
    int32 Count = NormHeights.Num();
    Archive << Count;
    Archive.Serialize(const_cast<float*>(NormHeights.GetData()), Count * sizeof(float));

    bool bOk = FFileHelper::SaveArrayToFile(Archive, *Path);
    if (bOk)
    {
        UE_LOG(LogURLabGenerator, Log, TEXT("[MjHeightfieldActor] Saved cache: %s (%d heights)"), *Path, Count);
    }
    return bOk;
}

bool AMjHeightfieldActor::LoadCache(TArray<float>& OutNormHeights, float& OutMinH, float& OutElevRange,
    FBox& OutBounds, const FString& ExpectedCacheKey) const
{
    FString Path = GetCacheFilePath();
    if (!FPaths::FileExists(Path)) return false;

    TArray<uint8> RawData;
    if (!FFileHelper::LoadFileToArray(RawData, *Path)) return false;

    FMemoryReader Archive(RawData, true);

    int32 Version, NRows, NCols;
    Archive << Version << NRows << NCols;
    if (Version != 1 || NRows != Resolution || NCols != Resolution) return false;

    Archive << OutMinH << OutElevRange;

    FString StoredKey;
    Archive << StoredKey;
    if (StoredKey != ExpectedCacheKey) return false;

    FVector BMin, BMax;
    Archive << BMin << BMax;
    OutBounds = FBox(BMin, BMax);

    int32 Count;
    Archive << Count;
    if (Count != NRows * NCols) return false;

    OutNormHeights.SetNumUninitialized(Count);
    Archive.Serialize(OutNormHeights.GetData(), Count * sizeof(float));

    UE_LOG(LogURLabGenerator, Log, TEXT("[MjHeightfieldActor] Loaded cache: %s (%d heights)"), *Path, Count);
    return true;
}
