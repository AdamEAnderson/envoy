#include "static_limits.h"

#include <cstdint>

#include "source/common/runtime/runtime_features.h"

namespace Envoy {
namespace Extensions {
namespace Retry {
namespace AdmissionControl {

void StaticLimits::StreamAdmissionController::onTrySucceeded(uint64_t attempt_number) {
  if (stream_active_retry_attempt_numbers_.erase(attempt_number)) {
    // once a retry reaches the "success" phase, it is no longer considered an active retry
    active_retries_->dec();
    cb_stats_.remaining_retries_.inc();
    setStats();
  }
}

void StaticLimits::StreamAdmissionController::onTryAborted(uint64_t attempt_number) {
  if (stream_active_retry_attempt_numbers_.erase(attempt_number)) {
    active_retries_->dec();
    cb_stats_.remaining_retries_.inc();
    setStats();
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
  if (active_retries_->value() + active_retry_diff_on_retry > getMaxActiveRetries()) {
    setStats();
    return false;
  }
  active_retries_->add(active_retry_diff_on_retry);
  if (abort_previous_on_retry) {
    stream_active_retry_attempt_numbers_.erase(prev_attempt_number);
  }
  stream_active_retry_attempt_numbers_.insert(retry_attempt_number);
  setStats();
  return true;
};

uint64_t StaticLimits::StreamAdmissionController::getMaxActiveRetries() const {
  return runtime_.snapshot().getInteger(max_active_retries_key_, max_active_retries_);
}

void StaticLimits::StreamAdmissionController::setStats() {
  if (!Runtime::runtimeFeatureEnabled("envoy.reloadable_features.use_retry_admission_control")) {
    return;
  }
  uint64_t max_active_retries = getMaxActiveRetries();
  uint64_t retries_remaining = active_retries_->value() > max_active_retries
                                   ? 0UL
                                   : max_active_retries - active_retries_->value();
  cb_stats_.remaining_retries_.set(retries_remaining);
  cb_stats_.rq_retry_open_.set(retries_remaining > 0 ? 0 : 1);
}

} // namespace AdmissionControl
} // namespace Retry
} // namespace Extensions
} // namespace Envoy
