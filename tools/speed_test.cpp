#include <chrono>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <queue>
#include <random>

#include "buffer/stream_buffer.h"

using namespace std;
using namespace std::chrono;

void speed_test(
    const size_t input_len,    // NOLINT(bugprone-easily-swappable-parameters)
    const size_t capacity,     // NOLINT(bugprone-easily-swappable-parameters)
    const size_t random_seed,  // NOLINT(bugprone-easily-swappable-parameters)
    const size_t write_size,   // NOLINT(bugprone-easily-swappable-parameters)
    const size_t read_size)    // NOLINT(bugprone-easily-swappable-parameters)
{
  // 生成写入数据
  const string data = [&random_seed, &input_len] {
    default_random_engine rd{random_seed};
    uniform_int_distribution<char> ud;
    string ret;
    for (size_t i = 0; i < input_len; ++i) {
      ret += ud(rd);
    }
    return ret;
  }();

  // 把数据分段
  queue<string> split_data;
  for (size_t i = 0; i < data.size(); i += write_size) {
    split_data.emplace(data.substr(i, write_size));
  }

  StreamBuffer sb{capacity};
  string output_data;
  output_data.reserve(data.size());

  const auto start_time = steady_clock::now();
  while (!sb.reader().is_finished()) {
    if (split_data.empty()) {
      if (!sb.writer().is_closed()) {
        sb.writer().close();
      }
    } else {
      if (split_data.front().size() <= sb.writer().available_capacity()) {
        sb.writer().push(std::move(split_data.front()));
        split_data.pop();
      }
    }

    if (sb.reader().bytes_buffered()) {
      auto peeked = sb.reader().peek().substr(0, read_size);
      if (peeked.empty()) {
        throw runtime_error(
            "StreamBuffer::reader().peek() returned empty view");
      }
      output_data += peeked;
      sb.reader().pop(peeked.size());
    }
  }

  const auto stop_time = steady_clock::now();

  if (data != output_data) {
    throw runtime_error("Mismatch between data written and read");
  }

  auto test_duration = duration_cast<duration<double>>(stop_time - start_time);
  auto bytes_per_second =
      static_cast<double>(input_len) / test_duration.count();
  auto bits_per_second = 8 * bytes_per_second;
  auto gigabits_per_second = bits_per_second / 1e9;

  cout << "StreamBuffer with capacity=" << capacity
       << ", write_size=" << write_size << ", read_size=" << read_size
       << " reached " << fixed << setprecision(2) << gigabits_per_second
       << " Gbit/s.\n";
}

void program_body() { speed_test(1e7, 32768, 789, 1500, 128); }

int main() {
  try {
    program_body();
  } catch (const exception& e) {
    cerr << "Exception: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}