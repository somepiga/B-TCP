#pragma once

#include <sys/socket.h>

#include <cstdint>
#include <functional>

#include "datagram/address.h"
#include "datagram/file_descriptor.h"

/**
 * @brief 各种 Socket 的基类，具体的用法见各派生类
 */
class Socket : public FileDescriptor {
 private:
  //! 获取socket连接到的本地或对等地址
  Address get_address(
      const std::string& name_of_function,
      const std::function<int(int, sockaddr*, socklen_t*)>& function) const;

 protected:
  Socket(int domain, int type, int protocol = 0);

  template <typename option_type>
  socklen_t getsockopt(int level, int option, option_type& option_value) const;

  template <typename option_type>
  void setsockopt(int level, int option, const option_type& option_value);

  void setsockopt(int level, int option, std::string_view option_val);

 public:
  Socket(FileDescriptor&& fd, int domain, int type,
         int protocol = 0);  //!< 从文件描述符构造

  void bind(const Address& address);  //!< 将 Socket 和一个 Address 绑定

  void bind_to_device(
      std::string_view device_name);  //!< 将 Socket 和一个设备绑定

  void connect(const Address& address);  //!< 连接到 Address

  void shutdown(int how);  //!< 关闭 Socket

  Address local_address() const;  //!< 获得我端 Address
  Address peer_address() const;   //!< 获得他端 Address

  //! 通过 [SO_REUSEADDR] 允许更快地重用本地地址
  void set_reuseaddr();

  void throw_if_error() const;  //!< 检测故障
};
