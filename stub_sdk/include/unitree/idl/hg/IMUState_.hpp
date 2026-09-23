// Stub unitree_sdk2: unitree_hg IMUState_ (field-compatible stub).
#pragma once

#include <array>
#include <cstdint>

#include <unitree/idl/hg/_stub_macros.hpp>

namespace unitree_hg {
namespace msg {
namespace dds_ {

struct IMUState_ {
  UT_FIELD(quaternion, std::array<float, 4>)     // wire order is w, x, y, z
  UT_FIELD(gyroscope, std::array<float, 3>)       // body angular velocity [rad/s]
  UT_FIELD(accelerometer, std::array<float, 3>)   // body linear acceleration [m/s^2]
  UT_FIELD(rpy, std::array<float, 3>)             // roll, pitch, yaw [rad]
  UT_FIELD(temperature, int16_t)
};

}  // namespace dds_
}  // namespace msg
}  // namespace unitree_hg
