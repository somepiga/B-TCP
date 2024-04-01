#pragma once

#include "byte_stream.h"
#include "eventloop.h"
#include "file_descriptor.h"
#include "network_interface.h"
#include "socket.h"
#include "tcp_config.h"
#include "tcp_peer.h"
#include "tuntap_adapter.h"

#include <atomic>
#include <cstdint>
#include <optional>
#include <thread>
#include <vector>

//! TCPPeer 的多线程包装器，近似 Unix 套接字 API
template <typename AdaptT>
class TCPMinnowSocket : public LocalStreamSocket {
   private:
    //! 用于在所有者和 TCP 线程之间进行读写的流套接字
    LocalStreamSocket _thread_data;

   protected:
    //! 底层数据报套接字的适配器（例如 UDP 或 IP）
    AdaptT _datagram_adapter;

   private:
    //! 初始化 TCPPeer 和 event loop
    void _initialize_TCP(const TCPConfig& config);

    //! TCP状态机
    std::optional<TCPPeer> _tcp{};

    //! 排队等待在网络上发送的段
    std::queue<TCPSegment> outgoing_segments_{};

    //! 处理所有事件的事件循环（新的入站数据报、新的出站字节、新的入站字节）
    EventLoop _eventloop{};

    //! 当指定条件为真时处理事件
    void _tcp_loop(const std::function<bool()>& condition);

    //! TCPPeer线程的主循环
    void _tcp_main();

    //! TCPPeer 线程的句柄；所有者线程在析构函数中调用 join()
    std::thread _tcp_thread{};

    //! 从套接字对构造LocalStreamSocket fds，初始化eventloop
    TCPMinnowSocket(std::pair<FileDescriptor, FileDescriptor> data_socket_pair,
                    AdaptT&& datagram_interface);

    std::atomic_bool _abort{false};  //!< 所有者使用的标志来强制TCPPeer线程关闭

    bool _inbound_shutdown{
        false};  //!< TCPMinnowSocket 是否已关闭向所有者传入数据？

    bool _outbound_shutdown{false};  //!< 所有者是否关闭了出站数据到TCP连接？

    bool _fully_acked{false};  //!< 出站数据是否得到对方充分认可？

    void collect_segments();  //!< 从 TCPPeer 中排出段

   public:
    //! Construct from the interface that the TCPPeer thread will use to read
    //! and write datagrams
    explicit TCPMinnowSocket(AdaptT&& datagram_interface);

    //! Close socket, and wait for TCPPeer to finish
    //! \note Calling this function is only advisable if the socket has reached
    //! EOF, or else may wait foreever for remote peer to close the TCP
    //! connection.
    void wait_until_closed();

    //! 使用指定的配置进行连接
    // 一直阻塞，直到连接成功或失败
    void connect(const TCPConfig& c_tcp, const FdAdapterConfig& c_ad);

    //! 使用指定的配置监听并接受
    // 一直阻塞，直到连接成功或失败
    void listen_and_accept(const TCPConfig& c_tcp, const FdAdapterConfig& c_ad);

    //! When a connected socket is destructed, it will send a RST
    ~TCPMinnowSocket();

    //! \name
    //! 不能移动或拷贝，因为被两个线程占用

    //!@{
    TCPMinnowSocket(const TCPMinnowSocket&) = delete;
    TCPMinnowSocket(TCPMinnowSocket&&) = delete;
    TCPMinnowSocket& operator=(const TCPMinnowSocket&) = delete;
    TCPMinnowSocket& operator=(TCPMinnowSocket&&) = delete;
    //!@}

    //! \name
    //! 父 Socket 的某些方法在 TCP 套接字上无法按预期工作，因此删除

    //!@{
    void bind(const Address& address) = delete;
    Address local_address() const = delete;
    Address peer_address() const = delete;
    void set_reuseaddr() = delete;
    //!@}
};

using TCPOverIPv4MinnowSocket = TCPMinnowSocket<TCPOverIPv4OverTunFdAdapter>;
using TCPOverIPv4OverEthernetMinnowSocket =
    TCPMinnowSocket<TCPOverIPv4OverEthernetAdapter>;

using LossyTCPOverIPv4MinnowSocket =
    TCPMinnowSocket<LossyTCPOverIPv4OverTunFdAdapter>;

//! \class TCPMinnowSocket
//! This class involves the simultaneous operation of two threads.
//!
//! One, the "owner" or foreground thread, interacts with this class in much the
//! same way as one would interact with a TCPSocket: it connects or listens,
//! writes to and reads from a reliable data stream, etc. Only the owner thread
//! calls public methods of this class.
//!
//! The other, the "TCPPeer" thread, takes care of the back-end tasks that the
//! kernel would perform for a TCPSocket: reading and parsing datagrams from the
//! wire, filtering out segments unrelated to the connection, etc.
//!
//! There are a few notable differences between the TCPMinnowSocket and
//! TCPSocket interfaces:
//!
//! - a TCPMinnowSocket can only accept a single connection
//! - listen_and_accept() is a blocking function call that acts as both
//! [listen(2)](\ref man2::listen)
//!   and [accept(2)](\ref man2::accept)
//! - if TCPMinnowSocket is destructed while a TCP connection is open, the
//! connection is
//!   immediately terminated with a RST (call `wait_until_closed` to avoid this)

//! Helper class that makes a TCPOverIPv4MinnowSocket behave more like a
//! (kernel) TCPSocket
class CS144TCPSocket : public TCPOverIPv4MinnowSocket {
   public:
    CS144TCPSocket();
    void connect(const Address& address);
};

//! 使 TCPOverIPv4overEthernetMinnowSocket 表现得更好的帮助程序类
//! 就像（内核）TCPSocket
class FullStackSocket : public TCPOverIPv4OverEthernetMinnowSocket {
   public:
    //! 使用 CS144 TCPPeer 对象构造 TCP（流）套接字，
    // 将 TCP 段封装在 IP 数据报中，然后封装
    // 这些IP数据报以以太网帧的形式发送到下一跳的以太网地址。
    FullStackSocket();
    void connect(const Address& address);
};
