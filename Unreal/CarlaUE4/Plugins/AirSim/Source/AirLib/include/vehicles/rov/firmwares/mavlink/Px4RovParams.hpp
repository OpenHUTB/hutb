// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef msr_airlib_vehicles_Px4Rov_hpp
#define msr_airlib_vehicles_Px4Rov_hpp

#include "vehicles/rov/firmwares/mavlink/MavLinkRovApi.hpp"
#include "common/AirSimSettings.hpp"
#include "sensors/SensorFactory.hpp"
#include "vehicles/rov/RovBodyParams.hpp"

namespace msr
{
namespace airlib
{

    class Px4RovParams : public RovBodyParams
    {
    public:
        Px4RovParams(const AirSimSettings::MavLinkVehicleSetting& vehicle_setting, std::shared_ptr<const SensorFactory> sensor_factory)
            : sensor_factory_(sensor_factory)
        {
            connection_info_ = getConnectionInfo(vehicle_setting);
        }

        virtual ~Px4RovParams() = default;

        virtual std::unique_ptr<RovApiBase> createRovApi() override
        {
            unique_ptr<RovApiBase> api(new MavLinkRovApi());
            auto api_ptr = static_cast<MavLinkRovApi*>(api.get());
            api_ptr->initialize(connection_info_, &getSensors(), true);

            return api;
        }

        virtual void setupParams() override
        {
            auto& params = getParams();

            if (connection_info_.model == "TriRov") {
                setupBlueROV2Heavy(params);
            }
            else if (connection_info_.model == "QuadPlane") {
                setupQuadPlane(params);
            }
            //else //Generic
            //   setupGenericFixedWing(params);
        }

    protected:
        virtual const SensorFactory* getSensorFactory() const override
        {
            return sensor_factory_.get();
        }

    private:
        void setupQuadPlane(Params& params)
        {
            //TODO
            throw std::logic_error{ "QuadPlane params not yet implemented." };
        }
        static const AirSimSettings::MavLinkConnectionInfo& getConnectionInfo(const AirSimSettings::MavLinkVehicleSetting& vehicle_setting)
        {
            return vehicle_setting.connection_info;
        }

    private:
        AirSimSettings::MavLinkConnectionInfo connection_info_;
        std::shared_ptr<const SensorFactory> sensor_factory_;
    };
}
} //namespace
#endif
