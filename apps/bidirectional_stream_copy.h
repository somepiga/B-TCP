#pragma once

#include "socket.h"

// ！双向流复制功能，能够在非阻塞模式下处理输入流和输出流之间的数据传输
//! 将socket 输入/输出复制到 stdin/stdout
void bidirectional_stream_copy(Socket& socket);
