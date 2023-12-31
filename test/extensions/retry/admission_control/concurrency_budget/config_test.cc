#include <cstdint>
#include <memory>

#include "envoy/extensions/retry/admission_control/concurrency_budget/v3/concurrency_budget_config.pb.h"
#include "envoy/registry/registry.h"
#include "envoy/upstream/admission_control.h"

#include "source/extensions/retry/admission_control/concurrency_budget/config.h"

#include "test/mocks/runtime/mocks.h"
#include "test/mocks/stream_info/mocks.h"
#include "test/test_common/test_runtime.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <vector>

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

class ConcurrencyBudgetConfigTest : public testing::Test {
public:
  ConcurrencyBudgetConfigTest() : cb_stats_(clusterCircuitBreakersStats(store_)) {
    Upstream::RetryAdmissionControllerFactory* factory =
        Registry::FactoryRegistry<Upstream::RetryAdmissionControllerFactory>::getFactory(
            "envoy.retry_admission_control.concurrency_budget");
    EXPECT_NE(nullptr, factory);
    ConcurrencyBudgetFactory* concurrency_budget_factory = dynamic_cast<ConcurrencyBudgetFactory*>(factory);
    factory_ = std::make_unique<ConcurrencyBudgetFactory>(*concurrency_budget_factory);
    ON_CALL(runtime_.snapshot_, getInteger("test_prefix.retry_budget.min_retry_concurrency", _))
        .WillByDefault([](std::basic_string_view<char>, uint64_t default_value) -> uint64_t {
          return default_value;
        });
    ON_CALL(runtime_.snapshot_, getDouble("test_prefix.retry_budget.budget_percent", _))
        .WillByDefault([](std::basic_string_view<char>, double default_value) -> uint64_t {
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
  envoy::extensions::retry::admission_control::concurrency_budget::v3::ConcurrencyBudgetConfig config_;
  NiceMock<StreamInfo::MockStreamInfo> request_stream_info_;
  Upstream::RetryAdmissionControllerSharedPtr admission_controller_;
  Upstream::RetryStreamAdmissionControllerPtr retry_stream_admission_controller_;
};

TEST_F(ConcurrencyBudgetConfigTest, FactoryDefault) {
  // default config
  createAdmissionController();

  retry_stream_admission_controller_ =
      admission_controller_->createStreamAdmissionController(request_stream_info_);
  ASSERT_EQ(cb_stats_.remaining_retries_.value(), 3);
  ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 0);

  // by default, 3 retries are allowed
  retry_stream_admission_controller_->onTryStarted(1);
  ASSERT_TRUE(retry_stream_admission_controller_->isRetryAdmitted(1, 2, false));
  retry_stream_admission_controller_->onTryStarted(2);
  ASSERT_TRUE(retry_stream_admission_controller_->isRetryAdmitted(2, 3, false));
  retry_stream_admission_controller_->onTryStarted(3);
  ASSERT_TRUE(retry_stream_admission_controller_->isRetryAdmitted(3, 4, false));
  retry_stream_admission_controller_->onTryStarted(4);
  ASSERT_FALSE(retry_stream_admission_controller_->isRetryAdmitted(4, 5, false));
  ASSERT_EQ(cb_stats_.remaining_retries_.value(), 0);
  ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 1);
  // 4 active tries and 3 active retries

  std::vector<Upstream::RetryStreamAdmissionControllerPtr> extra_streams;
  extra_streams.reserve(16);
  for (int i = 0; i < 16; i++) {
    extra_streams.push_back(
        admission_controller_->createStreamAdmissionController(request_stream_info_));
    extra_streams.back()->onTryStarted(1);
  }
  // 20 active tries and 3 active retries => 20 * 20% = 4 => 4 retries allowed
  ASSERT_EQ(cb_stats_.remaining_retries_.value(), 1);
  ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 0);
  ASSERT_TRUE(extra_streams.front()->isRetryAdmitted(1, 2, false));
  extra_streams.front()->onTryStarted(2);
  ASSERT_EQ(cb_stats_.remaining_retries_.value(), 0);
  ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 1);
  ASSERT_FALSE(extra_streams.front()->isRetryAdmitted(2, 3, false));
}

TEST_F(ConcurrencyBudgetConfigTest, FactoryRuntimeOverrides) {
  // default config
  createAdmissionController();

  auto stream1 =
      admission_controller_->createStreamAdmissionController(request_stream_info_);

  // by default, 3 retries are allowed
  stream1->onTryStarted(1);
  ASSERT_TRUE(stream1->isRetryAdmitted(1, 2, false));
  stream1->onTryStarted(2);
  ASSERT_TRUE(stream1->isRetryAdmitted(2, 3, false));
  stream1->onTryStarted(3);
  ASSERT_TRUE(stream1->isRetryAdmitted(3, 4, false));
  stream1->onTryStarted(4);
  ASSERT_FALSE(stream1->isRetryAdmitted(4, 5, false));
  ASSERT_EQ(cb_stats_.remaining_retries_.value(), 0);
  ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 1);
  // 4 active tries and 3 active retries

  // override the min_retry_concurrency_limit via runtime
  EXPECT_CALL(runtime_.snapshot_, getInteger("test_prefix.retry_budget.min_retry_concurrency", _))
      .WillRepeatedly(Return(4U));
  auto stream2 =
      admission_controller_->createStreamAdmissionController(request_stream_info_);
  stream2->onTryStarted(1);
  ASSERT_TRUE(stream2->isRetryAdmitted(1, 2, false));
  stream2->onTryStarted(2);
  ASSERT_FALSE(stream2->isRetryAdmitted(2, 3, false));
  // 6 active tries and 4 active retries

  // override the budget_percent via runtime
  EXPECT_CALL(runtime_.snapshot_, getDouble("test_prefix.retry_budget.budget_percent", _))
      .WillRepeatedly(Return(72.0));
  auto stream3 =
      admission_controller_->createStreamAdmissionController(request_stream_info_);
  stream3->onTryStarted(1); // 7T, 4R
  ASSERT_TRUE(stream3->isRetryAdmitted(1, 2, true)); // 7T, 5R
  stream3->onTryStarted(2);
  ASSERT_FALSE(stream3->isRetryAdmitted(2, 3, false)); // 6 / 8 = 75% > 72%
}

// TEST_F(ConcurrencyBudgetConfigTest, StatsGuardedByRuntimeFeature) {
//   // default config
//   createAdmissionController();

//   retry_stream_admission_controller_ =
//       admission_controller_->createStreamAdmissionController(request_stream_info_);
//   ASSERT_EQ(cb_stats_.remaining_retries_.value(), 3);
//   ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 0);
//   retry_stream_admission_controller_->onTryStarted(1);
//   ASSERT_TRUE(retry_stream_admission_controller_->isRetryAdmitted(1, 2, true));
//   ASSERT_EQ(cb_stats_.remaining_retries_.value(), 2);
//   ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 0);

//   scoped_runtime_.mergeValues({{"envoy.reloadable_features.use_retry_admission_control", "false"}});

//   cb_stats_.remaining_retries_.set(42);
//   cb_stats_.rq_retry_open_.set(42);

//   retry_stream_admission_controller_->onTryStarted(2);
//   ASSERT_TRUE(retry_stream_admission_controller_->isRetryAdmitted(2, 3, false));
//   ASSERT_EQ(cb_stats_.remaining_retries_.value(), 42);
//   ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 42);
//   retry_stream_admission_controller_->onTryStarted(3);
//   ASSERT_TRUE(retry_stream_admission_controller_->isRetryAdmitted(3, 4, false));
//   retry_stream_admission_controller_->onTryStarted(4);
//   ASSERT_FALSE(retry_stream_admission_controller_->isRetryAdmitted(4, 5, false));
//   ASSERT_EQ(cb_stats_.remaining_retries_.value(), 42);
//   ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 42);
//   retry_stream_admission_controller_.reset();
//   ASSERT_EQ(cb_stats_.remaining_retries_.value(), 42);
//   ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 42);

//   retry_stream_admission_controller_ =
//       admission_controller_->createStreamAdmissionController(request_stream_info_);
//   retry_stream_admission_controller_->onTryStarted(1);
//   ASSERT_TRUE(retry_stream_admission_controller_->isRetryAdmitted(1, 2, false));
//   ASSERT_EQ(cb_stats_.remaining_retries_.value(), 42);
//   ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 42);

//   scoped_runtime_.mergeValues({{"envoy.reloadable_features.use_retry_admission_control", "true"}});

//   retry_stream_admission_controller_->onTryStarted(2);
//   ASSERT_TRUE(retry_stream_admission_controller_->isRetryAdmitted(2, 3, false));
//   ASSERT_EQ(cb_stats_.remaining_retries_.value(), 1);
//   ASSERT_EQ(cb_stats_.rq_retry_open_.value(), 0);
// }

// TEST_F(ConcurrencyBudgetConfigTest, FactoryConfigured) {
//   EXPECT_NE(nullptr, factory_);
//   config_.mutable_max_concurrent_retries()->set_value(1);
//   createAdmissionController();

//   retry_stream_admission_controller_ =
//       admission_controller_->createStreamAdmissionController(request_stream_info_);

//   // only 1 retry allowed
//   retry_stream_admission_controller_->onTryStarted(1);
//   ASSERT_TRUE(retry_stream_admission_controller_->isRetryAdmitted(1, 2, false));
//   retry_stream_admission_controller_->onTryStarted(2);
//   ASSERT_FALSE(retry_stream_admission_controller_->isRetryAdmitted(2, 3, false));
// }

TEST_F(ConcurrencyBudgetConfigTest, EmptyConfig) {
  ProtobufTypes::MessagePtr config = factory_->createEmptyConfigProto();
  EXPECT_TRUE(dynamic_cast<
              envoy::extensions::retry::admission_control::concurrency_budget::v3::ConcurrencyBudgetConfig*>(
      config.get()));
}

} // namespace
} // namespace AdmissionControl
} // namespace Retry
} // namespace Extensions
} // namespace Envoy
