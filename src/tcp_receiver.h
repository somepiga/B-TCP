#pragma once

#include "reassembler.h"
#include "tcp_receiver_message.h"
#include "tcp_sender_message.h"
#include <optional>

class TCPReceiver {
   public:
    /*
     * The TCPReceiver receives TCPSenderMessages, inserting their payload into
     * the Reassembler at the correct stream index.
     */
    void receive(TCPSenderMessage message,
                 Reassembler& reassembler,
                 Writer& inbound_stream);

    /* The TCPReceiver sends TCPReceiverMessages back to the TCPSender. */
    TCPReceiverMessage send(const Writer& inbound_stream) const;

    // 接收窗口的大小
    uint64_t sliding_window_size_{0};

    // 这一次交换数据的ISN
    std::optional<Wrap32> ISN_ = std::nullopt;

    // 期望下一个包的absolute_seqno
    uint64_t expect_{0};
};
