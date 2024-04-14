#pragma once

#include "connect/base_socket.h"

//! A wrapper around [TCP sockets](\ref man7::tcp)
class RawTCPSocket : public Socket {
 private:
  //! \brief Construct from FileDescriptor (used by accept())
  //! \param[in] fd is the FileDescriptor from which to construct
  explicit RawTCPSocket(FileDescriptor&& fd)
      : Socket(std::move(fd), AF_INET, SOCK_STREAM, IPPROTO_TCP) {}

 public:
  //! Default: construct an unbound, unconnected TCP socket
  RawTCPSocket() : Socket(AF_INET, SOCK_STREAM) {}

  //! Mark a socket as listening for incoming connections
  void listen(int backlog = 16);

  //! Accept a new incoming connection
  RawTCPSocket accept();
};