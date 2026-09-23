// Stub unitree_sdk2: DDS channel factory (no-op singleton).
#pragma once

#include <cstdint>
#include <string>

namespace unitree {
namespace robot {

class ChannelFactory {
 public:
  static ChannelFactory *Instance() {
    static ChannelFactory instance;
    return &instance;
  }

  // Real signature: Init(int32_t domainId, const std::string& networkInterface).
  void Init(int32_t /*domainId*/, const std::string & /*nic*/ = "") {}
  void Release() {}
};

}  // namespace robot
}  // namespace unitree
