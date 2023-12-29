#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include "envoy/config/core/v3/extension.pb.h"
#include "envoy/upstream/admission_control.h"

#include "source/common/config/utility.h"
#include "source/common/common/assert.h"

namespace Envoy {
namespace Upstream {
class AdmissionControlImpl : public AdmissionControl {
public:
  AdmissionControlImpl(const envoy::config::core::v3::TypedExtensionConfig& retry) {
    auto& factory = Config::Utility::getAndCheckFactory<
      RetryAdmissionControllerFactory>(retry);
    retry_ = factory.createAdmissionController(retry.typed_config());
  }

  RetryAdmissionControllerSharedPtr retry() override { return retry_; }
private:
  RetryAdmissionControllerSharedPtr retry_;
};
using AdmissionControlImplSharedPtr = std::shared_ptr<AdmissionControlImpl>;

} // namespace Upstream
} // namespace Envoy
