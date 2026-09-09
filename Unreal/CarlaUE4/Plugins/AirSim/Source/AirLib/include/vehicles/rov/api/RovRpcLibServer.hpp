// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef msr_airlib_RovRpcLibServer_hpp
#define msr_airlib_RovRpcLibServer_hpp

#include "common/Common.hpp"
#include <functional>
#include "api/RpcLibServerBase.hpp"
#include "vehicles/rov/api/RovApiBase.hpp"

namespace msr
{
namespace airlib
{

    class RovRpcLibServer : public RpcLibServerBase
    {
    public:
        RovRpcLibServer(ApiProvider* api_provider, string server_address, uint16_t port = RpcLibPort);
        virtual ~RovRpcLibServer();

    protected:
        virtual RovApiBase* getVehicleApi(const std::string& vehicle_name) override
        {
            return static_cast<RovApiBase*>(RpcLibServerBase::getVehicleApi(vehicle_name));
        }
    };
}
} //namespace
#endif
