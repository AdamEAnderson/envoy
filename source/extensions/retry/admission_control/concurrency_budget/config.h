#pragma once

#include <memory>

#include "envoy/extensions/retry/admission_control/concurrency_budget/v3/concurrency_budget_config.pb.validate.h"
#include "envoy/upstream/admission_control.h"

#include "concurrency_budget.h"

namespace Envoy {
namespace Extensions {
namespace Retry {
namespace AdmissionControl {

class ConcurrencyBudgetFactory : public Upstream::RetryAdmissionControllerFactory {
public:
  Upstream::RetryAdmissionControllerSharedPtr
  createAdmissionController(const Protobuf::Message& config,
                            ProtobufMessage::ValidationVisitor& validation_visitor,
                            Runtime::Loader& runtime) override;

  std::string name() const override { return "envoy.retry_admission_control.concurrency_budget"; }

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<envoy::extensions::retry::admission_control::concurrency_budget::v3::
                                ConcurrencyBudgetConfig>();
  }
};

} // namespace AdmissionControl
} // namespace Retry
} // namespace Extensions
} // namespace Envoy
