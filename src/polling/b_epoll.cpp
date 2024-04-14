#include "polling/b_epoll.h"

#include <unistd.h>

#include <cstring>
#include <iostream>
#include <stdexcept>

#include "polling/rule.h"
#include "utils/exception.h"

B_epoll::B_epoll() : added_fds{} {
  epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    throw std::runtime_error("Failed to create epoll instance");
  }
  num_events = 0;
  events = nullptr;
}

void B_epoll::insert_rule(std::shared_ptr<FDRule> rule_addr) {
  auto& rule = *rule_addr;
  auto it = added_fds.find(rule.fd.fd_num());
  if (it != added_fds.end()) {
    // 找到了，修改之
    added_fds[rule.fd.fd_num()] |= static_cast<int16_t>(rule.direction);
  } else {
    // 没找到，加入
    added_fds.insert({rule.fd.fd_num(), static_cast<int16_t>(rule.direction)});
  }
}

void B_epoll::insert_to_epoll() {
  for (auto& it : added_fds) {
    epoll_event ev;
    ev.data.fd = it.first;
    ev.events = it.second;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, it.first, &ev) == -1) {
      int error_code = errno;
      std::cerr << "errno: " << error_code
                << ", 错误信息: " << strerror(error_code) << std::endl;
      throw std::runtime_error("添加失败");
    }
  }
}

int B_epoll::b_epoll_wait(const int timeout_ms) {
  events = (epoll_event*)malloc(sizeof(epoll_event) * added_fds.size());
  if (0 == (num_events = CheckSystemCall(
                "epoll_wait", ::epoll_wait(epoll_fd, events, added_fds.size(),
                                           timeout_ms)))) {
    return -1;
  }
  return 1;
}

void B_epoll::call(std::list<std::shared_ptr<FDRule>>& _fd_rules) {
  for (int i = 0; i < num_events; ++i) {
    auto fd_num = events[i].data.fd;

    // 优先写事件
    if (events[i].events & EPOLLOUT) {
      for (auto it = _fd_rules.begin(); it != _fd_rules.end();) {
        auto& this_rule = **it;
        if (this_rule.fd.fd_num() == fd_num &&
            this_rule.direction == Direction::Out) {
          this_rule.callback();
          break;
        }
        ++it;
      }
    }

    // 再读事件
    if (events[i].events & EPOLLIN) {
      for (auto it = _fd_rules.begin(); it != _fd_rules.end();) {
        auto& this_rule = **it;
        if (this_rule.fd.fd_num() == fd_num &&
            this_rule.direction == Direction::In) {
          this_rule.callback();
          break;
        }
        ++it;
      }
    }
  }
}

void B_epoll::clear() {
  for (auto& it : added_fds) {
    epoll_event ev;
    ev.data.fd = it.first;
    ev.events = it.second;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, it.first, &ev) == -1) {
      int error_code = errno;
      std::cerr << "errno: " << error_code
                << ", 错误信息: " << strerror(error_code) << std::endl;
      throw std::runtime_error("删除失败");
    }
  }
  added_fds.clear();
  num_events = 0;
  free(events);
  events = nullptr;
}