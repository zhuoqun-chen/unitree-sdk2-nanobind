// Stub unitree_sdk2: DDS publisher (drops every write).
#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace unitree {
namespace robot {

template <typename MSG>
class ChannelPublisher {
 public:
  explicit ChannelPublisher(const std::string &channelName)
      : mChannelName(channelName) {}

  void InitChannel() {}

  bool Write(const MSG & /*msg*/, int64_t /*waitMicrosec*/ = 0) { return true; }

  void CloseChannel() {}

  const std::string &GetChannelName() const { return mChannelName; }

 private:
  std::string mChannelName;
};

template <typename MSG>
using ChannelPublisherPtr = std::shared_ptr<ChannelPublisher<MSG>>;

}  // namespace robot
}  // namespace unitree
