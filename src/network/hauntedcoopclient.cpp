#include "hauntedcoopclient.h"

// FIXME: this is a bad include path
#include "../launcher/networkconfig.h"

#include <array>
#include <boost/algorithm/string/split.hpp>
#include <boost/asio.hpp>
#include <boost/assert.hpp>
#include <boost/log/trivial.hpp>
#include <gsl/gsl-lite.hpp>
#include <utility>
#include <variant>

enum class ClientMessageId : uint8_t
{
  Login = 0,
  UpdateState = 1,
  StateQuery = 2,
  Failure = 3
};

enum class ServerMessageId : uint8_t
{
  ServerInfo = 0,
  Failure = 1,
  UpdateState = 2,
  FullSync = 3,
};

template<typename T>
void writeLE(std::vector<uint8_t>& msg, const T& data)
{
  for(size_t i = 0; i < sizeof(T); ++i)
  {
    msg.emplace_back(static_cast<uint8_t>(data >> (8u * i)));
  }
}

template<typename T>
T readLE(const uint8_t* data)
{
  T result{0};
  for(size_t i = 0; i < sizeof(T); ++i)
  {
    result |= static_cast<T>(data[i]) << (8u * i);
  }
  return result;
}

void writePascal(std::vector<uint8_t>& msg, const std::string& s)
{
  msg.reserve(msg.size() + s.length());
  msg.emplace_back(static_cast<uint8_t>(s.length()));
  for(const char c : s)
    msg.emplace_back(c);
}

void writePascal(std::vector<uint8_t>& msg, const std::vector<uint8_t>& s)
{
  BOOST_ASSERT(s.size() < 0x10000);
  msg.reserve(msg.size() + s.size());
  writeLE<uint16_t>(msg, s.size());
  for(const uint8_t c : s)
    msg.emplace_back(c);
}

void writeLogin(std::vector<uint8_t>& msg,
                const std::string& user,
                const std::string& authToken,
                const std::string& sessionId)
{
  msg.emplace_back(static_cast<uint8_t>(ClientMessageId::Login));
  writePascal(msg, user);
  writePascal(msg, authToken);
  writePascal(msg, sessionId);
}

void writeUpdateState(std::vector<uint8_t>& msg, const std::vector<uint8_t>& data)
{
  msg.emplace_back(static_cast<uint8_t>(ClientMessageId::UpdateState));
  writePascal(msg, data);
}

void write_query_state(std::vector<uint8_t>& msg)
{
  msg.emplace_back(static_cast<uint8_t>(ClientMessageId::StateQuery));
}

struct HauntedCoopClient::ClientImpl
{
  explicit ClientImpl(std::string sessionId)
      : m_sessionId{std::move(sessionId)}
      , m_resolver{m_ioService}
      , m_socket{m_ioContext}
  {
    BOOST_LOG_TRIVIAL(info) << "initializing client";

    Expects(m_networkConfig.color.size() == 3);

    std::vector<std::string> socketParts;
    boost::algorithm::split(socketParts,
                            m_networkConfig.socket,
                            [](char c)
                            {
                              return c == ':';
                            });
    gsl_Assert(socketParts.size() == 2);
    m_resolver.async_resolve(
      socketParts[0],
      socketParts[1],
      [this](const boost::system::error_code& err, const boost::asio::ip::tcp::resolver::results_type& endpoints)
      {
        if(err)
        {
          BOOST_LOG_TRIVIAL(error) << "resolve failed: " << err.message();
          return;
        }

        BOOST_LOG_TRIVIAL(info) << "service resolved, attempting connect";
        boost::asio::async_connect(
          m_socket,
          endpoints,
          [this](const boost::system::error_code& err, const boost::asio::ip::tcp::endpoint& endpoint)
          {
            BOOST_LOG_TRIVIAL(info) << "connected to " << endpoint.address().to_string() << ":" << endpoint.port();
            onConnected(err);
          });
      });
  }

  void run()
  {
    m_ioService.run();
    m_ioContext.run();
  }

  void close()
  {
    m_socket.close();
  }

  [[nodiscard]] auto getStates()
  {
    std::lock_guard guard{m_peerDatasMutex};
    return m_peerDatas;
  }

  void sendState(PeerData data)
  {
    if(!m_loggedIn || !m_socket.is_open())
      return;

    {
      std::lock_guard guard{m_sendMutex};
      m_sendBuffer.clear();

      data.emplace_back(m_networkConfig.color.at(0));
      data.emplace_back(m_networkConfig.color.at(1));
      data.emplace_back(m_networkConfig.color.at(2));
      writeUpdateState(m_sendBuffer, data);

      ++m_fullSyncCounter;
      if(m_fullSyncCounter >= 30 * 5)
      {
        m_fullSyncCounter = 0;
        write_query_state(m_sendBuffer);
      }

      if(!send())
      {
        BOOST_LOG_TRIVIAL(error) << "send state failed";
        m_loggedIn = false;
        return;
      }
    }
  }

private:
  void onConnected(const boost::system::error_code& err)
  {
    if(err)
    {
      BOOST_LOG_TRIVIAL(error) << "connection failed: " << err.message();
      return;
    }

    try
    {
      // m_socket.non_blocking(true);
      m_socket.set_option(boost::asio::socket_base::keep_alive{true});
      m_socket.set_option(boost::asio::ip::tcp::no_delay{true});
    }
    catch(std::exception& ex)
    {
      BOOST_LOG_TRIVIAL(error) << "error setting socket options: " << ex.what();
      throw;
    }

    {
      BOOST_LOG_TRIVIAL(info) << "Logging in to Haunted Coop Server";
      std::lock_guard guard{m_sendMutex};
      m_sendBuffer.clear();
      writeLogin(m_sendBuffer, "stohrendorf", "token-123", m_sessionId);
      if(!send())
      {
        BOOST_LOG_TRIVIAL(error) << "failed to send login credentials";
        m_loggedIn = false;
        return;
      }
    }

    BOOST_LOG_TRIVIAL(info) << "awaiting login response";
    continueWithRead(1, &ClientImpl::awaitLogin);
  }

  void handleServerInfo()
  {
    const auto protocolVersion = readLE<uint16_t>(&m_recvBuffer[0]);
    const auto messageSizeLimit = readLE<uint16_t>(&m_recvBuffer[2]);
    BOOST_LOG_TRIVIAL(info) << "Connection established. Server protocol " << protocolVersion << ", message size limit "
                            << messageSizeLimit;
    m_loggedIn = true;
    processMessages();
  }

  void awaitLogin()
  {
    switch(static_cast<ServerMessageId>(m_recvBuffer[0]))
    {
    case ServerMessageId::ServerInfo:
      continueWithRead(4, &ClientImpl::handleServerInfo);
      break;

    case ServerMessageId::Failure:
      BOOST_LOG_TRIVIAL(error) << "Login failed";
      continueWithRead(sizeof(uint8_t), &ClientImpl::readFailureLength);
      break;

    default:
      BOOST_LOG_TRIVIAL(error) << "Got unexpected message type " << static_cast<uint16_t>(m_recvBuffer[0]);
      break;
    }
  }

  void readPeerId()
  {
    m_peerId = readLE<uint64_t>(m_recvBuffer.data());
    continueWithRead(sizeof(uint16_t), &ClientImpl::readPeerDataSize);
  }

  void readPeerDataSize()
  {
    continueWithRead(readLE<uint16_t>(m_recvBuffer.data()), &ClientImpl::readPeerData);
  }

  void readPeerData()
  {
    {
      std::lock_guard guard{m_peerDatasMutex};
      m_peerDatas.insert_or_assign(m_peerId, m_recvBuffer);
    }
    processMessages();
  }

  void readFailureLength()
  {
    continueWithRead(m_recvBuffer[0], &ClientImpl::readFailure);
  }

  void readFailure()
  {
    std::string tmp{m_recvBuffer.begin(), m_recvBuffer.end()};
    BOOST_LOG_TRIVIAL(error) << "received failure message from server: " << tmp;
    processMessages();
  }

  void readFullSyncStatesCount()
  {
    m_fullSyncStatesCount = readLE<uint8_t>(m_recvBuffer.data());
    m_fullSyncPeerDatas.clear();

    if(m_fullSyncStatesCount > 0)
      continueWithRead(sizeof(uint64_t) + sizeof(uint16_t), &ClientImpl::readFullSyncPeerIdAndDataSize);
    else
    {
      std::lock_guard guard{m_peerDatasMutex};
      m_peerDatas.clear();
      processMessages();
    }
  }

  void readFullSyncPeerIdAndDataSize()
  {
    m_peerId = readLE<uint64_t>(m_recvBuffer.data());
    continueWithRead(readLE<uint16_t>(&m_recvBuffer[sizeof(uint64_t)]), &ClientImpl::readFullSyncPeerData);
  }

  void readFullSyncPeerData()
  {
    m_fullSyncPeerDatas.insert_or_assign(m_peerId, m_recvBuffer);
    BOOST_ASSERT(m_fullSyncStatesCount > 0);
    --m_fullSyncStatesCount;
    if(m_fullSyncStatesCount > 0)
      continueWithRead(sizeof(uint64_t) + sizeof(uint16_t), &ClientImpl::readFullSyncPeerIdAndDataSize);
    else
    {
      std::lock_guard guard{m_peerDatasMutex};
      m_peerDatas = std::move(m_fullSyncPeerDatas);
      processMessages();
    }
  }

  void dispatchMessage()
  {
    switch(static_cast<ServerMessageId>(m_recvBuffer[0]))
    {
    case ServerMessageId::UpdateState:
      continueWithRead(sizeof(uint64_t), &ClientImpl::readPeerId);
      break;

    case ServerMessageId::ServerInfo:
      BOOST_LOG_TRIVIAL(error) << "received unexpected server info";
      break;

    case ServerMessageId::Failure:
      continueWithRead(sizeof(uint8_t), &ClientImpl::readFailureLength);
      break;

    case ServerMessageId::FullSync:
      continueWithRead(sizeof(uint8_t), &ClientImpl::readFullSyncStatesCount);
      break;

    default:
      BOOST_LOG_TRIVIAL(error) << "received unexpected message id " << static_cast<uint16_t>(m_recvBuffer[0]);
      break;
    }
  }

  void processMessages()
  {
    continueWithRead(1, &ClientImpl::dispatchMessage);
  }

  bool send()
  {
    try
    {
      m_socket.send(boost::asio::buffer(m_sendBuffer));
    }
    catch(std::exception& ex)
    {
      BOOST_LOG_TRIVIAL(error) << "Failed to send data: " << ex.what();
      m_socket.close();
      return false;
    }

    return true;
  }

  const std::string m_sessionId;

  boost::asio::io_service m_ioService;
  boost::asio::ip::tcp::resolver m_resolver;
  boost::asio::io_context m_ioContext;
  boost::asio::ip::tcp::socket m_socket;

  std::atomic_bool m_loggedIn{false};

  std::mutex m_sendMutex;
  std::vector<uint8_t> m_sendBuffer;
  std::vector<uint8_t> m_recvBuffer;

  uint64_t m_peerId = 0;

  uint16_t m_fullSyncStatesCount = 0;
  std::map<uint64_t, PeerData> m_fullSyncPeerDatas{};

  std::mutex m_peerDatasMutex;
  std::map<uint64_t, PeerData> m_peerDatas{};

  uint16_t m_fullSyncCounter = 0;

  launcher::NetworkConfig m_networkConfig{launcher::NetworkConfig::load()};

  using ContinueWithReadFn = void (ClientImpl::*)();
  void continueWithRead(size_t n, ContinueWithReadFn fn)
  {
    if(!m_socket.is_open())
      return;

    m_recvBuffer.resize(n);
    boost::asio::async_read(m_socket,
                            boost::asio::buffer(m_recvBuffer),
                            [this, fn](const boost::system::error_code& err, std::size_t bytesTransferred)
                            {
                              if(err)
                              {
                                BOOST_LOG_TRIVIAL(error) << "read failed: " << err.message();
                                return;
                              }

                              if(bytesTransferred != m_recvBuffer.size())
                              {
                                BOOST_LOG_TRIVIAL(error) << "read failed: wanted " << m_recvBuffer.size()
                                                         << " bytes, got " << bytesTransferred;
                                return;
                              }

                              (this->*fn)();
                            });
  }
};

HauntedCoopClient::HauntedCoopClient(const std::string& sessionId)
    : impl{std::make_unique<ClientImpl>(sessionId)}
{
}

void HauntedCoopClient::sendState(const PeerData& data)
{
  impl->sendState(data);
}

void HauntedCoopClient::updateThread()
{
  impl->run();
}

std::map<uint64_t, PeerData> HauntedCoopClient::getStates() const
{
  std::lock_guard guard{m_statesMutex};
  return impl->getStates();
}

void HauntedCoopClient::start()
{
  m_thread = std::thread{&HauntedCoopClient::updateThread, this};
}

HauntedCoopClient::~HauntedCoopClient()
{
  stop();
}

void HauntedCoopClient::stop()
{
  impl->close();
  m_thread.join();
}
