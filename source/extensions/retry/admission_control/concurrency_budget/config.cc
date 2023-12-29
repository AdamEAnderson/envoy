#include "config.h"
#include "envoy/registry/registry.h"
#include "source/common/protobuf/message_validator_impl.h"
#include "source/common/protobuf/utility.h"

namespace Envoy {
namespace Extensions {
namespace Retry {
namespace AdmissionControl {

REGISTER_FACTORY(ConcurrencyBudgetFactory, Upstream::RetryAdmissionControllerFactory);

Upstream::RetryAdmissionControllerSharedPtr
ConcurrencyBudgetFactory::createAdmissionController(const Protobuf::Message& config) {
  const auto& concurrency_budget_config =
      MessageUtil::downcastAndValidate<
          const envoy::extensions::retry::admission_control::concurrency_budget::v3::ConcurrencyBudgetConfig&>(
            config, ProtobufMessage::getStrictValidationVisitor());
  return std::make_shared<ConcurrencyBudget>(concurrency_budget_config.min_concurrent_retry_limit(), concurrency_budget_config.budget_percent().value());
}

}
} // namespace AdmissionControl
} // namespace Extensions
} // namespace Envoy
