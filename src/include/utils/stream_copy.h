#pragma once

#include "connect/base_socket.h"

/**
 * @brief 用于交换 stdin/stdout 和 Socket input/output 的数据
 */
void bidirectional_stream_copy(Socket& socket);
