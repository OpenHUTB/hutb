#include "unitree_controller.hpp"
#include <stdexcept>
#include <cstddef>
#include <iostream>

using std::size_t;

inline uint32_t Crc32Core(uint32_t* ptr, uint32_t len) {
    uint32_t xbit = 0;
    uint32_t data = 0;
    uint32_t CRC32 = 0xFFFFFFFF;
    const uint32_t dwPolynomial = 0x04c11db7;
    for (uint32_t i = 0; i < len; i++) {
        xbit = 1 << 31;
        data = ptr[i];
        for (uint32_t bits = 0; bits < 32; bits++) {
            if (CRC32 & 0x80000000) {
                CRC32 <<= 1;
                CRC32 ^= dwPolynomial;
            } else
                CRC32 <<= 1;
            if (data & xbit)
                CRC32 ^= dwPolynomial;

            xbit >>= 1;
        }
    }
    return CRC32;
};

UnitreeController::UnitreeController(const UnitreeConfig& cfg)
    : cfg_(cfg),
      stiffness_(cfg.stiffness),
      damping_(cfg.damping),
      num_dofs_(cfg.num_dofs),
      mode_pr_(Mode::PR),
      mode_machine_(0) {
    if (cfg.hand_type == "Dex-3") {
        num_dofs_hand_ = 7;
    } else if (cfg.hand_type == "NONE") {
        num_dofs_hand_ = 0;
    } else {
        throw std::runtime_error("Unsupported hand type: " + cfg.hand_type);
    }
    std::cout << cfg.hand_type << " hand with " << num_dofs_hand_ << " DOFs." << std::endl;

    ChannelFactory::Instance()->Init(0, cfg_.net_if);
    std::cout << "UnitreeController initialized with network interface: " << cfg_.net_if << std::endl;

    // try to shutdown motion control-related service
    msc_ = std::make_shared<unitree::robot::b2::MotionSwitcherClient>();
    msc_->SetTimeout(5.0f);
    msc_->Init();
    std::string form, name;
    while (msc_->CheckMode(form, name), !name.empty()) {
        if (msc_->ReleaseMode())
            std::cout << "Failed to switch to Release Mode\n";
        sleep(1);
    }
    std::cout << "Motion control service shutdown successfully." << std::endl;

    // create publisher
    lowcmd_publisher_.reset(new ChannelPublisher<LowCmd_>(cfg_.lowcmd_topic));  // TODO: switch Cmd Type
    lowcmd_publisher_->InitChannel();
    lowstate_subscriber_.reset(new ChannelSubscriber<LowState_>(cfg_.lowstate_topic));
    lowstate_subscriber_->InitChannel(std::bind(&UnitreeController::LowStateHandler, this, std::placeholders::_1), 1);
    // imutorso_subscriber_.reset(new ChannelSubscriber<IMUState_>(HG_IMU_TORSO));
    // imutorso_subscriber_->InitChannel(std::bind(&UnitreeController::imuTorsoHandler, this, std::placeholders::_1), 1);

    if (cfg_.enable_odometry) {
        std::cout << "Odometry enabled, subscribing to sport state topic: " << cfg_.sport_state_topic << std::endl;
        estimate_state_subscriber.reset(new ChannelSubscriber<SportModeState_>(cfg_.sport_state_topic));
        estimate_state_subscriber->InitChannel(std::bind(&UnitreeController::SportStateHandler, this, std::placeholders::_1), 1);
    } else {
        std::cout << "Odometry disabled." << std::endl;
    }

    // std::string sub_namespace = "rt/dex3/left/state";
    // unitree_hg::msg::dds_::HandState_ state;
    // handstate_subscriber.reset(new unitree::robot::ChannelSubscriber<unitree_hg::msg::dds_::HandState_>(sub_namespace));

    // create threads
    command_writer_ptr_ = CreateRecurrentThreadEx("command_writer", UT_CPU_ID_NONE, uint(cfg.control_dt * 1e6), &UnitreeController::LowCommandWriter, this);

    handcmd_left_publisher_.reset(new ChannelPublisher<HandCmd_>("rt/dex3/left/cmd"));
    handcmd_left_publisher_->InitChannel();
    handcmd_right_publisher_.reset(new ChannelPublisher<HandCmd_>("rt/dex3/right/cmd"));
    handcmd_right_publisher_->InitChannel();
    handcmd_writer_ptr_ = CreateRecurrentThreadEx("handcmd_writer", UT_CPU_ID_NONE, uint(cfg.control_dt * 1e6 * 5), &UnitreeController::HandCommandWriter, this);

    init_done_ = true;
}

UnitreeController::~UnitreeController() {
}

bool UnitreeController::self_check() {
    if (!init_done_) {
        std::cerr << "UnitreeController not initialized properly." << std::endl;
        return false;
    }
    try {
        RobotState robot_state = get_robot_state();
        if (robot_state.tick == 0) {
            std::cerr << "Robot state tick is zero, no data received." << std::endl;
            return false;
        }
        if (cfg_.enable_odometry) {
            SportState sport_state = get_sport_state();
            if (sport_state.position.empty() || sport_state.velocity.empty()) {
                std::cerr << "Sport state data is empty." << std::endl;
                return false;
            }
        }
    } catch (const std::runtime_error& e) {
        std::cerr << "No data available: " << e.what() << std::endl;
        return false;
    }
    std::cout << "UnitreeController self-check passed." << std::endl;
    return true;
}

void UnitreeController::LowStateHandler(const void* message) {
    LowState_ low_state = *(const LowState_*)message;
    // std::cout << "LowState received: " << low_state.tick() << std::endl;
    if (low_state.crc() != Crc32Core((uint32_t*)&low_state, (sizeof(LowState_) >> 2) - 1)) {
        std::cout << "[ERROR] CRC Error" << std::endl;
        return;
    }
    // low_state_buffer_.SetData(low_state);

    RobotState robot_state_tmp(num_dofs_);

    robot_state_tmp.tick = low_state.tick();

    // get motor state
    // MotorState ms_tmp(num_dofs_);
    MotorState& ms_tmp = robot_state_tmp.motor_state;
    for (int i = 0; i < num_dofs_; ++i) {
        ms_tmp.q.at(i) = low_state.motor_state()[i].q();
        ms_tmp.dq.at(i) = low_state.motor_state()[i].dq();
        ms_tmp.tau_est.at(i) = low_state.motor_state()[i].tau_est();
        // if (low_state.motor_state()[i].motorstate() && i <= RightAnkleRoll)
        //     std::cout << "[ERROR] motor " << i << " with code " << low_state.motor_state()[i].motorstate() << "\n";
    }
    // motor_state_buffer_.SetData(ms_tmp);

    // get imu state
    // ImuState imu_tmp;
    ImuState& imu_tmp = robot_state_tmp.imu_state;
    imu_tmp.quaternion = low_state.imu_state().quaternion();
    imu_tmp.gyroscope = low_state.imu_state().gyroscope();
    imu_tmp.accelerometer = low_state.imu_state().accelerometer();
    imu_tmp.rpy = low_state.imu_state().rpy();
    // imu_state_buffer_.SetData(imu_tmp);

    memcpy(&robot_state_tmp.wireless_remote, &low_state.wireless_remote()[0], 40);
    // std::cout << "imu rpy: " << imu_tmp.rpy[0] << ", " << imu_tmp.rpy[1] << ", " << imu_tmp.rpy[2] << std::endl;

    robot_state_buffer_.SetData(robot_state_tmp);

    // update mode machine
    if (mode_machine_ != low_state.mode_machine()) {
        if (mode_machine_ == 0)
            std::cout << "G1 type: " << unsigned(low_state.mode_machine()) << std::endl;
        mode_machine_ = low_state.mode_machine();
    }
}

void UnitreeController::SportStateHandler(const void* message) {
    SportModeState_ estimator_state = *(const SportModeState_*)message;

    SportState sport_state_tmp;
    sport_state_tmp.position = estimator_state.position();
    sport_state_tmp.velocity = estimator_state.velocity();
    sport_state_buffer_.SetData(sport_state_tmp);
}

void UnitreeController::LowCommandWriter() {
    LowCmd_ dds_low_command;
    dds_low_command.mode_pr() = static_cast<uint8_t>(mode_pr_);
    dds_low_command.mode_machine() = mode_machine_;

    const std::shared_ptr<const MotorCommand> mc = motor_command_buffer_.GetData();
    if (mc) {
        // std::cout << "LowCommandWriter called with motor command data." << std::endl;
        for (size_t i = 0; i < num_dofs_; i++) {
            dds_low_command.motor_cmd().at(i).mode() = 1;  // 1:Enable, 0:Disable
            dds_low_command.motor_cmd().at(i).tau() = mc->tau_ff.at(i);
            dds_low_command.motor_cmd().at(i).q() = mc->q_target.at(i);
            dds_low_command.motor_cmd().at(i).dq() = mc->dq_target.at(i);
            dds_low_command.motor_cmd().at(i).kp() = mc->kp.at(i);
            dds_low_command.motor_cmd().at(i).kd() = mc->kd.at(i);
        }

        dds_low_command.crc() = Crc32Core((uint32_t*)&dds_low_command, (sizeof(dds_low_command) >> 2) - 1);
        lowcmd_publisher_->Write(dds_low_command);
    }
}

void UnitreeController::HandCommandWriter() {
    HandCmd_ dds_hand_command;

    dds_hand_command.motor_cmd().resize(num_dofs_hand_);

    const std::shared_ptr<const HandCommand> hc_l = hand_command_left_buffer_.GetData();
    if (hc_l) {
        // std::cout << "LowCommandWriter called with motor command data." << std::endl;
        for (size_t i = 0; i < num_dofs_hand_; i++) {
            dds_hand_command.motor_cmd().at(i).mode() = 1;  // 1:Enable, 0:Disable
            dds_hand_command.motor_cmd().at(i).tau() = hc_l->tau_ff.at(i);
            dds_hand_command.motor_cmd().at(i).q() = hc_l->q_target.at(i);
            dds_hand_command.motor_cmd().at(i).dq() = hc_l->dq_target.at(i);
            dds_hand_command.motor_cmd().at(i).kp() = hc_l->kp.at(i);
            dds_hand_command.motor_cmd().at(i).kd() = hc_l->kd.at(i);
        }

        handcmd_left_publisher_->Write(dds_hand_command);
    }
    const std::shared_ptr<const HandCommand> hc_r = hand_command_right_buffer_.GetData();
    if (hc_r) {
        // std::cout << "LowCommandWriter called with motor command data." << std::endl;
        for (size_t i = 0; i < num_dofs_hand_; i++) {
            dds_hand_command.motor_cmd().at(i).mode() = 1;  // 1:Enable, 0:Disable
            dds_hand_command.motor_cmd().at(i).tau() = hc_r->tau_ff.at(i);
            dds_hand_command.motor_cmd().at(i).q() = hc_r->q_target.at(i);
            dds_hand_command.motor_cmd().at(i).dq() = hc_r->dq_target.at(i);
            dds_hand_command.motor_cmd().at(i).kp() = hc_r->kp.at(i);
            dds_hand_command.motor_cmd().at(i).kd() = hc_r->kd.at(i);
        }

        handcmd_right_publisher_->Write(dds_hand_command);
    }
}

void UnitreeController::step(const std::vector<double>& actions) {
    if (actions.size() != num_dofs_) {
        throw std::runtime_error("actions size mismatch");
    }
    // std::cout << "UnitreeController step called with actions: ";

    // control the motors
    MotorCommand motor_command_tmp(num_dofs_);

    for (int i = 0; i < num_dofs_; ++i) {
        motor_command_tmp.kp.at(i) = stiffness_[i];
        motor_command_tmp.kd.at(i) = damping_[i];
        switch (cfg_.control_mode) {
            case ControlMode::POSITION:
                motor_command_tmp.q_target.at(i) = actions[i];
                break;
            case ControlMode::VELOCITY:
                motor_command_tmp.dq_target.at(i) = actions[i];
                break;
            case ControlMode::TORQUE:
                motor_command_tmp.tau_ff.at(i) = actions[i];
                break;
            default:
                throw std::runtime_error("Unknown control mode");
        }

        // motor_command_tmp.q_target.at(i) = 0.0;
        // motor_command_tmp.dq_target.at(i) = 0.0;
        // motor_command_tmp.tau_ff.at(i) = 0.0;
    }
    motor_command_buffer_.SetData(motor_command_tmp);
    LowCommandWriter(); // immediately send command
}

void UnitreeController::step_hands(const std::vector<double>& l_hand_pose, const std::vector<double>& r_hand_pose) {
    if (l_hand_pose.size() != num_dofs_hand_ || r_hand_pose.size() != num_dofs_hand_) {
        throw std::runtime_error("l_hand_pose or r_hand_pose size mismatch");
    }

    HandCommand hand_command_left_tmp(num_dofs_hand_);

    for (int i = 0; i < num_dofs_hand_; ++i) {
        hand_command_left_tmp.q_target.at(i) = l_hand_pose[i];
        hand_command_left_tmp.dq_target.at(i) = 0.0;
        hand_command_left_tmp.kp.at(i) = 1.5f;
        hand_command_left_tmp.kd.at(i) = 0.1f;
        hand_command_left_tmp.tau_ff.at(i) = 0.0f;
    }
    hand_command_left_buffer_.SetData(hand_command_left_tmp);

    HandCommand hand_command_right_tmp(num_dofs_hand_);
    for (int i = 0; i < num_dofs_hand_; ++i) {
        hand_command_right_tmp.q_target.at(i) = r_hand_pose[i];
        hand_command_right_tmp.dq_target.at(i) = 0.0;
        hand_command_right_tmp.kp.at(i) = 1.5f;
        hand_command_right_tmp.kd.at(i) = 0.1f;
        hand_command_right_tmp.tau_ff.at(i) = 0.0f;
    }
    hand_command_right_buffer_.SetData(hand_command_right_tmp);
    HandCommandWriter(); // immediately send command
}

void UnitreeController::set_gains(const std::vector<double>& stiffness, const std::vector<double>& damping) {
    if (stiffness.size() != num_dofs_ || damping.size() != num_dofs_) {
        throw std::runtime_error("stiffness or damping size mismatch");
    }
    stiffness_ = stiffness;
    damping_ = damping;

    std::cout << "Gains set: stiffness = [";
    for (const auto& s : stiffness_) {
        std::cout << s << " ";
    }
    std::cout << "], damping = [";
    for (const auto& d : damping_) {
        std::cout << d << " ";
    }
    std::cout << "]" << std::endl;
}

void UnitreeController::shutdown() {
    std::cout << "Shutting down UnitreeController..." << std::endl;
    set_gains(std::vector<double>(num_dofs_, 0.0), std::vector<double>(num_dofs_, 5.0));
    step(std::vector<double>(num_dofs_, 0.0));
}

RobotState UnitreeController::get_robot_state() {
    const std::shared_ptr<const RobotState> robot_state = robot_state_buffer_.GetData();

    if (robot_state) {
        return *robot_state;
    } else {
        throw std::runtime_error("Low state data is not available");
    }
}

SportState UnitreeController::get_sport_state() {
    const std::shared_ptr<const SportState> sport_state = sport_state_buffer_.GetData();

    if (sport_state) {
        return *sport_state;
    } else {
        throw std::runtime_error("Sport state data is not available");
    }
}

int main(int argc, char const* argv[]) {
    // Example usage of UnitreeController
    UnitreeConfig config;
    config.net_if = "enp13s0";
    config.control_dt = 0.1;
    config.msg_type = "hg";
    config.control_mode = ControlMode::POSITION;
    config.hand_type = "Dex-3";
    config.lowcmd_topic = "rt/lowcmd";
    config.lowstate_topic = "rt/lowstate";
    config.enable_odometry = false;
    config.sport_state_topic = "rt/odommodestate";
    config.stiffness = {1.0, 1.0, 1.0};  // Example stiffness values
    config.damping = {0.1, 0.1, 0.1};    // Example damping values
    config.num_dofs = 3;                 // Example number of DOFs

    UnitreeController controller(config);

    while (true)
        sleep(10);
    return 0;
}
