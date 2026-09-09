#pragma once

#include "IUpdatable.hpp"
#include "IOffboardApi.hpp"
#include "IStateEstimator.hpp"

namespace Rov_simple
{

class IFirmware : public IUpdatable
{
public:
    virtual IOffboardApi& offboardApi() = 0;
    virtual void overrideActuatorOutputs(const std::vector<float>& values) = 0;
};

} //namespace
