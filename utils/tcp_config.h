#pragma once

#include "address.h"
#include "wrapping_integers.h"

#include <cstddef>
#include <cstdint>
#include <optional>

//! Config for TCP sender and receiver
class TCPConfig {
   public:
    static constexpr size_t DEFAULT_CAPACITY = 64000;  //!< Default capacity
    static constexpr size_t MAX_PAYLOAD_SIZE =
        1000;  //!< Conservative max payload size for real Internet
    static constexpr uint16_t TIMEOUT_DFLT =
        1000;  //!< Default re-transmit timeout is 1 second
    static constexpr unsigned MAX_RETX_ATTEMPTS =
        8;  //!< Maximum re-transmit attempts before giving up

    uint16_t rt_timeout =
        TIMEOUT_DFLT;  //!< Initial value of the retransmission timeout, in
                       //!< milliseconds
    size_t recv_capacity = DEFAULT_CAPACITY;  //!< Receive capacity, in bytes
    size_t send_capacity = DEFAULT_CAPACITY;  //!< Sender capacity, in bytes
    std::optional<Wrap32> fixed_isn{};
};

//! 从 FdAdapter 派生的类的配置
class FdAdapterConfig {
   public:
    Address source{"0", 0};       //!< 源地址和端口
    Address destination{"0", 0};  //!< 目的地址和端口

    uint16_t loss_rate_dn = 0;  //!< 下行丢包率（对于LossyFdAdapter）
    uint16_t loss_rate_up = 0;  //!< 上行链路丢失率（对于 LossyFdAdapter）
};
