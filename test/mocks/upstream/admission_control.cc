#include "admission_control.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Upstream {

MockRetryStreamAdmissionController::MockRetryStreamAdmissionController() = default;

MockRetryStreamAdmissionController::~MockRetryStreamAdmissionController() = default;

} // namespace Upstream
} // namespace Envoy
