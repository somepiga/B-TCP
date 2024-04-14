#pragma once
#include <sys/epoll.h>
#include <unistd.h>

#include <unordered_map>

#include "polling/rule.h"

class B_epoll {
  int epoll_fd;

  /**
   * 由于一个文件描述符可能有多个事件需要监听
   * 这里整合起来
   * int: fd_num
   * int16_t: Direction
   */
  std::unordered_map<int, int16_t> added_fds;
  int num_events;
  epoll_event* events;

 public:
  B_epoll();
  void insert_rule(std::shared_ptr<FDRule> rule);

  void insert_to_epoll();

  int b_epoll_wait(const int timeout_ms);

  void call(std::list<std::shared_ptr<FDRule>>& _fd_rules);

  void clear();

  void close() {
    if (events != nullptr) {
      free(events);
    }
    ::close(epoll_fd);
  }
};
