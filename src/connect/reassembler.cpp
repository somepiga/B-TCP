#include "connect/reassembler.h"

void Reassembler::insert(uint64_t first_index, std::string data,
                         bool is_last_substring, Writer& output) {
  if (output.is_closed()) {
    return;
  }

  if (data.empty()) {
    if (is_last_substring) {
      output.close();
    }
    return;
  }

  if (is_last_substring) {
    _last_string_end = first_index + data.size();
  }

  if (first_index + data.size() <= output.bytes_pushed()) {
    return;
  }

  if (first_index < output.bytes_pushed()) {
    output.push(data.substr(output.bytes_pushed() - first_index));
    _last_popped_end = output.bytes_pushed();
    if (is_last_substring) {
      if (first_index + data.size() == output.bytes_pushed()) {
        _unassembled_strings.clear();
        output.close();
      }
    } else {
      _push_availables(output, first_index + data.size());
    }
    return;
  }

  if (!output.available_capacity()) {
    return;
  }

  if (first_index == output.bytes_pushed()) {
    output.push(data);
    _last_popped_end = output.bytes_pushed();
    _push_availables(output, first_index + data.size());
    return;
  }

  if (first_index >= output.bytes_pushed() + output.available_capacity()) {
    return;
  }

  if (_unassembled_strings.contains(first_index) &&
      _unassembled_strings.at(first_index).size() >= data.size()) {
    return;
  }

  if (first_index + data.size() - output.bytes_pushed() <=
      output.available_capacity()) {
    _unassembled_strings[first_index] = std::move(data);
  } else {
    _unassembled_strings[first_index] = data.substr(
        0, output.available_capacity() + output.bytes_pushed() - first_index);
  }
}

uint64_t Reassembler::bytes_pending() const {
  size_t res{};
  size_t prev_end = _last_popped_end;
  for (const auto& it : _unassembled_strings) {  // reversed with break?
    if (it.first >= prev_end) {
      res += it.second.size();
      prev_end = it.first + it.second.size();
    } else {
      if (it.first + it.second.size() > prev_end) {
        res += it.first + it.second.size() - prev_end;
      }
      prev_end = std::max(prev_end, it.first + it.second.size());
    }
  }
  return res;
}

// Push all available substrings after the given index
void Reassembler::_push_availables(Writer& output, size_t index) {
  if (index > output.bytes_pushed()) {
    return;
  }
  size_t prev_end = _last_popped_end;
  for (const auto& it : _unassembled_strings) {
    if (it.first > output.bytes_pushed()) {
      break;
    }
    if (it.first >= prev_end) {
      prev_end += it.second.size();
      output.push(it.second);
    } else {
      if (it.first + it.second.size() > prev_end) {
        output.push(it.second.substr(prev_end - it.first));
      }
      prev_end = std::max(prev_end, it.first + it.second.size());
    }
  }
  _last_popped_end = output.bytes_pushed();
  if (output.bytes_pushed() == _last_string_end) {
    _unassembled_strings.clear();
    output.close();
  }
}
