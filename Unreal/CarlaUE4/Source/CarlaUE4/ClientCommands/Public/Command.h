#pragma once

#include "Holodeck.h"

#include <vector>
#include "HolodeckAgent.h"
#include "GameFramework/Actor.h"
#include "Command.generated.h"

/**
  * UCommand
  * This is the abstract base class for commands in the holodeck
  * When instantiating an inherited command, you must call the Init function.
  * - Note: commands are more safely instantiated from the commandfactory. 
  * When inheriting from this class, you must: 
  * - Implement the execute function.
  * - Add the inhereted class and its name string to the CommandFactory.h file. 
  */
UCLASS(ClassGroup = (Custom), abstract)
class HOLODECK_API UCommand : public UObject {
	GENERATED_BODY()

public:

	/**
	  * Execute
	  * The command performs its function when this is called.
	  * Override this function with what you want the child command to do.
	  * Use NumberParams, StringParams, and Target as input variables for the execute function. 
	  */
	virtual void Execute() { check( 0 && "You must override UCommand::Execute" ); };

	/**
	* UCommand
	* Default constructor.
	*/
	UCommand();

	/**
	  * Init
	  * This must be called after creating a new command object in order to load it with the correct data.
	  * @param NumberParamters The numbers (represented as floats) to be used as parameters for the execute call.
	  * @param StringParameters the strings to be used as parameters for the execute call.
	  * @param TargetParameter The target of the execute function call. Usually will be the GameMode pointer. 
	*/
	virtual void Init(const std::vector<float>& NumberParameters, const std::vector<std::string>& StringParameters, AActor* const TargetParameter);

	/**
	* GetAgent
	* This returns a pointer to a HolodeckAgent from the HolodeckServer given an AgentName
	* @param AgentName The FString name of the agent to grab
	*/
	AHolodeckAgent* GetAgent(FString AgentName);

protected:
	std::vector<float> NumberParams;
	std::vector<std::string> StringParams;
	AActor* Target;
};
