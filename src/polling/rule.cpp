#include "polling/rule.h"

unsigned int FDRule::service_count() const {
  return direction == Direction::In ? fd.read_count() : fd.write_count();
}

BasicRule::BasicRule(size_t s_category_id, InterestT s_interest,
                     CallbackT s_callback)
    : category_id(s_category_id),
      interest(std::move(s_interest)),
      callback(std::move(s_callback)) {}

FDRule::FDRule(BasicRule&& base, FileDescriptor&& s_fd, Direction s_direction,
               CallbackT s_cancel, InterestT s_recover)
    : BasicRule(base),
      fd(std::move(s_fd)),
      direction(s_direction),
      cancel(std::move(s_cancel)),
      recover(std::move(s_recover)) {}

void RuleHandle::cancel() {
  const std::shared_ptr<BasicRule> rule_shared_ptr = rule_weak_ptr_.lock();
  if (rule_shared_ptr) {
    rule_shared_ptr->cancel_requested = true;
  }
}