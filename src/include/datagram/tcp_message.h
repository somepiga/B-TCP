#pragma once

#include <optional>
#include <string>

#include "buffer/string_buffer.h"
#include "utils/parser.h"
#include "wrapping_integers.h"

/**
 * @brief TCP发送报文格式
 */
struct TCPSenderMessage {
  Wrap32 seqno{0};
  bool SYN{false};
  Buffer payload{};
  bool FIN{false};

  // How many sequence numbers does this segment use?
  size_t sequence_length() const { return SYN + payload.size() + FIN; }
};

/**
 * @brief TCP接收报文格式
 */
struct TCPReceiverMessage {
  std::optional<Wrap32> ackno{};
  uint16_t window_size{};
};

struct UserDatagramInfo {
  uint16_t src_port;
  uint16_t dst_port;
  uint16_t cksum;
};

struct TCPSegment {
  TCPSenderMessage sender_message{};
  TCPReceiverMessage receiver_message{};
  bool reset{};  // Connection experienced an abnormal error and should be shut
                 // down
  UserDatagramInfo udinfo{};

  void parse(Parser& parser, uint32_t datagram_layer_pseudo_checksum);
  void serialize(Serializer& serializer) const;

  void compute_checksum(uint32_t datagram_layer_pseudo_checksum);
};

/**
 * @brief IP报头
 */
struct IPv4Header {
  static constexpr size_t LENGTH =
      20;  // IPv4 header length, not including options
  static constexpr uint8_t DEFAULT_TTL = 128;  // A reasonable default TTL value
  static constexpr uint8_t PROTO_TCP = 6;      // Protocol number for TCP

  static constexpr uint64_t serialized_length() { return LENGTH; }

  /*
   *   0                   1                   2                   3
   *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *  |Version|  IHL  |Type of Service|          Total Length         |
   *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *  |         Identification        |Flags|      Fragment Offset    |
   *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *  |  Time to Live |    Protocol   |         Header Checksum       |
   *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *  |                       Source Address                          |
   *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *  |                    Destination Address                        |
   *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *  |                    Options                    |    Padding    |
   *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   */

  // IPv4 Header fields
  uint8_t ver = 4;            // IP version
  uint8_t hlen = LENGTH / 4;  // header length (multiples of 32 bits)
  uint8_t tos = 0;            // type of service
  uint16_t len = 0;           // total length of packet
  uint16_t id = 0;            // identification number
  bool df = true;             // don't fragment flag
  bool mf = false;            // more fragments flag
  uint16_t offset = 0;        // fragment offset field
  uint8_t ttl = DEFAULT_TTL;  // time to live field
  uint8_t proto = PROTO_TCP;  // protocol field
  uint16_t cksum = 0;         // checksum field
  uint32_t src = 0;           // src address
  uint32_t dst = 0;           // dst address

  // Length of the payload
  uint16_t payload_length() const;

  // Pseudo-header's contribution to the TCP checksum
  uint32_t pseudo_checksum() const;

  // Set checksum to correct value
  void compute_checksum();

  // Return a string containing a header in human-readable format
  std::string to_string() const;

  void parse(Parser& parser);
  void serialize(Serializer& serializer) const;
};

/**
 * @brief TCP数据报
 */
struct IPv4Datagram {
  IPv4Header header{};
  std::vector<Buffer> payload{};

  void parse(Parser& parser) {
    header.parse(parser);
    parser.all_remaining(payload);
  }

  void serialize(Serializer& serializer) const {
    header.serialize(serializer);
    for (const auto& x : payload) {
      serializer.buffer(x);
    }
  }
};