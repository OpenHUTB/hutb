#pragma once

#include "CoreMinimal.h"

/**
 * @struct FMuJoCoDebugData
 * @brief Thread-safe buffer for debug visualization data.
 */
struct FMuJoCoDebugData
{
    TArray<FVector> ContactPoints;
    TArray<FVector> ContactNormals;
    TArray<float> ContactForces;

    /** Per-body awake state snapshot (`mjData.body_awake`), size nbody. 0 = asleep. */
    TArray<int32> BodyAwake;

    /**
     * Halton colour seed per body. `island_dofadr[island]` when in an active
     * constraint island, else `tree_dofadr[tree]` (with `mj_sleepCycle` when
     * asleep). -1 means "don't colour" (e.g. worldbody). Size nbody.
     */
    TArray<int32> BodyIslandSeed;

    /**
     * Flat array of 3D points mirroring MuJoCo's `mjData.wrap_xpos` (stride 3).
     * MuJoCo's own renderer reads point A at offset `3*j` and point B at
     * `3*j + 3`, where j iterates wrap-point index in `[ten_wrapadr[t],
     * ten_wrapadr[t] + ten_wrapnum[t] - 1)`. We convert to UE coords at
     * capture time. Logical size = 2 * nwrap (same as `nwrap x 6` layout).
     */
    TArray<FVector> WrapPointsFlat;

    /** Mirror of `mjData.wrap_obj` (nwrap * 2 ints). -2 = pulley, skip when drawing. */
    TArray<int32> WrapObj;

    /** Per-tendon: `ten_wrapadr` / `ten_wrapnum`, length, and limit range. Sizes = ntendon. */
    TArray<int32> TendonWrapAdr;
    TArray<int32> TendonWrapNum;
    TArray<float> TendonLength;
    TArray<uint8> TendonLimited;
    TArray<float> TendonRangeLo;
    TArray<float> TendonRangeHi;

    /**
     * Per-tendon muscle activation in [0, 1] if any muscle actuator drives this
     * tendon, else -1. Resolved by scanning actuators with trntype==TENDON.
     * Size = ntendon.
     */
    TArray<float> TendonActivation;

    /** Geom world positions (UE coords), size ngeom. Used to centre wrap-arc interpolation. */
    TArray<FVector> GeomXPos;
};
