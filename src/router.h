#pragma once

#include "network_interface.h"

#include <optional>
#include <queue>

// NetworkInterface 的包装器，使主机端异步接口
// 收到的数据包存起来稍后处理
class AsyncNetworkInterface : public NetworkInterface {
    std::queue<InternetDatagram> datagrams_in_{};

   public:
    using NetworkInterface::NetworkInterface;

    // Construct from a NetworkInterface
    explicit AsyncNetworkInterface(NetworkInterface&& interface)
        : NetworkInterface(interface) {}

    // \brief Receives and Ethernet frame and responds appropriately.

    // - 如果类型是 IPv4，则推送到“datagrams_out”队列以供稍后使用
    // - 如果类型是 ARP 请求，则从“sender”字段中学习映射，并且发送 ARP 回复
    // - 如果类型是 ARP 回复，则从“target”字段学习映射
    //
    // 输入参数：收到的以太网帧
    void recv_frame(const EthernetFrame& frame) {
        auto optional_dgram = NetworkInterface::recv_frame(frame);
        if (optional_dgram.has_value()) {
            datagrams_in_.push(std::move(optional_dgram.value()));
        }
    };

    // 已接收的 IPv4 数据报队列
    std::optional<InternetDatagram> maybe_receive() {
        if (datagrams_in_.empty()) {
            return {};
        }

        InternetDatagram datagram = std::move(datagrams_in_.front());
        datagrams_in_.pop();
        return datagram;
    }
};

class RouteNode {
   public:
    uint32_t route_prefix_;
    uint8_t prefix_length_;
    std::optional<Address> next_hop_;
    size_t interface_num_;

    RouteNode(uint32_t route_prefix,
              uint8_t prefix_length,
              std::optional<Address> next_hop,
              size_t interface_num)
        : route_prefix_(route_prefix),
          prefix_length_(prefix_length),
          next_hop_(next_hop),
          interface_num_(interface_num) {}
};

// 具有多个网络接口的路由器
// 在它们之间执行最长前缀匹配路由
class Router {
    // 路由器的网络接口集合
    std::vector<AsyncNetworkInterface> interfaces_{};

    std::vector<RouteNode> route_list_;

   public:
    // 添加一个接口到路由器
    // interface: 一个已经构建好的网络接口
    // 返回接口添加到路由器后的索引
    size_t add_interface(AsyncNetworkInterface&& interface) {
        interfaces_.push_back(std::move(interface));
        return interfaces_.size() - 1;
    }

    // 通过索引访问接口
    AsyncNetworkInterface& interface(size_t N) { return interfaces_.at(N); }

    // 添加路由（转发规则）
    void add_route(uint32_t route_prefix,
                   uint8_t prefix_length,
                   std::optional<Address> next_hop,
                   size_t interface_num);

    // 在接口之间路由数据包。对于每个接口，使用Maybe_receive()方法来消耗每个传入的数据报并在一个接口上将其发送到正确的下一跳
    // 根据最长前缀匹配选择下一跳
    void route();

    bool route_prefix_equal(uint32_t route_prefix,
                            InternetDatagram dgram,
                            uint8_t prefix_length_);
};
