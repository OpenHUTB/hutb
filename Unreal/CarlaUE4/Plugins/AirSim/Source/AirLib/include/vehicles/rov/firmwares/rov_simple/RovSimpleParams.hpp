#ifndef msr_airlib_vehicles_RovSimple_hpp
#define msr_airlib_vehicles_RovSimple_hpp

#include "vehicles/rov/firmwares/rov_simple/RovSimpleApi.hpp"
#include "vehicles/rov/RovBodyParams.hpp"
#include "common/AirSimSettings.hpp"
#include "sensors/SensorFactory.hpp"

namespace msr
{
namespace airlib
{

    class RovSimpleParams : public RovBodyParams
    {
    public:
        RovSimpleParams(const AirSimSettings::VehicleSetting* vehicle_setting, std::shared_ptr<const SensorFactory> sensor_factory)
            : vehicle_setting_(vehicle_setting), sensor_factory_(sensor_factory)
        {
        }

        virtual ~RovSimpleParams() = default;

        virtual std::unique_ptr<RovApiBase> createRovApi() override
        {
            return std::unique_ptr<RovApiBase>(new RovSimpleApi(this, vehicle_setting_));
        }

    protected:
        virtual void setupParams() override
        {
            auto& params = getParams();
            setupBlueROV2Heavy(params);
            
        }

        virtual const SensorFactory* getSensorFactory() const override
        {
            return sensor_factory_.get();
        }

    private:
        vector<unique_ptr<SensorBase>> sensor_storage_;
        const AirSimSettings::VehicleSetting* vehicle_setting_; //store as pointer because of derived classes
        std::shared_ptr<const SensorFactory> sensor_factory_;
    };

}
} //namespace
#endif
