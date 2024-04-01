#pragma once

#include "byte_stream.h"
#include "tcp_receiver_message.h"
#include "tcp_sender_message.h"
#include "timer.h"
#include <vector>

class TCPSender {
    Wrap32 isn_;
    uint64_t initial_RTO_ms_;

    // 从peer接收到的消息，还没建立连接时，初始化为{nullopt,1}
    TCPReceiverMessage receive_info_;

    // 发送窗口，保存等待被peer确认的包
    std::vector<TCPSenderMessage> wait_queue_;

    // 计时器
    Timer timer;

    // 是否发出[peer接收窗口为0，wait_queue_中没东西，要搞一个len=1的包过去]
    bool read_one_byte_{false};

    // SYN有没有push过
    bool is_SYN{false};

    // FIN有没有push过
    bool is_FIN{false};

    // 一次连接中最大的重传次数
    int consecutive_retransmissions_{0};

    // wait_queue_中所含bytes（尚未被peer确认的bytes）
    uint64_t sequence_numbers_in_flight_{0};

    // 在wait_queue_中还没发送过的数据包的下标，has_send_之前都是已经发送但还没被确认的包
    uint64_t has_send_{0};

    // 下一个*Sender*应该发的，仅用于send_empty_message()
    uint64_t expect_{0};

   public:
    /* Construct TCP sender with given default Retransmission Timeout and
     * possible ISN */
    TCPSender(uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn);

    /* Push bytes from the outbound stream */
    void push(Reader& outbound_stream);

    /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
    std::optional<TCPSenderMessage> maybe_send();

    /* Generate an empty TCPSenderMessage */
    TCPSenderMessage send_empty_message() const;

    /* Receive an act on a TCPReceiverMessage from the peer's receiver */
    void receive(const TCPReceiverMessage& msg);

    /* Time has passed by the given # of mied3lliseconds since the last time the
     * tick() method was called. */
    void tick(uint64_t ms_since_last_tick);

    /* Accessors for use in testing */
    uint64_t sequence_numbers_in_flight()
        const;  // How many sequence numbers are outstanding?
    uint64_t consecutive_retransmissions()
        const;  // How many consecutive *re*transmissions have happened?
};
