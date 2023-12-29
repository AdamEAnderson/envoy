#include "static_limits.h"

namespace Envoy {
namespace Extensions {
namespace Retry {
namespace AdmissionControl {

void StaticLimits::StreamAdmissionController::onTrySucceeded(uint64_t attempt_number) {
  if (stream_active_retry_attempt_numbers_.erase(attempt_number)) {
    // once a retry reaches the "success" phase, it is no longer considered an active retry
    active_retries_->dec();
  }
}

void StaticLimits::StreamAdmissionController::onTryAborted(uint64_t attempt_number) {
  if (stream_active_retry_attempt_numbers_.erase(attempt_number)) {
    active_retries_->dec();
  }
}

bool StaticLimits::StreamAdmissionController::isRetryAdmitted(uint64_t prev_attempt_number,
                                                              uint64_t retry_attempt_number,
                                                              bool abort_previous_on_retry) {
  uint64_t active_retry_diff_on_retry = 1;
  if (abort_previous_on_retry && stream_active_retry_attempt_numbers_.find(prev_attempt_number) !=
                                     stream_active_retry_attempt_numbers_.end()) {
    // if we admit the retry, we will abort the previous try which was a retry,
    // so the total number of active retries will not change
    active_retry_diff_on_retry = 0;
  }
  if (active_retries_->value() + active_retry_diff_on_retry > max_active_retries_) {
    return false;
  }
  active_retries_->add(active_retry_diff_on_retry);
  if (abort_previous_on_retry) {
    stream_active_retry_attempt_numbers_.erase(prev_attempt_number);
  }
  stream_active_retry_attempt_numbers_.insert(retry_attempt_number);
  return true;
};

} // namespace AdmissionControl
} // namespace Retry
} // namespace Extensions
} // namespace Envoy
