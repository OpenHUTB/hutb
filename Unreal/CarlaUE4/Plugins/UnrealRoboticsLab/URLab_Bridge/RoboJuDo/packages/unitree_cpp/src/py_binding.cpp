#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>
#include <vector>
#include "unitree_controller.hpp"

namespace py = pybind11;

void bind_UnitreeConfig(py::module_& m) {
    py::class_<UnitreeConfig>(m, "UnitreeConfig")
        .def(py::init<>())
        .def_readwrite("net_if", &UnitreeConfig::net_if)
        .def_readwrite("control_dt", &UnitreeConfig::control_dt)
        .def_readwrite("msg_type", &UnitreeConfig::msg_type)
        .def_readwrite("control_mode", &UnitreeConfig::control_mode)
        .def_readwrite("hand_type", &UnitreeConfig::hand_type)
        .def_readwrite("lowcmd_topic", &UnitreeConfig::lowcmd_topic)
        .def_readwrite("lowstate_topic", &UnitreeConfig::lowstate_topic)
        .def_readwrite("enable_odometry", &UnitreeConfig::enable_odometry)
        .def_readwrite("sport_state_topic", &UnitreeConfig::sport_state_topic)
        .def_readwrite("stiffness", &UnitreeConfig::stiffness)
        .def_readwrite("damping", &UnitreeConfig::damping)
        .def_readwrite("num_dofs", &UnitreeConfig::num_dofs);
}

void bind_RobotState(py::module_& m) {
    py::class_<MotorState>(m, "MotorState")
        .def(py::init<size_t>())
        .def_readwrite("q", &MotorState::q)
        .def_readwrite("dq", &MotorState::dq)
        .def_readwrite("tau_est", &MotorState::tau_est);

    py::class_<ImuState>(m, "ImuState")
        .def(py::init<>())
        .def_readwrite("rpy", &ImuState::rpy)
        .def_readwrite("gyroscope", &ImuState::gyroscope)
        .def_readwrite("quaternion", &ImuState::quaternion)
        .def_readwrite("accelerometer", &ImuState::accelerometer);

    py::class_<RobotState>(m, "RobotState")
        .def(py::init<size_t>())
        .def_readwrite("tick", &RobotState::tick)
        .def_readwrite("motor_state", &RobotState::motor_state)
        .def_readwrite("imu_state", &RobotState::imu_state)
        // .def_readwrite("wireless_remote", &RobotState::wireless_remote);
        .def_property(
            "wireless_remote",
            [](const RobotState& self) {
                return py::bytes(reinterpret_cast<const char*>(self.wireless_remote), 40);
            },
            [](RobotState& self, py::bytes b) {
                std::string buf = b;
                if (buf.size() != 40) {
                    throw std::runtime_error("Expected 40 bytes for wireless_remote");
                }
                std::memcpy(self.wireless_remote, buf.data(), 40);
            });

    py::class_<SportState>(m, "SportState")
        .def(py::init<>())
        .def_readwrite("position", &SportState::position)
        .def_readwrite("velocity", &SportState::velocity);
}

// UnitreeController Class
void bind_UnitreeController(py::module_& m) {
    py::class_<UnitreeController>(m, "UnitreeController")
        .def(py::init([](py::dict cfg_dict) {
            UnitreeConfig cfg;

            cfg.net_if = cfg_dict["net_if"].cast<std::string>();
            cfg.control_dt = cfg_dict["control_dt"].cast<double>();
            cfg.msg_type = cfg_dict["msg_type"].cast<std::string>();
            cfg.hand_type = cfg_dict["hand_type"].cast<std::string>();
            cfg.lowcmd_topic = cfg_dict["lowcmd_topic"].cast<std::string>();
            cfg.lowstate_topic = cfg_dict["lowstate_topic"].cast<std::string>();
            cfg.sport_state_topic = cfg_dict["sport_state_topic"].cast<std::string>();
            cfg.enable_odometry = cfg_dict["enable_odometry"].cast<bool>();
            cfg.stiffness = cfg_dict["stiffness"].cast<std::vector<double>>();
            cfg.damping = cfg_dict["damping"].cast<std::vector<double>>();
            cfg.num_dofs = cfg_dict["num_dofs"].cast<unsigned short>();

            std::string mode_str = cfg_dict["control_mode"].cast<std::string>();
            if (mode_str == "position")
                cfg.control_mode = ControlMode::POSITION;
            else if (mode_str == "velocity")
                cfg.control_mode = ControlMode::VELOCITY;
            else if (mode_str == "torque")
                cfg.control_mode = ControlMode::TORQUE;
            else
                throw std::invalid_argument("Invalid control_mode");

            return new UnitreeController(cfg);
        }))
        .def(py::init<const UnitreeConfig&>(), py::arg("config"))
        .def("self_check", &UnitreeController::self_check)
        .def("step", &UnitreeController::step, py::arg("actions"))
        .def("step_hands", &UnitreeController::step_hands, py::arg("l_hand_pose"), py::arg("r_hand_pose"))
        .def("set_gains", &UnitreeController::set_gains, py::arg("stiffness"), py::arg("damping"))
        .def("shutdown", &UnitreeController::shutdown)
        .def("get_robot_state", &UnitreeController::get_robot_state)
        .def("get_sport_state", &UnitreeController::get_sport_state);
}

PYBIND11_MODULE(unitree_cpp, m) {
    m.doc() = "pybind11 bindings for UnitreeController";

    // bind_ControlMode(m);
    bind_UnitreeConfig(m);
    bind_RobotState(m);
    bind_UnitreeController(m);
}
