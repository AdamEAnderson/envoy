#pragma once

#include <cstdint>
#include <memory>

#include "envoy/upstream/admission_control.h"

namespace Envoy {
namespace Extensions {
namespace Retry {
namespace AdmissionControl {

class StaticLimits : public Upstream::RetryAdmissionController {
public:
  StaticLimits(uint64_t max_concurrent_retries) : max_active_retries_(max_concurrent_retries){};

  Upstream::RetryStreamAdmissionControllerPtr
  createStreamAdmissionController(const StreamInfo::StreamInfo&) override {
    return std::make_unique<StreamAdmissionController>(active_retries_, max_active_retries_);
  };

private:
  using PrimitiveGaugeSharedPtr = std::shared_ptr<Stats::PrimitiveGauge>;

  class StreamAdmissionController : public Upstream::RetryStreamAdmissionController {
  public:
    StreamAdmissionController(PrimitiveGaugeSharedPtr active_retries,
                              const uint64_t max_active_retries)
        : active_retries_(active_retries), max_active_retries_(max_active_retries){};

    ~StreamAdmissionController() override {
      active_retries_->sub(stream_active_retry_attempt_numbers_.size());
    };

    void onTryStarted(uint64_t) override {}
    void onTrySucceeded(uint64_t attempt_number) override;
    void onSuccessfulTryFinished() override {}
    void onTryAborted(uint64_t attempt_number) override;
    bool isRetryAdmitted(uint64_t prev_attempt_number, uint64_t retry_attempt_number,
                         bool abort_previous_on_retry) override;

  private:
    PrimitiveGaugeSharedPtr active_retries_;
    std::set<uint64_t> stream_active_retry_attempt_numbers_{};
    const uint64_t max_active_retries_;
  };

  PrimitiveGaugeSharedPtr active_retries_{};
  const uint64_t max_active_retries_;
};

} // namespace AdmissionControl
} // namespace Retry
} // namespace Extensions
} // namespace Envoy
