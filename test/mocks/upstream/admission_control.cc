#include "admission_control.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Upstream {

using testing::_;
using testing::Invoke;
using testing::Return;

MockRetryStreamAdmissionController::MockRetryStreamAdmissionController() {
  ON_CALL(*this, isRetryAdmitted(_, _, _)).WillByDefault(Return(true));
};

MockRetryStreamAdmissionController::~MockRetryStreamAdmissionController() = default;

MockRetryAdmissionController::MockRetryAdmissionController()
    : stream_admission_controller_ptr_(
          std::make_unique<NiceMock<MockRetryStreamAdmissionController>>()),
      stream_admission_controller_(*stream_admission_controller_ptr_) {
  ON_CALL(*this, createStreamAdmissionController(_))
      .WillByDefault(
          Invoke([this](const StreamInfo::StreamInfo&) -> RetryStreamAdmissionControllerPtr {
            return std::move(stream_admission_controller_ptr_);
          }));
};

MockRetryAdmissionController::~MockRetryAdmissionController() = default;

MockAdmissionControl::MockAdmissionControl()
    : retry_admission_controller_(std::make_shared<NiceMock<MockRetryAdmissionController>>()) {
  ON_CALL(*this, retry()).WillByDefault(Return(retry_admission_controller_));
};

MockAdmissionControl::~MockAdmissionControl() = default;

} // namespace Upstream
} // namespace Envoy
