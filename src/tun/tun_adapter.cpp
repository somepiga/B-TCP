#include "tun/tun_adapter.h"

#include <arpa/inet.h>

#include "utils/parser.h"

using namespace std;

optional<TCPSegment> TCPOverIPv4OverTunFdAdapter::unwrap_tcp_in_ip(
    const IPv4Datagram& ip_dgram) {
  // is the IPv4 datagram for us?
  // Note: it's valid to bind to address "0" (INADDR_ANY) and reply from actual
  // address contacted
  if (not listening() and
      (ip_dgram.header.dst != config().source.ipv4_numeric())) {
    return {};
  }

  // is the IPv4 datagram from our peer?
  if (not listening() and
      (ip_dgram.header.src != config().destination.ipv4_numeric())) {
    return {};
  }

  // does the IPv4 datagram claim that its payload is a TCP segment?
  if (ip_dgram.header.proto != IPv4Header::PROTO_TCP) {
    return {};
  }

  // is the payload a valid TCP segment?
  TCPSegment tcp_seg;
  if (not parse(tcp_seg, ip_dgram.payload, ip_dgram.header.pseudo_checksum())) {
    return {};
  }

  // is the TCP segment for us?
  if (tcp_seg.udinfo.dst_port != config().source.port()) {
    return {};
  }

  // should we target this source addr/port (and use its destination addr as our
  // source) in reply?
  if (listening()) {
    if (tcp_seg.sender_message.SYN and not tcp_seg.reset) {
      config_mutable().source = Address{
          inet_ntoa({htobe32(ip_dgram.header.dst)}), config().source.port()};
      config_mutable().destination = Address{
          inet_ntoa({htobe32(ip_dgram.header.src)}), tcp_seg.udinfo.src_port};
      set_listening(false);
    } else {
      return {};
    }
  }

  // is the TCP segment from our peer?
  if (tcp_seg.udinfo.src_port != config().destination.port()) {
    return {};
  }

  return tcp_seg;
}

IPv4Datagram TCPOverIPv4OverTunFdAdapter::wrap_tcp_in_ip(TCPSegment& seg) {
  // set the port numbers in the TCP segment
  seg.udinfo.src_port = config().source.port();
  seg.udinfo.dst_port = config().destination.port();

  // create an Internet Datagram and set its addresses and length
  IPv4Datagram ip_dgram;
  ip_dgram.header.src = config().source.ipv4_numeric();
  ip_dgram.header.dst = config().destination.ipv4_numeric();
  ip_dgram.header.len = ip_dgram.header.hlen * 4 + 20 /* tcp header len */ +
                        seg.sender_message.payload.size();

  // set payload, calculating TCP checksum using information from IP header
  seg.compute_checksum(ip_dgram.header.pseudo_checksum());
  ip_dgram.header.compute_checksum();
  ip_dgram.payload = serialize(seg);

  return ip_dgram;
}

optional<TCPSegment> TCPOverIPv4OverTunFdAdapter::read() {
  vector<string> strs(2);
  strs.front().resize(IPv4Header::LENGTH);
  _tun.read(strs);

  IPv4Datagram ip_dgram;
  const vector<Buffer> buffers = {strs.at(0), strs.at(1)};
  if (parse(ip_dgram, buffers)) {
    if (_should_drop(false)) {
      return {};
    }
    return unwrap_tcp_in_ip(ip_dgram);
  }
  return {};
}
