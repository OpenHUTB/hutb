#pragma once

#include <vector>
#include "interfaces/CommonStructs.hpp"
#include "interfaces/IBoard.hpp"
#include "interfaces/ICommLink.hpp"
#include "interfaces/IStateEstimator.hpp"
#include "interfaces/IFirmware.hpp"
#include "Params.hpp"
#include "RemoteControl.hpp"
#include "OffboardApi.hpp"
#include "Mixer.hpp"
#include "CascadeController.hpp"
// #include "AdaptiveController.hpp"
#include "DoNothingController.hpp"

namespace Rov_simple
{

class Firmware : public IFirmware
{
public:
    Firmware(Params* params, IBoard* board, ICommLink* comm_link, IStateEstimator* state_estimator)
        : params_(params), board_(board), comm_link_(comm_link), state_estimator_(state_estimator), offboard_api_(params, board, board, state_estimator, comm_link), mixer_(params), overridden_outputs_(false)
    {
        // switch (params->controller_type) {
        // case Params::ControllerType::Cascade:
        //     controller_ = std::unique_ptr<CascadeController>(new CascadeController(params, board, comm_link));
        //     break;
        // case Params::ControllerType::Adaptive:
        //     controller_ = std::unique_ptr<AdaptiveController>(new AdaptiveController());
        //     break;
        // case Params::ControllerType::DoNothing:
        //this controller is used if you are using commandMotorPWMs. It literally does nothing,
        //just implements IController. commandMotorPWMs overrides the actuator outputs.
        // controller_ = std::unique_ptr<DoNothingController>(new DoNothingController());
        // break;
        // default:
        // throw std::invalid_argument("Cannot recognize controller specified by params->controller_type");
        // }

        // controller_->initialize(&offboard_api_, state_estimator_);
    }

    virtual void reset() override
    {
        IFirmware::reset();

        board_->reset();
        comm_link_->reset();
        // controller_->reset();
        offboard_api_.reset();

        actuator_outputs_.assign(params_->actuator.actuator_count, 0);
    }

    virtual void update() override
    {
        IFirmware::update();

        board_->update();
        offboard_api_.update();

        // no-op since controller is set to DoNothingController
        // controller_->update();
        // const Axis4r& output_controls = controller_->getOutput();

        //write the actuator outputs
        for (uint16_t actuator_index = 0; actuator_index < params_->actuator.actuator_count; ++actuator_index)
            board_->writeOutput(actuator_index, actuator_outputs_.at(actuator_index));

        comm_link_->update();
    }

    virtual IOffboardApi& offboardApi() override
    {
        return offboard_api_;
    }

    virtual void overrideActuatorOutputs(const std::vector<float>& values) override
    {
        actuator_outputs_ = values;
    }

private:
    //objects we use
    Params* params_;
    IBoard* board_;
    ICommLink* comm_link_;
    IStateEstimator* state_estimator_;

    OffboardApi offboard_api_;
    Mixer mixer_;
    // std::unique_ptr<IController> controller_;

    std::vector<float> actuator_outputs_;
    bool overridden_outputs_;
};

} //namespace
