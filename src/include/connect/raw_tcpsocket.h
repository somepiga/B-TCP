#pragma once

#include "connect/base_socket.h"

//! 对[TCP sockets]的封装
class RawTCPSocket : public Socket {
 private:
  //! \brief 从文件描述符构造 (used by accept())
  explicit RawTCPSocket(FileDescriptor&& fd)
      : Socket(std::move(fd), AF_INET, SOCK_STREAM, IPPROTO_TCP) {}

 public:
  //! 构造一个未绑定、未连接的 TCP 套接字
  RawTCPSocket() : Socket(AF_INET, SOCK_STREAM) {}

  void listen(int backlog = 16);

  RawTCPSocket accept();
};