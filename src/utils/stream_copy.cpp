#include "utils/stream_copy.h"

#include <unistd.h>

#include <algorithm>
#include <iostream>

#include "buffer/stream_buffer.h"
#include "polling/eventpolling.h"
#include "polling/rule.h"

void bidirectional_stream_copy(Socket& socket) {
  constexpr size_t buffer_size = 1048576;

  EventEpoll _eventloop{};
  FileDescriptor _input{STDIN_FILENO};
  FileDescriptor _output{STDOUT_FILENO};
  StreamBuffer _outbound{buffer_size};
  StreamBuffer _inbound{buffer_size};
  bool _outbound_shutdown{false};
  bool _inbound_shutdown{false};

  socket.set_blocking(false);
  _input.set_blocking(false);
  _output.set_blocking(false);

  // rule 1: read from stdin into outbound byte stream
  _eventloop.add_rule(
      "read from stdin into outbound byte stream", _input, Direction::In,
      [&] {
        std::string data;
        data.resize(_outbound.writer().available_capacity());
        _input.read(data);
        _outbound.writer().push(std::move(data));
        if (_input.eof()) {
          _outbound.writer().close();
        }
      },
      [&] {
        return (not _outbound.reader().has_error()) and
               (_outbound.writer().available_capacity() > 0) and
               (not _inbound.reader().has_error());
      },
      [&] { _outbound.writer().close(); });

  // rule 2: read from outbound byte stream into socket
  _eventloop.add_rule(
      "read from outbound byte stream into socket", socket, Direction::Out,
      [&] {
        if (_outbound.reader().bytes_buffered()) {
          _outbound.reader().pop(socket.write(_outbound.reader().peek()));
        }
        if (_outbound.reader().is_finished()) {
          socket.shutdown(SHUT_WR);
          _outbound_shutdown = true;
        }
      },
      [&] {
        return _outbound.reader().bytes_buffered() or
               (_outbound.reader().is_finished() and not _outbound_shutdown);
      },
      [&] { _outbound.writer().close(); });

  // rule 3: read from socket into inbound byte stream
  _eventloop.add_rule(
      "read from socket into inbound byte stream", socket, Direction::In,
      [&] {
        std::string data;
        data.resize(_inbound.writer().available_capacity());
        socket.read(data);
        _inbound.writer().push(std::move(data));
        if (socket.eof()) {
          _inbound.writer().close();
        }
      },
      [&] {
        return (not _inbound.reader().has_error()) and
               (_inbound.writer().available_capacity() > 0) and
               (not _outbound.reader().has_error());
      },
      [&] { _inbound.writer().close(); });

  // rule 4: read from inbound byte stream into stdout
  _eventloop.add_rule(
      "read from inbound byte stream into stdout", _output, Direction::Out,
      [&] {
        if (_inbound.reader().bytes_buffered()) {
          _inbound.reader().pop(_output.write(_inbound.reader().peek()));
        }
        if (_inbound.reader().is_finished()) {
          _output.close();
          _inbound_shutdown = true;
        }
      },
      [&] {
        return _inbound.reader().bytes_buffered() or
               (_inbound.reader().is_finished() and not _inbound_shutdown);
      },
      [&] { _inbound.writer().close(); });

  // loop until completion
  while (true) {
    if (Result::Exit == _eventloop.wait_next_event(-1)) {
      return;
    }
  }
}
