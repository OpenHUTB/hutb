// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef msr_airlib_RovSimpleEstimator_hpp
#define msr_airlib_RovSimpleEstimator_hpp

#include "firmware/interfaces/CommonStructs.hpp"
#include "RovSimpleCommon.hpp"
#include "physics/Kinematics.hpp"
#include "physics/Environment.hpp"
#include "common/Common.hpp"

namespace msr
{
namespace airlib
{

    class RovSimpleEstimator : public Rov_simple::IStateEstimator
    {
    public:
        virtual ~RovSimpleEstimator() {}

        //for now we don't do any state estimation and use ground truth (i.e. assume perfect sensors)
        void setGroundTruthKinematics(const Kinematics::State* kinematics, const Environment* environment)
        {
            kinematics_ = kinematics;
            environment_ = environment;
        }

        virtual Rov_simple::Axis3r getAngles() const override
        {
            Rov_simple::Axis3r angles;
            VectorMath::toEulerianAngle(kinematics_->pose.orientation,
                                        angles.pitch(),
                                        angles.roll(),
                                        angles.yaw());

            //Utils::log(Utils::stringf("Ang Est:\t(%f, %f, %f)", angles.pitch(), angles.roll(), angles.yaw()));

            return angles;
        }

        virtual Rov_simple::Axis3r getAngularVelocity() const override
        {
            const auto& anguler = kinematics_->twist.angular;

            Rov_simple::Axis3r conv;
            conv.x() = anguler.x();
            conv.y() = anguler.y();
            conv.z() = anguler.z();

            return conv;
        }

        virtual Rov_simple::Axis3r getPosition() const override
        {
            return RovSimpleCommon::toAxis3r(kinematics_->pose.position);
        }

        virtual Rov_simple::Axis3r transformToBodyFrame(const Rov_simple::Axis3r& world_frame_val) const override
        {
            const Vector3r& vec = RovSimpleCommon::toVector3r(world_frame_val);
            const Vector3r& trans = VectorMath::transformToBodyFrame(vec, kinematics_->pose.orientation);
            return RovSimpleCommon::toAxis3r(trans);
        }

        virtual Rov_simple::Axis3r getLinearVelocity() const override
        {
            return RovSimpleCommon::toAxis3r(kinematics_->twist.linear);
        }

        virtual Rov_simple::Axis4r getOrientation() const override
        {
            return RovSimpleCommon::toAxis4r(kinematics_->pose.orientation);
        }

        virtual Rov_simple::GeoPoint getGeoPoint() const override
        {
            return RovSimpleCommon::toSimpleFlightGeoPoint(environment_->getState().geo_point);
        }

        virtual Rov_simple::GeoPoint getHomeGeoPoint() const override
        {
            return RovSimpleCommon::toSimpleFlightGeoPoint(environment_->getHomeGeoPoint());
        }

        virtual Rov_simple::KinematicsState getKinematicsEstimated() const override
        {
            Rov_simple::KinematicsState state;
            state.position = getPosition();
            state.orientation = getOrientation();
            state.linear_velocity = getLinearVelocity();
            state.angular_velocity = getAngularVelocity();
            state.linear_acceleration = RovSimpleCommon::toAxis3r(kinematics_->accelerations.linear);
            state.angular_acceleration = RovSimpleCommon::toAxis3r(kinematics_->accelerations.angular);

            return state;
        }

    private:
        const Kinematics::State* kinematics_;
        const Environment* environment_;
    };

}
} //namespace
#endif
