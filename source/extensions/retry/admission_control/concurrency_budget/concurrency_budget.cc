#include "concurrency_budget.h"

#include <cstdint>

namespace Envoy {
namespace Extensions {
namespace Retry {
namespace AdmissionControl {

void ConcurrencyBudget::StreamAdmissionController::onTryStarted(uint64_t attempt_number) {
  if (stream_active_retry_attempt_numbers_.find(attempt_number) !=
      stream_active_retry_attempt_numbers_.end()) {
    // if this is a retry, we've already counted it as active when it was initially scheduled
    return;
  }
  active_tries_->inc();
  stream_active_tries_++;
}

void ConcurrencyBudget::StreamAdmissionController::onTrySucceeded(uint64_t attempt_number) {
  if (stream_active_retry_attempt_numbers_.erase(attempt_number)) {
    // once a retry reaches the "success" phase, it is no longer considered an active retry
    active_retries_->dec();
  }
}

void ConcurrencyBudget::StreamAdmissionController::onSuccessfulTryFinished() {
  active_tries_->dec();
  stream_active_tries_--;
}

void ConcurrencyBudget::StreamAdmissionController::onTryAborted(uint64_t attempt_number) {
  if (stream_active_retry_attempt_numbers_.erase(attempt_number)) {
    active_retries_->dec();
  }
  active_tries_->dec();
  stream_active_tries_--;
}

bool ConcurrencyBudget::StreamAdmissionController::isRetryAdmitted(uint64_t prev_attempt_number,
                                                                   uint64_t retry_attempt_number,
                                                                   bool abort_previous_on_retry) {
  uint64_t active_retry_diff_on_retry = 1;
  if (abort_previous_on_retry && stream_active_retry_attempt_numbers_.find(prev_attempt_number) !=
                                     stream_active_retry_attempt_numbers_.end()) {
    // if we admit the retry, we will abort the previous try which was a retry,
    // so the total number of active retries will not change
    active_retry_diff_on_retry = 0;
  }
  if (active_retries_->value() + active_retry_diff_on_retry > getRetryConcurrencyLimit()) {
    return false;
  }
  active_retries_->add(active_retry_diff_on_retry);
  if (abort_previous_on_retry) {
    // no change to active_tries
    stream_active_retry_attempt_numbers_.erase(prev_attempt_number);
  } else {
    active_tries_->inc();
    stream_active_tries_++;
  }
  stream_active_retry_attempt_numbers_.insert(retry_attempt_number);
  return true;
};

uint64_t ConcurrencyBudget::StreamAdmissionController::getRetryConcurrencyLimit() const {
  uint64_t retry_concurrency_limit = active_tries_->value() * budget_percent_ / 100.0;
  retry_concurrency_limit = std::max(retry_concurrency_limit, min_retry_concurrency_limit_);
  return retry_concurrency_limit;
}

} // namespace AdmissionControl
} // namespace Retry
} // namespace Extensions
} // namespace Envoy
