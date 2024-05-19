#pragma once

#include <concepts>
#include <optional>
#include <utility>

#include "buffer/stream_buffer.h"
#include "common.h"

static_assert(sizeof(Reader) == sizeof(StreamBuffer),
              "Please add member variables to the StreamBuffer base, not the "
              "StreamBuffer Reader.");

static_assert(sizeof(Writer) == sizeof(StreamBuffer),
              "Please add member variables to the StreamBuffer base, not the "
              "StreamBuffer Writer.");

class ByteStreamTestHarness : public TestHarness<StreamBuffer> {
 public:
  ByteStreamTestHarness(std::string test_name, uint64_t capacity)
      : TestHarness(std::move(test_name),
                    "capacity=" + std::to_string(capacity),
                    StreamBuffer{capacity}) {}

  size_t peek_size() { return object().reader().peek().size(); }
};

/* actions */

struct Push : public Action<StreamBuffer> {
  std::string data_;

  explicit Push(std::string data) : data_(std::move(data)) {}
  std::string description() const override {
    return "push \"" + Printer::prettify(data_) + "\" to the stream";
  }
  void execute(StreamBuffer& bs) const override { bs.writer().push(data_); }
};

struct Close : public Action<StreamBuffer> {
  std::string description() const override { return "close"; }
  void execute(StreamBuffer& bs) const override { bs.writer().close(); }
};

struct SetError : public Action<StreamBuffer> {
  std::string description() const override { return "set_error"; }
  void execute(StreamBuffer& bs) const override { bs.writer().set_error(); }
};

struct Pop : public Action<StreamBuffer> {
  size_t len_;

  explicit Pop(size_t len) : len_(len) {}
  std::string description() const override {
    return "pop( " + std::to_string(len_) + " )";
  }
  void execute(StreamBuffer& bs) const override { bs.reader().pop(len_); }
};

/* expectations */

struct Peek : public Expectation<StreamBuffer> {
  std::string output_;

  explicit Peek(std::string output) : output_(std::move(output)) {}

  std::string description() const override {
    return "peeking produces \"" + Printer::prettify(output_) + "\"";
  }

  void execute(StreamBuffer& bs) const override {
    const StreamBuffer orig = bs;
    std::string got;

    while (bs.reader().bytes_buffered()) {
      auto peeked = bs.reader().peek();
      if (peeked.empty()) {
        throw ExpectationViolation{"Reader::peek() returned empty string_view"};
      }
      got += peeked;
      bs.reader().pop(peeked.size());
    }

    if (got != output_) {
      throw ExpectationViolation{"Expected \"" + Printer::prettify(output_) +
                                 "\" in buffer, " + " but found \"" +
                                 Printer::prettify(got) + "\""};
    }

    bs = orig;
  }
};

struct PeekOnce : public Peek {
  using Peek::Peek;

  std::string description() const override {
    return "peek() gives exactly \"" + Printer::prettify(output_) + "\"";
  }

  void execute(StreamBuffer& bs) const override {
    auto peeked = bs.reader().peek();
    if (peeked != output_) {
      throw ExpectationViolation{"Expected exactly \"" +
                                 Printer::prettify(output_) +
                                 "\" at front of stream, " + "but found \"" +
                                 Printer::prettify(peeked) + "\""};
    }
  }
};

struct IsClosed : public ExpectBool<StreamBuffer> {
  using ExpectBool::ExpectBool;
  std::string name() const override { return "is_closed"; }
  bool value(StreamBuffer& bs) const override {
    return bs.writer().is_closed();
  }
};

struct IsFinished : public ExpectBool<StreamBuffer> {
  using ExpectBool::ExpectBool;
  std::string name() const override { return "is_finished"; }
  bool value(StreamBuffer& bs) const override {
    return bs.reader().is_finished();
  }
};

struct HasError : public ExpectBool<StreamBuffer> {
  using ExpectBool::ExpectBool;
  std::string name() const override { return "has_error"; }
  bool value(StreamBuffer& bs) const override {
    return bs.reader().has_error();
  }
};

struct BytesBuffered : public ExpectNumber<StreamBuffer, uint64_t> {
  using ExpectNumber::ExpectNumber;
  std::string name() const override { return "bytes_buffered"; }
  size_t value(StreamBuffer& bs) const override {
    return bs.reader().bytes_buffered();
  }
};

struct BufferEmpty : public ExpectBool<StreamBuffer> {
  using ExpectBool::ExpectBool;
  std::string name() const override { return "[buffer is empty]"; }
  bool value(StreamBuffer& bs) const override {
    return bs.reader().bytes_buffered() == 0;
  }
};

struct AvailableCapacity : public ExpectNumber<StreamBuffer, uint64_t> {
  using ExpectNumber::ExpectNumber;
  std::string name() const override { return "available_capacity"; }
  size_t value(StreamBuffer& bs) const override {
    return bs.writer().available_capacity();
  }
};

struct BytesPushed : public ExpectNumber<StreamBuffer, uint64_t> {
  using ExpectNumber::ExpectNumber;
  std::string name() const override { return "bytes_pushed"; }
  size_t value(StreamBuffer& bs) const override {
    return bs.writer().bytes_pushed();
  }
};

struct BytesPopped : public ExpectNumber<StreamBuffer, uint64_t> {
  using ExpectNumber::ExpectNumber;
  std::string name() const override { return "bytes_popped"; }
  size_t value(StreamBuffer& bs) const override {
    return bs.reader().bytes_popped();
  }
};

struct ReadAll : public Expectation<StreamBuffer> {
  std::string output_;
  BufferEmpty empty_{true};

  explicit ReadAll(std::string output) : output_(std::move(output)) {}

  std::string description() const override {
    if (output_.empty()) {
      return empty_.description();
    }
    return "reading \"" + Printer::prettify(output_) + "\" leaves buffer empty";
  }

  void execute(StreamBuffer& bs) const override {
    std::string got;
    read(bs.reader(), output_.size(), got);
    if (got != output_) {
      throw ExpectationViolation{
          "Expected to read \"" + Printer::prettify(output_) +
          "\", but found \"" + Printer::prettify(got) + "\""};
    }
    empty_.execute(bs);
  }
};
