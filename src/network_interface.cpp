#include "network_interface.h"

#include "arp_message.h"
#include "ethernet_frame.h"

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the
// interface ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress& ethernet_address,
                                   const Address& ip_address)
    : ethernet_address_(ethernet_address),
      ip_address_(ip_address),
      ip_needed_(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address "
         << to_string(ethernet_address_) << " and IP address "
         << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router
// or default gateway, but may also be another host if directly connected to the
// same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address)
// by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram(const InternetDatagram& dgram,
                                     const Address& next_hop) {
    bool need_arp = true;
    for (auto& ip_map : map_) {
        auto [ip, mac] = ip_map.map;
        if (ip == next_hop) {
            EthernetFrame send;
            EthernetHeader ethernet_header = {};
            ethernet_header.dst = mac;
            ethernet_header.src = ethernet_address_;
            ethernet_header.type = EthernetHeader::TYPE_IPv4;
            send.header = ethernet_header;
            send.payload = serialize(dgram);

            wait_send_.push(send);
            need_arp = false;
            break;
        }
    }

    // 在此之前没有已发送的ARP
    if (need_arp && ip_needed_ == ip_address_) {
        // cout << "发ARP " << next_hop.ip() << endl;
        ip_needed_ = next_hop;

        EthernetHeader ethernet_header = {};
        ethernet_header.type = EthernetHeader::TYPE_ARP;
        ethernet_header.dst = ETHERNET_BROADCAST;
        ethernet_header.src = ethernet_address_;

        ARPMessage payload;
        payload.opcode = ARPMessage::OPCODE_REQUEST;
        payload.sender_ethernet_address = ethernet_address_;
        payload.sender_ip_address = ip_address_.ipv4_numeric();
        payload.target_ethernet_address = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        payload.target_ip_address = next_hop.ipv4_numeric();

        EthernetFrame send;
        send.header = ethernet_header;
        send.payload = serialize(payload);
        wait_send_.push(send);

        wait_arp_.emplace_back(dgram, next_hop);
        // cout << "ARP发好了 " << next_hop.ip() << endl;
    }
    // 之前发过ARP就看不必再发了
    else if (need_arp && ip_needed_ == next_hop) {
        wait_arp_.emplace_back(dgram, next_hop);
    }
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(
    const EthernetFrame& frame) {
    EthernetHeader const ethernet_header = frame.header;
    uint16_t const type = ethernet_header.type;
    if (type == EthernetHeader::TYPE_IPv4) {
        // cout << ip_address_.ip() << "收到了一个IPv4" << endl;
        if (ethernet_header.dst != ethernet_address_) {
            return nullopt;
        }
        InternetDatagram internet_datagram;
        parse(internet_datagram, frame.payload);

        return internet_datagram;
    }

    if (type == EthernetHeader::TYPE_ARP) {
        // cout << ip_address_.ip() << "收到了一个ARP" << endl;
        ARPMessage recv_arp;
        parse(recv_arp, frame.payload);
        // 收到ARP请求，并且请求ip是我（说明是别人不知道我的mac）
        if (recv_arp.target_ip_address == ip_address_.ipv4_numeric() &&
            recv_arp.opcode == ARPMessage::OPCODE_REQUEST) {
            // cout << ip_address_.ip() << "回发ARP" << endl;
            EthernetHeader send_ethernet_header = {};
            send_ethernet_header.type = EthernetHeader::TYPE_ARP;
            send_ethernet_header.dst = recv_arp.sender_ethernet_address;
            send_ethernet_header.src = ethernet_address_;

            ARPMessage ipv4_payload;
            ipv4_payload.opcode = ARPMessage::OPCODE_REPLY;
            ipv4_payload.sender_ethernet_address = ethernet_address_;
            ipv4_payload.sender_ip_address = ip_address_.ipv4_numeric();
            ipv4_payload.target_ethernet_address =
                recv_arp.sender_ethernet_address;
            ipv4_payload.target_ip_address = recv_arp.sender_ip_address;

            EthernetFrame send;
            send.header = send_ethernet_header;
            send.payload = serialize(ipv4_payload);

            note_addr(Address::from_ipv4_numeric(recv_arp.sender_ip_address),
                      recv_arp.sender_ethernet_address);

            wait_send_.push(send);
        }
        // 收到ARP返回，并且返回mac是我（说明是我之前发出的ARP得到回应了）
        if (recv_arp.target_ethernet_address == ethernet_address_ &&
            recv_arp.opcode == ARPMessage::OPCODE_REPLY) {
            note_addr(Address::from_ipv4_numeric(recv_arp.sender_ip_address),
                      recv_arp.sender_ethernet_address);

            ready_to_send();

            // cout << ip_address_.ip() << "收到之前我发的ARP回复" << endl;
        }
    } else {
        // cout << ip_address_.ip() << "收到没用的包" << endl;
    }
    return nullopt;
}

// ms_since_last_tick: the number of milliseconds since the last call to this
// method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    if (arp_timer_.launch_) {
        arp_timer_.time_pass_ += ms_since_last_tick;
        if (arp_timer_.time_pass_ > 5000) {
            ip_needed_ = ip_address_;
            arp_timer_.time_pass_ = 0;
        }
    }
    for (auto it = map_.begin(); it != map_.end();) {
        auto& ip_mapping = *it;
        ip_mapping.timer.time_pass_ += ms_since_last_tick;
        if (ip_mapping.timer.time_pass_ > 30000) {
            it = map_.erase(it);
        } else {
            ++it;
        }
    }
}

optional<EthernetFrame> NetworkInterface::maybe_send() {
    if (!wait_send_.empty()) {
        EthernetFrame send = wait_send_.front();
        // IPv4直接发
        if (send.header.type == EthernetHeader::TYPE_IPv4) {
            wait_send_.pop();
            return send;
        }
        if (send.header.type == EthernetHeader::TYPE_ARP) {
            arp_timer_.launch();
            wait_send_.pop();
            return send;
        }
    }
    return nullopt;
}

void NetworkInterface::note_addr(Address ip, EthernetAddress mac) {
    for (auto& ip_map : map_) {
        auto [ip_, mac_] = ip_map.map;
        if (ip == ip_ && mac == mac_) {
            return;
        }
    }
    IPMAPPING map(ip, mac);
    map.timer.launch();
    map_.emplace_back(map);
}

void NetworkInterface::ready_to_send() {
    for (auto& ip_map : map_) {
        auto [ip, mac] = ip_map.map;
        for (auto it = wait_arp_.begin(); it != wait_arp_.end();) {
            auto [dgram, next_hop] = *it;
            if (ip == next_hop) {
                EthernetFrame send;
                EthernetHeader ethernet_header = {};
                ethernet_header.dst = mac;
                ethernet_header.src = ethernet_address_;
                ethernet_header.type = EthernetHeader::TYPE_IPv4;
                send.header = ethernet_header;
                send.payload = serialize(dgram);

                wait_send_.push(send);

                it = wait_arp_.erase(it);
                if (wait_send_.empty()) {
                    arp_timer_.launch_ = false;
                }
                return;
            }
            ++it;
        }
    }
}
