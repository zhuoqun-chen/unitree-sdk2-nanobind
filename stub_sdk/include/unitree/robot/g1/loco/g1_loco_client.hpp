// Stub unitree_sdk2: G1 loco client (no service to talk to).
//
// Mirrors only the read-only surface the binding uses (SetTimeout/Init plus the
// two FSM getters). The real client also exposes SetFsmId, Damp, StandUp, Move
// and friends; the binding does not wrap those, so the stub does not model them.
//
// Reports FSM 1 (Damp), which is the inert resting state, so a stub build looks
// like a robot that is powered but holding still. That matches the rest of the
// stub, whose LowState is a stationary synthetic pose.
#pragma once

#include <cstdint>

namespace unitree {
namespace robot {
namespace g1 {

class LocoClient {
 public:
  void SetTimeout(float /*timeout*/) {}
  void Init() {}
  int32_t GetFsmId(int &fsm_id) {
    fsm_id = 1;  // Damp
    return 0;
  }
  int32_t GetFsmMode(int &fsm_mode) {
    fsm_mode = 0;
    return 0;
  }

  // Setters: accepted and dropped, like the stub's publisher writes.
  int32_t SetFsmId(int /*fsm_id*/) { return 0; }
  int32_t Damp() { return SetFsmId(1); }
  int32_t StandUp() { return SetFsmId(4); }
  int32_t Start() { return SetFsmId(500); }
  int32_t ZeroTorque() { return SetFsmId(0); }
};

}  // namespace g1
}  // namespace robot
}  // namespace unitree
