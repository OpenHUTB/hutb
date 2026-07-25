#include "Holodeck.h"
#include "CommandCenter.h"
#include "HolodeckGameMode.h" // to avoid a circular dependency. 

const FString UCommandCenter::BUFFER_NAME = "command_buffer";
const FString UCommandCenter::BUFFER_SHOULD_READ_NAME = "command_bool";
const int UCommandCenter::BUFFER_SHOULD_READ_SIZE = 1;
const int UCommandCenter::BUFFER_SIZE = 1048576; //one megabyte
const int UCommandCenter::BYTE_SIZE = 8;

void UCommandCenter::GiveCommand(UCommand * const Input) {
	if (Input != nullptr)
		Commands.Add(Input);
}

UCommandCenter::UCommandCenter() {
	UE_LOG(LogHolodeck, Log, TEXT("CommandCenter::CommandCenter() constructed"));
}

void UCommandCenter::Tick(float DeltaTime) {
	if (ShouldReadBufferPtr && *ShouldReadBufferPtr == true) {
		ReadCommandBuffer();
		*ShouldReadBufferPtr = false;
	}
	for (const auto &i : Commands)
		i->Execute();
	Commands.Empty();
}

void UCommandCenter::GetCommandBuffer() {
	UE_LOG(LogHolodeck, Log, TEXT("CommandCenter:: is getting command buffer"));
	if (Server == nullptr) {
		UE_LOG(LogHolodeck, Warning, TEXT("CommandCenter could not find server..."));
	} else {
		Buffer = static_cast<char*>(Server->Malloc(TCHAR_TO_UTF8(*BUFFER_NAME), BUFFER_SIZE * BYTE_SIZE));

		if (!Buffer) {
			UE_LOG(LogHolodeck, Fatal, TEXT("CommandCenter::GetCommandBuffer: Failed to allocate shared memory for buffer!"));
		}

		ShouldReadBufferPtr = static_cast<bool*>(Server->Malloc(TCHAR_TO_UTF8(*BUFFER_SHOULD_READ_NAME), BUFFER_SHOULD_READ_SIZE * sizeof(bool)));
		if (ShouldReadBufferPtr != nullptr)
			*ShouldReadBufferPtr = false;
		else
			UE_LOG(LogHolodeck, Error, TEXT("UCommandCenter::ShouldReadBufferPtr is null"));
	}
}

void UCommandCenter::Init(UHolodeckServer* const ParameterServer, AHolodeckGameMode* const ParameterGameMode) {
	this->Server = ParameterServer;
	this->GameMode = ParameterGameMode;
	GetCommandBuffer();
}

int UCommandCenter::ReadCommandBuffer() {
	char *Endptr;
	gason::JsonValue Value;
	gason::JsonAllocator Allocator;
	int Status = gason::jsonParse(Buffer, &Endptr, &Value, Allocator);
	if (Status != gason::JSON_PARSE_OK) {
		UE_LOG(LogHolodeck, Error, TEXT("Unable to parse command buffer as a json file"));
	} else {
		ExtractCommandsFromJson(Value);
	}
	return Status;
}

void UCommandCenter::ExtractCommandsFromJson(const gason::JsonValue &Input){
	if (Input.getTag() == gason::JSON_OBJECT) {
		gason::JsonIterator Iter = begin(Input);
		//check if this is actually the array of commands, and then extract the commands from it.
		if (Iter->value.getTag() == gason::JSON_ARRAY)
			for (gason::JsonNode* ArrayIter : Iter->value)
				GetCommand(ArrayIter->value);
		else
			UE_LOG(LogHolodeck, Warning, TEXT("Command Buffer didn't contain the format of JSON we expected. Unable to process command."));
	} else {
		UE_LOG(LogHolodeck, Warning, TEXT("Command Buffer didn't contain the format of JSON we expected. Unable to process command."));
	}
}

void UCommandCenter::GetCommand(const gason::JsonValue &Input) {
	gason::JsonIterator Iter = begin(Input);
	std::string CommandName = Iter->value.toString();
	FString CommandFString = UTF8_TO_TCHAR(CommandName.c_str());
	std::vector<std::string> StringParameters;
	std::vector<float> FloatParameters;
	Iter.p = Iter->next;
	if (Iter->value.getTag() == gason::JSON_ARRAY) {
		//We are now inside the array
		for (gason::JsonNode* ArrayIter : Iter->value) {
			gason::JsonIterator ObjIter = begin(ArrayIter->value);
			if (ObjIter->value.getTag() == gason::JSON_NUMBER)
				FloatParameters.push_back(ObjIter->value.toNumber());
			else
				StringParameters.push_back(ObjIter->value.toString());
		}
	} else {
		UE_LOG(LogHolodeck, Warning, TEXT("Command Buffer didn't contain the format of JSON we expected. Unable to process command."));
	}
	UCommand* CommandPtr = UCommandFactory::MakeCommand(CommandName, FloatParameters, StringParameters, GameMode);
	this->GiveCommand(CommandPtr);
}
