// Stub unitree_sdk2: motion-switcher client (no service to talk to).
//
// Mirrors the real ``b2::MotionSwitcherClient`` surface the binding uses. The
// real client releases the onboard high-level controller so a policy can drive
// ``rt/lowcmd`` directly; the stub reports "no mode active" so a release loop
// exits at once.
#pragma once

#include <cstdint>
#include <string>

namespace unitree {
namespace robot {
namespace b2 {

class MotionSwitcherClient {
 public:
  void SetTimeout(float /*timeout*/) {}
  void Init() {}
  // Leaves ``name`` empty so the caller's release loop terminates immediately.
  int32_t CheckMode(std::string & /*form*/, std::string & /*name*/) { return 0; }
  int32_t ReleaseMode() { return 0; }
  int32_t SelectMode(const std::string & /*name*/) { return 0; }
};

}  // namespace b2
}  // namespace robot
}  // namespace unitree
