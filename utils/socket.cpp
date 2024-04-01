#include "socket.h"

#include "exception.h"

#include <cstddef>
#include <linux/if_packet.h>
#include <net/if.h>
#include <stdexcept>
#include <sys/ioctl.h>
#include <unistd.h>

using namespace std;

// 默认构造，调用sys/socket.h的库函数
Socket::Socket(const int domain, const int type, const int protocol)
    : FileDescriptor(
          ::CheckSystemCall("socket", socket(domain, type, protocol))) {}

// 从给定的文件描述符构造 socket，并验证其域、类型和协议是否匹配
Socket::Socket(FileDescriptor&& fd,
               int domain,
               int type,
               int protocol)  // NOLINT(*-swappable-parameters)
    : FileDescriptor(std::move(fd)) {
    int actual_value{};
    socklen_t len{};

    // verify domain
    len = getsockopt(SOL_SOCKET, SO_DOMAIN, actual_value);
    if ((len != sizeof(actual_value)) or (actual_value != domain)) {
        throw runtime_error("socket domain mismatch");
    }

    // verify type
    len = getsockopt(SOL_SOCKET, SO_TYPE, actual_value);
    if ((len != sizeof(actual_value)) or (actual_value != type)) {
        throw runtime_error("socket type mismatch");
    }

    // verify protocol
    len = getsockopt(SOL_SOCKET, SO_PROTOCOL, actual_value);
    if ((len != sizeof(actual_value)) or (actual_value != protocol)) {
        throw runtime_error("socket protocol mismatch");
    }
}

// 获取本地地址或对等端地址
// 具体用法看下面两个函数就知道了
Address Socket::get_address(
    const string& name_of_function,
    const function<int(int, sockaddr*, socklen_t*)>& function) const {
    Address::Raw address;
    socklen_t size = sizeof(address);

    CheckSystemCall(name_of_function, function(fd_num(), address, &size));

    return Address{address, size};
}

//! \returns 本地地址
Address Socket::local_address() const {
    return get_address("getsockname", getsockname);
}

//! \returns 对等端地址
Address Socket::peer_address() const {
    return get_address("getpeername", getpeername);
}

// 绑定到指定的本地地址(usually to listen/accept)
//! \param[in] address 本地地址
void Socket::bind(const Address& address) {
    CheckSystemCall("bind", ::bind(fd_num(), address, address.size()));
}

void Socket::bind_to_device(const string_view device_name) {
    setsockopt(SOL_SOCKET, SO_BINDTODEVICE, device_name);
}

// 连接到对等端
//! \param[in] address 对等端地址
void Socket::connect(const Address& address) {
    CheckSystemCall("connect", ::connect(fd_num(), address, address.size()));
}

// 以指定方式关闭套接字
//! \param[in] how can be `SHUT_RD`, `SHUT_WR`, or `SHUT_RDWR`
void Socket::shutdown(const int how) {
    CheckSystemCall("shutdown", ::shutdown(fd_num(), how));
    switch (how) {
        case SHUT_RD:
            register_read();
            break;
        case SHUT_WR:
            register_write();
            break;
        case SHUT_RDWR:
            register_read();
            register_write();
            break;
        default:
            throw runtime_error("Socket::shutdown() called with invalid `how`");
    }
}

//! \note If payload is too small to hold the received datagram, this method
//! throws a std::runtime_error
void DatagramSocket::recv(Address& source_address, string& payload) {
    // receive source address and payload
    Address::Raw datagram_source_address;
    socklen_t fromlen = sizeof(datagram_source_address);

    payload.clear();
    payload.resize(kReadBufferSize);

    const ssize_t recv_len = CheckSystemCall(
        "recvfrom", ::recvfrom(fd_num(), payload.data(), payload.size(),
                               MSG_TRUNC, datagram_source_address, &fromlen));

    if (recv_len > static_cast<ssize_t>(payload.size())) {
        throw runtime_error("recvfrom (oversized datagram)");
    }

    register_read();
    source_address = {datagram_source_address, fromlen};
    payload.resize(recv_len);
}

void DatagramSocket::sendto(const Address& destination,
                            const string_view payload) {
    CheckSystemCall("sendto",
                    ::sendto(fd_num(), payload.data(), payload.length(), 0,
                             destination, destination.size()));
    register_write();
}

void DatagramSocket::send(const string_view payload) {
    CheckSystemCall("send",
                    ::send(fd_num(), payload.data(), payload.length(), 0));
    register_write();
}

// 将套接字标记为侦听传入连接
//! \param[in] backlog is the number of waiting connections to queue
void TCPSocket::listen(const int backlog) {
    CheckSystemCall("listen", ::listen(fd_num(), backlog));
}

// 接受新的传入连接
//! \returns a new TCPSocket connected to the peer.
//! \note This function blocks until a new connection is available
TCPSocket TCPSocket::accept() {
    register_read();
    return TCPSocket(FileDescriptor(
        CheckSystemCall("accept", ::accept(fd_num(), nullptr, nullptr))));
}

// 获取套接字选项
template <typename option_type>
socklen_t Socket::getsockopt(const int level,
                             const int option,
                             option_type& option_value) const {
    socklen_t optlen = sizeof(option_value);
    CheckSystemCall("getsockopt", ::getsockopt(fd_num(), level, option,
                                               &option_value, &optlen));
    return optlen;
}

// 设置套接字选项
//! \param[in] level The protocol level at which the argument resides
//! \param[in] option A single option to set
//! \param[in] option_value The value to set
//! \details See [setsockopt(2)](\ref man2::setsockopt) for details.
template <typename option_type>
void Socket::setsockopt(const int level,
                        const int option,
                        const option_type& option_value) {
    CheckSystemCall("setsockopt",
                    ::setsockopt(fd_num(), level, option, &option_value,
                                 sizeof(option_value)));
}

// setsockopt with size only known at runtime
void Socket::setsockopt(const int level,
                        const int option,
                        const string_view option_val) {
    CheckSystemCall("setsockopt",
                    ::setsockopt(fd_num(), level, option, option_val.data(),
                                 option_val.size()));
}

// allow local address to be reused sooner, at the cost of some robustness
//! \note Using `SO_REUSEADDR` may reduce the robustness of your application
void Socket::set_reuseaddr() {
    setsockopt(SOL_SOCKET, SO_REUSEADDR, int{true});
}

void Socket::throw_if_error() const {
    int socket_error = 0;
    const socklen_t len = getsockopt(SOL_SOCKET, SO_ERROR, socket_error);
    if (len != sizeof(socket_error)) {
        throw runtime_error("unexpected length from getsockopt: " +
                            to_string(len));
    }

    if (socket_error) {
        throw unix_error("socket error", socket_error);
    }
}

void PacketSocket::set_promiscuous() {
    setsockopt(SOL_PACKET, PACKET_ADD_MEMBERSHIP,
               packet_mreq{local_address().as<sockaddr_ll>()->sll_ifindex,
                           PACKET_MR_PROMISC,
                           {},
                           {}});
}
