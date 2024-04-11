#pragma once

#include <map>
#include <optional>
#include <queue>

#include "buffer/stream_buffer.h"
#include "datagram/tcp_message.h"
#include "reassembler.h"

class Transceiver {
  /* 发送端 */
 private:
  Wrap32 send_isn_;

  // 重传时间
  uint64_t initial_RTO_ms_;
  uint16_t _rto_factor{1};
  uint64_t _ms_since_last_ticked{};

  // 重传次数
  uint64_t consecutive_retransmissions_{};

  // 用于请求对方数据的包  待发送队列
  std::queue<uint64_t> isn_send_queue_;
  std::map<uint64_t, TCPSenderMessage> _messages;  // abs seqno to message
  uint64_t _popped_bytes{};  // use bytes_popped() to get abs seqno
  bool _started{};           // mark SYN written
  bool _finished{};          // mark FIN written
  uint64_t _last_ackno{};
  uint64_t _window_size{1};  // as said in FAQ (for back off RTO)

 public:
  void push(Reader &outbound_stream);

  std::optional<TCPSenderMessage> maybe_send();

  TCPSenderMessage send_empty_message() const;

  void receive_ack(const TCPReceiverMessage &msg);

  void tick(uint64_t ms_since_last_tick);

  uint64_t sequence_numbers_in_flight() const;
  uint64_t consecutive_retransmissions() const;

  /* 接收端 */
 private:
  std::optional<Wrap32> receive_isn_{};

 public:
  void receive_isn(TCPSenderMessage message, Reassembler &reassembler,
                   Writer &inbound_stream);

  TCPReceiverMessage send_ack(const Writer &inbound_stream) const;

 public:
  Transceiver(uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn);
};
