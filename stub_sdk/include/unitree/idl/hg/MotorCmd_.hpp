// Stub unitree_sdk2: unitree_hg MotorCmd_ (field-compatible stub).
//
// Mirrors the field accessors of the real
// ``unitree/idl/hg/MotorCmd_.hpp`` so the binding compiles against either.
#pragma once

#include <cstdint>

#include <unitree/idl/hg/_stub_macros.hpp>

namespace unitree_hg {
namespace msg {
namespace dds_ {

struct MotorCmd_ {
  UT_FIELD(mode, uint8_t)     // 0 = disable, 1 = enable (PD + feedforward)
  UT_FIELD(q, float)          // target joint position [rad]
  UT_FIELD(dq, float)         // target joint velocity [rad/s]
  UT_FIELD(tau, float)        // feedforward torque [N.m]
  UT_FIELD(kp, float)         // position gain
  UT_FIELD(kd, float)         // velocity gain
  UT_FIELD(reserve, uint32_t)
};

}  // namespace dds_
}  // namespace msg
}  // namespace unitree_hg
