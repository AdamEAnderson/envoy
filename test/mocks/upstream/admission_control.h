#pragma once

#include "envoy/upstream/admission_control.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include <cstdint>

namespace Envoy {
namespace Upstream {

class MockRetryStreamAdmissionController : public RetryStreamAdmissionController {
public:
  MockRetryStreamAdmissionController();
  ~MockRetryStreamAdmissionController() override;

  MOCK_METHOD(void, onTryStarted, (uint64_t));
  MOCK_METHOD(void, onTrySucceeded, (uint64_t));
  MOCK_METHOD(void, onSuccessfulTryFinished, ());
  MOCK_METHOD(void, onTryAborted, (uint64_t));
  MOCK_METHOD(bool, isRetryAdmitted, (uint64_t, uint64_t, bool));
};

} // namespace Upstream
} // namespace Envoy
