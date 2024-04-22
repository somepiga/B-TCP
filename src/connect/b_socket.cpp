#include "connect/b_socket.h"

#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstddef>
#include <exception>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>

#include "polling/rule.h"
#include "tun/tun.h"
#include "utils/exception.h"
#include "utils/parser.h"

using namespace std;

static constexpr size_t TCP_TICK_MS = 10;

static inline uint64_t timestamp_ms() {
  static_assert(std::is_same<std::chrono::steady_clock::duration,
                             std::chrono::nanoseconds>::value);

  return std::chrono::steady_clock::now().time_since_epoch().count() / 1000000;
}

//! \param[in] condition 如果返回true，就应该继续循环
template <typename AdaptT>
void TCPSocket<AdaptT>::_tcp_loop(const function<bool()>& condition) {
  auto base_time = timestamp_ms();
  while (condition()) {
    auto ret = _eventloop.wait_next_event(TCP_TICK_MS);
    if (ret == Result::Exit or _abort) {
      break;
    }

    if (not _tcp.has_value()) {
      throw runtime_error("_tcp_loop entered before TCPPeer initialized");
    }

    if (_tcp.value().active()) {
      const auto next_time = timestamp_ms();
      _tcp.value().tick(next_time - base_time);
      collect_segments();
      _datagram_adapter.tick(next_time - base_time);
      base_time = next_time;
    }
  }
}

//! \param[in] data_socket_pair 一对连接的 AF_UNIX SOCK_STREAM socket
//! \param[in] datagram_interface 读写数据报的接口
template <typename AdaptT>
TCPSocket<AdaptT>::TCPSocket(
    pair<FileDescriptor, FileDescriptor> data_socket_pair,
    AdaptT&& datagram_interface, EventEpoll&& eventloop)
    : Socket(std::move(data_socket_pair.first), AF_UNIX, SOCK_STREAM),
      _thread_data(std::move(data_socket_pair.second), AF_UNIX, SOCK_STREAM),
      _datagram_adapter(std::move(datagram_interface)),
      _eventloop(std::move(eventloop)) {
  _thread_data.set_blocking(false);
  set_blocking(false);
}

template <typename AdaptT>
void TCPSocket<AdaptT>::_initialize_TCP(const TCPConfig& config) {
  _tcp.emplace(config);

  // rule 1: read from filtered packet stream and dump into TCPConnection
  _eventloop.add_rule(
      "receive TCP segment from the network", _datagram_adapter.fd(),
      Direction::In,
      [&] {
        if (auto seg = _datagram_adapter.read()) {
          _tcp->receive(std::move(seg.value()));
          collect_segments();
        }

        if (_thread_data.eof() and
            _tcp.value().transceiver().sequence_numbers_in_flight() == 0 and
            not _fully_acked) {
          cerr << "DEBUG: Outbound stream to "
               << _datagram_adapter.config().destination.to_string()
               << " has been fully acknowledged.\n";
          _fully_acked = true;
        }
      },
      [&] { return _tcp->active(); });

  // rule 2: read from pipe into outbound buffer
  _eventloop.add_rule(
      "push bytes to TCPPeer", _thread_data, Direction::In,
      [&] {
        string data;
        data.resize(_tcp->outbound_writer().available_capacity());
        _thread_data.read(data);
        _tcp->outbound_writer().push(std::move(data));

        if (_thread_data.eof()) {
          _tcp->outbound_writer().close();
          _outbound_shutdown = true;

          cerr << "DEBUG: Outbound stream to "
               << _datagram_adapter.config().destination.to_string()
               << " finished ("
               << _tcp.value().transceiver().sequence_numbers_in_flight()
               << " seqno"
               << (_tcp.value().transceiver().sequence_numbers_in_flight() == 1
                       ? ""
                       : "s")
               << " still in flight).\n";
        }

        _tcp->push();
        collect_segments();
      },
      [&] {
        return (_tcp->active()) and (not _outbound_shutdown) and
               (_tcp->outbound_writer().available_capacity() > 0);
      },
      [&] {
        _tcp->outbound_writer().close();
        _outbound_shutdown = true;
      });

  // rule 3: read from inbound buffer into pipe
  _eventloop.add_rule(
      "read bytes from inbound stream", _thread_data, Direction::Out,
      [&] {
        Reader& inbound = _tcp->inbound_reader();
        if (inbound.bytes_buffered()) {
          const std::string_view buffer = inbound.peek();
          const auto bytes_written = _thread_data.write(buffer);
          inbound.pop(bytes_written);
        }

        if (inbound.is_finished() or inbound.has_error()) {
          _thread_data.shutdown(SHUT_WR);
          _inbound_shutdown = true;

          cerr << "DEBUG: Inbound stream from "
               << _datagram_adapter.config().destination.to_string()
               << " finished "
               << (inbound.has_error() ? "with an error/reset.\n"
                                       : "cleanly.\n");
        }
      },
      [&] {
        return _tcp->inbound_reader().bytes_buffered() or
               ((_tcp->inbound_reader().is_finished() or
                 _tcp->inbound_reader().has_error()) and
                not _inbound_shutdown);
      });

  // rule 4: read outbound segments from TCPConnection and send as datagrams
  _eventloop.add_rule(
      "send TCP segment", _datagram_adapter.fd(), Direction::Out,
      [&] {
        while (not outgoing_segments_.empty()) {
          _datagram_adapter.write(outgoing_segments_.front());
          outgoing_segments_.pop();
        }
      },
      [&] { return not outgoing_segments_.empty(); });
}

//! \brief 调用socketpair来返回一对指定type的socket
static inline pair<FileDescriptor, FileDescriptor> socket_pair_helper(
    const int type) {
  array<int, 2> fds{};
  CheckSystemCall("socketpair", ::socketpair(AF_UNIX, type, 0, fds.data()));
  return {FileDescriptor(fds[0]), FileDescriptor(fds[1])};
}

//! \param[in] datagram_interface 底层接口(比如网络层，数据链路层)
template <typename AdaptT>
TCPSocket<AdaptT>::TCPSocket(AdaptT&& datagram_interface,
                             EventEpoll&& eventloop)
    : TCPSocket(socket_pair_helper(SOCK_STREAM), std::move(datagram_interface),
                std::move(eventloop)) {}

template <typename AdaptT>
TCPSocket<AdaptT>::~TCPSocket() {
  try {
    if (_tcp_thread.joinable()) {
      cerr << "Warning: unclean shutdown of TCPSocket\n";
      // 强制让对方退出
      _abort.store(true);
      _tcp_thread.join();
    }
  } catch (const exception& e) {
    cerr << "Exception destructing TCPSocket: " << e.what() << endl;
  }
}

template <typename AdaptT>
void TCPSocket<AdaptT>::wait_until_closed() {
  shutdown(SHUT_RDWR);
  if (_tcp_thread.joinable()) {
    cerr << "Waiting for clean shutdown... ";
    _tcp_thread.join();
    cerr << "done.\n";
  }
}

//! \param[in] c_tcp TCP连接配置
//! \param[in] c_ad FdAdapter配置
template <typename AdaptT>
void TCPSocket<AdaptT>::connect(const TCPConfig& c_tcp,
                                const FdAdapterConfig& c_ad) {
  if (_tcp) {
    throw runtime_error("connect() with TCPConnection already initialized");
  }

  _initialize_TCP(c_tcp);

  _datagram_adapter.config_mut() = c_ad;

  cerr << "\033[1;32mConnecting to " << c_ad.destination.to_string()
       << " ...\n\033[0m";

  if (not _tcp.has_value()) {
    throw runtime_error("TCPPeer not successfully initialized");
  }

  _tcp->push();
  collect_segments();

  if (_tcp->transceiver().sequence_numbers_in_flight() != 1) {
    throw runtime_error(
        "After TCPConnection::connect(), expected "
        "sequence_numbers_in_flight() == 1");
  }

  _tcp_loop(
      [&] { return _tcp->transceiver().sequence_numbers_in_flight() == 1; });
  if (not _tcp->inbound_reader().has_error()) {
    cerr << "\033[1;32mSuccessfully connected to "
         << c_ad.destination.to_string() << ".\n\033[0m";
  } else {
    cerr << "\033[1;31mError on connecting to " << c_ad.destination.to_string()
         << ".\n\033[0m";
  }

  _tcp_thread = thread(&TCPSocket::_tcp_main, this);
}

//! \param[in] c_tcp TCP连接配置
//! \param[in] c_ad FdAdapter配置
template <typename AdaptT>
void TCPSocket<AdaptT>::listen_and_accept(const TCPConfig& c_tcp,
                                          const FdAdapterConfig& c_ad) {
  if (_tcp) {
    throw runtime_error(
        "listen_and_accept() with TCPConnection already initialized");
  }

  _initialize_TCP(c_tcp);

  _datagram_adapter.config_mut() = c_ad;
  _datagram_adapter.set_listening(true);

  cerr << "\033[1;32mListening for incoming connection...\n\033[0m";
  _tcp_loop([&] {
    return (not _tcp->has_ackno()) or
           (_tcp->transceiver().sequence_numbers_in_flight());
  });
  cerr << "\033[1;32mNew connection from "
       << _datagram_adapter.config().destination.to_string() << ".\n\033[0m";

  _tcp_thread = thread(&TCPSocket::_tcp_main, this);
}

template <typename AdaptT>
void TCPSocket<AdaptT>::_tcp_main() {
  try {
    if (not _tcp.has_value()) {
      throw runtime_error("no TCP");
    }
    _tcp_loop([] { return true; });
    shutdown(SHUT_RDWR);
    if (not _tcp.value().active()) {
      if (_tcp->inbound_reader().has_error()) {
        cerr << "\033[1;31mDEBUG: TCP connection finished "
             << "uncleanly.\n\033[0m";
      } else {
        cerr << "\033[1;32mDEBUG: TCP connection finished "
             << "cleanly.\n\033[0m";
      }
    }
    _tcp.reset();
  } catch (const exception& e) {
    cerr << "\033[1;31mException in TCPConnection runner thread: " << e.what()
         << "\n\033[0m";
    throw e;
  }
}

template <typename AdaptT>
void TCPSocket<AdaptT>::collect_segments() {
  if (not _tcp.has_value()) {
    return;
  }

  while (auto seg = _tcp->maybe_send()) {
    outgoing_segments_.push(std::move(seg.value()));
  }
}

template class TCPSocket<TCPOverIPv4OverTunFdAdapter>;

B_TCPSocketEpoll::B_TCPSocketEpoll(const std::string& devname)
    : TCPSocket<TCPOverIPv4OverTunFdAdapter>(
          TCPOverIPv4OverTunFdAdapter(TunFD(devname)), EventEpoll()) {}

void B_TCPSocketEpoll::connect(const Address& address) {
  TCPConfig tcp_config;
  tcp_config.rt_timeout = 100;

  FdAdapterConfig multiplexer_config;
  multiplexer_config.source =
      Address{"169.254.144.9", to_string(uint16_t(std::random_device()()))};
  multiplexer_config.destination = address;

  connect(tcp_config, multiplexer_config);
}

void B_TCPSocketEpoll::connect(const TCPConfig& c_tcp,
                               const FdAdapterConfig& c_ad) {
  TCPSocket<TCPOverIPv4OverTunFdAdapter>::connect(c_tcp, c_ad);
}