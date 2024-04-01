#pragma once

#include "byte_stream.h"

#include <algorithm>
#include <string>
#include <vector>

class Reassembler {
   public:
    /*
     * Insert a new substring to be reassembled into a ByteStream.
     *   `first_index`: the index of the first byte of the substring
     *   `data`: the substring itself
     *   `is_last_substring`: this substring represents the end of the stream
     *   `output`: a mutable reference to the Writer
     *
     * The Reassembler's job is to reassemble the indexed substrings (possibly
     * out-of-order and possibly overlapping) back into the original ByteStream.
     * As soon as the Reassembler learns the next byte in the stream, it should
     * write it to the output.
     *
     * If the Reassembler learns about bytes that fit within the stream's
     * available capacity but can't yet be written (because earlier bytes remain
     * unknown), it should store them internally until the gaps are filled in.
     *
     * The Reassembler should discard any bytes that lie beyond the stream's
     * available capacity (i.e., bytes that couldn't be written even if earlier
     * gaps get filled in).
     *
     * The Reassembler should close the stream after writing the last byte.
     */
    void insert(uint64_t first_index,
                std::string data,
                bool is_last_substring,
                Writer& output);

    // How many bytes are stored in the Reassembler itself?
    uint64_t bytes_pending() const;

    // 存放substring
    std::string data_;

    // 存放data所有substring的区间
    std::vector<std::vector<uint64_t>> indexs_ = {};

    // 目前接收方的滑动窗口
    std::vector<uint64_t> sliding_window_;

    // 注意，这里是bytes数量，不是substring数量
    uint64_t stored_bytes_{0};

    // 看看substring有没有发送完，当is_last_substring = true时，is_finished 置为
    // true
    bool is_finished_{false};
};
