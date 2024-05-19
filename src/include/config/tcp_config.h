#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "datagram/address.h"
#include "datagram/wrapping_integers.h"

/**
 * @brief 一条 TCP 信道的配置内容
 */
class TCPConfig {
 public:
  static constexpr size_t DEFAULT_CAPACITY = 64000;  //!< 容量大小 (bytes)
  static constexpr size_t MAX_PAYLOAD_SIZE = 1000;  //!< 最大报文载荷 (bytes)
  static constexpr uint16_t TIMEOUT_DFLT = 1000;    //!< 默认重传时间 (ms)
  static constexpr unsigned MAX_RETX_ATTEMPTS = 8;  //!< 最大重传次数

  uint16_t rt_timeout = TIMEOUT_DFLT;
  size_t recv_capacity = DEFAULT_CAPACITY;
  size_t send_capacity = DEFAULT_CAPACITY;

  std::optional<Wrap32> fixed_isn{};
};

/**
 * @brief 适配器的配置内容
 * 包含原ip, port ; 目的ip, port
 */
class FdAdapterConfig {
 public:
  Address source{"0", 0};
  Address destination{"0", 0};

  uint16_t loss_rate_dn = 0;  //!< 下行丢包率
  uint16_t loss_rate_up = 0;  //!< 上行丢包率
};

/**
 * Socket 创建时的默认Tun配置
 */
constexpr const char* TUN_DFLT = "tun100";
constexpr const char* LOCAL_ADDRESS_DFLT = "169.254.100.9";
