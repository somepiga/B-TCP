#include "datagram/address.h"

#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <netdb.h>

#include <array>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <system_error>

#include "utils/exception.h"

using namespace std;

//! 将 Raw 转换为 `sockaddr *`
Address::Raw::operator sockaddr*() {
  return reinterpret_cast<sockaddr*>(&storage);  // NOLINT(*-reinterpret-cast)
}

//! 将 Raw 转换为 `const sockaddr *`.
Address::Raw::operator const sockaddr*() const {
  return reinterpret_cast<const sockaddr*>(
      &storage);  // NOLINT(*-reinterpret-cast)
}

//! \param[in] addr 指向原始socket的指针
//! \param[in] size `addr`的长度
Address::Address(const sockaddr* addr, const size_t size) : _size(size) {
  // make sure proposed sockaddr can fit
  if (size > sizeof(_address.storage)) {
    throw runtime_error("invalid sockaddr size");
  }

  memcpy(&_address.storage, addr, size);
}

//! \brief getaddrinfo 和 getnameinfo 的错误类别
class gai_error_category : public error_category {
 public:
  //! error名称
  const char* name() const noexcept override { return "gai_error_category"; }
  //! \brief 错误信息
  //! \param[in] return_value [getaddrinfo(3)]或[getnameinfo(3)]返回的错误值
  string message(const int return_value) const noexcept override {
    return gai_strerror(return_value);
  }
};

//! \param[in] node 主机名或点分四地址
//! \param[in] service 服务名称或数字字符串
//! \param[in] hints 解析提供的名称的标准
Address::Address(const string& node, const string& service,
                 const addrinfo& hints)
    : _size() {
  addrinfo* resolved_address = nullptr;

  const int gai_ret =
      getaddrinfo(node.c_str(), service.c_str(), &hints, &resolved_address);
  if (gai_ret != 0) {
    throw tagged_error(gai_error_category(),
                       "getaddrinfo(" + node + ", " + service + ")", gai_ret);
  }

  // 如果成功，应该至少有一个条目
  if (resolved_address == nullptr) {
    throw runtime_error(
        "getaddrinfo returned successfully but with no results");
  }

  // 封装 resolved_address，在 throw exception 时释放
  auto addrinfo_deleter = [](addrinfo* const x) { freeaddrinfo(x); };
  unique_ptr<addrinfo, decltype(addrinfo_deleter)> wrapped_address(
      resolved_address, std::move(addrinfo_deleter));

  *this = Address(wrapped_address->ai_addr, wrapped_address->ai_addrlen);
}

//! \brief 构建一个包含 [getaddrinfo(3)] 提示的 `struct addrinfo`
//! \param[in] ai_flags [struct addrinfo] 中 `ai_flags` 字段的值
//! \param[in] ai_family [struct addrinfo] 中 `ai_family` 字段的值
static inline addrinfo make_hints(
    int ai_flags, int ai_family)  // NOLINT(*-swappable-parameters)
{
  addrinfo hints{};  // 初始化为全零
  hints.ai_flags = ai_flags;
  hints.ai_family = ai_family;
  return hints;
}

//! \param[in] hostname to resolve
//! \param[in] service  服务方式 (from `/etc/services`, 如 "http" is port 80)
Address::Address(const string& hostname, const string& service)
    : Address(hostname, service, make_hints(AI_ALL, AF_INET)) {}

//! \param[in] ip IP地址，形如(1.1.1.1)
//! \param[in] port 端口
Address::Address(const string& ip, const uint16_t port)
    : Address(ip, ::to_string(port),
              make_hints(AI_NUMERICHOST | AI_NUMERICSERV, AF_INET)) {}

pair<string, uint16_t> Address::ip_port() const {
  array<char, NI_MAXHOST> ip{};
  array<char, NI_MAXSERV> port{};

  const int gni_ret = getnameinfo(static_cast<const sockaddr*>(_address), _size,
                                  ip.data(), ip.size(), port.data(),
                                  port.size(), NI_NUMERICHOST | NI_NUMERICSERV);
  if (gni_ret != 0) {
    throw tagged_error(gai_error_category(), "getnameinfo", gni_ret);
  }

  return {ip.data(), stoi(port.data())};
}

string Address::to_string() const {
  const auto ip_and_port = ip_port();
  return ip_and_port.first + ":" + ::to_string(ip_and_port.second);
}

uint32_t Address::ipv4_numeric() const {
  if (_address.storage.ss_family != AF_INET or _size != sizeof(sockaddr_in)) {
    throw runtime_error("ipv4_numeric called on non-IPV4 address");
  }

  sockaddr_in ipv4_addr{};
  memcpy(&ipv4_addr, &_address.storage, _size);

  return be32toh(ipv4_addr.sin_addr.s_addr);
}

Address Address::from_ipv4_numeric(const uint32_t ip_address) {
  sockaddr_in ipv4_addr{};
  ipv4_addr.sin_family = AF_INET;
  ipv4_addr.sin_addr.s_addr = htobe32(ip_address);

  return {reinterpret_cast<sockaddr*>(&ipv4_addr),
          sizeof(ipv4_addr)};  // NOLINT(*-reinterpret-cast)
}

bool Address::operator==(const Address& other) const {
  if (_size != other._size) {
    return false;
  }

  return 0 == memcmp(&_address, &other._address, _size);
}

// 与每个 sockaddr 类型对应的地址族
template <typename sockaddr_type>
constexpr int sockaddr_family = -1;

template <>
constexpr int sockaddr_family<sockaddr_in> = AF_INET;

template <>
constexpr int sockaddr_family<sockaddr_in6> = AF_INET6;

template <>
constexpr int sockaddr_family<sockaddr_ll> = AF_PACKET;

// 安全地将地址转换为其基础 sockaddr 类型
template <typename sockaddr_type>
const sockaddr_type* Address::as() const {
  const sockaddr* raw{_address};
  if (sizeof(sockaddr_type) < size() or
      raw->sa_family != sockaddr_family<sockaddr_type>) {
    throw std::runtime_error("Address::as() conversion failure");
  }

  return reinterpret_cast<const sockaddr_type*>(
      raw);  // NOLINT(*-reinterpret-cast)
}

template const sockaddr_in* Address::as<sockaddr_in>() const;
template const sockaddr_in6* Address::as<sockaddr_in6>() const;
template const sockaddr_ll* Address::as<sockaddr_ll>() const;
