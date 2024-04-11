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

// get the local or peer address the socket is connected to
//! \param[in] name_of_function is the function to call (string passed to
//! CheckSystemCall()) \param[in] function is a pointer to the function \returns
//! the requested Address
Address Socket::get_address(
    const string& name_of_function,
    const function<int(int, sockaddr*, socklen_t*)>& function) const {
  Address::Raw address;
  socklen_t size = sizeof(address);

  CheckSystemCall(name_of_function, function(fd_num(), address, &size));

  return Address{address, size};
}

//! \returns the local Address of the socket
Address Socket::local_address() const {
  return get_address("getsockname", getsockname);
}

//! \returns the socket's peer's Address
Address Socket::peer_address() const {
  return get_address("getpeername", getpeername);
}

// bind socket to a specified local address (usually to listen/accept)
//! \param[in] address is a local Address to bind
void Socket::bind(const Address& address) {
  CheckSystemCall("bind", ::bind(fd_num(), address, address.size()));
}

void Socket::bind_to_device(const string_view device_name) {
  setsockopt(SOL_SOCKET, SO_BINDTODEVICE, device_name);
}

// connect socket to a specified peer address
//! \param[in] address is the peer's Address
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

// setsockopt with size only known at runtime
void Socket::setsockopt(const int level, const int option,
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
    throw runtime_error("unexpected length from getsockopt: " + to_string(len));
  }

  if (socket_error) {
    throw unix_error("socket error", socket_error);
  }
}
