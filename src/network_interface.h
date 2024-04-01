#pragma once

#include "address.h"
#include "ethernet_frame.h"
#include "ipv4_datagram.h"
#include "timer.h"

#include <iostream>
#include <list>
#include <optional>
#include <queue>
#include <unordered_map>
#include <utility>

class IPMAPPING {
   public:
    std::pair<Address, EthernetAddress> map;

    // [ip, mac] 映射关系保存30秒
    Timer timer;

    IPMAPPING(const Address& address, const EthernetAddress& ethAddress)
        : map(address, ethAddress) {}
};

// 网络层和数据链路层之间的接口

// 该模块是 TCP/IP 堆栈的最底层(将IP与下层网络协议连接起来，例如以太网)
// 该模块也在路由器Router中被反复使用

// 实现 TCP/IP 报文 和 以太网帧 相互转化
// 填写目的mac地址时，使用ARP协议
// 接收到以太网帧时，若为IPv4报文，向上提交；若为ARP包，储存到ARP表中
class NetworkInterface {
   public:
    // mac地址
    EthernetAddress ethernet_address_;

    // IP地址
    Address ip_address_;

    // 直接和maybe_send挂钩，都是已经封装好的EthernetFrame
    std::queue<EthernetFrame> wait_send_;

    // 好像是后面的lab6要用到，仅仅用来模拟广播
    // std::queue<EthernetFrame> broadcast_arp_;

    std::list<std::pair<InternetDatagram, Address>> wait_arp_;

    std::vector<IPMAPPING> map_ = {};

    // ARP 5秒计时器
    Timer arp_timer_;

    // 当前已经向该地址发过ARP了,5秒钟清除一次(换成本机ip)
    Address ip_needed_;

   public:
    // 输入参数：本地IP和mac地址
    NetworkInterface(const EthernetAddress& ethernet_address,
                     const Address& ip_address);

    // 等待传输的以太网帧队列
    std::optional<EthernetFrame> maybe_send();

    // 发送封装在以太网帧中的 IPv4 数据报;这里会调用ARP请求
    // ("Sending" is accomplished by making sure maybe_send() will release the
    // frame when next called, but please consider the frame sent as soon as it
    // is generated.)
    void send_datagram(const InternetDatagram& dgram, const Address& next_hop);

    // 接收以太网帧并做出适当响应
    // 如果类型为 IPv4，则返回数据报
    // If type is ARP request, learn a mapping from the "sender" fields, and
    // send an ARP reply. If type is ARP reply, learn a mapping from the
    // "sender" fields.
    std::optional<InternetDatagram> recv_frame(const EthernetFrame& frame);

    // Called periodically when time elapses
    void tick(size_t ms_since_last_tick);

    void note_addr(Address ip, EthernetAddress mac);

    void ready_to_send();
};
