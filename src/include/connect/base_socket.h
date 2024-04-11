#pragma once

#include <sys/socket.h>

#include <cstdint>
#include <functional>

#include "datagram/address.h"
#include "datagram/file_descriptor.h"

//! \brief Base class for network sockets (TCP, UDP, etc.)
//! \details Socket is generally used via a subclass. See TCPSocket and
//! UDPSocket for usage examples.
class Socket : public FileDescriptor {
 private:
  //! Get the local or peer address the socket is connected to
  Address get_address(
      const std::string& name_of_function,
      const std::function<int(int, sockaddr*, socklen_t*)>& function) const;

 protected:
  //! Construct via [socket(2)](\ref man2::socket)
  Socket(int domain, int type, int protocol = 0);

  //! Wrapper around [getsockopt(2)](\ref man2::getsockopt)
  template <typename option_type>
  socklen_t getsockopt(int level, int option, option_type& option_value) const;

  //! Wrappers around [setsockopt(2)](\ref man2::setsockopt)
  template <typename option_type>
  void setsockopt(int level, int option, const option_type& option_value);

  void setsockopt(int level, int option, std::string_view option_val);

 public:
  //! Construct from a file descriptor.
  Socket(FileDescriptor&& fd, int domain, int type, int protocol = 0);

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

  //! Get local address of socket with [getsockname(2)](\ref man2::getsockname)
  Address local_address() const;
  //! Get peer address of socket with [getpeername(2)](\ref man2::getpeername)
  Address peer_address() const;

  //! Allow local address to be reused sooner via [SO_REUSEADDR](\ref
  //! man7::socket)
  void set_reuseaddr();

  //! Check for errors (will be seen on non-blocking sockets)
  void throw_if_error() const;
};
