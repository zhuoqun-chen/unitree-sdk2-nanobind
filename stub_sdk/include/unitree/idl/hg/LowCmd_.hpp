// Stub unitree_sdk2: unitree_hg LowCmd_ (field-compatible stub).
//
// 35 motor slots to match the real G1 ``unitree_hg`` layout (G1 fills the first
// 29). ``crc`` is the final member so a CRC computed over every preceding 32-bit
// word matches the real wire contract.
#pragma once

#include <array>
#include <cstdint>

#include <unitree/idl/hg/MotorCmd_.hpp>
#include <unitree/idl/hg/_stub_macros.hpp>

namespace unitree_hg {
namespace msg {
namespace dds_ {

struct LowCmd_ {
  UT_FIELD(mode_pr, uint8_t)
  UT_FIELD(mode_machine, uint8_t)
  UT_FIELD(motor_cmd, std::array<MotorCmd_, 35>)
  UT_FIELD(reserve, std::array<uint32_t, 4>)
  UT_FIELD(crc, uint32_t)
};

}  // namespace dds_
}  // namespace msg
}  // namespace unitree_hg
