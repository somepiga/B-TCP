#pragma once

#include <queue>
#include <string>
#include <string_view>

class Reader;
class Writer;

/* 照搬过来的，可操作性不大了 */
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
  std::string_view peek() const;  // Peek at the next bytes in the buffer
  void pop(uint64_t len);         // Remove `len` bytes from the buffer

  bool is_finished()
      const;               // Is the stream finished (closed and fully popped)?
  bool has_error() const;  // Has the stream had an error?

  uint64_t bytes_buffered()
      const;  // Number of bytes currently buffered (pushed and not popped)
  uint64_t bytes_popped()
      const;  // Total number of bytes cumulatively popped from stream
};

/*
 * read: A (provided) helper function thats peeks and pops up to `len` bytes
 * from a StreamBuffer Reader into a string;
 */
void read(Reader &reader, uint64_t len, std::string &out);
