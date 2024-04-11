#include "datagram/wrapping_integers.h"

#include <algorithm>

using namespace std;

Wrap32 Wrap32::wrap(uint64_t n, Wrap32 zero_point) {
  return zero_point + n;  // n % ( 1LL << 32 )
}

uint64_t Wrap32::unwrap(Wrap32 zero_point, uint64_t checkpoint) const {
  if (!checkpoint) {
    return raw_value_ - zero_point.raw_value_;
  }
  uint32_t offset = raw_value_ - zero_point.raw_value_;
  uint32_t count = checkpoint / (1LL << 32);
  uint32_t ckpt_offset = checkpoint % (1LL << 32);
  if (max(offset, ckpt_offset) - min(offset, ckpt_offset) > (1LL << 31)) {
    if (ckpt_offset > offset) {
      count += 1;
    } else if (count) {
      count -= 1;
    }
  }
  return (1UL << 32) * count + offset;
}
