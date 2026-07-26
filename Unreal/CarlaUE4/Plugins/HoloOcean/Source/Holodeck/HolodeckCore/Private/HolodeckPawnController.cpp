// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Holodeck.h"
#include "HolodeckAgent.h"
#include "HolodeckPawnController.h"

const FString CONTROL_SCHEME_KEY = "control_scheme";
const FString TELEPORT_FLAG_KEY = "teleport_flag";
const FString TELEPORT_COMMAND_KEY = "teleport_command";


AHolodeckPawnController::AHolodeckPawnController(const FObjectInitializer& ObjectInitializer)
		: AHolodeckPawnControllerInterface(ObjectInitializer) {
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
}

AHolodeckPawnController::~AHolodeckPawnController() { }

void AHolodeckPawnController::BeginPlay() {
	Super::BeginPlay();
	AddTickPrerequisiteActor(GetWorld()->GetAuthGameMode());
}

void AHolodeckPawnController::OnPossess(APawn* InPawn) {
	ControlledAgent = static_cast<AHolodeckAgentInterface*>(InPawn);

	if (ControlledAgent == nullptr) {
		UE_LOG(LogHolodeck, Fatal, TEXT("HolodeckPawnController attached to non-HolodeckAgent!"));
	}

	ControlledAgent->Controller = this;

	UE_LOG(LogHolodeck, Log, TEXT("Pawn Possessed: %s, Controlled by: %s"), *InPawn->GetHumanReadableName(), *this->GetClass()->GetName());
	UpdateServerInfo();

	URawControlScheme* RawControlScheme = NewObject<URawControlScheme>();
	RawControlScheme->Agent = ControlledAgent;
	ControlSchemes.Add(RawControlScheme);
	AddControlSchemes();
}

void AHolodeckPawnController::Tick(float DeltaSeconds) {
	Super::Tick(DeltaSeconds);

	if (Server == nullptr) {
		UE_LOG(LogHolodeck, Fatal, TEXT("HolodeckPawnController couldn't find server..."));
	}

	if (ShouldChangeStateBuffer && *ShouldChangeStateBuffer & 0x4) {
		ExecuteSetState();
	}
	else if (ShouldChangeStateBuffer && *ShouldChangeStateBuffer) {
		ExecuteTeleport();
	}

	unsigned int index = *ControlSchemeIdBuffer % ControlSchemes.Num();
	ControlSchemes[index]->Execute(ControlledAgent->GetRawActionBuffer(), ActionBuffer, DeltaSeconds);
}

void AHolodeckPawnController::UpdateServerInfo() {
	if (Server != nullptr) return;
	UHolodeckGameInstance* Instance = static_cast<UHolodeckGameInstance*>(GetGameInstance());
	if (Instance != nullptr)
		Server = Instance->GetServer();
	else
		UE_LOG(LogHolodeck, Warning, TEXT("Game Instance is not UHolodeckGameInstance."));
}

void AHolodeckPawnController::AllocateBuffers(const FString& AgentName) {
	UpdateServerInfo();
	if (Server != nullptr) {
		if (!ControlledAgent) {
			// LOG HERE
			return;
		}

		unsigned int MaxControlSize = 0;
		for (UHolodeckControlScheme* const ControlScheme : ControlSchemes) {
			if (ControlScheme->GetControlSchemeSizeInBytes() > MaxControlSize)
				MaxControlSize = ControlScheme->GetControlSchemeSizeInBytes();
		}

		void* TempBuffer;
		ActionBuffer = Server->Malloc(TCHAR_TO_UTF8(*AgentName),
									  MaxControlSize);

		TempBuffer = Server->Malloc(UHolodeckServer::MakeKey(AgentName, CONTROL_SCHEME_KEY),
											   sizeof(uint8));
		ControlSchemeIdBuffer = static_cast<uint8*>(TempBuffer);

		TempBuffer = Server->Malloc(UHolodeckServer::MakeKey(AgentName, TELEPORT_FLAG_KEY),
											 sizeof(uint8));
		ShouldChangeStateBuffer = static_cast<uint8*>(TempBuffer);

		TempBuffer = Server->Malloc(UHolodeckServer::MakeKey(AgentName, TELEPORT_COMMAND_KEY),
										TELEPORT_COMMAND_SIZE * sizeof(float));
		TeleportBuffer = static_cast<float*>(TempBuffer);
	}
}

void AHolodeckPawnController::ExecuteTeleport() {
	UE_LOG(LogHolodeck, Log, TEXT("Executing teleport"));
	if (ControlledAgent == nullptr) {
		UE_LOG(LogHolodeck, Warning, TEXT("AHolodeckPawnController::ExecuteTeleport: Couldn't get reference to controlled HolodeckAgent"));
		return;
	}

	float* FloatPtr = static_cast<float*>(TeleportBuffer);

	FVector TeleportLocation;
	if (*ShouldChangeStateBuffer & 0x1) {
		TeleportLocation = FVector(FloatPtr[0], FloatPtr[1], FloatPtr[2]);
		TeleportLocation = ConvertLinearVector(TeleportLocation, ClientToUE);
	} else {
		TeleportLocation = ControlledAgent->GetActorLocation();
	}

	FRotator NewRotation;
	if (*ShouldChangeStateBuffer & 0x2) {
		NewRotation = RPYToRotator(FloatPtr[3], FloatPtr[4], FloatPtr[5]);
	} else {
		NewRotation = ControlledAgent->GetActorRotation();
	}

	ControlledAgent->Teleport(TeleportLocation, NewRotation);
	*ShouldChangeStateBuffer = 0;
}


void AHolodeckPawnController::ExecuteSetState() {
	UE_LOG(LogHolodeck, Log, TEXT("AHolodeckPawnController::ExecuteSetState"));

	if (ControlledAgent == nullptr) {
		UE_LOG(LogHolodeck, Fatal, TEXT("AHolodeckPawnController::ExecuteSetState: Couldn't get reference to controlled HolodeckAgent"));
		return;
	}

	float* FloatPtr = static_cast<float*>(TeleportBuffer);
	FVector TeleportLocation = FVector(FloatPtr[0], FloatPtr[1], FloatPtr[2]);
	FRotator NewRotation = RPYToRotator(FloatPtr[3], FloatPtr[4], FloatPtr[5]);
	FVector NewVelocity = FVector(FloatPtr[6], FloatPtr[7], FloatPtr[8]);
	FVector NewAngVelocity = FVector(FloatPtr[9], FloatPtr[10], FloatPtr[11]);

	// Perform conversion
	TeleportLocation = ConvertLinearVector(TeleportLocation, ClientToUE);
	NewVelocity = ConvertLinearVector(NewVelocity, ClientToUE);
	NewAngVelocity = ConvertAngularVector(NewAngVelocity, NoScale);

	ControlledAgent->SetState(TeleportLocation, NewRotation, NewVelocity, NewAngVelocity);
	*ShouldChangeStateBuffer = 0;
}

bool AHolodeckPawnController::CheckBoolBuffer(void* Buffer) {
	bool* BoolPtr = static_cast<bool*>(Buffer);
	if (BoolPtr && *BoolPtr) {
		*BoolPtr = false;
		return true;
	}
	return false;
}

void AHolodeckPawnController::SetServer(UHolodeckServer* const ServerParam) {
	this->Server = ServerParam;
}

UHolodeckServer* AHolodeckPawnController::GetServer() {
	return this->Server;
}
