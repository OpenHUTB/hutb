// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef airsimcore_RotorTiltable_hpp
#define airsimcore_RotorTiltable_hpp

#include <limits>
#include "common/Common.hpp"
#include "physics/Environment.hpp"
#include "common/FirstOrderFilter.hpp"
#include "physics/PhysicsBodyVertex.hpp"
#include "vehicles/multirotor/RotorActuator.hpp"
#include "RotorTiltableParams.hpp"

namespace msr
{
namespace airlib
{

    //RotorTiltable inherits from RotorActuator. It is a rotor that is allowed to rotate
    //about a single axis with specified range.

    class RotorTiltable : public RotorActuator
    {
    public: //types
        struct TiltOutput
        {
            Output rotor_output;
            real_T angle_signal_filtered;
            real_T angle_signal_input;
            real_T angle;
            real_T angle_from_vertical;
            bool is_fixed;
        };

    public: //methods
        RotorTiltable()
        {
            //allow default constructor with later call for initialize
        }
        RotorTiltable(const Vector3r& position, const Vector3r& normal_nominal,
                      RotorTurningDirection turning_direction, bool is_fixed, const Vector3r& rotation_axis,
                      const real_T max_angle, const RotorTiltableParams& params, const Environment* environment, uint id = -1)
        {
            initialize(position, normal_nominal, turning_direction, is_fixed, rotation_axis, max_angle, params, environment, id);
        }

        void initialize(const Vector3r& position, const Vector3r& normal_nominal,
                        RotorTurningDirection turning_direction, bool is_fixed, const Vector3r& rotation_axis,
                        const real_T max_angle, const RotorTiltableParams& params, const Environment* environment, uint id = -1)
        {
            normal_nominal_ = normal_nominal;
            normal_nominal_.normalize();
            is_fixed_ = is_fixed; //don't allow rotor to rotate if is_fixed_ is true
            rotation_axis_ = rotation_axis; //this gets ignored if is_fixed_ is true
            rotation_axis_.normalize();
            max_angle_ = max_angle;
            tilt_params_ = params;
            environment_ = environment;

            angle_signal_filter_.initialize(params.angle_signal_filter_tc, 0, 0);
            angle_filter_.initialize(params.angle_filter_tc, 0, 0);
            airspeed_ = 0.0f;

            RotorActuator::initialize(position, normal_nominal, turning_direction, params.rotor_params, environment, id);
            initializeTiltOutput(is_fixed);
        }

        void initializeTiltOutput(const bool is_fixed)
        {
            tilt_output_.angle_signal_filtered = 0;
            tilt_output_.angle_signal_input = 0;
            tilt_output_.angle = 0;
            tilt_output_.angle_from_vertical = VectorMath::sgn(normal_nominal_(0)) * std::acos(-normal_nominal_(2));
            tilt_output_.is_fixed = is_fixed;
        }

        //-1 to 1, will be scaled to -max_angle_, +max_angle_
        void setAngleSignal(real_T angle_signal)
        {
            if (!is_fixed_) {
                angle_signal_filter_.setInput(Utils::clip(angle_signal, -1.0f, 1.0f));
            }
        }

        void setAirspeedRotor(const Vector3r& airspeed_body_vector)
        {
            airspeed_ = normal_current_.dot(airspeed_body_vector);
        }

        //allows manually setting tilt from client
        void overwriteTilt(const real_T angle, bool spin_props)
        {
            if (!is_fixed_) {
                tilt_output_.angle_from_vertical = angle;
            }
            // speed isn't being controlled, so set to arbitrary constant value
            if (spin_props) {
                tilt_output_.rotor_output.speed = tilt_params_.rotor_params.max_speed * 0.7;
            }

            // zero these out since they won't be used to avoid confusion when viewing output
            tilt_output_.angle = 0.0f;
            tilt_output_.angle_signal_input = 0.0f;
            tilt_output_.angle_signal_filtered = 0.0f;
            tilt_output_.rotor_output.thrust = 0.0f;
            tilt_output_.rotor_output.torque_scaler = 0.0f;
            tilt_output_.rotor_output.control_signal_input = 0.0f;
            tilt_output_.rotor_output.control_signal_filtered = 0.0f;

            // compute angle from nominal given angle from vertical
            // probably not worth the extra computation
            // real_T angle_nom_from_vert = VectorMath::sgn(normal_nominal_(0)) * std::acos(-normal_nominal_(2));
            // AngleAxisr rotate_cv = AngleAxisr(angle, rotation_axis_);
            // AngleAxisr rotate_nc = AngleAxisr(-angle_nom_from_vert, rotation_axis_);
            // Vector3r normal = rotate_cv * rotate_nc * normal_nominal_;
            // tilt_output_.angle = std::acos( normal.dot(normal_nominal_) / ( normal.norm() * normal_nominal_.norm() ) );
        }

        TiltOutput getOutput() const
        {
            return tilt_output_;
        }

        //*** Start: UpdatableState implementation ***//
        virtual void resetImplementation() override
        {
            RotorActuator::resetImplementation();

            angle_signal_filter_.reset();
            angle_filter_.reset();
            normal_current_ = normal_nominal_;
            setNormal(normal_nominal_);
            airspeed_ = 0.0f;

            setTiltOutput();
        }

        virtual void update() override
        {
            //call rotor update
            RotorActuator::update();

            //update tilt output
            setTiltOutput();

            //update angle filters
            if (!is_fixed_) {
                angle_signal_filter_.update();
                angle_filter_.setInput(max_angle_ * angle_signal_filter_.getOutput());
                angle_filter_.update();

                //calculate new normal and set normal in PhysicsBodyVertex
                AngleAxisr rotate = AngleAxisr(angle_filter_.getOutput(), rotation_axis_);
                normal_current_ = rotate * normal_nominal_;
                setNormal(normal_current_);
            }
        }

        virtual void reportState(StateReporter& reporter) override
        {
            reporter.writeValue("Dir", static_cast<int>(tilt_output_.rotor_output.turning_direction));
            reporter.writeValue("Ctrl-in", tilt_output_.rotor_output.control_signal_input);
            reporter.writeValue("Ctrl-fl", tilt_output_.rotor_output.control_signal_filtered);
            reporter.writeValue("speed", tilt_output_.rotor_output.speed);
            reporter.writeValue("thrust", tilt_output_.rotor_output.thrust);
            reporter.writeValue("torque_scaler", tilt_output_.rotor_output.torque_scaler);
            reporter.writeValue("Angl-in", tilt_output_.angle_signal_input);
            reporter.writeValue("Angl-fl", tilt_output_.angle_signal_filtered);
            reporter.writeValue("Angle", tilt_output_.angle);
            reporter.writeValue("Fixed", static_cast<int>(tilt_output_.is_fixed));
        }
        //*** End: UpdatableState implementation ***//

    protected:
        //override RotorActuator's setWrench function using TiltOutput
        virtual void setWrench(Wrench& wrench) override
        {
            if (tilt_params_.use_simple_rotor_model) {
                RotorActuator::setWrench(wrench);
            }
            else {
                Vector3r normal = getNormal();
                wrench.force = normal * tilt_output_.rotor_output.thrust;
                wrench.torque = normal * tilt_output_.rotor_output.torque_scaler;
            }
        }

    private: //methods
        void setTiltOutput()
        {
            //populate rotor_output with output from RotorActuator
            tilt_output_.rotor_output = RotorActuator::getOutput();

            //if we want to use more complicated rotor model, need to modify thrust and torque outputs
            if (!tilt_params_.use_simple_rotor_model) {
                if (tilt_output_.rotor_output.speed > 0.0f) {
                    calculateThrustTorque(tilt_output_);
                }
                //else {
                //    tilt_output_.rotor_output.thrust = 0.0f;
                //    tilt_output_.rotor_output.torque_scaler = 0.0f;
                //}
            }

            tilt_output_.angle_signal_filtered = angle_signal_filter_.getOutput();
            tilt_output_.angle_signal_input = angle_signal_filter_.getInput();
            tilt_output_.angle = angle_filter_.getOutput();
            Vector3r normal = getNormal();
            tilt_output_.angle_from_vertical = VectorMath::sgn(normal(0)) * std::acos(-normal(2));
        }

        //this more complicated rotor model is necessary because it more acurately calculates the effects
        //of airspeed on the thrust a rotor is able to produce. It wouldn't really matter for multirotors,
        //but is very important for fixedwing vehicles which travel at high airspeeds
        void calculateThrustTorque(TiltOutput& output)
        {
            //motor model from "Small Unmanned Aircraft: Theory and Practice", supplement
            //see https://uavbook.byu.edu/lib/exe/fetch.php?media=uavbook_supplement.pdf, pg. 8
            const real_T& throttle = output.rotor_output.control_signal_filtered;
            const RotorTurningDirection& turn_dir = output.rotor_output.turning_direction;
            const RotorTiltableParams& p = tilt_params_;
            const real_T rho = environment_->getState().air_density;

            real_T v_in = p.max_voltage * throttle;
            real_T a = p.CQ0 * rho * pow(p.prop_diameter, 5) / pow(2 * M_PIf, 2);

            real_T b1 = airspeed_ * p.CQ1 * rho * pow(p.prop_diameter, 4) / (2 * M_PIf);
            real_T b2 = pow(p.motor_KQ, 2) / p.motor_resistance;
            real_T b = b1 + b2;

            real_T c1 = pow(airspeed_, 2) * p.CQ2 * rho * pow(p.prop_diameter, 3);
            real_T c2 = -v_in * p.motor_KQ / p.motor_resistance;
            real_T c3 = p.motor_KQ * p.no_load_current;
            real_T c = c1 + c2 + c3;

            real_T Omega_op = (-b + sqrt(pow(b, 2) - 4 * a * c)) / (2 * a);

            real_T J_op = 2 * M_PIf * airspeed_ / (Omega_op * p.prop_diameter);
            real_T CT = p.CT2 * pow(J_op, 2) + p.CT1 * J_op + p.CT0;
            real_T CQ = p.CQ2 * pow(J_op, 2) + p.CQ1 * J_op + p.CQ0;
            real_T n = Omega_op / (2 * M_PIf);
            real_T T_p = rho * pow(n, 2) * pow(p.prop_diameter, 4) * CT;
            real_T Q_p = rho * pow(n, 2) * pow(p.prop_diameter, 5) * CQ;

            output.rotor_output.thrust = T_p;
            //output.rotor_output.torque_scaler = Q_p * static_cast<int>(turn_dir);
            output.rotor_output.torque_scaler = 0.0;
        }

    private: //fields
        Vector3r normal_nominal_;
        Vector3r normal_current_;
        bool is_fixed_;
        Vector3r rotation_axis_;
        real_T max_angle_;
        RotorTiltableParams tilt_params_;
        FirstOrderFilter<real_T> angle_signal_filter_; //first order filter for rotor angle signal
        FirstOrderFilter<real_T> angle_filter_; //first order filter for actual rotor angle (much slower)
        TiltOutput tilt_output_;
        // Vector3r airspeed_body_vector_;
        real_T airspeed_;

        const Environment* environment_ = nullptr;
    };

}
} //namespace
#endif
