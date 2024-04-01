#pragma once

#include <cstdint>

/*
 * The Wrap32 type represents a 32-bit unsigned integer that:
 *    - starts at an arbitrary "zero point" (initial value), and
 *    - wraps back to zero when it reaches 2^32 - 1.
 */

class Wrap32
{
public:
  uint32_t raw_value_ {};

public:
  explicit Wrap32( uint32_t raw_value ) : raw_value_( raw_value ) {}

  /* Construct a Wrap32 given an absolute sequence number n and the zero point. */

  /**
   * @brief 将 absolute_seqno 转换为 seqno
   * @param n absolute_seqno
   * @param zero_point ISN
   * @note
   * 实现得很取巧
   * static_cast<uint32_t>(n)               截断数据，相当于直接把 n mod 2^32
   * zero_point + static_cast<uint32_t>(n)  两个uint32_t相加，同理也是 mod 2^32，不用担心相加溢出的问题
   */
  static Wrap32 wrap( uint64_t n, Wrap32 zero_point );

  /*
   * The unwrap method returns an absolute sequence number that wraps to this Wrap32, given the zero point
   * and a "checkpoint": another absolute sequence number near the desired answer.
   *
   * There are many possible absolute sequence numbers that all wrap to the same Wrap32.
   * The unwrap method should return the one that is closest to the checkpoint.
   */

  /**
   * @brief 将 seqno 转换为 checkpoint *后面*最近的absolute_seqno
   * @param zero_point ISN
   * @param checkpoint absolute_seqno
   */
  uint64_t unwrap( Wrap32 zero_point, uint64_t checkpoint ) const;

  Wrap32 operator+( uint32_t n ) const { return Wrap32 { raw_value_ + n }; }
  bool operator==( const Wrap32& other ) const { return raw_value_ == other.raw_value_; }
};
