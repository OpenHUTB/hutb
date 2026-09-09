#pragma once

#include "CommonStructs.hpp"
#include <algorithm>

namespace Rov_simple
{

template <typename T>
class IPidIntegrator
{
public:
    virtual ~IPidIntegrator() {}
    virtual void reset() = 0;
    virtual void set(T val) = 0;
    virtual void update(float dt, T error, uint64_t last_time) = 0;
    virtual T getOutput() = 0;
};

} //namespace
