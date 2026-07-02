#include "error_system/plugin/error_dedup_sampler.h"

#include <thread>

#include <gtest/gtest.h>

#include "error_system/core/error_code.h"
#include "error_system/core/error_context.h"
#include "error_system/core/error_level.h"
#include "error_system/core/error_registry.h"
#include "error_system/domain/system_domain.h"
#include "error_system/i18n/subsystem_module_catalog.h"

namespace error_system::plugin {

    using error_system::core::error_code_t;

    namespace {
        /** 构造已注册的错误码（避免 initializer 将未注册码替换为统一 fatal 码） */
        core::error_code_t make_registered_code(uint16_t number) noexcept {
            return error_code_t(core::error_level_t::error, domain::system_domain_t::application, core::subsystem_id_t{1}, core::module_id_t{1}, core::error_number_t{number});
        }

        core::error_context_t make_ctx(uint16_t number) noexcept {
            return core::error_context_t(core::located_code_t{make_registered_code(number)}, "test error");
        }
    }  // namespace

    class error_dedup_sampler_test_t : public ::testing::Test {
    protected:
        void SetUp() override {
            auto& registry = core::error_registry_t::instance();
            registry.unregister_all();
            registry.set_duplicate_policy(core::duplicate_policy_t::skip);
            registry.set_duplicate_warn_callback(nullptr);

            auto& catalog = error_system::i18n::subsystem_module_catalog_t::instance();
            catalog.clear();
            catalog.register_subsystem_module(1, 1, "test", "sampler");
            for (uint16_t n = 1; n <= 20; ++n) {
                registry.register_error(make_registered_code(n),
                                        "ERR_TEST_" + std::to_string(n),
                                        "test error " + std::to_string(n));
            }

            sampler_.reset_stats();
            sampler_.clear_dedup_cache();
            sampler_.set_dedup_window_ms(0);
            sampler_.set_sample_rate(1.0);
        }

        void TearDown() override {
            core::error_registry_t::instance().unregister_all();
            error_system::i18n::subsystem_module_catalog_t::instance().clear();
        }

        error_dedup_sampler_t sampler_;
    };


    TEST_F(error_dedup_sampler_test_t, default_forwards_all) {
        EXPECT_TRUE(sampler_.should_be_forwarded(make_ctx(1)));
        EXPECT_TRUE(sampler_.should_be_forwarded(make_ctx(1)));
        EXPECT_TRUE(sampler_.should_be_forwarded(make_ctx(2)));
        EXPECT_EQ(sampler_.forwarded_count(), 3u);
        EXPECT_EQ(sampler_.deduped_count(), 0u);
        EXPECT_EQ(sampler_.sampled_count(), 0u);
    }


    TEST_F(error_dedup_sampler_test_t, dedup_suppresses_same_code_within_window) {
        sampler_.set_dedup_window_ms(1000);
        auto ctx = make_ctx(1);

        EXPECT_TRUE(sampler_.should_be_forwarded(ctx));
        EXPECT_FALSE(sampler_.should_be_forwarded(ctx));
        EXPECT_FALSE(sampler_.should_be_forwarded(ctx));
        EXPECT_EQ(sampler_.forwarded_count(), 1u);
        EXPECT_EQ(sampler_.deduped_count(), 2u);
    }

    TEST_F(error_dedup_sampler_test_t, dedup_different_codes_not_suppressed) {
        sampler_.set_dedup_window_ms(1000);
        EXPECT_TRUE(sampler_.should_be_forwarded(make_ctx(1)));
        EXPECT_TRUE(sampler_.should_be_forwarded(make_ctx(2)));
        EXPECT_TRUE(sampler_.should_be_forwarded(make_ctx(3)));
        EXPECT_EQ(sampler_.forwarded_count(), 3u);
        EXPECT_EQ(sampler_.deduped_count(), 0u);
    }

    TEST_F(error_dedup_sampler_test_t, dedup_window_zero_disables) {
        sampler_.set_dedup_window_ms(0);
        auto ctx = make_ctx(1);
        EXPECT_TRUE(sampler_.should_be_forwarded(ctx));
        EXPECT_TRUE(sampler_.should_be_forwarded(ctx));
        EXPECT_EQ(sampler_.deduped_count(), 0u);
    }

    TEST_F(error_dedup_sampler_test_t, dedup_identity_code_ignores_sign_and_reserved) {
        sampler_.set_dedup_window_ms(1000);
        core::error_code_t code1(core::error_level_t::error,
                                  domain::system_domain_t::application,
                                  core::subsystem_id_t{1},
                                  core::module_id_t{1},
                                  core::error_number_t{1});
        core::error_code_t code2 = code1;
        code2.set_retryable(true);
        core::error_context_t ctx1{core::located_code_t{code1}, "a"};
        core::error_context_t ctx2{core::located_code_t{code2}, "b"};

        EXPECT_TRUE(sampler_.should_be_forwarded(ctx1));
        EXPECT_FALSE(sampler_.should_be_forwarded(ctx2));
        EXPECT_EQ(sampler_.deduped_count(), 1u);
    }


    TEST_F(error_dedup_sampler_test_t, sample_rate_half_forwards_half) {
        sampler_.set_sample_rate(0.5);
        for (int i = 0; i < 10; ++i) {
            [[maybe_unused]] const bool ok = sampler_.should_be_forwarded(make_ctx(static_cast<uint16_t>(i)));
        }
        EXPECT_EQ(sampler_.forwarded_count(), 5u);
        EXPECT_EQ(sampler_.sampled_count(), 5u);
    }

    TEST_F(error_dedup_sampler_test_t, sample_rate_tenth_forwards_one_in_ten) {
        sampler_.set_sample_rate(0.1);
        for (int i = 0; i < 100; ++i) {
            [[maybe_unused]] const bool ok = sampler_.should_be_forwarded(make_ctx(static_cast<uint16_t>(i)));
        }
        EXPECT_EQ(sampler_.forwarded_count(), 10u);
        EXPECT_EQ(sampler_.sampled_count(), 90u);
    }

    TEST_F(error_dedup_sampler_test_t, sample_rate_full_forwards_all) {
        sampler_.set_sample_rate(1.0);
        for (int i = 0; i < 5; ++i) {
            EXPECT_TRUE(sampler_.should_be_forwarded(make_ctx(static_cast<uint16_t>(i))));
        }
        EXPECT_EQ(sampler_.sampled_count(), 0u);
    }

    TEST_F(error_dedup_sampler_test_t, sample_rate_zero_suppresses_all) {
        sampler_.set_sample_rate(0.0);
        for (int i = 0; i < 5; ++i) {
            EXPECT_FALSE(sampler_.should_be_forwarded(make_ctx(static_cast<uint16_t>(i))));
        }
        EXPECT_EQ(sampler_.forwarded_count(), 0u);
        EXPECT_EQ(sampler_.sampled_count(), 5u);
    }


    TEST_F(error_dedup_sampler_test_t, sample_then_dedup_combines) {
        sampler_.set_sample_rate(0.5);
        sampler_.set_dedup_window_ms(1000);

        EXPECT_TRUE(sampler_.should_be_forwarded(make_ctx(1)));
        EXPECT_FALSE(sampler_.should_be_forwarded(make_ctx(2)));
        EXPECT_FALSE(sampler_.should_be_forwarded(make_ctx(1)));
        EXPECT_FALSE(sampler_.should_be_forwarded(make_ctx(3)));
        EXPECT_TRUE(sampler_.should_be_forwarded(make_ctx(2)));

        EXPECT_EQ(sampler_.forwarded_count(), 2u);
        EXPECT_EQ(sampler_.sampled_count(), 2u);
        EXPECT_EQ(sampler_.deduped_count(), 1u);
    }


    TEST_F(error_dedup_sampler_test_t, reset_stats_clears_counters) {
        sampler_.set_dedup_window_ms(1000);
        [[maybe_unused]] const bool r1 = sampler_.should_be_forwarded(make_ctx(1));
        [[maybe_unused]] const bool r2 = sampler_.should_be_forwarded(make_ctx(1));

        EXPECT_EQ(sampler_.forwarded_count(), 1u);
        EXPECT_EQ(sampler_.deduped_count(), 1u);

        sampler_.reset_stats();
        EXPECT_EQ(sampler_.forwarded_count(), 0u);
        EXPECT_EQ(sampler_.deduped_count(), 0u);
        EXPECT_EQ(sampler_.sampled_count(), 0u);
    }

    TEST_F(error_dedup_sampler_test_t, clear_dedup_cache_empties_map) {
        sampler_.set_dedup_window_ms(1000);
        [[maybe_unused]] const bool r1 = sampler_.should_be_forwarded(make_ctx(1));
        [[maybe_unused]] const bool r2 = sampler_.should_be_forwarded(make_ctx(2));
        EXPECT_EQ(sampler_.dedup_cache_size(), 2u);

        sampler_.clear_dedup_cache();
        EXPECT_EQ(sampler_.dedup_cache_size(), 0u);
    }


    TEST_F(error_dedup_sampler_test_t, concurrent_access_is_safe) {
        sampler_.set_sample_rate(0.5);
        sampler_.set_dedup_window_ms(100);

        static constexpr int THREAD_COUNT = 4;
        static constexpr int ITERATIONS = 100;
        std::thread threads[THREAD_COUNT];
        for (auto& t : threads) {
            t = std::thread([this]() {
                for (int i = 0; i < ITERATIONS; ++i) {
                    [[maybe_unused]] const bool ok =
                        sampler_.should_be_forwarded(make_ctx(static_cast<uint16_t>(i % 10)));
                }
            });
        }
        for (auto& t : threads) {
            t.join();
        }

        const uint64_t total = sampler_.forwarded_count()
                             + sampler_.sampled_count()
                             + sampler_.deduped_count();
        EXPECT_EQ(total, static_cast<uint64_t>(THREAD_COUNT * ITERATIONS));
    }

}  // namespace error_system::plugin
