#include "config.h"

#include "envoy/extensions/retry/admission_control/static_limits/v3/static_limits_config.pb.h"
#include "envoy/registry/registry.h"
#include "envoy/upstream/admission_control.h"

#include "source/common/protobuf/message_validator_impl.h"
#include "source/common/protobuf/utility.h"

#include "static_limits.h"

namespace Envoy {
namespace Extensions {
namespace Retry {
namespace AdmissionControl {

REGISTER_FACTORY(StaticLimitsFactory, Upstream::RetryAdmissionControllerFactory);

Upstream::RetryAdmissionControllerSharedPtr StaticLimitsFactory::createAdmissionController(
    const Protobuf::Message& config, ProtobufMessage::ValidationVisitor& validation_visitor,
    Runtime::Loader&) {
  const auto& static_limits_config = MessageUtil::downcastAndValidate<
      const envoy::extensions::retry::admission_control::static_limits::v3::StaticLimitsConfig&>(
      config, validation_visitor);
  return std::make_shared<StaticLimits>(static_limits_config.max_concurrent_retries());
}

} // namespace AdmissionControl
} // namespace Retry
} // namespace Extensions
} // namespace Envoy
