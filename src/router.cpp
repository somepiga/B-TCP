#include "router.h"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's
// destination address against prefix_length: For this route to be applicable,
// how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the
//    datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is
// directly attached to the router (in
//    which case, the next hop address should be the datagram's final
//    destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route "
         << Address::from_ipv4_numeric(route_prefix).ip() << "/"
         << static_cast<int>(prefix_length) << " => "
         << (next_hop.has_value() ? next_hop->ip() : "(direct)")
         << " on interface " << interface_num << "\n";

    route_list_.emplace_back(route_prefix, prefix_length, next_hop,
                             interface_num);
}

void Router::route() {
    for (auto& interface : interfaces_) {
        while (true) {
            optional<InternetDatagram> maybe_receive =
                interface.maybe_receive();
            if (!maybe_receive.has_value()) {
                break;
            }
            // // cout << "收到了一个包" << endl;
            if (maybe_receive.value().header.ttl == 0 ||
                maybe_receive.value().header.ttl - 1 == 0) {
                continue;
            }
            bool has_default_route = false;
            RouteNode default_route = route_list_[0];
            uint32_t max_length = 0;
            RouteNode cur_route = route_list_[0];
            for (auto route_node : route_list_) {
                if (route_node.prefix_length_ == 0) {
                    default_route = route_node;
                    has_default_route = true;
                }
                if (route_prefix_equal(route_node.route_prefix_,
                                       maybe_receive.value(),
                                       route_node.prefix_length_)) {
                    if (max_length < route_node.route_prefix_) {
                        max_length = route_node.prefix_length_;
                        cur_route = route_node;
                    }
                }
            }
            if (max_length == 0 && has_default_route) {
                maybe_receive.value().header.ttl--;
                if (default_route.next_hop_.has_value()) {
                    interface.send_datagram(maybe_receive.value(),
                                            default_route.next_hop_.value());
                } else {
                    interface.send_datagram(
                        maybe_receive.value(),
                        Address::from_ipv4_numeric(
                            maybe_receive.value().header.dst));
                }
            } else if (max_length > 0) {
                maybe_receive.value().header.ttl--;
                if (cur_route.next_hop_.has_value()) {
                    interface.send_datagram(maybe_receive.value(),
                                            cur_route.next_hop_.value());
                } else {
                    interface.send_datagram(
                        maybe_receive.value(),
                        Address::from_ipv4_numeric(
                            maybe_receive.value().header.dst));
                }
            }
        }
    }
}

bool Router::route_prefix_equal(uint32_t route_prefix,
                                InternetDatagram dgram,
                                uint8_t prefix_length_) {
    if (route_prefix == 0) {
        return false;
    }
    uint32_t const dgram_ip = dgram.header.dst;
    uint32_t const offset =
        (prefix_length_ == 0) ? 0 : 0xffffffff << (32 - prefix_length_);
    // cout << ( route_prefix & offset ) << "  " << ( dgram_ip & offset ) <<
    // endl;
    return (route_prefix & offset) == (dgram_ip & offset);
}
