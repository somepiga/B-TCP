#include <stdexcept>

#include "byte_stream.h"

using namespace std;

ByteStream::ByteStream(uint64_t capacity) : capacity_(capacity) {}

void Writer::push(string data) {
    uint64_t const push_size = (available_capacity() < data.size())
                                   ? available_capacity()
                                   : data.size();
    for (uint64_t i = 0; i < push_size; ++i) {
        buffer.push_back(data.at(i));
    }
    bytes_pushed_ += push_size;
}

void Writer::close() {
    is_closed_ = true;
}

void Writer::set_error() {
    has_error_ = true;
}

bool Writer::is_closed() const {
    return is_closed_;
}

uint64_t Writer::available_capacity() const {
    return capacity_ - buffer.size();
}

uint64_t Writer::bytes_pushed() const {
    return bytes_pushed_;
}

string_view Reader::peek() const {
    return {string_view(&buffer.front(), 1)};
}

bool Reader::is_finished() const {
    return is_closed_ && buffer.empty();
}

bool Reader::is_closed() const {
    return is_closed_;
}

bool Reader::has_error() const {
    return has_error_;
}

void Reader::pop(uint64_t len) {
    uint64_t const pop_size = (len > buffer.size()) ? buffer.size() : len;
    for (uint64_t i = 0; i < pop_size; ++i) {
        buffer.pop_front();
    }
}

uint64_t Reader::bytes_buffered() const {
    return buffer.size();
}

uint64_t Reader::bytes_popped() const {
    return bytes_pushed_ - buffer.size();
}
