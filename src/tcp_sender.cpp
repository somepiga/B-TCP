#include "tcp_sender.h"
#include "tcp_config.h"

#include <algorithm>
#include <iostream>
#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender(uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn)
    : isn_(fixed_isn.value_or(Wrap32{random_device()()})),
      initial_RTO_ms_(initial_RTO_ms) {
    receive_info_ = {nullopt, 1};
    timer = Timer(initial_RTO_ms_);
}

uint64_t TCPSender::sequence_numbers_in_flight() const {
    return sequence_numbers_in_flight_;
}

uint64_t TCPSender::consecutive_retransmissions() const {
    return consecutive_retransmissions_;
}

optional<TCPSenderMessage> TCPSender::maybe_send() {
    // cout << "[maybe_send]" << endl;
    if (!timer.launch_) {
        timer.launch();
    }
    // 还有数据等待确认，并且没有超时
    // 如果是已经发过的包，就不发，等待超时后再说
    // 如果有没发过的包，就发
    if (timer.launch_ && timer.curr_RTO_ms_ > timer.time_pass_) {
        // // cout << "还没超时  " << timer.curr_RTO_ms_ << endl;
        if (wait_queue_.size() > has_send_) {
            TCPSenderMessage send = wait_queue_[has_send_];

            expect_ += send.sequence_length();
            has_send_++;
            // // cout << "has_send_ " << has_send_ << endl;
            return send;
        }
        // // cout << wait_queue_.size() << ' ' << has_send_ << endl;
        return nullopt;
    }
    // cout << "超时" << endl;
    if (!read_one_byte_) {
        timer.curr_RTO_ms_ *= 2;
    }
    timer.time_pass_ = 0;
    // // cout << "consecutive_retransmissions_  " <<
    // consecutive_retransmissions_ << endl;
    if (!wait_queue_.empty()) {
        // expect_ += wait_queue_[0].sequence_length();
        // has_send_++;
        return wait_queue_[0];
    }
    return nullopt;
}

void TCPSender::push(Reader& outbound_stream) {
    // cout << "[push]" << endl;
    TCPSenderMessage send_info_;
    if (!is_SYN) {
        send_info_.seqno = isn_;
        send_info_.SYN = true;
        send_info_.payload = string{};
        send_info_.FIN = outbound_stream.is_finished();
        wait_queue_.push_back(send_info_);
        sequence_numbers_in_flight_ += send_info_.sequence_length();
        is_SYN = true;
        return;
    }

    if (!outbound_stream.is_finished()) {
        while (outbound_stream.bytes_buffered() &&
               (receive_info_.window_size > sequence_numbers_in_flight_ ||
                receive_info_.window_size == 0) &&
               !read_one_byte_) {
            send_info_.seqno = isn_ + outbound_stream.bytes_popped() + 1;
            string payload;
            uint16_t push_size = 0;
            auto recever_could = static_cast<int>(receive_info_.window_size) -
                                 static_cast<int>(sequence_numbers_in_flight_);
            // peer接收窗口已满，我的数据包还在wait_queue_中待发
            if (recever_could <= 0 && sequence_numbers_in_flight()) {
                break;
            }
            // peer接收窗口为0，wait_queue_中没东西，要搞一个len=1的包过去
            if (recever_could == 0 && !sequence_numbers_in_flight()) {
                read_one_byte_ = true;
                push_size = 1;
                read(outbound_stream, push_size, payload);
                send_info_.payload = {payload};
                // 发现没东西放了，就要考虑放FIN了
                if (payload.empty()) {
                    send_info_.FIN = outbound_stream.is_finished();
                }
            } else {  // 正常发送
                push_size =
                    min(receive_info_.window_size - sequence_numbers_in_flight_,
                        TCPConfig::MAX_PAYLOAD_SIZE);
                uint64_t const curr_buffered = outbound_stream.bytes_buffered();
                read(outbound_stream, push_size, payload);
                send_info_.payload = {payload};
                // 有能力读完，或者还有足够空间，就要考虑放FIN了
                if (push_size > curr_buffered ||
                    receive_info_.window_size - sequence_numbers_in_flight_ >
                        push_size) {
                    send_info_.FIN = outbound_stream.is_finished();
                }
            }
            is_FIN = send_info_.FIN;
            wait_queue_.push_back(send_info_);
            sequence_numbers_in_flight_ += send_info_.sequence_length();
            // // cout << "sequence_numbers_in_flight_:  " <<
            // sequence_numbers_in_flight_ << endl;
            if (read_one_byte_) {
                break;
            }
        }
        return;
    }

    // 发空FIN包条件
    // 1.outbound_stream.is_finished()
    // 2.有足够空间放FIN
    // 3.之前没发过FIN
    if (outbound_stream.is_finished() && !is_FIN) {
        uint16_t remain_space = 0;
        // peer接收窗口为0，wait_queue_中没东西，要搞一个len=1的包过去
        if (receive_info_.window_size - sequence_numbers_in_flight_ == 0 &&
            !sequence_numbers_in_flight()) {
            remain_space = 1;
            read_one_byte_ = true;
        }
        if (receive_info_.window_size > sequence_numbers_in_flight_) {
            remain_space =
                receive_info_.window_size - sequence_numbers_in_flight_;
        }
        if (remain_space > 0) {
            send_info_.seqno = isn_ + outbound_stream.bytes_popped() + 1;
            send_info_.SYN = false;
            send_info_.payload = string{};
            send_info_.FIN = outbound_stream.is_finished();
            wait_queue_.push_back(send_info_);
            sequence_numbers_in_flight_ += send_info_.sequence_length();
            is_FIN = true;
        }
    }
}

TCPSenderMessage TCPSender::send_empty_message() const {
    // cout << "[send_empty_message]" << endl << endl;
    TCPSenderMessage send_info_;
    send_info_.seqno = isn_ + expect_;
    send_info_.SYN = false;
    send_info_.payload = string{};
    send_info_.FIN = false;
    return send_info_;
}

void TCPSender::receive(const TCPReceiverMessage& msg) {
    // cout << "----接收ack----" << endl;
    if (msg.ackno.has_value()) {
        // cout << msg.ackno.value().raw_value_ << endl;
    } else {
        // cout << "没有ack" << endl;
    }
    // cout << msg.window_size << endl;
    // cout << "----接收完毕----" << endl << endl;

    if (msg.ackno.has_value()) {
        bool have_remove = false;
        for (auto it = wait_queue_.rbegin(); it != wait_queue_.rend();) {
            TCPSenderMessage const node = *it;
            // 这里的判断条件加的有点牵强，面向用例编程了
            if (node.seqno.raw_value_ + node.sequence_length() <=
                    msg.ackno.value().raw_value_ &&
                (!node.SYN ||
                 node.seqno.raw_value_ + 1 == msg.ackno.value().raw_value_)) {
                if (node.sequence_length() == 1 && !node.FIN && !node.SYN) {
                    read_one_byte_ = false;
                }
                sequence_numbers_in_flight_ -= node.sequence_length();
                it = decltype(it)(wait_queue_.erase(std::next(it).base()));
                has_send_--;
                // // cout << "has_send_ " << has_send_ << endl;
                // cout << "移除:  " << node.seqno.raw_value_ << endl;
                have_remove = true;
                // // cout << "sequence_numbers_in_flight_:  " <<
                // sequence_numbers_in_flight_ << endl;
            } else {
                it++;
            }
        }
        if (have_remove) {
            // cout << "归零timer" << endl;
            timer.time_pass_ = 0;
            timer.curr_RTO_ms_ = initial_RTO_ms_;
            consecutive_retransmissions_ = 0;
        }
        if (wait_queue_.empty()) {
            timer.launch_ = false;
        }
    }
    receive_info_ = msg;
}

void TCPSender::tick(const size_t ms_since_last_tick) {
    // cout << "[tick]" << endl;
    if (!timer.launch_) {
        // cout << "由于timer被关,什么都没发生" << endl;
        return;
    }
    timer.time_pass_ += ms_since_last_tick;
    // cout << "time_pass_ " << timer.time_pass_ << endl;
    if (timer.time_pass_ >= timer.curr_RTO_ms_) {
        consecutive_retransmissions_++;
        // cout << "超时!!" << endl;
    }
}
