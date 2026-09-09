// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef msr_airlib_vehicles_RovBodyParamsFactory_hpp
#define msr_airlib_vehicles_RovBodyParamsFactory_hpp

#include "vehicles/rov/firmwares/rov_simple/RovSimpleParams.hpp"
#include "vehicles/rov/firmwares/mavlink/Px4RovParams.hpp"

namespace msr
{
namespace airlib
{

    class RovBodyParamsFactory
    {
    public:
        static std::unique_ptr<RovBodyParams> createConfig(const AirSimSettings::VehicleSetting* vehicle_setting,
                                                            std::shared_ptr<const SensorFactory> sensor_factory)
        {
            std::unique_ptr<RovBodyParams> config;

            if (vehicle_setting->vehicle_type == AirSimSettings::kVehicleTypePX4Rov) {
                config.reset(new Px4RovParams(*static_cast<const AirSimSettings::MavLinkVehicleSetting*>(vehicle_setting),
                                                    sensor_factory));
            }
            else if (vehicle_setting->vehicle_type == "" || //default config
                     vehicle_setting->vehicle_type == AirSimSettings::kVehicleTypeRovSimple) {
                config.reset(new RovSimpleParams(vehicle_setting, sensor_factory));
            }
            else
                throw std::runtime_error(Utils::stringf(
                    "Cannot create vehicle config because vehicle name '%s' is not recognized",
                    vehicle_setting->vehicle_name.c_str()));

            config->initialize(vehicle_setting);

            return config;
        }
    };
}
} //namespace
#endif
