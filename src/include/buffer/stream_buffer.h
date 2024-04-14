#pragma once

#include <queue>
#include <string>
#include <string_view>

class Reader;
class Writer;

/**
 * @brief 有大小限制的字节流，用于 TCP 通信时收发消息，详见 TCPEndpoint
 */
class StreamBuffer {
 protected:
  uint64_t capacity_;
  bool _error{};
  std::queue<std::string> _buffer;
  std::string_view _front_view;
  size_t _bytes_buffered{};

  size_t _bytes_popped{};
  size_t _bytes_pushed{};
  bool _closed{};

 public:
  explicit StreamBuffer(uint64_t capacity);
  Reader &reader();
  const Reader &reader() const;
  Writer &writer();
  const Writer &writer() const;
};

class Writer : public StreamBuffer {
 public:
  void push(std::string data);
  void close();
  void set_error();

  bool is_closed() const;
  uint64_t available_capacity() const;
  uint64_t bytes_pushed() const;
};

class Reader : public StreamBuffer {
 public:
  std::string_view peek() const;  //!< 查看 Buffer 中下一个字节
  void pop(uint64_t len);

  bool is_finished() const;  //!< Stream 是否结束 (closed and fully popped)?
  bool has_error() const;    //!< Stream 是否出错?

  uint64_t bytes_buffered() const;  //!< StreamBuffer 此时存放了多少字节
  uint64_t bytes_popped() const;    //!< 累计 pop 了多少字节
};

void read(Reader &reader, uint64_t len, std::string &out);
