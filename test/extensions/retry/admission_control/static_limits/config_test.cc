#include <cstdint>
#include <memory>

#include "envoy/extensions/retry/admission_control/static_limits/v3/static_limits_config.pb.h"
#include "envoy/registry/registry.h"
#include "envoy/upstream/admission_control.h"

#include "source/extensions/retry/admission_control/static_limits/config.h"

#include "test/mocks/runtime/mocks.h"
#include "test/mocks/stream_info/mocks.h"
#include "test/test_common/test_runtime.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace testing;

namespace Envoy {
namespace Extensions {
namespace Retry {
namespace AdmissionControl {
namespace {

Upstream::ClusterCircuitBreakersStats clusterCircuitBreakersStats(Stats::Store& store) {
  return {
      ALL_CLUSTER_CIRCUIT_BREAKERS_STATS(c, POOL_GAUGE(store), h, tr, GENERATE_STATNAME_STRUCT)};
}

class StaticLimitsConfigTest : public testing::Test {
public:
  StaticLimitsConfigTest() : cb_stats_(clusterCircuitBreakersStats(store_)) {
    Upstream::RetryAdmissionControllerFactory* factory =
        Registry::FactoryRegistry<Upstream::RetryAdmissionControllerFactory>::getFactory(
            "envoy.retry_admission_control.static_limits");
    EXPECT_NE(nullptr, factory);
    StaticLimitsFactory* static_limits_factory = dynamic_cast<StaticLimitsFactory*>(factory);
    factory_ = std::make_unique<StaticLimitsFactory>(*static_limits_factory);
    ON_CALL(runtime_.snapshot_, getInteger("test_prefix.max_retries", 3U))
        .WillByDefault([](std::basic_string_view<char>, uint64_t default_value) -> uint64_t {
          return default_value;
        });
  };

  void createAdmissionController() {
    admission_controller_ =
        factory_->createAdmissionController(config_, ProtobufMessage::getStrictValidationVisitor(),
                                            runtime_, runtime_prefix_, cb_stats_);
  }

  std::unique_ptr<Upstream::RetryAdmissionControllerFactory> factory_;
  NiceMock<Runtime::MockLoader> runtime_;
  TestScopedRuntime scoped_runtime_;
  Stats::IsolatedStoreImpl store_;
  Upstream::ClusterCircuitBreakersStats cb_stats_;
  std::string runtime_prefix_{"test_prefix."};
  envoy::extensions::retry::admission_control::static_limits::v3::StaticLimitsConfig config_;
  NiceMock<StreamInfo::MockStreamInfo> request_stream_info_;
  Upstream::RetryAdmissionControllerSharedPtr admission_controller_;
  Upstream::RetryStreamAdmissionControllerPtr retry_stream_admission_controller_;
};

TEST_F(StaticLimitsConfigTest, FactoryDefault) {
  // default config
  createAdmissionController();

  retry_stream_admission_controller_ =
      admission_controller_->createStreamAdmissionController(request_stream_info_);

  // by default, 3 retries are allowed
  retry_stream_admission_controller_->onTryStarted(1);
  ASSERT_TRUE(retry_stream_admission_controller_->isRetryAdmitted(1, 2, true));
  retry_stream_admission_controller_->onTryStarted(2);
  ASSERT_TRUE(retry_stream_admission_controller_->isRetryAdmitted(2, 3, false));
  retry_stream_admission_controller_->onTryStarted(3);
  ASSERT_TRUE(retry_stream_admission_controller_->isRetryAdmitted(3, 4, false));
  retry_stream_admission_controller_->onTryStarted(4);
  ASSERT_FALSE(retry_stream_admission_controller_->isRetryAdmitted(4, 5, false));
}

TEST_F(StaticLimitsConfigTest, MultipleStreams) {
  createAdmissionController();

  auto stream1 = admission_controller_->createStreamAdmissionController(request_stream_info_);
  auto stream2 = admission_controller_->createStreamAdmissionController(request_stream_info_);

  ASSERT_EQ(cb_stats_.remaining_retries_.value(), 3);
  ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 0);
  stream1->onTryStarted(1);                          // s1: 1
  ASSERT_TRUE(stream1->isRetryAdmitted(1, 2, true)); // s1: 2
  ASSERT_EQ(cb_stats_.remaining_retries_.value(), 2);
  stream2->onTryStarted(1);                          // s2: 1
  ASSERT_TRUE(stream2->isRetryAdmitted(1, 2, true)); // s2: 2
  stream2->onTryStarted(2);
  ASSERT_EQ(cb_stats_.remaining_retries_.value(), 1);
  ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 0);
  stream1->onTryStarted(2);
  ASSERT_TRUE(stream1->isRetryAdmitted(2, 3, false)); // s1: 3
  stream1->onTryStarted(3);
  ASSERT_EQ(cb_stats_.remaining_retries_.value(), 0);
  ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 1);
  ASSERT_TRUE(stream2->isRetryAdmitted(2, 3, true)); // s2: 3, abort s2: 2
  stream2->onTryStarted(3);
  ASSERT_EQ(cb_stats_.remaining_retries_.value(), 0);
  ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 1);
  stream2->onTryAborted(1); // abort s2: 1
  ASSERT_EQ(cb_stats_.remaining_retries_.value(), 0);
  ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 1);
  stream2->onTryAborted(3); // abort s2: 3
  ASSERT_EQ(cb_stats_.remaining_retries_.value(), 1);
  ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 0);
  stream2.reset(); // no effect
  ASSERT_EQ(cb_stats_.remaining_retries_.value(), 1);
  stream1.reset(); // abort s1: 1, s1: 2, s1: 3
  ASSERT_EQ(cb_stats_.remaining_retries_.value(), 3);
  ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 0);
}

TEST_F(StaticLimitsConfigTest, FactoryRuntimeOverrides) {
  // default config
  createAdmissionController();

  ASSERT_EQ(cb_stats_.remaining_retries_.value(), 3);
  ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 0);
  retry_stream_admission_controller_ =
      admission_controller_->createStreamAdmissionController(request_stream_info_);
  ASSERT_EQ(cb_stats_.remaining_retries_.value(), 3);
  ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 0);
  retry_stream_admission_controller_->onTryStarted(1);
  ASSERT_TRUE(retry_stream_admission_controller_->isRetryAdmitted(1, 2, true));
  ASSERT_EQ(cb_stats_.remaining_retries_.value(), 2);
  ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 0);

  // can be overridden by runtime
  EXPECT_CALL(runtime_.snapshot_, getInteger("test_prefix.max_retries", 3U))
      .Times(2)
      .WillRepeatedly(Return(1U));
  retry_stream_admission_controller_->onTryStarted(2);
  ASSERT_FALSE(retry_stream_admission_controller_->isRetryAdmitted(2, 3, false));
  ASSERT_EQ(cb_stats_.remaining_retries_.value(), 0);
  ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 1);
  retry_stream_admission_controller_.reset();
  ASSERT_EQ(cb_stats_.remaining_retries_.value(), 1);
  ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 0);
}

TEST_F(StaticLimitsConfigTest, StatsGuardedByRuntimeFeature) {
  // default config
  createAdmissionController();

  retry_stream_admission_controller_ =
      admission_controller_->createStreamAdmissionController(request_stream_info_);
  ASSERT_EQ(cb_stats_.remaining_retries_.value(), 3);
  ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 0);
  retry_stream_admission_controller_->onTryStarted(1);
  ASSERT_TRUE(retry_stream_admission_controller_->isRetryAdmitted(1, 2, true));
  ASSERT_EQ(cb_stats_.remaining_retries_.value(), 2);
  ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 0);

  scoped_runtime_.mergeValues({{"envoy.reloadable_features.use_retry_admission_control", "false"}});

  cb_stats_.remaining_retries_.set(42);
  cb_stats_.rq_retry_open_.set(42);

  retry_stream_admission_controller_->onTryStarted(2);
  ASSERT_TRUE(retry_stream_admission_controller_->isRetryAdmitted(2, 3, false));
  ASSERT_EQ(cb_stats_.remaining_retries_.value(), 42);
  ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 42);
  retry_stream_admission_controller_->onTryStarted(3);
  ASSERT_TRUE(retry_stream_admission_controller_->isRetryAdmitted(3, 4, false));
  retry_stream_admission_controller_->onTryStarted(4);
  ASSERT_FALSE(retry_stream_admission_controller_->isRetryAdmitted(4, 5, false));
  ASSERT_EQ(cb_stats_.remaining_retries_.value(), 42);
  ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 42);
  retry_stream_admission_controller_.reset();
  ASSERT_EQ(cb_stats_.remaining_retries_.value(), 42);
  ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 42);

  retry_stream_admission_controller_ =
      admission_controller_->createStreamAdmissionController(request_stream_info_);
  retry_stream_admission_controller_->onTryStarted(1);
  ASSERT_TRUE(retry_stream_admission_controller_->isRetryAdmitted(1, 2, false));
  ASSERT_EQ(cb_stats_.remaining_retries_.value(), 42);
  ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 42);

  scoped_runtime_.mergeValues({{"envoy.reloadable_features.use_retry_admission_control", "true"}});

  retry_stream_admission_controller_->onTryStarted(2);
  ASSERT_TRUE(retry_stream_admission_controller_->isRetryAdmitted(2, 3, false));
  ASSERT_EQ(cb_stats_.remaining_retries_.value(), 1);
  ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 0);
}

TEST_F(StaticLimitsConfigTest, FactoryConfigured) {
  EXPECT_NE(nullptr, factory_);
  config_.mutable_max_concurrent_retries()->set_value(1);
  createAdmissionController();

  retry_stream_admission_controller_ =
      admission_controller_->createStreamAdmissionController(request_stream_info_);

  // only 1 retry allowed
  retry_stream_admission_controller_->onTryStarted(1);
  ASSERT_TRUE(retry_stream_admission_controller_->isRetryAdmitted(1, 2, false));
  retry_stream_admission_controller_->onTryStarted(2);
  ASSERT_FALSE(retry_stream_admission_controller_->isRetryAdmitted(2, 3, false));
}

TEST_F(StaticLimitsConfigTest, EmptyConfig) {
  ProtobufTypes::MessagePtr config = factory_->createEmptyConfigProto();
  EXPECT_TRUE(dynamic_cast<
              envoy::extensions::retry::admission_control::static_limits::v3::StaticLimitsConfig*>(
      config.get()));
}

} // namespace
} // namespace AdmissionControl
} // namespace Retry
} // namespace Extensions
} // namespace Envoy
