#pragma once

#include <optional>
#include <unordered_map>
#include <utility>

#include "config/tcp_config.h"
#include "datagram/tcp_message.h"
#include "tun/tun.h"

//! \brief 实现传输层和网络层的报文转换
class TCPOverIPv4OverTunFdAdapter {
 private:
  TunFD _tun;

  FdAdapterConfig _cfg{};  //!< 配置内容
  bool _listen = false;    //!< 是否处于监听态

 public:
  explicit TCPOverIPv4OverTunFdAdapter(TunFD&& tun) : _tun(std::move(tun)) {}

  //! 尝试读取并解析包含与当前连接相关的 TCP 数据报的 IPv4 数据报
  std::optional<TCPSegment> read();

  //! 从 TCP 数据报创建 IPv4 数据报并将其写入 Tun 设备
  void write(TCPSegment& seg) { _tun.write(serialize(wrap_tcp_in_ip(seg))); }

  explicit operator TunFD&() { return _tun; }

  explicit operator const TunFD&() const { return _tun; }

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
