#include "config.h"

#include "envoy/registry/registry.h"

#include "source/common/protobuf/message_validator_impl.h"
#include "source/common/protobuf/utility.h"

namespace Envoy {
namespace Extensions {
namespace Retry {
namespace AdmissionControl {

REGISTER_FACTORY(ConcurrencyBudgetFactory, Upstream::RetryAdmissionControllerFactory);

Upstream::RetryAdmissionControllerSharedPtr ConcurrencyBudgetFactory::createAdmissionController(
    const Protobuf::Message& config, ProtobufMessage::ValidationVisitor& validation_visitor,
    Runtime::Loader&) {
  const auto& concurrency_budget_config =
      MessageUtil::downcastAndValidate<const envoy::extensions::retry::admission_control::
                                           concurrency_budget::v3::ConcurrencyBudgetConfig&>(
          config, validation_visitor);
  return std::make_shared<ConcurrencyBudget>(concurrency_budget_config.min_concurrent_retry_limit(),
                                             concurrency_budget_config.budget_percent().value());
}

} // namespace AdmissionControl
} // namespace Retry
} // namespace Extensions
} // namespace Envoy
