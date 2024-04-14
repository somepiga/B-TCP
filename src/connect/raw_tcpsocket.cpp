#include "connect/raw_tcpsocket.h"

//! \param[in] backlog 等待队列的连接数
void RawTCPSocket::listen(const int backlog) {
  CheckSystemCall("listen", ::listen(fd_num(), backlog));
}

//! \returns 一个连接到对等端的 RawTCPSocket
//! \note 运行时阻塞
RawTCPSocket RawTCPSocket::accept() {
  register_read();
  return RawTCPSocket(FileDescriptor(
      CheckSystemCall("accept", ::accept(fd_num(), nullptr, nullptr))));
}