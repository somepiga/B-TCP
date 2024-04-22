#pragma once

#include <map>
#include <string>

#include "buffer/stream_buffer.h"

/**
 * @brief 流重组器，将乱序收到的包重组后向上层提交
 */
class Reassembler {
 public:
  void insert(uint64_t first_index, std::string data, bool is_last_substring,
              Writer& output);

  uint64_t bytes_pending() const;  //!< Reassembler 内已经存放多少数据

 private:
  std::map<size_t, std::string> _unassembled_strings;
  size_t _last_string_end;
  size_t _last_popped_end;
  void _push_availables(Writer& output, size_t index);
};
