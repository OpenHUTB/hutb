// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef msr_airlib_rovbody_hpp
#define msr_airlib_rovbody_hpp

#include "common/Common.hpp"
#include "common/CommonStructs.hpp"
#include "RotorTiltable.hpp"
#include "AeroVertex.hpp"
#include "api/VehicleApiBase.hpp"
#include "api/VehicleSimApiBase.hpp"
#include "RovBodyParams.hpp"
#include "physics/PhysicsBody.hpp"

namespace msr
{
namespace airlib
{

    class RovPhysicsBody : public PhysicsBody
    {
    public:
        RovPhysicsBody(RovBodyParams* params, VehicleApiBase* vehicle_api,
                 Kinematics* kinematics, Environment* environment)
            : params_(params), vehicle_api_(vehicle_api)
        {
            setName("RovPhysicsBody");
            vehicle_api_->setParent(this);
            initialize(kinematics, environment);
        }

        //*** Start: UpdatableState implementation ***//
        virtual void resetImplementation() override
        {
            //reset rotors, kinematics and environment
            PhysicsBody::resetImplementation();

            //reset sensors last after their ground truth has been reset
            resetSensors();
        }

        virtual void update() override
        {
            //update forces on vertices that we will use next
            PhysicsBody::update();

            //Note that controller gets updated after kinematics gets updated in updateKinematics
            //otherwise sensors will have values from previous cycle causing lags which will appear
            //as crazy jerks whenever commands like velocity is issued
        }
        virtual void reportState(StateReporter& reporter) override
        {
            //call base
            PhysicsBody::reportState(reporter);

            reportSensors(*params_, reporter);

            //report rotors
            for (uint rotor_index = 0; rotor_index < rotors_.size(); ++rotor_index) {
                reporter.startHeading("", 1);
                reporter.writeValue("RotorTiltable", rotor_index);
                reporter.endHeading(false, 1);
                rotors_.at(rotor_index).reportState(reporter);
            }

            //report AeroVertex
            reporter.startHeading("", 1);
            reporter.writeValue("AeroVertex", 0);
            reporter.endHeading(false, 1);
            aero_vertex_.reportState(reporter);
        }
        //*** End: UpdatableState implementation ***//

        //Fast Physics engine calls this method to set next kinematics
        virtual void updateKinematics(const Kinematics::State& kinematics) override
        {
            PhysicsBody::updateKinematics(kinematics);

            updateSensorsAndController();
        }

        //External Physics engine calls this method to keep physics bodies updated and move rotors
        virtual void updateKinematics() override
        {
            PhysicsBody::updateKinematics();

            updateSensorsAndController();
        }

        void updateSensorsAndController()
        {
            updateSensors(*params_, getKinematics(), getEnvironment());

            //update controller which will update actuator control signal
            vehicle_api_->update();

            //update all control inputs
            //for now assuming actuation order is {flap1, flap2, flap3, rotor1thr, rotor1ang, rotor2thr, rotor2ang, ...}
            vector<real_T> rov_inputs = { 0.0, 0.0, 0.0 };
            //vector<real_T> rov_inputs = { vehicle_api_->getActuation(0), vehicle_api_->getActuation(1), vehicle_api_->getActuation(2) };
            aero_vertex_.setFlapInputs(rov_inputs);

            //transfer new input values from controller to rotors
            for (uint rotor_index = 0; rotor_index < rotors_.size(); ++rotor_index) {
                rotors_.at(rotor_index).setControlSignal(vehicle_api_->getActuation(rotor_index));
                //rotors_.at(rotor_index).setAngleSignal(vehicle_api_->getActuation(rotor_index));
                
                //rotors_.at(rotor_index).setControlSignal(vehicle_api_->getActuation(3 + 2 * rotor_index));
                //rotors_.at(rotor_index).setAngleSignal(vehicle_api_->getActuation(3 + 2 * rotor_index + 1));
            }
        }

        //this gets called in getDragWrench function in physics engine
        virtual void setAirspeedBody(const Vector3r airspeed_body_vector)
        {
            aero_vertex_.setAirspeedVertex(airspeed_body_vector);

            for (uint rotor_index = 0; rotor_index < rotors_.size(); ++rotor_index) {
                rotors_[rotor_index].setAirspeedRotor(airspeed_body_vector);
            }
        }

        void overwriteRotorTilts(const vector<float>& angles, bool spin_props)
        {
            for (uint i = 0; i < rotors_.size(); ++i) {
                auto& rotor = rotors_.at(i);
                rotor.overwriteTilt(angles[i], spin_props);
            }
        }

        //sensor getter
        const SensorCollection& getSensors() const
        {
            return params_->getSensors();
        }

        //physics body interface
        virtual uint wrenchVertexCount() const override
        {
            return params_->getParams().rotor_count ; //+1 for aerovertex
        }
        virtual PhysicsBodyVertex& getWrenchVertex(uint index) override
        {
          
                return rotors_.at(index );
            
        }
        virtual const PhysicsBodyVertex& getWrenchVertex(uint index) const override
        {
         
                return rotors_.at(index);
            
        }

        virtual real_T getRestitution() const override
        {
            return params_->getParams().restitution;
        }
        virtual real_T getFriction() const override
        {
            return params_->getParams().friction;
        }

        RotorTiltable::TiltOutput getRotorOutput(uint index) const
        {
            return rotors_.at(index).getOutput();
        }

        AeroVertex::Output getAeroOutput() const
        {
            return aero_vertex_.getOutput();
        }

        uint rotorCount() const
        {
            return params_->getParams().rotor_count;
        }

        virtual ~RovPhysicsBody() = default;

    private: //methods
        void initialize(Kinematics* kinematics, Environment* environment)
        {
            PhysicsBody::initialize(params_->getParams().mass, params_->getParams().inertia, kinematics, environment);

            createRotors(*params_, rotors_, environment);
            createAeroVertex(*params_, aero_vertex_, environment, kinematics);

            initSensors(*params_, getKinematics(), getEnvironment());
        }

        static void createRotors(const RovBodyParams& params, vector<RotorTiltable>& rotors, const Environment* environment)
        {
            rotors.clear();
            //for each rotor pose
            for (uint rotor_index = 0; rotor_index < params.getParams().rotor_configs.size(); ++rotor_index) {
                const RovBodyParams::RotorTiltableConfiguration& rotor_config = params.getParams().rotor_configs.at(rotor_index);
                rotors.emplace_back(rotor_config.position, rotor_config.normal_nominal, rotor_config.direction, rotor_config.is_fixed, rotor_config.rotation_axis, rotor_config.max_angle, rotor_config.params, environment, rotor_index);
            }
        }

        static void createAeroVertex(const RovBodyParams& params, AeroVertex& aero_vertex, const Environment* environment, const Kinematics* kinematics)
        {
            AirState air_state;
            air_state.zero();
            aero_vertex.initialize(params.getParams().aero_params, environment, kinematics, air_state);
        }

        void reportSensors(RovBodyParams& params, StateReporter& reporter)
        {
            params.getSensors().reportState(reporter);
        }

        void updateSensors(RovBodyParams& params, const Kinematics::State& state, const Environment& environment)
        {
            unused(state);
            unused(environment);
            params.getSensors().update();
        }

        void initSensors(RovBodyParams& params, const Kinematics::State& state, const Environment& environment)
        {
            params.getSensors().initialize(&state, &environment);
        }

        void resetSensors()
        {
            params_->getSensors().reset();
        }

    private: //fields
        RovBodyParams* params_;
        AeroVertex aero_vertex_; //one aerovertex at the center of mass for calculating aerodynamic forces and moments
        vector<RotorTiltable> rotors_;

        VehicleApiBase* vehicle_api_;
    };

}
} //namespace

#endif
