#pragma once

#include <map>
#include <string>

#include "buffer/stream_buffer.h"

class Reassembler {
 public:
  void insert(uint64_t first_index, std::string data, bool is_last_substring,
              Writer& output);

  // How many bytes are stored in the Reassembler itself?
  uint64_t bytes_pending() const;

 private:
  //* Reassembler throughput: 12.20 Gbit/s
  std::map<size_t, std::string> _unassembled_strings;
  size_t _last_string_end;
  size_t _last_popped_end;
  void _push_availables(Writer& output, size_t index);
};
