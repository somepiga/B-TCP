#include "datagram/tcp_message.h"

#include <arpa/inet.h>

#include <cstddef>
#include <sstream>

#include "datagram/checksum.h"
#include "datagram/wrapping_integers.h"

static constexpr uint32_t TCPHeaderMinLen = 5;  // 32-bit words

using namespace std;

void TCPSegment::parse(Parser& parser,
                       uint32_t datagram_layer_pseudo_checksum) {
  {
    /* verify checksum */
    Parser parser2 = parser;
    Buffer all_remaining;
    parser2.all_remaining(all_remaining);
    InternetChecksum check{datagram_layer_pseudo_checksum};
    check.add(all_remaining);
    if (check.value()) {
      parser.set_error();
      return;
    }
  }

  uint32_t raw32{};
  uint16_t raw16{};
  uint8_t octet{};

  parser.integer(udinfo.src_port);
  parser.integer(udinfo.dst_port);

  parser.integer(raw32);
  sender_message.seqno = Wrap32{raw32};

  parser.integer(raw32);
  receiver_message.ackno = Wrap32{raw32};

  parser.integer(octet);
  const uint8_t data_offset = octet >> 4;

  parser.integer(octet);  // flags
  if (not(octet & 0b0001'0000)) {
    receiver_message.ackno.reset();  // no ACK
  }

  reset = octet & 0b0000'0100;
  sender_message.SYN = octet & 0b0000'0010;
  sender_message.FIN = octet & 0b0000'0001;

  parser.integer(receiver_message.window_size);
  parser.integer(udinfo.cksum);
  parser.integer(raw16);  // urgent pointer

  // skip any options or anything extra in the header
  if (data_offset < TCPHeaderMinLen) {
    parser.set_error();
  }
  parser.remove_prefix(data_offset * 4 - TCPHeaderMinLen * 4);

  parser.all_remaining(sender_message.payload);
}

class Wrap32Serializable : public Wrap32 {
 public:
  uint32_t raw_value() const { return raw_value_; }
};

void TCPSegment::serialize(Serializer& serializer) const {
  serializer.integer(udinfo.src_port);
  serializer.integer(udinfo.dst_port);
  serializer.integer(Wrap32Serializable{sender_message.seqno}.raw_value());
  serializer.integer(
      Wrap32Serializable{receiver_message.ackno.value_or(Wrap32{0})}
          .raw_value());
  serializer.integer(uint8_t{TCPHeaderMinLen << 4});  // data offset
  const uint8_t flags =
      (receiver_message.ackno.has_value() ? 0b0001'0000U : 0) |
      (reset ? 0b0000'0100U : 0) | (sender_message.SYN ? 0b0000'0010U : 0) |
      (sender_message.FIN ? 0b0000'0001U : 0);
  serializer.integer(flags);
  serializer.integer(receiver_message.window_size);
  serializer.integer(udinfo.cksum);
  serializer.integer(uint16_t{0});  // urgent pointer
  serializer.buffer(sender_message.payload);
}

void TCPSegment::compute_checksum(uint32_t datagram_layer_pseudo_checksum) {
  udinfo.cksum = 0;
  Serializer s;
  serialize(s);

  InternetChecksum check{datagram_layer_pseudo_checksum};
  check.add(s.output());
  udinfo.cksum = check.value();
}

void IPv4Header::parse(Parser& parser) {
  uint8_t first_byte{};
  parser.integer(first_byte);
  ver = first_byte >> 4;     // version
  hlen = first_byte & 0x0f;  // header length
  parser.integer(tos);       // type of service
  parser.integer(len);
  parser.integer(id);

  uint16_t fo_val{};
  parser.integer(fo_val);
  df = static_cast<bool>(fo_val & 0x4000);  // don't fragment
  mf = static_cast<bool>(fo_val & 0x2000);  // more fragments
  offset = fo_val & 0x1fff;                 // offset

  parser.integer(ttl);
  parser.integer(proto);
  parser.integer(cksum);
  parser.integer(src);
  parser.integer(dst);

  if (ver != 4) {
    parser.set_error();
  }

  if (hlen < 5) {
    parser.set_error();
  }

  if (parser.has_error()) {
    return;
  }

  parser.remove_prefix(static_cast<uint64_t>(hlen) * 4 - IPv4Header::LENGTH);

  // Verify checksum
  const uint16_t given_cksum = cksum;
  compute_checksum();
  if (cksum != given_cksum) {
    parser.set_error();
  }
}

// Serialize the IPv4Header (does not recompute the checksum)
void IPv4Header::serialize(Serializer& serializer) const {
  // consistency checks
  if (ver != 4) {
    throw runtime_error("wrong IP version");
  }

  const uint8_t first_byte = (static_cast<uint32_t>(ver) << 4) | (hlen & 0xfU);
  serializer.integer(first_byte);  // version and header length
  serializer.integer(tos);
  serializer.integer(len);
  serializer.integer(id);

  const uint16_t fo_val =
      (df ? 0x4000U : 0) | (mf ? 0x2000U : 0) | (offset & 0x1fffU);
  serializer.integer(fo_val);

  serializer.integer(ttl);
  serializer.integer(proto);

  serializer.integer(cksum);

  serializer.integer(src);
  serializer.integer(dst);
}

uint16_t IPv4Header::payload_length() const { return len - 4 * hlen; }

//! \details 计算封装 TCP 段的校验和时需要该值.
//!
//!   0      7 8     15 16    23 24    31
//!  +--------+--------+--------+--------+
//!  |          source address           |
//!  +--------+--------+--------+--------+
//!  |        destination address        |
//!  +--------+--------+--------+--------+
//!  |  zero  |protocol|  payload length |
//!  +--------+--------+--------+--------+
//!
uint32_t IPv4Header::pseudo_checksum() const {
  uint32_t pcksum = 0;
  pcksum +=
      (src >> 16) + static_cast<uint16_t>(src);  // 源地址 (source address)
  pcksum += (dst >> 16) +
            static_cast<uint16_t>(dst);  // 目的地址 (destination address)
  pcksum += proto;                       // 协议 (protocol)
  pcksum += payload_length();            // 载荷长度 (payload length)
  return pcksum;
}

void IPv4Header::compute_checksum() {
  cksum = 0;
  Serializer s;
  serialize(s);

  // IP 校验和仅检验头部
  InternetChecksum check;
  check.add(s.output());
  cksum = check.value();
}

std::string IPv4Header::to_string() const {
  std::stringstream ss{};
  ss << std::hex << std::boolalpha << "IPv" << +ver << ", "
     << "len=" << std::dec << +len << ", "
     << "protocol=" << +proto << ", "
     << (ttl >= 10 ? "" : "ttl=" + ::to_string(ttl) + ", ")
     << "src=" << inet_ntoa({htobe32(src)}) << ", "
     << "dst=" << inet_ntoa({htobe32(dst)});
  return ss.str();
}
