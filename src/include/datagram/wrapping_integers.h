#pragma once

#include <cstdint>

/**
 * @brief 转换绝对序列号和相对序列号
 * Wrap32 类型表示 32 位无符号整数
 *  - 从任意"零点"（初始值）开始
 *  - 当达到 2^32 - 1 时回绕到零
 */
class Wrap32 {
 protected:
  uint32_t raw_value_{};  //!< 绝对序列号 (uint32)

 public:
  explicit Wrap32(uint32_t raw_value) : raw_value_(raw_value) {}

  //!< 给定绝对序列号 n 和零点，构造一个 Wrap32
  static Wrap32 wrap(uint64_t n, Wrap32 zero_point);

  /*
   * The unwrap method returns an absolute sequence number that wraps to this
   * Wrap32, given the zero point and a "checkpoint": another absolute sequence
   * number near the desired answer.
   *
   * There are many possible absolute sequence numbers that all wrap to the same
   * Wrap32. The unwrap method should return the one that is closest to the
   * checkpoint.
   */
  uint64_t unwrap(Wrap32 zero_point, uint64_t checkpoint) const;

  Wrap32 operator+(uint32_t n) const { return Wrap32{raw_value_ + n}; }
  bool operator==(const Wrap32& other) const {
    return raw_value_ == other.raw_value_;
  }
};
