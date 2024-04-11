#include "buffer/stream_buffer.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

using namespace std;

StreamBuffer::StreamBuffer(uint64_t capacity) : capacity_(capacity) {}

void Writer::push(string data) {
  //* vector<string> impl: StreamBuffer throughput: 30.00 Gbit/s
  //* https://zhuanlan.zhihu.com/p/630739394
  const auto write_num = min(available_capacity(), data.size());
  if (!write_num) {
    return;
  }

  if (data.size() > write_num) {
    data = data.substr(0, write_num);
  }
  _buffer.push(std::move(data));  // ~3 Gbit/s faster than copy
  if (_buffer.size() == 1) {
    _front_view = _buffer.front();
  }
  _bytes_pushed += write_num;
  _bytes_buffered += write_num;
}

void Writer::close() { _closed = true; }

void Writer::set_error() { _error = true; }

bool Writer::is_closed() const { return _closed; }

uint64_t Writer::available_capacity() const {
  return capacity_ - _bytes_buffered;
}

uint64_t Writer::bytes_pushed() const { return _bytes_pushed; }

string_view Reader::peek() const { return _front_view; }

bool Reader::is_finished() const {
  return writer().is_closed() && !bytes_buffered();
}

bool Reader::has_error() const { return _error; }

void Reader::pop(uint64_t len) {
  //* for queue<string>
  auto pop_num = min(len, _bytes_buffered);

  //* queue<string> with front_view
  while (len && !_buffer.empty()) {
    const size_t firstsize = _front_view.size();
    if (len >= firstsize) {
      // queue.pop() ~2 Gbit/s faster than vector.erase(vector.begin())
      _buffer.pop();
      if (!_buffer.empty()) {
        _front_view = _buffer.front();
      }
      len -= firstsize;
      continue;
    }
    _front_view.remove_prefix(pop_num);
    len = 0;
  }
  _bytes_popped += pop_num;
  _bytes_buffered -= pop_num;
}

uint64_t Reader::bytes_buffered() const {
  //* for vector<string>
  return _bytes_buffered;
}

uint64_t Reader::bytes_popped() const { return _bytes_popped; }

void read(Reader& reader, uint64_t len, std::string& out) {
  out.clear();

  while (reader.bytes_buffered() and out.size() < len) {
    auto view = reader.peek();

    if (view.empty()) {
      throw std::runtime_error("Reader::peek() returned empty string_view");
    }

    view = view.substr(
        0, len - out.size());  // Don't return more bytes than desired.
    out += view;
    reader.pop(view.size());
  }
}

Reader& StreamBuffer::reader() {
  static_assert(sizeof(Reader) == sizeof(StreamBuffer),
                "Please add member variables to the StreamBuffer base, not the "
                "StreamBuffer Reader.");

  return static_cast<Reader&>(*this);  // NOLINT(*-downcast)
}

const Reader& StreamBuffer::reader() const {
  static_assert(sizeof(Reader) == sizeof(StreamBuffer),
                "Please add member variables to the StreamBuffer base, not the "
                "StreamBuffer Reader.");

  return static_cast<const Reader&>(*this);  // NOLINT(*-downcast)
}

Writer& StreamBuffer::writer() {
  static_assert(sizeof(Writer) == sizeof(StreamBuffer),
                "Please add member variables to the StreamBuffer base, not the "
                "StreamBuffer Writer.");

  return static_cast<Writer&>(*this);  // NOLINT(*-downcast)
}

const Writer& StreamBuffer::writer() const {
  static_assert(sizeof(Writer) == sizeof(StreamBuffer),
                "Please add member variables to the StreamBuffer base, not the "
                "StreamBuffer Writer.");

  return static_cast<const Writer&>(*this);  // NOLINT(*-downcast)
}
