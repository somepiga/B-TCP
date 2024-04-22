#pragma once

#include <optional>

#include "config/tcp_config.h"
#include "connect/transceiver.h"
#include "datagram/tcp_message.h"

/**
 * @brief TCPSocket 的一端
 */
class TCPEndpoint {
  TCPConfig cfg_;                                             //!< 配置内容
  Transceiver transceiver_{cfg_.rt_timeout, cfg_.fixed_isn};  //!< 收发器
  Reassembler reassembler_{};                                 //!< 流重组器

  StreamBuffer outbound_stream_{cfg_.send_capacity},
      inbound_stream_{cfg_.recv_capacity};

  bool need_send_{};

 public:
  explicit TCPEndpoint(const TCPConfig& cfg) : cfg_(cfg) {}

  Writer& outbound_writer() { return outbound_stream_.writer(); }
  Reader& inbound_reader() { return inbound_stream_.reader(); }

  void push() { transceiver_.push(outbound_stream_.reader()); };
  void tick(uint64_t ms_since_last_tick) {
    transceiver_.tick(ms_since_last_tick);
  }

  bool has_ackno() const {
    return transceiver_.send_ack(inbound_stream_.writer()).ackno.has_value();
  }

  bool active() const {
    if (inbound_stream_.reader().has_error()) {
      return false;
    }
    return (!outbound_stream_.reader().is_finished()) ||
           (!inbound_stream_.writer().is_closed()) ||
           transceiver_.sequence_numbers_in_flight();
  }

  void receive(TCPSegment seg) {
    if (seg.reset || inbound_reader().has_error()) {
      inbound_stream_.writer().set_error();
      return;
    }

    transceiver_.receive_ack(seg.receiver_message);

    need_send_ |= (seg.sender_message.sequence_length() > 0);
    const auto our_ackno =
        transceiver_.send_ack(inbound_stream_.writer()).ackno;
    need_send_ |= (our_ackno.has_value() &&
                   seg.sender_message.seqno + 1 == our_ackno.value());

    transceiver_.receive_isn(std::move(seg.sender_message), reassembler_,
                             inbound_stream_.writer());
  }

  std::optional<TCPSegment> maybe_send() {
    auto receiver_msg = transceiver_.send_ack(inbound_stream_.writer());

    if (receiver_msg.ackno.has_value()) {
      push();
    }

    auto sender_msg = transceiver_.maybe_send();

    if (need_send_ && !sender_msg.has_value()) {
      sender_msg = transceiver_.send_empty_message();
    }

    need_send_ = false;

    if (sender_msg.has_value()) {
      return TCPSegment{sender_msg.value(), receiver_msg,
                        outbound_stream_.reader().has_error() or
                            inbound_reader().has_error()};
    }

    return {};
  }

  const Transceiver& transceiver() const { return transceiver_; }
};
