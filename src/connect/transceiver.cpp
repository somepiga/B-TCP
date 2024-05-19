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
        if (const auto& msg = _messages.find(seqno); msg != _messages.end()) {
            return msg->second;
        }
    }
    return {};
}

void Transceiver::push(Reader& outbound_stream) {
    using value_type = decltype(_messages)::value_type;
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

        uint16_t payload_max_size = std::min(
            read_size, static_cast<uint16_t>(TCPConfig::MAX_PAYLOAD_SIZE));
        read(outbound_stream, payload_max_size, payload);
        read_size -= payload.size();
        if (!_finished && outbound_stream.is_finished() && read_size) {
            _finished = true;
            fin = true;
        }
        if (!(syn + payload.size() + fin)) {
            break;
        }

        _messages.emplace(value_type{
            abs_seqno, {send_isn_ + abs_seqno, syn, {payload}, fin}});
        isn_send_queue_.emplace(abs_seqno);
    }

    _popped_bytes = outbound_stream.bytes_popped();
}

TCPSenderMessage Transceiver::send_empty_message() const {
    return {.seqno = send_isn_ + _started + _popped_bytes + _finished};
}

void Transceiver::receive_ack(const TCPReceiverMessage& msg) {
    _window_size = msg.window_size;

    if (msg.ackno) {
        uint64_t ackno = msg.ackno->unwrap(send_isn_, _last_ackno);
        if ((_last_ackno && ackno <= _last_ackno) ||
            ackno > _started + _popped_bytes + _finished) {
            return;
        }
        _last_ackno = ackno;

        auto erase_end = _messages.begin();
        while (erase_end != _messages.end()) {
            if (ackno <
                erase_end->first + erase_end->second.sequence_length()) {
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
            _rto_factor *= 2;
            consecutive_retransmissions_ += 1;
        }
        _ms_since_last_ticked = 0;
        return;
    }
}

/* 接收端 */

//! 接收数据包
//! 1. 保存接收数据的ISN
//! 2. 把内容传给Reassembler
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
    auto ackno = receive_isn_;
    if (ackno) {
        ackno.emplace(ackno.value() + 1 + inbound_stream.bytes_pushed() +
                      inbound_stream.is_closed());
    }
    return {
        .ackno = ackno,
        .window_size = static_cast<uint16_t>(std::min(
            inbound_stream.available_capacity(), uint64_t{UINT16_MAX})),
    };
}
