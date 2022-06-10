#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

using PeerData = std::vector<uint8_t>;
using PeerState = std::tuple<uint64_t, PeerData>;

class HauntedCoopClient
{
public:
  explicit HauntedCoopClient(const std::string& sessionId);
  ~HauntedCoopClient();

  void sendState(const PeerData& data);

  [[nodiscard]] std::map<uint64_t, PeerData> getStates() const;

  void start();

  void stop();

private:
  struct ClientImpl;

  mutable std::mutex m_statesMutex;
  std::unique_ptr<ClientImpl> impl;
  std::thread m_thread;

  void updateThread();
};
