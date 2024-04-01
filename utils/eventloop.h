#pragma once

#include <functional>
#include <list>
#include <memory>
#include <ostream>
#include <poll.h>
#include <string_view>

#include "file_descriptor.h"

//! EventLoop类是一个用于等待文件描述符上的事件并执行相应回调的工具类
class EventLoop {
   public:
    //! 表示有兴趣 读(In) 或 写(Out) 轮询的 fd
    enum class Direction : int16_t {
        In = POLLIN,   //!< 当 Rule::fd 可读时会触发回调
        Out = POLLOUT  //!< 当 Rule::fd 可写时会触发回调
    };

   private:
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

        BasicRule(size_t s_category_id,
                  InterestT s_interest,
                  CallbackT s_callback);
    };

    struct FDRule : public BasicRule {
        FileDescriptor fd;    // 用于监视活动的 FileDescriptor.
        Direction direction;  //!< Direction::In for reading from fd,
                              //!< Direction::Out for writing to fd.
        CallbackT cancel;   //!< 取消规则时调用的回调 (e.g. on hangup)
        InterestT recover;  //!< 当fd ERR时调用的回调.
                            //!< Returns true to keep rule.

        FDRule(BasicRule&& base,
               FileDescriptor&& s_fd,
               Direction s_direction,
               CallbackT s_cancel,
               InterestT s_recover);

        //! 返回fd被读取或写入的次数,具体取决于Rule::direction的值。该函数由EventLoop内部使用,你不需要调用它
        unsigned int service_count() const;
    };

    std::vector<RuleCategory> _rule_categories{};
    std::list<std::shared_ptr<FDRule>> _fd_rules{};
    std::list<std::shared_ptr<BasicRule>> _non_fd_rules{};

   public:
    EventLoop() { _rule_categories.reserve(64); }

    // EventLoop::wait_next_event的返回值
    enum class Result {
        Success,  // 至少触发了一项规则
        Timeout,  // 超时之前没有触发任何规则
        Exit  // 所有规则已被取消或不感兴趣；不再调用EventLoop::wait_next_event。
    };

    size_t add_category(const std::string& name);

    class RuleHandle {
        std::weak_ptr<BasicRule> rule_weak_ptr_;

       public:
        template <class RuleType>
        explicit RuleHandle(const std::shared_ptr<RuleType> x)
            : rule_weak_ptr_(x) {}

        void cancel();
    };

    RuleHandle add_rule(
        size_t category_id,
        FileDescriptor& fd,
        Direction direction,
        const CallbackT& callback,
        const InterestT& interest = [] { return true; },
        const CallbackT& cancel = [] {},
        const InterestT& recover = [] { return false; });

    RuleHandle add_rule(
        size_t category_id,
        const CallbackT& callback,
        const InterestT& interest = [] { return true; });

    //! Calls [poll(2)](\ref man2::poll) and then executes callback for each
    //! ready fd.
    Result wait_next_event(int timeout_ms);

    // 同时添加类别和规则的便捷功能
    template <typename... Targs>
    auto add_rule(const std::string& name, Targs&&... Fargs) {
        return add_rule(add_category(name), std::forward<Targs>(Fargs)...);
    }
};

using Direction = EventLoop::Direction;
