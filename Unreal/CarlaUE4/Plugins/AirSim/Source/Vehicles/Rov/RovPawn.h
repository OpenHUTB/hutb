#pragma once

#include "GameFramework/RotatingMovementComponent.h"

#include <memory>
#include "PIPCamera.h"
#include "common/common_utils/Signal.hpp"
#include "common/common_utils/UniqueValueMap.hpp"
#include "RovPawnEvents.h"

#include "RovPawn.generated.h"

UCLASS()
class AIRSIM_API ARovPawn : public APawn
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debugging")
    float RotatorFactor = 1.3f;

    ARovPawn();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void NotifyHit(class UPrimitiveComponent* MyComp, class AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation,
                           FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;

    //interface
    void initializeForBeginPlay();
    const common_utils::UniqueValueMap<std::string, APIPCamera*> getCameras() const;
    RovPawnEvents* getPawnEvents()
    {
        return &pawn_events_;
    }
    //called by API to set rotor speed
    void setRotorRenderedStates(const std::vector<RovPawnEvents::RotorTiltableInfo>& rotor_infos);

private: //variables
    UPROPERTY()
    class UStaticMeshComponent* body_mesh_;

    UPROPERTY()
    class UChildActorComponent* camera_front_right_comp_;
    UPROPERTY()
    class UChildActorComponent* camera_front_left_comp_;
    UPROPERTY()
    class UChildActorComponent* camera_front_center_comp_;
    UPROPERTY()
    class UChildActorComponent* camera_back_center_comp_;
    UPROPERTY()
    class UChildActorComponent* camera_bottom_center_comp_;

    //Unreal components
    UPROPERTY()
    APIPCamera* camera_front_left_;
    UPROPERTY()
    APIPCamera* camera_front_right_;
    UPROPERTY()
    APIPCamera* camera_front_center_;
    UPROPERTY()
    APIPCamera* camera_back_center_;
    UPROPERTY()
    APIPCamera* camera_bottom_center_;

    UPROPERTY()
    TArray<URotatingMovementComponent*> rotor_speed_components_;
    UPROPERTY()
    TArray<UStaticMeshComponent*> rotor_angle_components_;

    // TODO: hard-coding this value here since using tri-Rov physics with dual-Rov mesh
    // back rotor is not animated; order of rotors is 0: left, 1: right, 2: back, so just iterate
    // through first two rotors for visualization
    const int num_visible_rotors_ = 8;

    RovPawnEvents pawn_events_;
    int init_id_;
};
