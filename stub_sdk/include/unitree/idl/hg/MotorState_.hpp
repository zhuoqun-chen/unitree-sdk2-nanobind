// Stub unitree_sdk2: unitree_hg MotorState_ (field-compatible stub).
//
// Only the fields the binding reads back are modelled (mode/q/dq/ddq/tau_est);
// the real type carries extra telemetry (temperature, voltage, sensor words)
// that the binding does not expose, so they are omitted from the stub.
#pragma once

#include <cstdint>

#include <unitree/idl/hg/_stub_macros.hpp>

namespace unitree_hg {
namespace msg {
namespace dds_ {

struct MotorState_ {
  UT_FIELD(mode, uint8_t)
  UT_FIELD(q, float)        // measured joint position [rad]
  UT_FIELD(dq, float)       // measured joint velocity [rad/s]
  UT_FIELD(ddq, float)      // measured joint acceleration [rad/s^2]
  UT_FIELD(tau_est, float)  // estimated joint torque [N.m]
  UT_FIELD(reserve, uint32_t)
};

}  // namespace dds_
}  // namespace msg
}  // namespace unitree_hg
