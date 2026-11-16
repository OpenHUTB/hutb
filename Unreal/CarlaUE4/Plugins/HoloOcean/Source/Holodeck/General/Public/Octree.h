// MIT License (c) 2021 BYU FRoStLab see LICENSE file

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManagerGeneric.h"
#include "Containers/Map.h"
#include "Containers/DiscardableKeyValueCache.h"
#include "DrawDebugHelpers.h"
#include "LandscapeProxy.h"

#include "Conversion.h"
#include "gason.h"
#include "jsonbuilder.h"
#include <string>
#include <fstream>
#include <streambuf>
#include <functional>

class Octree
{
	private:
        // Globals used for calculations
        static TArray<FVector> corners;
        static TArray<FVector> sides;
        static FCollisionQueryParams params;
        static float cornerSize;
        static FVector EnvMin;
        static FVector EnvMax;
        static UWorld* World;
        static TDiscardableKeyValueCache<FString,float> materials;

        /** OpenHUTB 追加：为 true 时 initOctree 跳过命令行解析，直接使用 setEnvironmentBounds 设置的值 */
        static bool bExplicitBounds;

        static FVector EnvCenter;

        static void loadJson(gason::JsonValue& json, TArray<Octree*>& parent, float size);
        void toJson(gason::JSonBuilder& doc);

        static FCollisionQueryParams init_params(){
            FCollisionQueryParams p;
            p.bTraceComplex = false;
            p.TraceTag = "";
            // p.bFindInitialOverlaps = true;
            // p.bReturnPhysicalMaterial = true;
            // p.bReturnFaceIndex = true;
            return p;
        }

        static FString getMaterialName(FHitResult hit);
        void fillMaterialProperties(FString mat);

    public:
        static float OctreeRoot;
        static float OctreeMax;
        static float OctreeMin;

        /**
         * 直接指定环境包围盒（UE 厘米单位），跳过命令行参数。
         * 必须在 initOctree 之前调用。
         *
         * OpenHUTB 追加：HoloOcean 原本只支持通过 -EnvMinX= / -EnvMaxX= 等命令行
         * 参数指定环境范围（单位米），与 Carla 集成时没有传参渠道。
         * 提供此接口后，可由 UnderwaterSensorSubsystem 按场景实际包围盒设置。
         */
        static void setEnvironmentBounds(const FVector& InMin, const FVector& InMax);

        Octree(){};
		Octree(FVector loc, float size, FString file="") : size(size), loc(loc), file(file) {};
		~Octree(){ 
            for(Octree* leaf : leaves){
                delete leaf;
            }
            leaves.Reset();
        }

        // Used to setup octree globals
        static void initOctree(UWorld* w);

        // Figures out where octree roots are
        static Octree* makeEnvOctreeRoot();

        // iterative constructs octree
        static Octree* makeOctree(FVector center, float octreeSize, float octreeMin, FString actorName="");

        void unload();
        void load();

        // helpers for saving
        void toJson();
		
        // ignore actors
        static void ignoreActor(const AActor * InIgnoreActor){
            params.AddIgnoredActor(InIgnoreActor);
        }
        static void resetParams(){ params = init_params(); }

        int numLeaves();

        // Used to check if it's a dynamic octree for an agent
        bool isAgent = false;
        
        // Given to all
        float size;
        FVector loc;

        // Given to octree roots that have been saved/loaded from file
        FString file;
        float makeTill;

        // Given to each non-leaf
        TArray<Octree*> leaves;

        // Given to each leaf 
        FVector normal;
        FString material;
        // impedance
        float z = 1.0f;

        // Used during computations
        // Value of Range, Elevation, and Azimuth in that order (in cm/degrees/degrees).
        FVector locSpherical;
        FVector normalImpact;
        // Index of Range, Elevation, and Azimuth in that order.
        FIntVector idx;
        // Holds cos of angle, and value to put in
        float cos;
        float val;
};
