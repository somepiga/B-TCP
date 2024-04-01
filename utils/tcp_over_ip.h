#pragma once

#include "buffer.h"
#include "fd_adapter.h"
#include "ipv4_datagram.h"
#include "tcp_segment.h"

#include <optional>

//! \brief 从 TCP 段到序列化 IPv4 数据报的转换器
class TCPOverIPv4Adapter : public FdAdapterBase {
   public:
    std::optional<TCPSegment> unwrap_tcp_in_ip(
        const InternetDatagram& ip_dgram);

    InternetDatagram wrap_tcp_in_ip(TCPSegment& seg);
};
