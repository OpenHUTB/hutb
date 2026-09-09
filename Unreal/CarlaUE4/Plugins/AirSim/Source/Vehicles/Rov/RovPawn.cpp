#include "RovPawn.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ChildActorComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "AirBlueprintLib.h"
#include "common/CommonStructs.hpp"
#include "common/Common.hpp"

ARovPawn::ARovPawn()
{
    init_id_ = pawn_events_.getActuatorSignal().connect_member(this, &ARovPawn::setRotorRenderedStates);

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

    body_mesh_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
    body_mesh_->SetupAttachment(RootComponent);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> cube_mesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (cube_mesh.Succeeded()) {
        body_mesh_->SetStaticMesh(cube_mesh.Object);
        body_mesh_->SetRelativeScale3D(FVector(0.5f, 0.35f, 0.25f));
    }

    static ConstructorHelpers::FClassFinder<APIPCamera> pip_camera_class(TEXT("Blueprint'/AirSim/Blueprints/BP_PIPCamera'"));
    UClass* pip_class = pip_camera_class.Succeeded() ? pip_camera_class.Class : nullptr;

    camera_front_center_comp_ = CreateDefaultSubobject<UChildActorComponent>(TEXT("FrontCenterCamera"));
    camera_front_center_comp_->SetupAttachment(RootComponent);
    camera_front_center_comp_->SetRelativeLocation(FVector(25.f, 0.f, 0.f));
    if (pip_class) camera_front_center_comp_->SetChildActorClass(pip_class);

    camera_front_right_comp_ = CreateDefaultSubobject<UChildActorComponent>(TEXT("FrontRightCamera"));
    camera_front_right_comp_->SetupAttachment(RootComponent);
    camera_front_right_comp_->SetRelativeLocation(FVector(25.f, 15.f, 0.f));
    if (pip_class) camera_front_right_comp_->SetChildActorClass(pip_class);

    camera_front_left_comp_ = CreateDefaultSubobject<UChildActorComponent>(TEXT("FrontLeftCamera"));
    camera_front_left_comp_->SetupAttachment(RootComponent);
    camera_front_left_comp_->SetRelativeLocation(FVector(25.f, -15.f, 0.f));
    if (pip_class) camera_front_left_comp_->SetChildActorClass(pip_class);

    camera_back_center_comp_ = CreateDefaultSubobject<UChildActorComponent>(TEXT("BackCenterCamera"));
    camera_back_center_comp_->SetupAttachment(RootComponent);
    camera_back_center_comp_->SetRelativeLocation(FVector(-25.f, 0.f, 0.f));
    if (pip_class) camera_back_center_comp_->SetChildActorClass(pip_class);

    camera_bottom_center_comp_ = CreateDefaultSubobject<UChildActorComponent>(TEXT("BottomCenterCamera"));
    camera_bottom_center_comp_->SetupAttachment(RootComponent);
    camera_bottom_center_comp_->SetRelativeLocation(FVector(0.f, 0.f, -15.f));
    camera_bottom_center_comp_->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
    if (pip_class) camera_bottom_center_comp_->SetChildActorClass(pip_class);
}

void ARovPawn::BeginPlay()
{
    Super::BeginPlay();

    if (body_mesh_) {
        const TCHAR* candidates[] = {
            TEXT("/AirSim/Models/RoV/bluerovheavy2.bluerovheavy2"),
            TEXT("/AirSim/Models/RoV/bluerovheavy2_BROV2-HEAVY-ASM-BROV2-HEAVY-R1_001.bluerovheavy2_BROV2-HEAVY-ASM-BROV2-HEAVY-R1_001"),
            TEXT("/Game/bluerovheavy2_BROV2-HEAVY-ASM-BROV2-HEAVY-R1_001.bluerovheavy2_BROV2-HEAVY-ASM-BROV2-HEAVY-R1_001"),
            TEXT("/Game/bluerovheavy2.bluerovheavy2")
        };
        UStaticMesh* rov_mesh = nullptr;
        for (const TCHAR* candidate : candidates) {
            rov_mesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, candidate));
            if (rov_mesh) {
                UE_LOG(LogTemp, Log, TEXT("ARovPawn: Loaded ROV mesh from %s"), candidate);
                break;
            }
        }
        if (rov_mesh) {
            body_mesh_->SetStaticMesh(rov_mesh);

            FBoxSphereBounds bounds = rov_mesh->GetBounds();
            FVector extent = bounds.BoxExtent;

            UE_LOG(LogTemp, Warning, TEXT("ARovPawn ROV Mesh Bounds: Origin=(%f, %f, %f), Extent=(%f, %f, %f)"),
                   bounds.Origin.X, bounds.Origin.Y, bounds.Origin.Z, extent.X, extent.Y, extent.Z);

            float max_dim = FMath::Max3(extent.X, extent.Y, extent.Z) * 2.0f;
            float target_size = 50.0f; // BlueROV2 is approx 50cm
            float scale = 1.0f;
            if (max_dim > 100.0f) {
                scale = target_size / max_dim;
            } else if (max_dim < 5.0f && max_dim > 0.001f) {
                scale = target_size / max_dim;
            }

            body_mesh_->SetRelativeScale3D(FVector(scale, scale, scale));
            body_mesh_->SetRelativeLocation(-bounds.Origin * scale);
        }
    }

    rotor_speed_components_.Empty();
    rotor_speed_components_.Add(UAirBlueprintLib::GetActorComponent<URotatingMovementComponent>(this, TEXT("RotationV1")));
    rotor_speed_components_.Add(UAirBlueprintLib::GetActorComponent<URotatingMovementComponent>(this, TEXT("RotationV2")));
    rotor_speed_components_.Add(UAirBlueprintLib::GetActorComponent<URotatingMovementComponent>(this, TEXT("RotationV3")));
    rotor_speed_components_.Add(UAirBlueprintLib::GetActorComponent<URotatingMovementComponent>(this, TEXT("RotationV4")));

    rotor_speed_components_.Add(UAirBlueprintLib::GetActorComponent<URotatingMovementComponent>(this, TEXT("RotationH1")));
    rotor_speed_components_.Add(UAirBlueprintLib::GetActorComponent<URotatingMovementComponent>(this, TEXT("RotationH2")));
    rotor_speed_components_.Add(UAirBlueprintLib::GetActorComponent<URotatingMovementComponent>(this, TEXT("RotationH3")));
    rotor_speed_components_.Add(UAirBlueprintLib::GetActorComponent<URotatingMovementComponent>(this, TEXT("RotationH4")));

    rotor_angle_components_.Empty();
    rotor_angle_components_.Add(UAirBlueprintLib::GetActorComponent<UStaticMeshComponent>(this, TEXT("Engine_L")));
    rotor_angle_components_.Add(UAirBlueprintLib::GetActorComponent<UStaticMeshComponent>(this, TEXT("Engine_R")));
}

void ARovPawn::initializeForBeginPlay()
{
    //get references of existing camera safely
    auto c_fr = UAirBlueprintLib::GetActorComponent<UChildActorComponent>(this, TEXT("FrontRightCamera"));
    camera_front_right_ = c_fr ? Cast<APIPCamera>(c_fr->GetChildActor()) : nullptr;

    auto c_fl = UAirBlueprintLib::GetActorComponent<UChildActorComponent>(this, TEXT("FrontLeftCamera"));
    camera_front_left_ = c_fl ? Cast<APIPCamera>(c_fl->GetChildActor()) : nullptr;

    auto c_fc = UAirBlueprintLib::GetActorComponent<UChildActorComponent>(this, TEXT("FrontCenterCamera"));
    camera_front_center_ = c_fc ? Cast<APIPCamera>(c_fc->GetChildActor()) : nullptr;

    auto c_bc = UAirBlueprintLib::GetActorComponent<UChildActorComponent>(this, TEXT("BackCenterCamera"));
    camera_back_center_ = c_bc ? Cast<APIPCamera>(c_bc->GetChildActor()) : nullptr;

    auto c_bot = UAirBlueprintLib::GetActorComponent<UChildActorComponent>(this, TEXT("BottomCenterCamera"));
    camera_bottom_center_ = c_bot ? Cast<APIPCamera>(c_bot->GetChildActor()) : nullptr;
}

void ARovPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    pawn_events_.getPawnTickSignal().emit(DeltaSeconds);
}

void ARovPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    camera_front_right_ = nullptr;
    camera_front_left_ = nullptr;
    camera_front_center_ = nullptr;
    camera_back_center_ = nullptr;
    camera_bottom_center_ = nullptr;

    pawn_events_.getActuatorSignal().disconnect_all();
    rotor_speed_components_.Empty();
    rotor_angle_components_.Empty();

    Super::EndPlay(EndPlayReason);
}

const common_utils::UniqueValueMap<std::string, APIPCamera*> ARovPawn::getCameras() const
{
    common_utils::UniqueValueMap<std::string, APIPCamera*> cameras;
    cameras.insert_or_assign("front_center", camera_front_center_);
    cameras.insert_or_assign("front_right", camera_front_right_);
    cameras.insert_or_assign("front_left", camera_front_left_);
    cameras.insert_or_assign("bottom_center", camera_bottom_center_);
    cameras.insert_or_assign("back_center", camera_back_center_);

    cameras.insert_or_assign("0", camera_front_center_);
    cameras.insert_or_assign("1", camera_front_right_);
    cameras.insert_or_assign("2", camera_front_left_);
    cameras.insert_or_assign("3", camera_bottom_center_);
    cameras.insert_or_assign("4", camera_back_center_);

    cameras.insert_or_assign("", camera_front_center_);
    cameras.insert_or_assign("fpv", camera_front_center_);

    return cameras;
}

void ARovPawn::NotifyHit(class UPrimitiveComponent* MyComp, class AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation,
                               FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
    pawn_events_.getCollisionSignal().emit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);
}

void ARovPawn::setRotorRenderedStates(const std::vector<RovPawnEvents::RotorTiltableInfo>& rotor_infos)
{
    int info_count = static_cast<int>(rotor_infos.size());
    int max_rotors = FMath::Min(num_visible_rotors_, info_count);

    for (auto rotor_index = 0; rotor_index < max_rotors; ++rotor_index) {
        if (rotor_index < rotor_speed_components_.Num()) {
            URotatingMovementComponent* rot_movement = rotor_speed_components_[rotor_index];
            if (rot_movement != nullptr) {
                rot_movement->RotationRate.Yaw =
                    rotor_infos.at(rotor_index).rotor_speed * rotor_infos.at(rotor_index).rotor_direction * RotatorFactor;
            }
        }
        if (!rotor_infos.at(rotor_index).is_fixed) {
            if (rotor_index < rotor_angle_components_.Num()) {
                UStaticMeshComponent* rotor_angle_comp = rotor_angle_components_[rotor_index];
                if (rotor_angle_comp != nullptr) {
                    float pitch = -rotor_infos.at(rotor_index).rotor_angle_from_vertical * 180.0f / M_PIf; // negate for unreal axes
                    FRotator new_rotation = FRotator(pitch, 0.0f, 0.0f);
                    rotor_angle_comp->SetRelativeRotation(new_rotation);
                }
            }
        }
    }
}

