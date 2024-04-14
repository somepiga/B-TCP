#pragma once

#include <atomic>
#include <cstdint>
#include <optional>
#include <thread>
#include <vector>

#include "buffer/string_buffer.h"
#include "config/tcp_config.h"
#include "connect/base_socket.h"
#include "connect/tcp_endpoint.h"
#include "datagram/file_descriptor.h"
#include "polling/eventpolling.h"
#include "tun/tun_adapter.h"

/**
 * @brief TCPSocket
 */
template <typename AdaptT>
class TCPSocket : public Socket {
 private:
  Socket _thread_data;

 protected:
  AdaptT _datagram_adapter;

 private:
  void _initialize_TCP(const TCPConfig& config);

  std::optional<TCPEndpoint> _tcp{};  //!< TCP状态机

  std::queue<TCPSegment> outgoing_segments_{};  //!< 待发送数据包的队列

  EventEpoll _eventloop;  //!< 轮询事件

  void _tcp_loop(const std::function<bool()>& condition);

  void _tcp_main();

  std::thread _tcp_thread{};

  TCPSocket(std::pair<FileDescriptor, FileDescriptor> data_socket_pair,
            AdaptT&& datagram_interface, EventEpoll&& eventloop);

  std::atomic_bool _abort{false};

  bool _inbound_shutdown{false};

  bool _outbound_shutdown{false};

  bool _fully_acked{false};

  void collect_segments();

 public:
  explicit TCPSocket(AdaptT&& datagram_interface, EventEpoll&& eventloop);

  void wait_until_closed();

  void connect(const TCPConfig& c_tcp, const FdAdapterConfig& c_ad);

  void listen_and_accept(const TCPConfig& c_tcp, const FdAdapterConfig& c_ad);

  ~TCPSocket();

  //!@{
  TCPSocket(const TCPSocket&) = delete;
  TCPSocket(TCPSocket&&) = delete;
  TCPSocket& operator=(const TCPSocket&) = delete;
  TCPSocket& operator=(TCPSocket&&) = delete;
  //!@}

  //!@{
  void bind(const Address& address) = delete;
  Address local_address() const = delete;
  Address peer_address() const = delete;
  void set_reuseaddr() = delete;
  //!@}
};

/**
 * @brief 利用 tun 技术建立的 TCPSocket, 内部采用 epoll
 * 可以屏蔽网络层以下的细节
 */
class B_TCPSocketEpoll : public TCPSocket<TCPOverIPv4OverTunFdAdapter> {
 public:
  B_TCPSocketEpoll(const std::string& devname);
  void connect(const Address& address);
  void connect(const TCPConfig& c_tcp, const FdAdapterConfig& c_ad);
};