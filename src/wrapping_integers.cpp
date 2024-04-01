#include "wrapping_integers.h"
#include <iostream>

using namespace std;

Wrap32 Wrap32::wrap(uint64_t n, Wrap32 zero_point) {
    return Wrap32{zero_point + static_cast<uint32_t>(n)};
}

uint64_t Wrap32::unwrap(Wrap32 zero_point, uint64_t checkpoint) const {
    int32_t const min_step = static_cast<int32_t>(
        raw_value_ - wrap(checkpoint, zero_point).raw_value_);
    auto const res = static_cast<int64_t>(min_step + checkpoint);
    uint64_t const ans =
        (res >= 0) ? static_cast<uint64_t>(res) : res + (1UL << 32);
    return ans;
}
