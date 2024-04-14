#pragma once

#include <poll.h>
#include <sys/epoll.h>

#include <functional>
#include <list>
#include <memory>
#include <ostream>
#include <string_view>

#include "datagram/file_descriptor.h"

enum class Direction : int16_t { In = EPOLLIN, Out = EPOLLOUT };

using CallbackT = std::function<void(void)>;
using InterestT = std::function<bool(void)>;

struct RuleCategory {
  std::string name;
};

struct BasicRule {
  size_t category_id;
  InterestT interest;
  CallbackT callback;
  bool cancel_requested{};

  BasicRule(size_t s_category_id, InterestT s_interest, CallbackT s_callback);
};

/**
 * @note 注意：一个文件描述符可以对应多个 Rule
 */
struct FDRule : public BasicRule {
  FileDescriptor fd;    //!< FileDescriptor to monitor for activity.
  Direction direction;  //!< Direction::In for reading from fd, Direction::Out
                        //!< for writing to fd.
  CallbackT cancel;   //!< A callback that is called when the rule is cancelled
                      //!< (e.g. on hangup)
  InterestT recover;  //!< A callback that is called when the fd is ERR.
                      //!< Returns true to keep rule.

  FDRule(BasicRule&& base, FileDescriptor&& s_fd, Direction s_direction,
         CallbackT s_cancel, InterestT s_recover);

  //! Returns the number of times fd has been read or written, depending on
  //! the value of Rule::direction. \details This function is used internally
  //! by EventLoop; you will not need to call it
  unsigned int service_count() const;
};

//! Returned by each call to EventLoop::wait_next_event.
enum class Result {
  Success,  //!< At least one Rule was triggered.
  Timeout,  //!< No rules were triggered before timeout.
  Exit      //!< All rules have been canceled or were uninterested; make no
            //!< further calls to EventLoop::wait_next_event.
};

class RuleHandle {
  std::weak_ptr<BasicRule> rule_weak_ptr_;

 public:
  template <class RuleType>
  explicit RuleHandle(const std::shared_ptr<RuleType> x) : rule_weak_ptr_(x) {}

  void cancel();
};