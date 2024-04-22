#pragma once

#include "polling/eventloop.h"

/**
 * @brief 加载了 B_epoll 的 EventLoop，作为一个封装好的事件监听类，供 TCPSocket
 * 使用
 */
class EventEpoll : public EventLoop<B_epoll> {
 public:
  EventEpoll() : EventLoop<B_epoll>(B_epoll()){};
};