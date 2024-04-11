#pragma once

#include <atomic>
#include <cstdint>
#include <optional>
#include <thread>
#include <vector>

#include "buffer/string_buffer.h"
#include "connect/base_socket.h"
#include "datagram/file_descriptor.h"
#include "utils/eventloop.h"
// #include "network_interface.hh"
// #include "socket.hh"
#include "config/tcp_config.h"
#include "connect/tcp_endpoint.h"
#include "tun/tun_adapter.h"

template <typename AdaptT>
class TCPSocket : public Socket {
 private:
  Socket _thread_data;

 protected:
  AdaptT _datagram_adapter;

 private:
  void _initialize_TCP(const TCPConfig& config);

  std::optional<TCPEndpoint> _tcp{};

  std::queue<TCPSegment> outgoing_segments_{};

  EventLoop _eventloop{};

  void _tcp_loop(const std::function<bool()>& condition);

  void _tcp_main();

  std::thread _tcp_thread{};

  TCPSocket(std::pair<FileDescriptor, FileDescriptor> data_socket_pair,
            AdaptT&& datagram_interface);

  std::atomic_bool _abort{false};

  bool _inbound_shutdown{false};

  bool _outbound_shutdown{false};

  bool _fully_acked{false};

  void collect_segments();

 public:
  explicit TCPSocket(AdaptT&& datagram_interface);

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

  //! \name
  //! Some methods of the parent Socket wouldn't work as expected on the TCP
  //! socket, so delete them

  //!@{
  void bind(const Address& address) = delete;
  Address local_address() const = delete;
  Address peer_address() const = delete;
  void set_reuseaddr() = delete;
  //!@}
};

class B_TCPSocket : public TCPSocket<TCPOverIPv4OverTunFdAdapter> {
 public:
  B_TCPSocket();
  void connect(const Address& address);
};