#include "reassembler.h"
#include <iostream>
using namespace std;

void Reassembler::insert(uint64_t first_index,
                         string data,
                         bool is_last_substring,
                         Writer& output) {
    // 初始化sliding_window大小
    if (sliding_window_.empty()) {
        sliding_window_.push_back(0);
        sliding_window_.push_back(output.GetCapacity());
    }

    // 如果不在sliding_window范围内的就直接扔掉
    // 但是有可能是空字符串携带EOF，因此还需判断is_finished_
    if (first_index >= sliding_window_[1] ||
        first_index + data.length() <= sliding_window_[0]) {
        is_finished_ |= is_last_substring;
        if (is_finished_) {
            output.close();
        }
        return;
    }

    // 先区间合并
    indexs_.push_back({first_index, first_index + data.length()});
    std::sort(indexs_.begin(), indexs_.end(),
              [](const std::vector<uint64_t>& a,
                 const std::vector<uint64_t>& b) { return a[0] < b[0]; });
    vector<vector<uint64_t>> indexs = {indexs_[0]};
    for (uint64_t i = 1; i < indexs_.size(); ++i) {
        vector<uint64_t> cur = indexs_[i];
        vector<uint64_t>& last = indexs.at(indexs.size() - 1);
        if (cur[0] <= last[1]) {
            last[1] = (cur[1] > last[1]) ? cur[1] : last[1];
        } else {
            indexs.push_back(cur);
        }
    }
    if (indexs[0][1] > sliding_window_[1]) {
        indexs[0][1] = sliding_window_[1];
    }
    indexs_ = indexs;

    // 后面的字符串先到，需要将本地data_扩容
    if (first_index > data_.length()) {
        data_.resize(first_index + data.length());
    }
    data_.replace(first_index, data.length(), data);
    is_finished_ |= is_last_substring;

    // 更新stored_bytes_
    if (indexs_.empty()) {
        stored_bytes_ = 0;
    } else {
        uint64_t temp = 0;
        for (auto& index : indexs_) {
            temp += index[1] - index[0];
        }
        stored_bytes_ = temp;
    }
    // 至此，substring已经被存入Reassembler
    // 判断是否需要传给ByteStream
    if (indexs_[0][0] <= sliding_window_[0] &&
        sliding_window_[0] <= indexs_[0][1]) {
        uint64_t const push_size =
            min(min(output.available_capacity(),
                    sliding_window_[1] - sliding_window_[0]),
                indexs_[0][1] - sliding_window_[0]);
        output.push(data_.substr(sliding_window_[0], push_size));
        sliding_window_[0] += push_size;
        sliding_window_[1] += push_size;
        stored_bytes_ -= indexs_[0][1] - indexs_[0][0];
        indexs_.erase(indexs_.begin());
        if (is_finished_) {
            output.close();
        }
    }
}

uint64_t Reassembler::bytes_pending() const {
    return stored_bytes_;
}