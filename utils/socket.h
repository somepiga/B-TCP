#pragma once

#include "address.h"
#include "file_descriptor.h"

#include <cstdint>
#include <functional>
#include <sys/socket.h>
#include <string_view>

//! \brief 基类
// Socket里面的所有基本函数都是通过调用 FDWrapper 的系统函数实现的
// 通过自定义的 CheckSystemCall 中间函数
class Socket : public FileDescriptor {
   private:
    //! 获取套接字连接到的本地或对等地址
    Address get_address(
        const std::string& name_of_function,
        const std::function<int(int, sockaddr*, socklen_t*)>& function) const;

   protected:
    //! 默认系统构造
    Socket(int domain, int type, int protocol = 0);

    //! 从文件描述符构造
    Socket(FileDescriptor&& fd, int domain, int type, int protocol = 0);

    //! Wrapper around [getsockopt(2)](\ref man2::getsockopt)
    template <typename option_type>
    socklen_t getsockopt(int level,
                         int option,
                         option_type& option_value) const;

    //! Wrappers around [setsockopt(2)](\ref man2::setsockopt)
    template <typename option_type>
    void setsockopt(int level, int option, const option_type& option_value);

    void setsockopt(int level, int option, std::string_view option_val);

   public:
    //! Bind a socket to a specified address with [bind(2)](\ref man2::bind),
    //! usually for listen/accept
    void bind(const Address& address);

    //! Bind a socket to a specified device
    void bind_to_device(std::string_view device_name);

    //! Connect a socket to a specified peer address with [connect(2)](\ref
    //! man2::connect)
    void connect(const Address& address);

    //! Shut down a socket via [shutdown(2)](\ref man2::shutdown)
    void shutdown(int how);

    //! Get local address of socket with [getsockname(2)](\ref
    //! man2::getsockname)
    Address local_address() const;
    //! Get peer address of socket with [getpeername(2)](\ref man2::getpeername)
    Address peer_address() const;

    //! Allow local address to be reused sooner via [SO_REUSEADDR](\ref
    //! man7::socket)
    void set_reuseaddr();

    //! Check for errors (will be seen on non-blocking sockets)
    void throw_if_error() const;
};

class DatagramSocket : public Socket {
    using Socket::Socket;

   public:
    //! Receive a datagram and the Address of its sender
    void recv(Address& source_address, std::string& payload);

    //! Send a datagram to specified Address
    void sendto(const Address& destination, std::string_view payload);

    //! Send datagram to the socket's connected address (must call connect()
    //! first)
    void send(std::string_view payload);
};

//! A wrapper around [UDP sockets](\ref man7::udp)
class UDPSocket : public DatagramSocket {
    //! \param[in] fd is the FileDescriptor from which to construct
    explicit UDPSocket(FileDescriptor&& fd)
        : DatagramSocket(std::move(fd), AF_INET, SOCK_DGRAM) {}

   public:
    //! Default: construct an unbound, unconnected UDP socket
    UDPSocket() : DatagramSocket(AF_INET, SOCK_DGRAM) {}
};

//! A wrapper around [TCP sockets](\ref man7::tcp)
class TCPSocket : public Socket {
   private:
    //! \brief Construct from FileDescriptor (used by accept())
    //! \param[in] fd is the FileDescriptor from which to construct
    explicit TCPSocket(FileDescriptor&& fd)
        : Socket(std::move(fd), AF_INET, SOCK_STREAM, IPPROTO_TCP) {}

   public:
    //! Default: construct an unbound, unconnected TCP socket
    TCPSocket() : Socket(AF_INET, SOCK_STREAM) {}

    //! Mark a socket as listening for incoming connections
    void listen(int backlog = 16);

    //! Accept a new incoming connection
    TCPSocket accept();
};

//! A wrapper around [packet sockets](\ref man7:packet)
class PacketSocket : public DatagramSocket {
   public:
    PacketSocket(const int type, const int protocol)
        : DatagramSocket(AF_PACKET, type, protocol) {}

    void set_promiscuous();
};

//! A wrapper around [Unix-domain stream sockets]
/* Unix-domain stream sockets，简称UDS（UNIX Domain
 * Sockets），是一种在同一台机器上进行进程间通信（IPC: Inter-Process
 * Communication）的可靠的IPC机制。它不需要经过网络协议栈，
 * 不需要打包拆包、计算校验和、维护序号和应答等，
 * 只是将应用层数据从一个进程拷贝到另一个进程。
 */
class LocalStreamSocket : public Socket {
   public:
    //! 从文件描述符构造
    explicit LocalStreamSocket(FileDescriptor&& fd)
        : Socket(std::move(fd), AF_UNIX, SOCK_STREAM) {}
};
