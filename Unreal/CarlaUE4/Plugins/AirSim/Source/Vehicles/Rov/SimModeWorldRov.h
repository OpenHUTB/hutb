#pragma once

#include "CoreMinimal.h"

#include "RovPawn.h"
#include "common/Common.hpp"
#include "SimMode/SimModeWorldBase.h"
#include "api/VehicleSimApiBase.hpp"
#include "SimModeWorldRov.generated.h"

UCLASS()
class AIRSIM_API ASimModeWorldRov : public ASimModeWorldBase
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** 是否自动生成障碍物场景（管道 / 框架 / 礁石）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV")
    bool bSpawnObstacleField = true;

    /** 是否启用 ROV 实时状态面板与力/速度向量显示。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV")
    bool bSpawnStatusHUD = true;

protected: //overrides
    virtual APawn* createVehiclePawn(const AirSimSettings::VehicleSetting& vehicle_setting) override;
    virtual void setupClockSpeed() override;
    virtual std::unique_ptr<msr::airlib::ApiServerBase> createApiServer() const override;
    virtual void getExistingVehiclePawns(TArray<AActor*>& pawns) const override;
    virtual bool isVehicleTypeSupported(const std::string& vehicle_type) const override;
    virtual std::string getVehiclePawnPathName(const AirSimSettings::VehicleSetting& vehicle_setting) const override;
    virtual PawnEvents* getVehiclePawnEvents(APawn* pawn) const override;
    virtual const common_utils::UniqueValueMap<std::string, APIPCamera*> getVehiclePawnCameras(APawn* pawn) const override;
    virtual void initializeVehiclePawn(APawn* pawn) override;
    virtual std::unique_ptr<PawnSimApi> createVehicleSimApi(
        const PawnSimApi::Params& pawn_sim_api_params) const override;
    virtual msr::airlib::VehicleApiBase* getVehicleApi(const PawnSimApi::Params& pawn_sim_api_params,
                                                       const PawnSimApi* sim_api) const override;

private:
    typedef ARovPawn TVehiclePawn;
};
