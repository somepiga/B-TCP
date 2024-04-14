#pragma once

#include "polling/eventloop.h"

class EventEpoll : public EventLoop<B_epoll> {
 public:
  EventEpoll() : EventLoop<B_epoll>(B_epoll()){};
};