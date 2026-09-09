// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef airsimcore_aero_vertex_hpp
#define airsimcore_aero_vertex_hpp

#include "common/Common.hpp"
#include "common/FirstOrderFilter.hpp"
#include "physics/Environment.hpp"
#include "physics/Kinematics.hpp"
#include "physics/PhysicsBodyVertex.hpp"
#include "vehicles/rov/AeroParams.hpp"

namespace msr
{
namespace airlib
{

    class AeroVertex : public PhysicsBodyVertex
    {
    public:
        struct Output
        {
            real_T alpha;
            real_T beta;
            real_T Va;
            real_T flap_input_1;
            real_T flap_input_2;
            real_T flap_input_3;
            real_T flap_angle_1;
            real_T flap_angle_2;
            real_T flap_angle_3;
        };

    public:
        AeroVertex()
        {
            //allow default constructor with later call for initialize
        }

        AeroVertex(const AeroParams& params, const Environment* environment, const Kinematics* kinematics, const AirState air_state)
        {
            initialize(params, environment, kinematics, air_state);
        }

        void initialize(const AeroParams& params, const Environment* environment, const Kinematics* kinematics, const AirState air_state)
        {
            params_ = params;
            environment_ = environment;
            kinematics_ = kinematics;
            air_state_ = air_state;

            //computed wrench is about center of mass, normal direction not used
            PhysicsBodyVertex::initialize(Vector3r::Zero(), Vector3r::Zero());

            for (uint i = 0; i < 3; ++i) {
                control_flap_filters_.emplace_back(params_.flap_rise_time, 0.0, 0.0);
            }

            setOutput();
        }

        //ElevatorAileronRudder: {elevator, aileron, rudder}
        //AileronRudderVator: {aileron, right ruddervator, left ruddervator}
        //ElevonRudder: {right elevon, left elevon, rudder}
        //values from -1 to 1
        void setFlapInputs(const vector<real_T>& inputs)
        {
            for (uint i = 0; i < inputs.size(); ++i) {
                control_flap_filters_[i].setInput(params_.flap_max_angle * Utils::clip(inputs[i], -1.f, 1.f));
            }
        }

        void setAirspeedVertex(const Vector3r airspeed_body_vector)
        {
            air_state_.setAirspeedState(airspeed_body_vector);
        }

        Output getOutput() const
        {
            return output_;
        }

        //*** Start: UpdatableState implementation ***//
        virtual void resetImplementation() override
        {
            PhysicsBodyVertex::resetImplementation();

            air_state_.zero();

            //update environmental factors before we call base
            updateEnvironmentalFactors();

            for (uint i = 0; i < control_flap_filters_.size(); ++i) {
                control_flap_filters_[i].reset();
            }
        }

        virtual void update() override
        {
            //update environmental factors before we call base
            updateEnvironmentalFactors();

            //this will in turn call setWrench
            PhysicsBodyVertex::update();

            //set the output
            setOutput();

            //update filters - this should be after so that first output is same as initial
            for (uint i = 0; i < control_flap_filters_.size(); ++i) {
                control_flap_filters_[i].update();
            }
        }

        virtual void reportState(StateReporter& reporter) override
        {
            reporter.writeValue("Va", output_.Va);
            reporter.writeValue("alpha", output_.alpha);
            reporter.writeValue("beta", output_.beta);
            reporter.writeValue("flap1", output_.flap_angle_1);
            reporter.writeValue("flap2", output_.flap_angle_2);
            reporter.writeValue("flap3", output_.flap_angle_3);
        }
        //*** End: UpdatableState implementation ***//

    protected:
        virtual void setWrench(Wrench& wrench) override
        {
            calculateAerodynamicForces(wrench.force, wrench.torque);
        }

    private:
        void calculateAerodynamicForces(Vector3r& output_force, Vector3r& output_torque)
        {
            output_force.setZero();
            output_torque.setZero();

            Vector3r flap_angles;
            flap_angles << output_.flap_angle_1, output_.flap_angle_2, output_.flap_angle_3;

            //convert to standard {elevator, aileron, rudder}
            Vector3r flap_angles_standard = params_.aero_control_mixer * flap_angles;
            real_T elevator = flap_angles_standard(0);
            real_T aileron = flap_angles_standard(1);
            real_T rudder = flap_angles_standard(2);

            Vector3r angular_vel = kinematics_->getState().twist.angular;
            real_T p = angular_vel(0);
            real_T q = angular_vel(1);
            real_T r = angular_vel(2);

            real_T alpha = air_state_.alpha;
            real_T beta = air_state_.beta;

            real_T ca = cos(alpha);
            real_T sa = sin(alpha);

            real_T qbar = 0.5f * air_state_.rho * pow(air_state_.Va, 2.f);

            real_T p_nondim;
            real_T q_nondim;
            real_T r_nondim;
            if (air_state_.Va > 0.5f) {
                p_nondim = p * params_.b / (2.0 * air_state_.Va);
                q_nondim = q * params_.c / (2.0 * air_state_.Va);
                r_nondim = r * params_.b / (2.0 * air_state_.Va);
            }
            else {
                p_nondim = 0.0;
                q_nondim = 0.0;
                r_nondim = 0.0;
            }

            double tmp1 = std::exp(-static_cast<double>(params_.M * (alpha - params_.alpha0))); //these numbers are often too large/small to handle as floats
            double tmp2 = std::exp(static_cast<double>(params_.M * (alpha + params_.alpha0)));
            real_T sigma_a = static_cast<real_T>((1.0 + tmp1 + tmp2) / ((1.0 + tmp1) * (1.0 + tmp2)));
            real_T CL_a = (1.f - sigma_a) * (params_.CL.O + params_.CL.alpha * alpha) + sigma_a * (2.f * VectorMath::sgn(alpha) * sa * sa * ca);
            real_T CD_a = (1.f - sigma_a) * (params_.CD.p + ((pow((params_.CL.O + params_.CL.alpha * alpha), 2.0)) / (M_PIf * params_.e * params_.aspect_ratio))) + sigma_a * (2.f * VectorMath::sgn(alpha) * sa);

            real_T f_lift = qbar * params_.S * (CL_a + params_.CL.q * q_nondim + params_.CL.delta_e * elevator);
            real_T f_drag = qbar * params_.S * (CD_a + params_.CD.q * q_nondim + params_.CD.delta_e * elevator);

            output_force(0) = 0.0;
            output_force(1) = 0.0;
            output_force(2) = 0.0;
            output_torque(0) = 0.0;
            output_torque(1) = 0.0;
            output_torque(2) = 0.0;
        }

        void setOutput()
        {
            output_.alpha = air_state_.alpha;
            output_.beta = air_state_.beta;
            output_.Va = air_state_.Va;
            output_.flap_input_1 = control_flap_filters_[0].getInput();
            output_.flap_input_2 = control_flap_filters_[1].getInput();
            output_.flap_input_3 = control_flap_filters_[2].getInput();
            output_.flap_angle_1 = control_flap_filters_[0].getOutput();
            output_.flap_angle_2 = control_flap_filters_[1].getOutput();
            output_.flap_angle_3 = control_flap_filters_[2].getOutput();
        }

        void updateEnvironmentalFactors()
        {
            //update air density
            air_state_.setAirDensity(environment_->getState().air_density);
        }

    private:
        AeroParams params_;
        vector<FirstOrderFilter<real_T>> control_flap_filters_;
        const Environment* environment_ = nullptr;
        const Kinematics* kinematics_ = nullptr; //need kinematics for calculating aerodynamic forces and moments
        Output output_;
        AirState air_state_;
    };

}
} //namespace
#endif
