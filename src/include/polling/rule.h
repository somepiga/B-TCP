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

/**
 * @brief 事件规则基类
 * @param interest 感兴趣的事件
 * @param callback 回调函数
 */
struct BasicRule {
  size_t category_id;
  InterestT interest;
  CallbackT callback;
  bool cancel_requested{};

  BasicRule(size_t s_category_id, InterestT s_interest, CallbackT s_callback);
};

/**
 * @brief 关于文件描述符的事件规则
 * @note 注意：一个文件描述符可以对应多个 Rule
 */
struct FDRule : public BasicRule {
  FileDescriptor fd;
  Direction direction;
  CallbackT cancel;   //!< 取消规则时调用的回调
  InterestT recover;  //!< 当 fd ERR 时调用的回调

  FDRule(BasicRule&& base, FileDescriptor&& s_fd, Direction s_direction,
         CallbackT s_cancel, InterestT s_recover);

  //! 返回 fd 被读取或写入的次数，具体取决于 Rule::direction 的值
  //! \details 该函数由EventLoop内部使用。你不需要调用它
  unsigned int service_count() const;
};

/**
 * @brief 每次调用 EventLoop::wait_next_event 时返回
 */
enum class Result {
  Success,  //!< 至少触发了一条规则
  Timeout,  //!< 超时之前没有触发任何规则
  Exit  //!< 所有规则已被取消或不感兴趣。不再调用 EventLoop::wait_next_event
};

class RuleHandle {
  std::weak_ptr<BasicRule> rule_weak_ptr_;

 public:
  template <class RuleType>
  explicit RuleHandle(const std::shared_ptr<RuleType> x) : rule_weak_ptr_(x) {}

  void cancel();
};