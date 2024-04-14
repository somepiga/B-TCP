#pragma once

// #include "ethernet_header.hh"
// #include "network_interface.hh"
// #include "tcp_over_ip.hh"
#include <optional>
#include <unordered_map>
#include <utility>

#include "config/tcp_config.h"
#include "datagram/tcp_message.h"
#include "tun/tun.h"

//! \brief A FD adapter for IPv4 datagrams read from and written to a TUN device
class TCPOverIPv4OverTunFdAdapter {
 private:
  TunFD _tun;

  FdAdapterConfig _cfg{};  //!< 配置内容
  bool _listen = false;    //!< 是否处于监听态

 public:
  explicit TCPOverIPv4OverTunFdAdapter(TunFD&& tun) : _tun(std::move(tun)) {}

  //! Attempts to read and parse an IPv4 datagram containing a TCP segment
  //! related to the current connection
  std::optional<TCPSegment> read();

  //! Creates an IPv4 datagram from a TCP segment and writes it to the TUN
  //! device
  void write(TCPSegment& seg) { _tun.write(serialize(wrap_tcp_in_ip(seg))); }

  //! Access the underlying TUN device
  explicit operator TunFD&() { return _tun; }

  //! Access the underlying TUN device
  explicit operator const TunFD&() const { return _tun; }

  //! Access underlying file descriptor
  FileDescriptor& fd() { return _tun; }

  std::optional<TCPSegment> unwrap_tcp_in_ip(const IPv4Datagram& ip_dgram);

  IPv4Datagram wrap_tcp_in_ip(TCPSegment& seg);

  FdAdapterConfig& config_mutable() { return _cfg; }

  void set_listening(const bool l) { _listen = l; }

  bool listening() const { return _listen; }

  void tick(const size_t) {}

  const FdAdapterConfig& config() const { return _cfg; }

  FdAdapterConfig& config_mut() { return _cfg; }
};
