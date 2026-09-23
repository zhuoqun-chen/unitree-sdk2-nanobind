// Stub unitree_sdk2: unitree_hg LowState_ (field-compatible stub).
#pragma once

#include <array>
#include <cstdint>

#include <unitree/idl/hg/IMUState_.hpp>
#include <unitree/idl/hg/MotorState_.hpp>
#include <unitree/idl/hg/_stub_macros.hpp>

namespace unitree_hg {
namespace msg {
namespace dds_ {

struct LowState_ {
  UT_FIELD(version, std::array<uint32_t, 2>)
  UT_FIELD(mode_pr, uint8_t)
  UT_FIELD(mode_machine, uint8_t)
  UT_FIELD(tick, uint32_t)
  UT_FIELD(imu_state, IMUState_)
  UT_FIELD(motor_state, std::array<MotorState_, 35>)
  UT_FIELD(wireless_remote, std::array<uint8_t, 40>)
  UT_FIELD(reserve, std::array<uint32_t, 4>)
  UT_FIELD(crc, uint32_t)
};

}  // namespace dds_
}  // namespace msg
}  // namespace unitree_hg
