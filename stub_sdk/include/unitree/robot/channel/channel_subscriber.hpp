// Stub unitree_sdk2: DDS subscriber.
//
// Real DDS delivers messages from the network on a background thread. With no
// network, this stub synthesizes one message (see stub_simulator.hpp) and
// delivers it synchronously to the handler at InitChannel time, so a poller sees
// a populated (stationary) state immediately and no background thread can
// outlive the subscriber.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include <unitree/stub_simulator.hpp>

namespace unitree {
namespace robot {

template <typename MSG>
class ChannelSubscriber {
 public:
  explicit ChannelSubscriber(const std::string &channelName)
      : mChannelName(channelName) {}

  void InitChannel(const std::function<void(const void *)> &handler,
                   int64_t /*queuelen*/ = 0) {
    mHandler = handler;
    MSG msg{};
    unitree_stub::stub_fill(msg, 1u);
    if (mHandler) mHandler(static_cast<const void *>(&msg));
  }

  void CloseChannel() {}

  int64_t GetLastDataAvailableTime() const { return -1; }

  const std::string &GetChannelName() const { return mChannelName; }

 private:
  std::string mChannelName;
  std::function<void(const void *)> mHandler;
};

template <typename MSG>
using ChannelSubscriberPtr = std::shared_ptr<ChannelSubscriber<MSG>>;

}  // namespace robot
}  // namespace unitree
