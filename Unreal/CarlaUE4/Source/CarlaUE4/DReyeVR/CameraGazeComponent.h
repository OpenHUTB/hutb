#pragma once

#include "Components/PrimitiveComponent.h"
#include "CameraGazeComponent.generated.h"

// Display-only primitive. Camera attachment lets the XR late-update system
// move its persistent scene proxy along with the headset.
UCLASS()
class CARLAUE4_API UCameraGazeComponent : public UPrimitiveComponent
{
    GENERATED_BODY()

  public:
    UCameraGazeComponent();
    void SetSample(const FVector &Origin, const FVector &Direction, bool bValid);
    void Configure(float TraceLengthCm, AActor *VehicleToIgnore);
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction *ThisTickFunction) override;
    virtual FPrimitiveSceneProxy *CreateSceneProxy() override;
    virtual FBoxSphereBounds CalcBounds(const FTransform &LocalToWorld) const override;
    virtual void SendRenderDynamicData_Concurrent() override;

    static FVector LocalStart() { return FVector(30.f, 0.f, 0.f); }

  private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FCameraGazeAttachmentTest;
#endif
    void Publish(const FVector &End, bool bDraw);
    bool HasFreshSample() const;
    TWeakObjectPtr<AActor> IgnoredVehicle;
    FVector SampleOrigin = FVector::ZeroVector;
    FVector SampleDirection = FVector::ForwardVector;
    FVector LocalEnd = LocalStart();
    float MaxTraceLengthCm = 10000.f;
    uint64 SampleFrame = 0;
    bool bSampleValid = false;
    bool bDrawGaze = false;
};
