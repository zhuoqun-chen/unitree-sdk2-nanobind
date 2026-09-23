// Stub unitree_sdk2: synthetic data for hardware-free runs.
//
// Without real DDS there is no robot on the wire, so the stub subscriber
// (channel_subscriber.hpp) synthesizes a message and hands it straight to the
// registered callback at subscription time. The synthesized LowState is a
// stationary robot: zeroed joints, identity base orientation, a valid CRC.
// Delivery is synchronous (no background thread), so a callback can never
// outlive the object that registered it.
#pragma once

#include <cstdint>

#include <unitree/idl/hg/IMUState_.hpp>
#include <unitree/idl/hg/LowState_.hpp>

namespace unitree_stub {

// Unitree CRC-32 (poly 0x04c11db7, init 0xFFFFFFFF); mirrors the SDK's
// crc32_core so a synthesized LowState carries a CRC the same way the robot does.
inline uint32_t crc32(const uint32_t *ptr, uint32_t len) {
  uint32_t crc = 0xFFFFFFFF;
  const uint32_t poly = 0x04c11db7;
  for (uint32_t i = 0; i < len; ++i) {
    uint32_t xbit = 1u << 31;
    uint32_t data = ptr[i];
    for (uint32_t bit = 0; bit < 32; ++bit) {
      crc = (crc & 0x80000000) ? ((crc << 1) ^ poly) : (crc << 1);
      if (data & xbit) crc ^= poly;
      xbit >>= 1;
    }
  }
  return crc;
}

// Customization point: the generic version leaves a default-constructed message
// untouched; the LowState overload fills a plausible stationary sample.
template <typename MSG>
inline void stub_fill(MSG & /*msg*/, uint32_t /*tick*/) {}

inline void stub_fill(unitree_hg::msg::dds_::LowState_ &s, uint32_t tick) {
  s.tick() = (tick == 0) ? 1u : tick;  // a real consumer rejects tick == 0
  s.mode_machine() = 4;
  s.imu_state().quaternion() = {1.0f, 0.0f, 0.0f, 0.0f};  // identity (w, x, y, z)
  // joints and wireless_remote stay zero-initialized.
  // crc is the final member; CRC every preceding 32-bit word.
  s.crc() = crc32(reinterpret_cast<const uint32_t *>(&s), (sizeof(s) >> 2) - 1);
}

}  // namespace unitree_stub
