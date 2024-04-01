#pragma once
#include "byte_stream.h"

class Timer {
   public:
    bool launch_{false};
    size_t time_pass_{0};
    size_t curr_RTO_ms_;
    uint64_t initial_RTO_ms_;

    Timer() {
        launch_ = false;
        time_pass_ = 0;
        curr_RTO_ms_ = 0;
        initial_RTO_ms_ = 0;
    }

    Timer(uint64_t initial_RTO_ms) {
        curr_RTO_ms_ = initial_RTO_ms;
        initial_RTO_ms_ = initial_RTO_ms;
    }

    void launch() { launch_ = true; }
};