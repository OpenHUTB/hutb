// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef msr_airlib_rov_body_params_hpp
#define msr_airlib_rov_body_params_hpp

#include "common/Common.hpp"
#include "RotorTiltableParams.hpp"
#include "AeroParams.hpp"
#include "sensors/SensorCollection.hpp"
#include "sensors/SensorFactory.hpp"
#include "vehicles/rov/api/RovApiBase.hpp"
namespace msr
{
namespace airlib
{

    class RovBodyParams
    {
        //All units are SI
    public: //types
        struct RotorTiltableConfiguration
        {
            Vector3r position; //relative to center of gravity of vehicle body
            Vector3r normal_nominal;
            Vector3r rotation_axis;
            
            bool is_fixed;
            real_T max_angle;
            RotorTurningDirection direction;
            RotorTiltableParams params; //each rotor may have different parameters if different motor types are used on different parts of the aircraft

            RotorTiltableConfiguration()
            {
            }
            RotorTiltableConfiguration(const Vector3r& position_val, const Vector3r& normal_nominal_val, const Vector3r& rotation_axis_val,
                                       const bool is_fixed_val, const real_T max_angle_val, const RotorTurningDirection direction_val, const RotorTiltableParams& params_val)
                : position{ position_val }, normal_nominal{ normal_nominal_val }, rotation_axis{ rotation_axis_val }, is_fixed{ is_fixed_val }, max_angle{ max_angle_val }, direction{ direction_val }, params{ params_val }
            {
            }
        };

        struct Params
        {   
        
            Vector3r added_mass_linear;
            Vector3r added_mass_angular;
            Vector3r damping_linear;
            Vector3r damping_angular;
            Vector3r damping_linear_q;
            Vector3r damping_angular_q;
            
            real_T off_z;
            uint rotor_count;
            vector<RotorTiltableConfiguration> rotor_configs;
            real_T mass;
            //vector<RotorPose> rotor_poses;
            Matrix3x3r inertia;
            //Vector3r body_box;
            real_T restitution = 0.55f; // value of 1 would result in perfectly elastic collisions, 0 would be completely inelastic.
            real_T friction = 0.5f;
            AeroParams aero_params;
        };

    protected: //must override by derived class
        virtual void setupParams() = 0;
        virtual const SensorFactory* getSensorFactory() const = 0;

    public: //interface
        virtual std::unique_ptr<RovApiBase> createRovApi() = 0;

        virtual ~RovBodyParams() = default;
        virtual void initialize(const AirSimSettings::VehicleSetting* vehicle_setting)
        {
            sensor_storage_.clear();
            sensors_.clear();

            setupParams();

            addSensorsFromSettings(vehicle_setting);
        }

        const Params& getParams() const
        {
            return params_;
        }
        Params& getParams()
        {
            return params_;
        }
        SensorCollection& getSensors()
        {
            return sensors_;
        }
        const SensorCollection& getSensors() const
        {
            return sensors_;
        }

        void addSensorsFromSettings(const AirSimSettings::VehicleSetting* vehicle_setting)
        {
            const auto& sensor_settings = vehicle_setting->sensors;

            getSensorFactory()->createSensorsFromSettings(sensor_settings, sensors_, sensor_storage_);
        }

    protected:
        //TODO: is there a way to read params in from vehicle_setting?
         /*
        static void computeInertiaMatrix(Matrix3x3r& inertia, const Vector3r& body_box, const vector<RotorPose>& rotor_poses,
                                         real_T box_mass, real_T motor_assembly_weight)
        {
            inertia = Matrix3x3r::Zero();

            //http://farside.ph.utexas.edu/teaching/336k/Newtonhtml/node64.html
            inertia(0, 0) = box_mass / 12.0f * (body_box.y() * body_box.y() + body_box.z() * body_box.z());
            inertia(1, 1) = box_mass / 12.0f * (body_box.x() * body_box.x() + body_box.z() * body_box.z());
            inertia(2, 2) = box_mass / 12.0f * (body_box.x() * body_box.x() + body_box.y() * body_box.y());

            for (size_t i = 0; i < rotor_poses.size(); ++i) {
                const auto& pos = rotor_poses.at(i).position;
                inertia(0, 0) += (pos.y() * pos.y() + pos.z() * pos.z()) * motor_assembly_weight;
                inertia(1, 1) += (pos.x() * pos.x() + pos.z() * pos.z()) * motor_assembly_weight;
                inertia(2, 2) += (pos.x() * pos.x() + pos.y() * pos.y()) * motor_assembly_weight;
            }
        }
         */
        void setupBlueROV2Heavy(Params& params)
        {
        
    

                /* Note: rotor_poses are built in this order:
                
                Vertical Motors:
                 x-axis
         (2) X   |    X (1)
                 |
            -------------- y-axis                  
                 |
         (3) X   |    X (4)
            
               Horizontal Motors:  Note: Horiztonal motor are oriented at 45 Degress
                 x-axis
            (6)/  |   \(5)
                  |
            -------------- y-axis                  
                  |
            (7)\  |   /(8)
            */
      
            // Define the rotors position, orientation and rotation direction
            
            
             params.rotor_count = 8;
             
             
            // A) Vertical Rotors (r1-r4)
            
            // r1 - front right rotor
            Vector3r r1_pos(0.1, 0.1, 0.0);   //position 
            Vector3r r1_norm(0.0, 0.0, -1.0); //direction (upwards)
            Vector3r r1_rot(0.0, -1.0, 0.0);  //Tilting axis (if it tilts)
            bool r1_fixed = true;             //No tilting 
            real_T r1_max = 0.0;     
            RotorTurningDirection r1_direction = RotorTurningDirection::RotorTurningDirectionCW;
            RotorTiltableParams r1_params = RotorTiltableParams();
            r1_params.rotor_params.calculateMaxThrust();
            RotorTiltableConfiguration r1_config(r1_pos, r1_norm, r1_rot, r1_fixed, r1_max, r1_direction, r1_params);
            params.rotor_configs.push_back(r1_config);

            //r2 - front left rotor
            Vector3r r2_pos(0.1, -0.1, 0.f);
            Vector3r r2_norm(0.f, 0.f, -1.0);
            Vector3r r2_rot(0.f, 1.f, 0.f);
            bool r2_fixed = true;
            real_T r2_max = 0.0;
            RotorTurningDirection r2_direction = RotorTurningDirection::RotorTurningDirectionCCW;
            RotorTiltableParams r2_params = RotorTiltableParams();
            r2_params.rotor_params.calculateMaxThrust();
            RotorTiltableConfiguration r2_config(r2_pos, r2_norm, r2_rot, r2_fixed, r2_max, r2_direction, r2_params);
            params.rotor_configs.push_back(r2_config);

            //r3 - rear left rotor
            Vector3r r3_pos(-0.1, -0.1, 0.f);    //position 
            Vector3r r3_norm(0.f, 0.f, -1.0);     //direction (upwards)
            Vector3r r3_rot(1.f, 0.f, 0.f);      //Tilting axis (if it tilts)
            bool r3_fixed = true;                //No tilting
            real_T r3_max = 0.f;
            RotorTurningDirection r3_direction = RotorTurningDirection::RotorTurningDirectionCCW;
            RotorTiltableParams r3_params = RotorTiltableParams();
            r3_params.rotor_params.calculateMaxThrust();
            RotorTiltableConfiguration r3_config(r3_pos, r3_norm, r3_rot, r3_fixed, r3_max, r3_direction, r3_params);
            params.rotor_configs.push_back(r3_config);
            
            
            //r4 - rear right rotor
            Vector3r r4_pos(-0.1, 0.1, 0.f);
            Vector3r r4_norm(0.f, 0.f, -1.0);
            Vector3r r4_rot(1.f, 0.f, 0.f); //this gets ignored
            bool r4_fixed = true;
            real_T r4_max = 0.f;
            RotorTurningDirection r4_direction = RotorTurningDirection::RotorTurningDirectionCW;
            RotorTiltableParams r4_params = RotorTiltableParams();
            r4_params.rotor_params.calculateMaxThrust();
            RotorTiltableConfiguration r4_config(r4_pos, r4_norm, r4_rot, r4_fixed, r4_max, r4_direction, r4_params);
            params.rotor_configs.push_back(r4_config);


            //Horizontal rotors   (r5-r6)

            //r5 - front right rotor
            Vector3r r5_pos(0.1, 0.1, 0.f);
            Vector3r r5_norm(0.5, -0.5, 0.0);
            Vector3r r5_rot(1.f, 0.f, 0.f); //this gets ignored
            bool r5_fixed = true;
            real_T r5_max = 0.f;
            RotorTurningDirection r5_direction = RotorTurningDirection::RotorTurningDirectionCW;


            RotorTiltableParams r5_params = RotorTiltableParams();
            r5_params.rotor_params.calculateMaxThrust();
            RotorTiltableConfiguration r5_config(r5_pos, r5_norm, r5_rot, r5_fixed, r5_max, r5_direction, r5_params);
            params.rotor_configs.push_back(r5_config);



            //r6 - front right rotor
            Vector3r r6_pos(0.1, -0.1, 0.f);
            Vector3r r6_norm(0.5, 0.5, 0.0);
            Vector3r r6_rot(1.f, 0.f, 0.f); //this gets ignored
            bool r6_fixed = true;
            real_T r6_max = 0.f;
            RotorTurningDirection r6_direction = RotorTurningDirection::RotorTurningDirectionCCW;


            RotorTiltableParams r6_params = RotorTiltableParams();
            r6_params.rotor_params.calculateMaxThrust();
            RotorTiltableConfiguration r6_config(r6_pos, r6_norm, r6_rot, r6_fixed, r6_max, r6_direction, r6_params);
            params.rotor_configs.push_back(r6_config);



            //r7 - front right rotor
            Vector3r r7_pos(-0.1, -0.1, 0.f);
            Vector3r r7_norm(-0.5, 0.5, 0.0);
            Vector3r r7_rot(1.f, 0.f, 0.f); //this gets ignored
            bool r7_fixed = true;
            real_T r7_max = 0.f;
            RotorTurningDirection r7_direction = RotorTurningDirection::RotorTurningDirectionCW;


            RotorTiltableParams r7_params = RotorTiltableParams();
            r7_params.rotor_params.calculateMaxThrust();
            RotorTiltableConfiguration r7_config(r7_pos, r7_norm, r7_rot, r7_fixed, r7_max, r7_direction, r7_params);
            params.rotor_configs.push_back(r7_config);


            //r8 - front right rotor
            Vector3r r8_pos(-0.1, 0.1, 0.f);
            Vector3r r8_norm(-0.5, -0.5, 0.0);
            Vector3r r8_rot(1.f, 0.f, 0.f); //this gets ignored
            bool r8_fixed = true;
            real_T r8_max = 0.f;
            RotorTurningDirection r8_direction = RotorTurningDirection::RotorTurningDirectionCCW;


            RotorTiltableParams r8_params = RotorTiltableParams();
            r8_params.rotor_params.calculateMaxThrust();
            RotorTiltableConfiguration r8_config(r8_pos, r8_norm, r8_rot, r8_fixed, r8_max, r8_direction, r8_params);
            params.rotor_configs.push_back(r8_config);






            //BlueROV2 parameters  
            //https://flex.flinders.edu.au/file/27aa0064-9de2-441c-8a17-655405d5fc2e/1/ThesisWu2018.pdf
            
            
            real_T rov_mass = 11.5f;   //Mass
            params.aero_params = AeroParams();     //Aerodynamics params 
         


            //params.off_z = 0.02;                //offset in z direction between the centre of bouyancy COB and centre of mass COM  
            params.off_z = 0.02;

            //Hydrodynamic cofeceients
            
            //Added Mass Coffecients 
            params.added_mass_linear.x() = -5.5f;  
            params.added_mass_linear.y() = -12.7f;
            params.added_mass_linear.z() = -14.57f;
   
            params.added_mass_angular.x() = -0.12f;
            params.added_mass_angular.y() = -0.12f;
            params.added_mass_angular.z() = -0.12f;
            
            
            params.mass = rov_mass ;   //Mass
            
            params.inertia << 0.30f, 0.0f, 0.0f,     //Inertia Matrix
                              0.0f, 0.30f, 0.0f,
                              0.0f, 0.0f, 0.30f;


            //Damping Coffecients 
            params.damping_linear.x() = -4.03f;    
            params.damping_linear.y() = -6.22f;
            params.damping_linear.z() = -5.18f;     
        
            params.damping_angular.x() = -0.07f;
            params.damping_angular.y() = -0.07f;
            params.damping_angular.z() = -0.07f; 
            
            params.damping_linear_q.x() = -18.18f;
            params.damping_linear_q.y() = -21.66f;
            params.damping_linear_q.z() = -36.99f;     
        
            params.damping_angular_q.x() = -1.55f;
            params.damping_angular_q.y() = -1.55f;
            params.damping_angular_q.z() = -1.55f;         
        

            /*                  
            params.body_box.x() = 0.180f;
            params.body_box.y() = 0.11f;
            params.body_box.z() = 0.040f;
            real_T box_mass = params.mass
            real_T motor_assembly_weight = 0.052f; // weight for TBS motors

            params.rotor_poses.clear();
            for (uint i = 0; i < 4; i++) {
                Quaternionr angle(AngleAxisr(arm_angles[i] * M_PIf / 180, unit_z));
                params.rotor_poses.emplace_back(VectorMath::rotateVector(Vector3r(arm_lengths[i], 0, rotor_z), angle, true), unit_z, rotor_directions[i]);
            };
            computeInertiaMatrix(params.inertia, params.body_box, params.rotor_poses, box_mass, motor_assembly_weight);
            */
        }

    private:
        Params params_;
        SensorCollection sensors_; //maintains sensor type indexed collection of sensors
        vector<shared_ptr<SensorBase>> sensor_storage_; //RAII for created sensors
    };
}
} //namespace
#endif
