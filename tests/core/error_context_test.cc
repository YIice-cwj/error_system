#include "error_system/config/error_config.h"
#include "error_system/core/error_context.h"
#include "error_system/core/error_registry.h"
#include "error_system/plugin/plugin_registry.h"
#include <gtest/gtest.h>
#include <string_view>

namespace error_system::core {
    namespace {
        error_code_t make_code(uint16_t number) noexcept {
            return error_code_t(error_level_t::error, domain::system_domain_t::database, 9, 9, number);
        }
    }  // namespace

    class error_context_test : public ::testing::Test {
        protected:
        void SetUp() override {
            auto& registry = error_registry_t::instance();
            registry.unregister_all();
            registry.set_duplicate_policy(duplicate_policy_t::skip);
            registry.set_duplicate_warn_callback(nullptr);

            plugin::plugin_registry_t::instance().clear();
            config::error_config_t::set_custom_formatter(nullptr);
            config::error_config_t::set_enable_text_output(true);
#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
            config::error_config_t::set_enable_stacktrace(false);
#endif
#ifdef ERROR_SYSTEM_ENABLE_VALIDATION
            config::error_config_t::set_enable_validation(true);
#endif
#ifdef ERROR_SYSTEM_ENABLE_LOCATION
            config::error_config_t::set_enable_source_location(true);
            config::error_config_t::set_enable_short_filename(false);
#endif

            registered_code_ = make_code(1);
            registry.register_subsystem_module(registered_code_.get_subsys(), registered_code_.get_module(), "perf", "context");
            registry.register_error(registered_code_, "ERR_CTX_REGISTERED", "Registered error context");
        }

        void TearDown() override {
            plugin::plugin_registry_t::instance().clear();
            error_registry_t::instance().unregister_all();
            config::error_config_t::set_custom_formatter(nullptr);
            config::error_config_t::set_enable_text_output(true);
#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
            config::error_config_t::set_enable_stacktrace(true);
#endif
#ifdef ERROR_SYSTEM_ENABLE_VALIDATION
            config::error_config_t::set_enable_validation(true);
#endif
#ifdef ERROR_SYSTEM_ENABLE_LOCATION
            config::error_config_t::set_enable_source_location(true);
            config::error_config_t::set_enable_short_filename(true);
#endif
        }

        error_code_t registered_code_{};
    };

    TEST_F(error_context_test, constructor_with_registered_code_keeps_code_and_message) {
        error_context_t context(registered_code_, "hello {}", "world");

        EXPECT_EQ(context.get_code().get_code(), registered_code_.get_code());
        EXPECT_EQ(context.message, "hello world");
        EXPECT_TRUE(context.get_payload().empty());
    }

    TEST_F(error_context_test, constructor_with_unregistered_code_marks_context_invalid) {
        const auto unregistered_code = make_code(99);
        error_context_t context(unregistered_code, "boom");

        EXPECT_EQ(context.get_code().get_level(), error_level_t::fatal);
        EXPECT_NE(context.message.find("[UNREGISTERED CODE]"), std::string::npos);

        const auto& payload = context.get_payload();
        auto find_val = [&](const std::string& key) -> const std::string& {
            static const std::string empty;
            for (const auto& [k, v] : payload) {
                if (k == key) return v;
            }
            return empty;
        };
        EXPECT_EQ(find_val("illegal_raw_code"), std::to_string(unregistered_code.get_code()));
    }

    TEST_F(error_context_test, with_string_view_inserts_payload_without_extra_overload_work) {
        error_context_t context(registered_code_, "payload test");
        context.with(std::string_view("user"), std::string_view("alice"));

        const auto& payload = context.get_payload();
        auto find_val = [&](const std::string& key) -> const std::string& {
            static const std::string empty;
            for (const auto& [k, v] : payload) {
                if (k == key) return v;
            }
            return empty;
        };
        EXPECT_EQ(find_val("user"), "alice");
    }

    TEST_F(error_context_test, with_multiple_types_inserts_payload) {
        error_context_t context(registered_code_, "multi type test");
        context.with("int_val", 42)
               .with("bool_val", true)
               .with("double_val", 3.14);

        const auto& payload = context.get_payload();
        auto find_val = [&](const std::string& key) -> const std::string& {
            static const std::string empty;
            for (const auto& [k, v] : payload) {
                if (k == key) return v;
            }
            return empty;
        };
        EXPECT_EQ(find_val("int_val"), "42");
        EXPECT_EQ(find_val("bool_val"), "true");
        EXPECT_EQ(find_val("double_val"), std::to_string(3.14));
    }

    TEST_F(error_context_test, serialization_contains_registered_metadata_and_payload) {
        error_context_t context(registered_code_, "serialize me");
        context.with("user", "alice");

        const std::string text = context.to_string();
        const std::string json = context.to_json();

        EXPECT_NE(text.find("ERR_CTX_REGISTERED"), std::string::npos);
        EXPECT_NE(text.find("Registered error context"), std::string::npos);
        EXPECT_NE(json.find("\"message\":\"serialize me\""), std::string::npos);
        EXPECT_NE(json.find("\"user\":\"alice\""), std::string::npos);
    }

    // ========== to_binary() 因果链测试 ==========

    TEST_F(error_context_test, to_binary_includes_cause_chain) {
        error_context_t cause_context(registered_code_, "root cause");
        error_context_t context(registered_code_, "wrapper error");
        auto wrapped = context.wrap(cause_context);

        std::string binary_with_cause = wrapped.to_binary();

        // 不带 cause 的二进制应小于带 cause 的
        error_context_t ctx_no_cause(registered_code_, "wrapper error");
        std::string binary_no_cause = ctx_no_cause.to_binary();

        EXPECT_GT(binary_with_cause.size(), binary_no_cause.size());
    }

    TEST_F(error_context_test, to_binary_without_cause_has_zero_flag) {
        error_context_t context(registered_code_, "no cause");

        std::string binary = context.to_binary();
        EXPECT_GT(binary.size(), 0);

        // 无 cause 时最后 1 字节为 has_cause 标志，值为 0
        EXPECT_EQ(static_cast<uint8_t>(binary.back()), 0x00);
    }

    // ========== with_batch() 批量添加 payload ==========

    TEST_F(error_context_test, with_batch_adds_multiple_payload_items) {
        error_context_t context(registered_code_, "batch test");
        context.with_batch({
            {"host", "192.168.1.1"},
            {"port", "3306"},
            {"db", "users"}
        });

        const auto& payload = context.get_payload();
        auto find_val = [&](const std::string& key) -> const std::string& {
            static const std::string empty;
            for (const auto& [k, v] : payload) {
                if (k == key) return v;
            }
            return empty;
        };
        EXPECT_EQ(find_val("host"), "192.168.1.1");
        EXPECT_EQ(find_val("port"), "3306");
        EXPECT_EQ(find_val("db"), "users");
        EXPECT_EQ(payload.size(), 3);
    }

    // ========== is_success() / is_error() ==========

    TEST_F(error_context_test, is_error_for_registered_error_code) {
        error_context_t context(registered_code_, "error status");
        EXPECT_TRUE(context.is_error());
        EXPECT_FALSE(context.is_success());
    }

    TEST_F(error_context_test, is_success_for_success_code) {
        error_context_t context(error_code_t::make_success(), "success status");
        EXPECT_TRUE(context.is_success());
        EXPECT_FALSE(context.is_error());
    }

    // ========== payload overflow（SSO → unordered_map） ==========

    TEST_F(error_context_test, payload_overflow_switches_to_unordered_map) {
        error_context_t context(registered_code_, "overflow test");
        context.with("k1", "v1")
               .with("k2", "v2")
               .with("k3", "v3")
               .with("k4", "v4")
               .with("k5", "v5");

        const auto& payload = context.get_payload();
        EXPECT_EQ(payload.size(), 5);

        auto find_val = [&](const std::string& key) -> const std::string& {
            static const std::string empty;
            for (const auto& [k, v] : payload) {
                if (k == key) return v;
            }
            return empty;
        };
        EXPECT_EQ(find_val("k1"), "v1");
        EXPECT_EQ(find_val("k3"), "v3");
        EXPECT_EQ(find_val("k5"), "v5");
    }

    // ========== operator== / operator!= ==========

    TEST_F(error_context_test, equality_same_code_and_message) {
        error_context_t ctx_a{registered_code_, "same message"};
        error_context_t ctx_b{registered_code_, "same message"};

        EXPECT_TRUE(ctx_a == ctx_b);
        EXPECT_FALSE(ctx_a != ctx_b);
    }

    TEST_F(error_context_test, inequality_different_message) {
        error_context_t ctx_a{registered_code_, "message A"};
        error_context_t ctx_b{registered_code_, "message B"};

        EXPECT_FALSE(ctx_a == ctx_b);
        EXPECT_TRUE(ctx_a != ctx_b);
    }

    TEST_F(error_context_test, equality_considers_payload) {
        error_context_t ctx_a{registered_code_, "msg"};
        ctx_a.with("key", "val");

        error_context_t ctx_b{registered_code_, "msg"};
        ctx_b.with("key", "val");

        error_context_t ctx_c{registered_code_, "msg"};
        ctx_c.with("key", "different");

        EXPECT_TRUE(ctx_a == ctx_b);
        EXPECT_FALSE(ctx_a == ctx_c);
    }

    // ========== deep cause chain（depth > 1） ==========

    TEST_F(error_context_test, deep_cause_chain_multiple_levels) {
        error_context_t root{registered_code_, "root cause"};
        error_context_t middle{registered_code_, "middle error"};
        error_context_t top{registered_code_, "top error"};

        auto wrapped = top.wrap(middle.wrap(root));

        // top 层
        EXPECT_EQ(wrapped.message, "top error");
        ASSERT_NE(wrapped.cause, nullptr);

        // middle 层
        EXPECT_EQ(wrapped.cause->message, "middle error");
        ASSERT_NE(wrapped.cause->cause, nullptr);

        // root 层
        EXPECT_EQ(wrapped.cause->cause->message, "root cause");
        EXPECT_EQ(wrapped.cause->cause->cause, nullptr);
    }

// ========== get_payload_value() 测试 ==========

    TEST_F(error_context_test, get_payload_value_returns_existing_key) {
        error_context_t ctx{registered_code_, "test"};
        ctx.with("key1", "value1");
        auto val = ctx.get_payload_value("key1");
        ASSERT_TRUE(val.has_value());
        EXPECT_EQ(*val, "value1");
    }

    TEST_F(error_context_test, get_payload_value_returns_nullopt_for_missing_key) {
        error_context_t ctx{registered_code_, "test"};
        auto val = ctx.get_payload_value("nonexistent");
        EXPECT_FALSE(val.has_value());
    }

    // ========== with() const char* key 测试 ==========

    TEST_F(error_context_test, with_const_char_key_works) {
        error_context_t ctx{registered_code_, "test"};
        ctx.with("c_string_key", 42);
        auto val = ctx.get_payload_value("c_string_key");
        ASSERT_TRUE(val.has_value());
        EXPECT_EQ(*val, "42");
    }

    // ========== from_exception() 测试 ==========

    TEST_F(error_context_test, from_exception_creates_context_with_what_message) {
        std::runtime_error ex("something went wrong");
        auto ctx = error_context_t::from_exception(registered_code_, ex);
        EXPECT_EQ(ctx.message, "something went wrong");
        EXPECT_EQ(ctx.get_code().get_code(), registered_code_.get_code());
    }

}  // namespace error_system::core