#pragma once

#include <functional>
#include <list>
#include <memory>
#include <ostream>
#include <string_view>

#include "datagram/file_descriptor.h"
#include "polling/b_epoll.h"
#include "polling/b_poll.h"
#include "polling/rule.h"

/**
 * @brief 轮询事件类
 */
template <typename PollingType>
class EventLoop {
 protected:
  PollingType _polling;

 public:
  std::vector<RuleCategory> _rule_categories{};
  std::list<std::shared_ptr<FDRule>> _fd_rules{};
  std::list<std::shared_ptr<BasicRule>> _non_fd_rules{};

 public:
  explicit EventLoop(PollingType&& polling);

  size_t add_category(const std::string& name);

  RuleHandle add_rule(
      size_t category_id, FileDescriptor& fd, Direction direction,
      const CallbackT& callback,
      const InterestT& interest = [] { return true; },
      const CallbackT& cancel = [] {},
      const InterestT& recover = [] { return false; });

  RuleHandle add_rule(
      size_t category_id, const CallbackT& callback,
      const InterestT& interest = [] { return true; });

  Result wait_next_event(int timeout_ms);

  //!< 帮助函数：可同时添加类别和规则
  template <typename... Targs>
  auto add_rule(const std::string& name, Targs&&... Fargs) {
    return add_rule(add_category(name), std::forward<Targs>(Fargs)...);
  }
};

//**************************************************************************
//******          以下是方法实现                           *******************
//**************************************************************************

template <typename PollingType>
EventLoop<PollingType>::EventLoop(PollingType&& polling)
    : _polling(std::move(polling)) {
  _rule_categories.reserve(64);
}

template <typename PollingType>
size_t EventLoop<PollingType>::add_category(const std::string& name) {
  if (_rule_categories.size() >= _rule_categories.capacity()) {
    throw std::runtime_error("maximum categories reached");
  }

  _rule_categories.push_back({name});
  return _rule_categories.size() - 1;
}

template <typename PollingType>
RuleHandle EventLoop<PollingType>::add_rule(
    size_t category_id, FileDescriptor& fd, Direction direction,
    const CallbackT& callback, const InterestT& interest,
    const CallbackT& cancel, const InterestT& recover) {
  if (category_id >= _rule_categories.size()) {
    throw std::out_of_range("bad category_id");
  }

  _fd_rules.emplace_back(
      std::make_shared<FDRule>(BasicRule{category_id, interest, callback},
                               fd.duplicate(), direction, cancel, recover));

  return RuleHandle{_fd_rules.back()};
}

template <typename PollingType>
RuleHandle EventLoop<PollingType>::add_rule(const size_t category_id,
                                            const CallbackT& callback,
                                            const InterestT& interest) {
  if (category_id >= _rule_categories.size()) {
    throw std::out_of_range("bad category_id");
  }

  _non_fd_rules.emplace_back(
      std::make_shared<BasicRule>(category_id, interest, callback));

  return RuleHandle{_non_fd_rules.back()};
}

// NOLINTBEGIN(*-cognitive-complexity)
// NOLINTBEGIN(*-signed-bitwise)
template <typename PollingType>
Result EventLoop<PollingType>::wait_next_event(const int timeout_ms) {
  // 先处理非文件描述符的规则
  {
    for (auto it = _non_fd_rules.begin(); it != _non_fd_rules.end();) {
      auto& this_rule = **it;
      bool rule_fired = false;

      if (this_rule.cancel_requested) {
        it = _non_fd_rules.erase(it);
        continue;
      }

      uint8_t iterations = 0;
      while (this_rule.interest()) {
        if (iterations++ >= 128) {
          throw std::runtime_error(
              "EventLoop: busy wait detected: rule \"" +
              _rule_categories.at(this_rule.category_id).name +
              "\" is still interested after " + std::to_string(iterations) +
              " iterations");
        }

        rule_fired = true;
        this_rule.callback();
      }

      if (rule_fired) {
        return Result::Success; /* 每次迭代只处理一条规则 */
      }

      ++it;
    }
  }

  // 处理文件描述符的规则
  bool something_to_poll = false;

  for (auto it = _fd_rules.begin(); it != _fd_rules.end();) {
    auto& this_rule_addr = *it;
    auto& this_rule = **it;

    if (this_rule.cancel_requested) {
      it = _fd_rules.erase(it);
      continue;
    }

    if (this_rule.direction == Direction::In && this_rule.fd.eof()) {
      // no more reading on this rule, it's reached eof
      this_rule.cancel();
      it = _fd_rules.erase(it);
      continue;
    }

    if (this_rule.fd.closed()) {
      this_rule.cancel();
      it = _fd_rules.erase(it);
      continue;
    }

    if (this_rule.interest()) {
      _polling.insert_rule(this_rule_addr);
      something_to_poll = true;
    }
    ++it;
  }

  // 如果没有什么可轮询的则退出
  if (not something_to_poll) {
    _polling.clear();
    return Result::Exit;
  }

  _polling.insert_to_epoll();

  if (-1 == _polling.b_epoll_wait(timeout_ms)) {
    _polling.clear();
    return Result::Timeout;
  }

  _polling.call(_fd_rules);

  _polling.clear();

  return Result::Success;
}
// NOLINTEND(*-signed-bitwise)
// NOLINTEND(*-cognitive-complexity)
