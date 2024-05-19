#pragma once

#include <string.h>

#include <functional>
#include <list>
#include <memory>
#include <ostream>
#include <string_view>

#include "datagram/file_descriptor.h"
#include "polling/b_epoll.h"
#include "polling/b_poll.h"
#include "polling/rule.h"
#include "utils/exception.h"

using namespace std;

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
  // first, handle the non-file-descriptor-related rules
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
          throw runtime_error("EventLoop: busy wait detected: rule \"" +
                              _rule_categories.at(this_rule.category_id).name +
                              "\" is still interested after " +
                              to_string(iterations) + " iterations");
        }

        rule_fired = true;
        this_rule.callback();
      }

      if (rule_fired) {
        return Result::Success; /* only serve one rule on each iteration */
      }

      ++it;
    }
  }

  // now the file-descriptor-related rules. poll any "interested" file
  // descriptors
  vector<pollfd> pollfds{};
  pollfds.reserve(_fd_rules.size());
  bool something_to_poll = false;

  // set up the pollfd for each rule
  for (auto it = _fd_rules.begin();
       it !=
       _fd_rules.end();) {  // NOTE: it gets erased or incremented in loop body
    auto& this_rule = **it;

    if (this_rule.cancel_requested) {
      //      this_rule.cancel();
      //      if rule is cancelled externally, no need to call the cancellation
      //      callback this makes it easier to cancel rules and delete captured
      //      objects right away
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
      pollfds.push_back({this_rule.fd.fd_num(),
                         static_cast<int16_t>(this_rule.direction), 0});
      something_to_poll = true;
    } else {
      pollfds.push_back({this_rule.fd.fd_num(), 0,
                         0});  // placeholder --- we still want errors
    }
    ++it;
  }

  // quit if there is nothing left to poll
  if (not something_to_poll) {
    return Result::Exit;
  }

  // call poll -- wait until one of the fds satisfies one of the rules
  // (writeable/readable)
  if (0 == CheckSystemCall(
               "poll", ::poll(pollfds.data(), pollfds.size(), timeout_ms))) {
    return Result::Timeout;
  }

  // go through the poll results
  for (auto [it, idx] = make_pair(_fd_rules.begin(), static_cast<size_t>(0));
       it != _fd_rules.end(); ++idx) {
    const auto& this_pollfd = pollfds.at(idx);
    auto& this_rule = **it;

    const auto poll_error =
        static_cast<bool>(this_pollfd.revents & (POLLERR | POLLNVAL));
    if (poll_error) {
      /* recoverable error? */
      if (not static_cast<bool>(this_pollfd.revents & POLLNVAL)) {
        if (this_rule.recover()) {
          ++it;
          continue;
        }
      }

      /* see if fd is a socket */
      int socket_error = 0;
      socklen_t optlen = sizeof(socket_error);
      const int ret = getsockopt(this_rule.fd.fd_num(), SOL_SOCKET, SO_ERROR,
                                 &socket_error, &optlen);
      if (ret == -1 and errno == ENOTSOCK) {
        cerr << "error on polled file descriptor for rule \""
             << _rule_categories.at(this_rule.category_id).name << "\"\n";
      } else if (ret == -1) {
        throw unix_error("getsockopt");
      } else if (optlen != sizeof(socket_error)) {
        throw runtime_error("unexpected length from getsockopt: " +
                            to_string(optlen));
      } else if (socket_error) {
        cerr << "error on polled socket for rule \""
             << _rule_categories.at(this_rule.category_id).name
             << "\": " << strerror(socket_error) << "\n";
      }

      this_rule.cancel();
      it = _fd_rules.erase(it);
      continue;
    }

    const auto poll_ready =
        static_cast<bool>(this_pollfd.revents & this_pollfd.events);
    const auto poll_hup = static_cast<bool>(this_pollfd.revents & POLLHUP);
    if (poll_hup && ((this_pollfd.events && !poll_ready) or
                     (this_rule.direction == Direction::Out))) {
      // if we asked for the status, and the _only_ condition was a hangup, this
      // FD is defunct:
      //   - if it was POLLIN and nothing is readable, no more will ever be
      //   readable
      //   - if it was POLLOUT, it will not be writable again
      // additionally, consider FD defunct if rule will only query for
      // Direction::Out
      this_rule.cancel();
      it = _fd_rules.erase(it);
      continue;
    }

    if (poll_ready) {
      // we only want to call callback if revents includes the event we asked
      // for
      const auto count_before = this_rule.service_count();
      this_rule.callback();

      if (count_before == this_rule.service_count() and
          (not this_rule.fd.closed()) and this_rule.interest()) {
        throw runtime_error("EventLoop: busy wait detected: rule \"" +
                            _rule_categories.at(this_rule.category_id).name +
                            "\" did not read/write fd and is still interested");
      }

      return Result::Success; /* only serve one rule on each iteration */
    }

    ++it;  // if we got here, it means we didn't call _fd_rules.erase()
  }

  return Result::Success;
}
// NOLINTEND(*-signed-bitwise)
// NOLINTEND(*-cognitive-complexity)
