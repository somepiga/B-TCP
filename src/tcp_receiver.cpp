#include "tcp_receiver.h"
#include <cstdint>
#include <iostream>

using namespace std;

void TCPReceiver::receive(TCPSenderMessage message,
                          Reassembler& reassembler,
                          Writer& inbound_stream) {
    if (message.SYN && !ISN_.has_value()) {
        ISN_ = message.seqno + 1;
    }
    if (!ISN_.has_value()) {
        return;
    }
    uint64_t const first_index =
        (message.seqno + message.SYN)
            .unwrap(ISN_.value(), (reassembler.sliding_window_.empty())
                                      ? 0
                                      : reassembler.sliding_window_[0]);
    reassembler.insert(first_index, message.payload, message.FIN,
                       inbound_stream);
    expect_ = reassembler.sliding_window_[0];
}

TCPReceiverMessage TCPReceiver::send(const Writer& inbound_stream) const {
    auto available_size = inbound_stream.available_capacity();
    auto const window_size = static_cast<uint16_t>(
        (available_size > static_cast<uint64_t> UINT16_MAX) ? UINT16_MAX
                                                            : available_size);
    if (ISN_.has_value()) {
        return {
            Wrap32::wrap(expect_, ISN_.value()) + inbound_stream.is_closed(),
            window_size};
    }
    return {std::nullopt, window_size};
}
