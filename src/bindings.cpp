// unitree-sdk2-bind: nanobind bindings for the Unitree SDK2 ``unitree_hg``
// (G1 humanoid) low-level channel API.
//
// One translation unit, compiled with the CPython Stable ABI (abi3) so a single
// wheel serves every CPython >= 3.12. It binds exactly what a low-level RL
// deploy loop needs: initialize the DDS channel, publish ``LowCmd`` (auto-CRC),
// poll the latest ``LowState``, and release the onboard motion service. Joint
// arrays cross the boundary as zero-copy ``nb::ndarray`` rather than per-element
// Python loops.
//
// The same source compiles against the real SDK or the bundled stub SDK
// (-DUSE_STUB_SDK=ON): both expose identical field accessors and channel
// classes, so nothing here is ``#ifdef``-ed except the ``__is_stub__`` flag.

#include <array>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/string.h>

#include <unitree/idl/hg/LowCmd_.hpp>
#include <unitree/idl/hg/LowState_.hpp>
#include <unitree/robot/b2/motion_switcher/motion_switcher_client.hpp>
#include <unitree/robot/channel/channel_factory.hpp>
#include <unitree/robot/g1/loco/g1_loco_client.hpp>
#include <unitree/robot/channel/channel_publisher.hpp>
#include <unitree/robot/channel/channel_subscriber.hpp>

namespace nb = nanobind;
using namespace nb::literals;

using LowCmd_ = unitree_hg::msg::dds_::LowCmd_;
using LowState_ = unitree_hg::msg::dds_::LowState_;

// G1 ``unitree_hg`` low-level message arrays carry 35 motor slots (G1 fills the
// first 29). Exposed as the module constant ``MOTOR_COUNT``.
static constexpr size_t kMotorCount = 35;

// Unitree CRC-32 (poly 0x04c11db7, init 0xFFFFFFFF), identical to the SDK's
// ``crc32_core``. Embedded here so the stub and real builds stay symmetric and
// the binding never depends on an SDK-only header.
static uint32_t crc32_core(const uint32_t *ptr, uint32_t len) {
  uint32_t crc = 0xFFFFFFFF;
  const uint32_t poly = 0x04c11db7;
  for (uint32_t i = 0; i < len; ++i) {
    uint32_t xbit = 1u << 31;
    uint32_t data = ptr[i];
    for (uint32_t bit = 0; bit < 32; ++bit) {
      crc = (crc & 0x80000000u) ? ((crc << 1) ^ poly) : (crc << 1);
      if (data & xbit) crc ^= poly;
      xbit >>= 1;
    }
  }
  return crc;
}

// ---- ndarray helpers ------------------------------------------------------

// A fresh, NumPy-owned 1-D float32 array (the C buffer is freed by the capsule).
using OwnedF32 = nb::ndarray<nb::numpy, float, nb::ndim<1>>;
static OwnedF32 owned_f32(const float *src, size_t n) {
  float *data = new float[n];
  std::memcpy(data, src, n * sizeof(float));
  nb::capsule owner(data,
                    [](void *p) noexcept { delete[] static_cast<float *>(p); });
  return OwnedF32(data, {n}, owner);
}

// Same, for the uint8 per-motor mode flags.
using OwnedU8 = nb::ndarray<nb::numpy, uint8_t, nb::ndim<1>>;
static OwnedU8 owned_u8(const uint8_t *src, size_t n) {
  uint8_t *data = new uint8_t[n];
  std::memcpy(data, src, n * sizeof(uint8_t));
  nb::capsule owner(data,
                    [](void *p) noexcept { delete[] static_cast<uint8_t *>(p); });
  return OwnedU8(data, {n}, owner);
}

// A read-only, C-contiguous 1-D float32 input view (no copy, no conversion).
using InF32 = nb::ndarray<const float, nb::ndim<1>, nb::c_contig, nb::device::cpu>;

// ---- transport wrappers ---------------------------------------------------

// Owns a typed ``LowCmd`` publisher. ``init_channel`` must run first.
class LowCmdPublisher {
 public:
  explicit LowCmdPublisher(const std::string &topic) : pub_(topic) {
    pub_.InitChannel();
  }

  // Fill the CRC (over every word but the trailing crc field, matching the wire
  // contract) and publish. The GIL is released around the DDS write.
  void write(LowCmd_ &cmd, bool compute_crc) {
    if (compute_crc) {
      cmd.crc() = crc32_core(reinterpret_cast<const uint32_t *>(&cmd),
                             (sizeof(LowCmd_) >> 2) - 1);
    }
    nb::gil_scoped_release release;
    pub_.Write(cmd);
  }

  void close() { pub_.CloseChannel(); }

 private:
  unitree::robot::ChannelPublisher<LowCmd_> pub_;
};

// Owns a typed ``LowState`` subscriber and double-buffers the latest sample.
// The DDS receive thread only touches C++ under a mutex (never Python), so there
// is no GIL hazard; Python polls ``latest()``.
class LowStateSubscriber {
 public:
  explicit LowStateSubscriber(const std::string &topic) : sub_(topic) {}

  void start(int64_t queue_len) {
    sub_.InitChannel([this](const void *msg) { this->on_msg(msg); }, queue_len);
  }

  std::optional<LowState_> latest() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!has_) return std::nullopt;
    return latest_;
  }

  bool has() {
    std::lock_guard<std::mutex> lock(mutex_);
    return has_;
  }

  void close() { sub_.CloseChannel(); }

 private:
  void on_msg(const void *msg) {
    const LowState_ *state = static_cast<const LowState_ *>(msg);
    std::lock_guard<std::mutex> lock(mutex_);
    latest_ = *state;
    has_ = true;
  }

  unitree::robot::ChannelSubscriber<LowState_> sub_;
  std::mutex mutex_;
  LowState_ latest_{};
  bool has_ = false;
};

// Releases the onboard high-level controller so a policy can drive rt/lowcmd.
class MotionSwitcher {
 public:
  void init(float timeout) {
    client_.SetTimeout(timeout);
    client_.Init();
  }

  // (form, name); an empty name means no high-level mode is active.
  std::pair<std::string, std::string> check_mode() {
    std::string form, name;
    client_.CheckMode(form, name);
    return {form, name};
  }

  int32_t release_mode() { return client_.ReleaseMode(); }

  // The counterpart to release_mode(): load a high-level service by name
  // ("ai", "normal", "advanced", "ai-w"). MOVES THE ROBOT -- the service takes
  // hold of the joints as soon as it loads, so the robot must be supported.
  int32_t select_mode(const std::string &name) { return client_.SelectMode(name); }

 private:
  unitree::robot::b2::MotionSwitcherClient client_;
};

// Read-only view of the G1 loco service's state machine. ``check_mode()`` above
// only says *which* high-level service holds the robot; this says what that
// service is currently doing (damping, zero-torque, ready to walk, ...).
//
// Deliberately wraps the getters ONLY. ``LocoClient`` also exposes SetFsmId,
// Damp, StandUp, Move and friends, which command the robot; those are out of
// scope here, so this class cannot move anything.
class LocoStateClient {
 public:
  void init(float timeout) {
    client_.SetTimeout(timeout);
    client_.Init();
  }

  // See FSM_IDS in the Python package for the id -> name mapping.
  int fsm_id() {
    int value = -1;
    int32_t rc = client_.GetFsmId(value);
    if (rc != 0)
      throw std::runtime_error("GetFsmId failed, rc=" + std::to_string(rc));
    return value;
  }

  int fsm_mode() {
    int value = -1;
    int32_t rc = client_.GetFsmMode(value);
    if (rc != 0)
      throw std::runtime_error("GetFsmMode failed, rc=" + std::to_string(rc));
    return value;
  }

 private:
  unitree::robot::g1::LocoClient client_;
};

// Drives the loco service's state machine: the software equivalent of the
// remote's bring-up buttons, for getting the robot standing before a policy
// run. THIS MOVES THE ROBOT. Kept separate from the read-only
// ``LocoStateClient`` on purpose, so the type name alone tells you whether a
// call site can move anything.
//
// Only works while a high-level service is loaded (select_mode). After
// release_mode() every call fails with rc 3104, so this is a bring-up tool,
// not a way to recover mid-run.
//
// NOTE: ``damp()`` is a convenience, NOT a replacement for the remote's
// L2+B. The remote must stay an independent path to damp, because software
// damp cannot help when the software itself has stopped running.
class AiModeSetupClient {
 public:
  void init(float timeout) {
    client_.SetTimeout(timeout);
    client_.Init();
  }

  // The three the remote's bring-up sequence sends, measured on a real G1:
  // L2+B -> 1, L2+UP -> 4, R1+X -> 500.
  int32_t damp() { return client_.Damp(); }            // fsm 1
  int32_t stand_up() { return client_.StandUp(); }      // fsm 4
  int32_t start() { return client_.Start(); }           // fsm 500
  int32_t zero_torque() { return client_.ZeroTorque(); }  // fsm 0

  // Escape hatch for the states without a named helper (2 squat, 3 sit).
  int32_t set_fsm_id(int fsm_id) { return client_.SetFsmId(fsm_id); }

 private:
  unitree::robot::g1::LocoClient client_;
};

// ---- module ---------------------------------------------------------------

NB_MODULE(_core, m) {
  m.doc() = "nanobind bindings for the Unitree SDK2 unitree_hg (G1) low-level API";
  m.attr("__is_stub__") = static_cast<bool>(UNITREE_BIND_IS_STUB);
  m.attr("MOTOR_COUNT") = kMotorCount;

  m.def(
      "init_channel",
      [](int32_t domain_id, const std::string &interface) {
        auto *factory = unitree::robot::ChannelFactory::Instance();
        if (interface.empty())
          factory->Init(domain_id);
        else
          factory->Init(domain_id, interface);
      },
      "domain_id"_a = 0, "interface"_a = "",
      "Initialize the DDS channel factory on (domain_id, network interface). "
      "Call once before creating any publisher or subscriber.");

  m.def(
      "crc32",
      [](nb::bytes data) {
        size_t n = data.size() / 4;
        return crc32_core(reinterpret_cast<const uint32_t *>(data.c_str()),
                          static_cast<uint32_t>(n));
      },
      "data"_a, "Unitree CRC-32 over a little-endian uint32 buffer.");

  nb::class_<LowCmd_>(m, "LowCmd",
                      "unitree_hg LowCmd_. Build one, fill the motor targets in "
                      "bulk, hand it to LowCmdPublisher.write().")
      .def(nb::init<>())
      .def_prop_rw(
          "mode_pr", [](LowCmd_ &c) { return c.mode_pr(); },
          [](LowCmd_ &c, uint8_t v) { c.mode_pr() = v; })
      .def_prop_rw(
          "mode_machine", [](LowCmd_ &c) { return c.mode_machine(); },
          [](LowCmd_ &c, uint8_t v) { c.mode_machine() = v; })
      .def_prop_rw(
          "crc", [](LowCmd_ &c) { return c.crc(); },
          [](LowCmd_ &c, uint32_t v) { c.crc() = v; })
      .def(
          "set_motors",
          [](LowCmd_ &cmd, InF32 q, InF32 dq, InF32 kp, InF32 kd, InF32 tau,
             uint8_t mode, int64_t num_motors) {
            size_t n = (num_motors >= 0) ? static_cast<size_t>(num_motors)
                                         : q.shape(0);
            if (n > kMotorCount) n = kMotorCount;
            const InF32 *cols[] = {&q, &dq, &kp, &kd, &tau};
            for (const InF32 *col : cols) {
              if (col->shape(0) < n)
                throw std::invalid_argument(
                    "set_motors: array shorter than num_motors");
            }
            auto &mc = cmd.motor_cmd();
            for (size_t i = 0; i < n; ++i) {
              auto &motor = mc[i];
              motor.mode() = mode;
              motor.q() = q(i);
              motor.dq() = dq(i);
              motor.kp() = kp(i);
              motor.kd() = kd(i);
              motor.tau() = tau(i);
            }
          },
          "q"_a, "dq"_a, "kp"_a, "kd"_a, "tau"_a, "mode"_a = uint8_t{1},
          "num_motors"_a = int64_t{-1},
          "Bulk-fill the first num_motors slots from float32 arrays "
          "(num_motors<0 uses len(q)).");

  nb::class_<LowState_>(m, "LowState",
                        "unitree_hg LowState_. Read joints + IMU as float32 "
                        "arrays; wireless_remote as raw bytes.")
      .def(nb::init<>())
      .def_prop_ro("mode_machine", [](LowState_ &s) { return s.mode_machine(); })
      .def_prop_ro("tick", [](LowState_ &s) { return s.tick(); })
      .def_prop_ro("crc", [](LowState_ &s) { return s.crc(); })
      .def_prop_ro("q",
                   [](LowState_ &s) {
                     float t[kMotorCount];
                     for (size_t i = 0; i < kMotorCount; ++i)
                       t[i] = s.motor_state()[i].q();
                     return owned_f32(t, kMotorCount);
                   },
                   nb::rv_policy::move)
      .def_prop_ro("dq",
                   [](LowState_ &s) {
                     float t[kMotorCount];
                     for (size_t i = 0; i < kMotorCount; ++i)
                       t[i] = s.motor_state()[i].dq();
                     return owned_f32(t, kMotorCount);
                   },
                   nb::rv_policy::move)
      .def_prop_ro("tau_est",
                   [](LowState_ &s) {
                     float t[kMotorCount];
                     for (size_t i = 0; i < kMotorCount; ++i)
                       t[i] = s.motor_state()[i].tau_est();
                     return owned_f32(t, kMotorCount);
                   },
                   nb::rv_policy::move)
      .def_prop_ro("motor_mode",
                   [](LowState_ &s) {
                     uint8_t t[kMotorCount];
                     for (size_t i = 0; i < kMotorCount; ++i)
                       t[i] = s.motor_state()[i].mode();
                     return owned_u8(t, kMotorCount);
                   },
                   nb::rv_policy::move)
      .def_prop_ro("quaternion",
                   [](LowState_ &s) {
                     return owned_f32(s.imu_state().quaternion().data(), 4);
                   },
                   nb::rv_policy::move)
      .def_prop_ro("gyroscope",
                   [](LowState_ &s) {
                     return owned_f32(s.imu_state().gyroscope().data(), 3);
                   },
                   nb::rv_policy::move)
      .def_prop_ro("accelerometer",
                   [](LowState_ &s) {
                     return owned_f32(s.imu_state().accelerometer().data(), 3);
                   },
                   nb::rv_policy::move)
      .def_prop_ro("rpy",
                   [](LowState_ &s) {
                     return owned_f32(s.imu_state().rpy().data(), 3);
                   },
                   nb::rv_policy::move)
      .def_prop_ro("wireless_remote", [](LowState_ &s) {
        return nb::bytes(reinterpret_cast<const char *>(s.wireless_remote().data()),
                         s.wireless_remote().size());
      });

  nb::class_<LowCmdPublisher>(m, "LowCmdPublisher")
      .def(nb::init<const std::string &>(), "topic"_a = "rt/lowcmd")
      .def("write", &LowCmdPublisher::write, "cmd"_a, "compute_crc"_a = true,
           "Publish a LowCmd, filling the CRC unless compute_crc=False.")
      .def("close", &LowCmdPublisher::close);

  nb::class_<LowStateSubscriber>(m, "LowStateSubscriber")
      .def(nb::init<const std::string &>(), "topic"_a = "rt/lowstate")
      .def("start", &LowStateSubscriber::start, "queue_len"_a = int64_t{10},
           "Subscribe and begin caching the latest LowState.")
      .def("latest", &LowStateSubscriber::latest,
           "The most recent LowState, or None if none has arrived.")
      .def("has", &LowStateSubscriber::has)
      .def("close", &LowStateSubscriber::close);

  nb::class_<MotionSwitcher>(m, "MotionSwitcherClient")
      .def(nb::init<>())
      .def("init", &MotionSwitcher::init, "timeout"_a = 5.0f)
      .def("check_mode", &MotionSwitcher::check_mode)
      .def("release_mode", &MotionSwitcher::release_mode)
      .def("select_mode", &MotionSwitcher::select_mode, "name"_a,
           "Load a high-level service by name (e.g. 'ai'). MOVES THE ROBOT.");

  nb::class_<LocoStateClient>(m, "LocoStateClient",
                              "Read-only view of the G1 loco service's state "
                              "machine. Getters only; it cannot command.")
      .def(nb::init<>())
      .def("init", &LocoStateClient::init, "timeout"_a = 5.0f)
      .def("fsm_id", &LocoStateClient::fsm_id,
           "Current FSM id (see FSM_IDS): 1 is damping, 0 is zero-torque.")
      .def("fsm_mode", &LocoStateClient::fsm_mode);

  nb::class_<AiModeSetupClient>(
      m, "AiModeSetupClient",
      "Bring the robot up under a loaded high-level service: the software "
      "equivalent of the remote's L2+B / L2+UP / R1+X. MOVES THE ROBOT.")
      .def(nb::init<>())
      .def("init", &AiModeSetupClient::init, "timeout"_a = 5.0f)
      .def("damp", &AiModeSetupClient::damp,
           "FSM 1. Convenience only -- keep the remote's L2+B as an "
           "independent damp path.")
      .def("stand_up", &AiModeSetupClient::stand_up, "FSM 4. MOVES THE ROBOT.")
      .def("start", &AiModeSetupClient::start,
           "FSM 500, operating stance. MOVES THE ROBOT.")
      .def("zero_torque", &AiModeSetupClient::zero_torque, "FSM 0.")
      .def("set_fsm_id", &AiModeSetupClient::set_fsm_id, "fsm_id"_a,
           "Raw SetFsmId for states without a named helper (2 squat, 3 sit).");
}
