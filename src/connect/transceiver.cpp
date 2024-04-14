#include "connect/transceiver.h"

#include <algorithm>
#include <random>

#include "config/tcp_config.h"
#include "datagram/tcp_message.h"

Transceiver::Transceiver(uint64_t initial_RTO_ms,
                         std::optional<Wrap32> fixed_isn)
    : send_isn_(fixed_isn.value_or(Wrap32{std::random_device()()})),
      initial_RTO_ms_(initial_RTO_ms) {}

uint64_t Transceiver::sequence_numbers_in_flight() const {
  //* Get the length of all segments not acked (no bigger than window size)
  // TODO(iyume): Optimize
  uint64_t res{};
  for (const auto& it : _messages) {
    res += it.second.sequence_length();
  }
  return res;
}

uint64_t Transceiver::consecutive_retransmissions() const {
  return consecutive_retransmissions_;
}

std::optional<TCPSenderMessage> Transceiver::maybe_send() {
  if (!isn_send_queue_.empty()) {
    uint64_t seqno = isn_send_queue_.front();
    isn_send_queue_.pop();
    // Retransmittion is possibly not in the flighting messages
    if (const auto& msg = _messages.find(seqno); msg != _messages.end()) {
      return msg->second;
    }
  }
  return {};
}

void Transceiver::push(Reader& outbound_stream) {
  //* This function read stream into segments as many as possible
  //* The sequence number in flight should not beyond the window size
  //* Specially, pretend window size as one only if no sequence number in flight
  // and no window size

  using value_type = decltype(_messages)::value_type;
  // New definition is needed because window_size enables RTO back off
  uint16_t window_size = _window_size;
  if (!window_size && !sequence_numbers_in_flight()) {
    window_size = 1;
  }

  if (_finished) {
    return;
  }

  std::string payload;

  while (window_size > sequence_numbers_in_flight()) {
    bool syn{};
    bool fin{};
    uint64_t abs_seqno = 1 + outbound_stream.bytes_popped();
    uint16_t read_size = window_size - sequence_numbers_in_flight();

    if (!_started) {
      _started = true;
      syn = true;
      read_size -= 1;
      abs_seqno -= 1;
    }
    // TEST: MAX_PAYLOAD_SIZE limits payload only
    uint16_t payload_max_size =
        std::min(read_size, static_cast<uint16_t>(TCPConfig::MAX_PAYLOAD_SIZE));
    read(outbound_stream, payload_max_size, payload);
    read_size -= payload.size();
    if (!_finished && outbound_stream.is_finished() && read_size) {
      _finished = true;
      fin = true;
    }
    if (!(syn + payload.size() + fin)) {
      break;  // nothing to send
    }

    _messages.emplace(
        value_type{abs_seqno, {send_isn_ + abs_seqno, syn, {payload}, fin}});
    isn_send_queue_.emplace(abs_seqno);
  }

  _popped_bytes = outbound_stream.bytes_popped();
}

TCPSenderMessage Transceiver::send_empty_message() const {
  return {.seqno = send_isn_ + _started + _popped_bytes + _finished};
}

void Transceiver::receive_ack(const TCPReceiverMessage& msg) {
  //* In the tests, the ACK triggers push method to call
  //* TEST "SYN + FIN", is it possible to receive msg without ackno and take new
  // window size?

  _window_size = msg.window_size;  // update unconditional

  if (msg.ackno) {
    // just find the closest ackno to last ackno, but not to the stream's end
    // (maybe more accurate)
    uint64_t ackno = msg.ackno->unwrap(send_isn_, _last_ackno);
    if ((_last_ackno && ackno <= _last_ackno) ||
        ackno > _started + _popped_bytes + _finished) {
      // (_last_ackno && ackno <= _last_ackno) is a trick to let timer not
      // restart because _last_ackno is not initialized less than 0
      return;
    }
    _last_ackno = ackno;

    //? try some lower_bound trick?
    // _messages.erase( _messages.begin(), --_messages.lower_bound( ackno ) );
    auto erase_end = _messages.begin();
    while (erase_end != _messages.end()) {
      if (ackno < erase_end->first + erase_end->second.sequence_length()) {
        break;
      }
      erase_end++;
    }
    _messages.erase(_messages.begin(), erase_end);

    _rto_factor = 1;
    consecutive_retransmissions_ = 0;
    _ms_since_last_ticked = 0;
  }
}

void Transceiver::tick(const uint64_t ms_since_last_tick) {
  if (_messages.empty()) {
    return;
  }
  _ms_since_last_ticked += ms_since_last_tick;
  if (_ms_since_last_ticked >= initial_RTO_ms_ * _rto_factor) {
    isn_send_queue_.emplace(_messages.begin()->first);
    if (_window_size) {
      //* Back off RTO only if the network is lousy.
      //* The zero window size indicates that receiver is busy, and there is
      // only one special
      //* message in the flighting messages which only contains one length.
      _rto_factor *= 2;
      consecutive_retransmissions_ += 1;
    }
    _ms_since_last_ticked = 0;
    return;
  }
}

/* 接收端 */
void Transceiver::receive_isn(TCPSenderMessage message,
                              Reassembler& reassembler,
                              Writer& inbound_stream) {
  if (message.SYN) {
    receive_isn_ = Wrap32{message.seqno};
  }
  if (receive_isn_) {
    reassembler.insert(message.seqno.unwrap(receive_isn_.value(),
                                            inbound_stream.bytes_pushed()) -
                           !message.SYN,
                       message.payload, message.FIN, inbound_stream);
  }
}

TCPReceiverMessage Transceiver::send_ack(const Writer& inbound_stream) const {
  // Your code here.
  auto ackno = receive_isn_;
  if (ackno) {
    ackno.emplace(ackno.value() + 1 + inbound_stream.bytes_pushed() +
                  inbound_stream.is_closed());
  }
  return {
      .ackno = ackno,
      .window_size = static_cast<uint16_t>(
          std::min(inbound_stream.available_capacity(), uint64_t{UINT16_MAX})),
  };
}
