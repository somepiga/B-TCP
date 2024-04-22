#include "connect/base_socket.h"

#include <linux/if_packet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cstddef>
#include <stdexcept>

#include "utils/exception.h"

using namespace std;

Socket::Socket(const int domain, const int type, const int protocol)
    : FileDescriptor(
          ::CheckSystemCall("socket", socket(domain, type, protocol))) {}

Socket::Socket(FileDescriptor&& fd, int domain, int type,
               int protocol)  // NOLINT(*-swappable-parameters)
    : FileDescriptor(std::move(fd)) {
  int actual_value{};
  socklen_t len{};

  // 验证域名
  len = getsockopt(SOL_SOCKET, SO_DOMAIN, actual_value);
  if ((len != sizeof(actual_value)) or (actual_value != domain)) {
    throw runtime_error("socket domain mismatch");
  }

  // 验证 socket 类型
  len = getsockopt(SOL_SOCKET, SO_TYPE, actual_value);
  if ((len != sizeof(actual_value)) or (actual_value != type)) {
    throw runtime_error("socket type mismatch");
  }

  // 验证协议
  len = getsockopt(SOL_SOCKET, SO_PROTOCOL, actual_value);
  if ((len != sizeof(actual_value)) or (actual_value != protocol)) {
    throw runtime_error("socket protocol mismatch");
  }
}

// 获取 socket 连接到的本地或对等地址
//! \param[in] name_of_function 要调用的函数（传递给CheckSystemCall()的字符串）
//! \param[in] function 一个指向函数的指针
//! \returns 请求的地址
Address Socket::get_address(
    const string& name_of_function,
    const function<int(int, sockaddr*, socklen_t*)>& function) const {
  Address::Raw address;
  socklen_t size = sizeof(address);

  CheckSystemCall(name_of_function, function(fd_num(), address, &size));

  return Address{address, size};
}

//! \returns socket的本地地址
Address Socket::local_address() const {
  return get_address("getsockname", getsockname);
}

//! \returns socket对等端地址
Address Socket::peer_address() const {
  return get_address("getpeername", getpeername);
}

// 将套接字绑定到指定的本地地址（通常用于listen/accept）
//! \param[in] address 绑定的本地地址
void Socket::bind(const Address& address) {
  CheckSystemCall("bind", ::bind(fd_num(), address, address.size()));
}

void Socket::bind_to_device(const string_view device_name) {
  setsockopt(SOL_SOCKET, SO_BINDTODEVICE, device_name);
}

// 将socket连接到指定的对等地址
//! \param[in] address 对等端的地址
void Socket::connect(const Address& address) {
  CheckSystemCall("connect", ::connect(fd_num(), address, address.size()));
}

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

template <typename option_type>
socklen_t Socket::getsockopt(const int level, const int option,
                             option_type& option_value) const {
  socklen_t optlen = sizeof(option_value);
  CheckSystemCall("getsockopt", ::getsockopt(fd_num(), level, option,
                                             &option_value, &optlen));
  return optlen;
}

template <typename option_type>
void Socket::setsockopt(const int level, const int option,
                        const option_type& option_value) {
  CheckSystemCall("setsockopt",
                  ::setsockopt(fd_num(), level, option, &option_value,
                               sizeof(option_value)));
}

// setsockopt 的大小仅在运行时已知
void Socket::setsockopt(const int level, const int option,
                        const string_view option_val) {
  CheckSystemCall("setsockopt",
                  ::setsockopt(fd_num(), level, option, option_val.data(),
                               option_val.size()));
}

// 允许更快地重用本地地址，但会牺牲一些稳健性
//! \note 使用“SO_REUSEADDR”可能会降低应用程序的稳健性
void Socket::set_reuseaddr() {
  setsockopt(SOL_SOCKET, SO_REUSEADDR, int{true});
}

void Socket::throw_if_error() const {
  int socket_error = 0;
  const socklen_t len = getsockopt(SOL_SOCKET, SO_ERROR, socket_error);
  if (len != sizeof(socket_error)) {
    throw runtime_error("unexpected length from getsockopt: " + to_string(len));
  }

  if (socket_error) {
    throw unix_error("socket error", socket_error);
  }
}
