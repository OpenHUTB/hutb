#include "CameraGazeComponent.h"
#include "Engine/World.h"
#include "EngineGlobals.h"
#include "PrimitiveSceneProxy.h"
#include "SceneManagement.h"
#include "RenderingThread.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Camera/CameraComponent.h"
#include "Misc/AutomationTest.h"
#endif

namespace
{
class FCameraGazeSceneProxy final : public FPrimitiveSceneProxy
{
  public:
    FCameraGazeSceneProxy(const UCameraGazeComponent *Component, const FVector &End, bool bDraw)
        : FPrimitiveSceneProxy(Component), LocalEnd(End), bDrawGaze(bDraw)
    {
        bWillEverBeLit = false;
    }

    void SetData(const FVector &End, bool bDraw)
    {
        LocalEnd = End;
        bDrawGaze = bDraw;
    }

    virtual SIZE_T GetTypeHash() const override
    {
        static size_t Unique;
        return reinterpret_cast<SIZE_T>(&Unique);
    }

    virtual uint32 GetMemoryFootprint() const override
    {
        return sizeof(*this) + GetAllocatedSize();
    }

    virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView *View) const override
    {
        FPrimitiveViewRelevance Result;
        Result.bDrawRelevance = IsShown(View) && bDrawGaze;
        Result.bDynamicRelevance = true;
        Result.bNormalTranslucency = Result.bSeparateTranslucency = true;
        return Result;
    }

    virtual void GetDynamicMeshElements(const TArray<const FSceneView *> &Views,
                                       const FSceneViewFamily &ViewFamily, uint32 VisibilityMap,
                                       FMeshElementCollector &Collector) const override
    {
        if (!bDrawGaze)
            return;
        // Resolve local coordinates on the render thread, AFTER XR late update.
        // Do not cache world-space vertices or recreate the proxy each frame.
        const FVector Start = GetLocalToWorld().TransformPosition(UCameraGazeComponent::LocalStart());
        const FVector End = GetLocalToWorld().TransformPosition(LocalEnd);
        for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
        {
            if (VisibilityMap & (1u << ViewIndex))
            {
                auto *PDI = Collector.GetPDI(ViewIndex);
                PDI->DrawLine(Start, End, FLinearColor::Blue, SDPG_World, 1.f);
                PDI->DrawPoint(End, FLinearColor::Blue, 8.f, SDPG_World);
            }
        }
    }

  private:
    FVector LocalEnd;
    bool bDrawGaze;
};
} // namespace

UCameraGazeComponent::UCameraGazeComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork; // after camera manager update
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    CastShadow = false;
}

void UCameraGazeComponent::Configure(float TraceLengthCm, AActor *VehicleToIgnore)
{
    MaxTraceLengthCm = FMath::Max(TraceLengthCm, 1.f);
    IgnoredVehicle = VehicleToIgnore;
}

void UCameraGazeComponent::SetSample(const FVector &Origin, const FVector &Direction, bool bValid)
{
    SampleFrame = GFrameCounter;
    bSampleValid = bValid && !Origin.ContainsNaN() && !Direction.ContainsNaN() && !Direction.IsNearlyZero();
    SampleOrigin = bSampleValid ? Origin : FVector::ZeroVector;
    SampleDirection = bSampleValid ? Direction.GetSafeNormal() : FVector::ForwardVector;
}

bool UCameraGazeComponent::HasFreshSample() const
{
    return bSampleValid && SampleFrame == GFrameCounter;
}

void UCameraGazeComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                       FActorComponentTickFunction *ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!HasFreshSample() || !GetAttachParent() || !GetWorld())
    {
        Publish(LocalStart(), false);
        return;
    }

    const FTransform CameraTransform = GetComponentTransform();
    const FVector Origin = CameraTransform.TransformPosition(SampleOrigin);
    const FVector End = Origin + CameraTransform.TransformVectorNoScale(SampleDirection) * MaxTraceLengthCm;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CameraGazeDisplay), true);
    if (IgnoredVehicle.IsValid())
        Params.AddIgnoredActor(IgnoredVehicle.Get());
    FHitResult Hit;
    const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Origin, End, ECC_Visibility, Params);
    // This later trace is only for display. Never write it back to the recorder.
    Publish(CameraTransform.InverseTransformPosition(bHit ? Hit.Location : End), true);
}

void UCameraGazeComponent::Publish(const FVector &End, bool bDraw)
{
    LocalEnd = End;
    bDrawGaze = bDraw;
    UpdateBounds();
    MarkRenderTransformDirty();
    MarkRenderDynamicDataDirty();
}

FPrimitiveSceneProxy *UCameraGazeComponent::CreateSceneProxy()
{
    return new FCameraGazeSceneProxy(this, LocalEnd, bDrawGaze);
}

FBoxSphereBounds UCameraGazeComponent::CalcBounds(const FTransform &LocalToWorld) const
{
    FBox Box(ForceInit);
    Box += LocalStart();
    Box += LocalEnd;
    return FBoxSphereBounds(Box.ExpandBy(10.f)).TransformBy(LocalToWorld);
}

void UCameraGazeComponent::SendRenderDynamicData_Concurrent()
{
    Super::SendRenderDynamicData_Concurrent();
    if (SceneProxy)
    {
        auto *Proxy = static_cast<FCameraGazeSceneProxy *>(SceneProxy);
        const FVector End = LocalEnd;
        const bool bDraw = bDrawGaze;
        ENQUEUE_RENDER_COMMAND(UpdateCameraGaze)(
            [Proxy, End, bDraw](FRHICommandListImmediate &) { Proxy->SetData(End, bDraw); });
    }
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraGazeAttachmentTest, "HUTB.Pimax.GazeDisplay.CameraAttachment",
                                EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCameraGazeAttachmentTest::RunTest(const FString &Parameters)
{
    auto *Camera = NewObject<UCameraComponent>();
    auto *Gaze = NewObject<UCameraGazeComponent>();
    TestTrue(TEXT("Attach display to camera"), Gaze->AttachToComponent(Camera, FAttachmentTransformRules::KeepRelativeTransform));
    TestTrue(TEXT("Display ticks after camera update"), Gaze->PrimaryComponentTick.TickGroup == TG_PostUpdateWork);
    const FVector EyeDirection(1.f, 0.3f, 0.1f);
    Gaze->SetSample(FVector::ZeroVector, EyeDirection, true);
    TestTrue(TEXT("Accept this frame's valid sample"), Gaze->HasFreshSample());
    const FVector StartBefore = Gaze->GetComponentTransform().TransformPosition(Gaze->LocalStart());
    Gaze->Publish(FVector(1000.f, 300.f, 100.f), true);
    TestTrue(TEXT("Gaze movement cannot move pivot"),
             Gaze->GetComponentTransform().TransformPosition(Gaze->LocalStart()).Equals(StartBefore));
    // Simulate a large vehicle displacement and a turn, with no new eye sample.
    const FTransform DrivenPose(FRotator(0.f, 90.f, 0.f), FVector(20000.f, 5000.f, 100.f));
    Camera->SetWorldTransform(DrivenPose);
    TestTrue(TEXT("Pivot follows camera during driving without velocity compensation"),
             Gaze->GetComponentTransform().TransformPosition(Gaze->LocalStart()).Equals(
                 DrivenPose.TransformPosition(FVector(30.f, 0.f, 0.f)), 0.01f));
    TestTrue(TEXT("Endpoint follows the same camera transform"),
             Gaze->GetComponentTransform().TransformPosition(Gaze->LocalEnd).Equals(
                 DrivenPose.TransformPosition(FVector(1000.f, 300.f, 100.f)), 0.01f));
    Gaze->SampleFrame = GFrameCounter - 1;
    TestFalse(TEXT("Missing sample in next frame hides stale gaze"), Gaze->HasFreshSample());
    Gaze->SetSample(FVector::ZeroVector, FVector::ZeroVector, true);
    TestFalse(TEXT("Zero direction is rejected"), Gaze->HasFreshSample());
    Gaze->SetSample(FVector::ZeroVector, EyeDirection, false);
    TestFalse(TEXT("Tracker invalidity is respected"), Gaze->HasFreshSample());
    Gaze->Publish(Gaze->LocalStart(), false);
    TestFalse(TEXT("Invalid sample hides line and endpoint"), Gaze->bDrawGaze);
    return true;
}
#endif
