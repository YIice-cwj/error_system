#include "error_system/core/error_context.h"
#include "error_system/core/error_context_serializer.h"

#include <string_view>
#include <unordered_map>

#include <gtest/gtest.h>

#include "error_system/config/error_config.h"
#include "error_system/core/error_registry.h"
#include "error_system/plugin/plugin_registry.h"

namespace error_system::core {
    namespace {
        error_code_t make_code(uint16_t number) noexcept {
            return error_code_t(error_level_t::error, domain::system_domain_t::database, 9, 9, number);
        }
    }  // namespace

    class error_context_test_t : public ::testing::Test {
        protected:
        void SetUp() override {
            auto& registry = error_registry_t::instance();
            registry.unregister_all();
            registry.set_duplicate_policy(duplicate_policy_t::skip);
            registry.set_duplicate_warn_callback(nullptr);

            plugin::plugin_registry_t::instance().clear();
            config::formatter_config_t::set_custom_formatter(nullptr);
            config::feature_flags_t::set_enable_text_output(true);
#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
            config::feature_flags_t::set_enable_stacktrace(false);
#endif
#ifdef ERROR_SYSTEM_ENABLE_VALIDATION
            config::feature_flags_t::set_enable_validation(true);
#endif
#ifdef ERROR_SYSTEM_ENABLE_LOCATION
            config::feature_flags_t::set_enable_source_location(true);
            config::feature_flags_t::set_enable_short_filename(false);
#endif

            registered_code_ = make_code(1);
            registry.register_subsystem_module(registered_code_.get_subsys(), registered_code_.get_module(), "perf", "context");
            registry.register_error(registered_code_, "ERR_CTX_REGISTERED", "Registered error context");
        }

        void TearDown() override {
            plugin::plugin_registry_t::instance().clear();
            error_registry_t::instance().unregister_all();
            config::formatter_config_t::set_custom_formatter(nullptr);
            config::feature_flags_t::set_enable_text_output(true);
#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
            config::feature_flags_t::set_enable_stacktrace(true);
#endif
#ifdef ERROR_SYSTEM_ENABLE_VALIDATION
            config::feature_flags_t::set_enable_validation(true);
#endif
#ifdef ERROR_SYSTEM_ENABLE_LOCATION
            config::feature_flags_t::set_enable_source_location(true);
            config::feature_flags_t::set_enable_short_filename(true);
#endif
        }

        error_code_t registered_code_{};
    };

    TEST_F(error_context_test_t, constructor_with_registered_code_keeps_code_and_message) {
        error_context_t context(registered_code_, "hello {}", "world");

        EXPECT_EQ(context.get_code().get_code(), registered_code_.get_code());
        EXPECT_EQ(context.message, "hello world");
        EXPECT_TRUE(context.get_payload().empty());
    }

    TEST_F(error_context_test_t, constructor_with_unregistered_code_marks_context_invalid) {
        const auto unregistered_code = make_code(99);
        error_context_t context(unregistered_code, "boom");

        EXPECT_EQ(context.get_code().get_level(), error_level_t::fatal);
        EXPECT_NE(context.message.find("[UNREGISTERED CODE]"), std::string::npos);

        const auto& payload = context.get_payload();
        auto find_val = [&payload](const std::string& key) -> const std::string& {
            static const std::string empty;
            for (const auto& [k, v] : payload) {
                if (k == key) return v;
            }
            return empty;
        };
        EXPECT_EQ(find_val("illegal_raw_code"), std::to_string(unregistered_code.get_code()));
    }

    TEST_F(error_context_test_t, with_string_view_inserts_payload_without_extra_overload_work) {
        error_context_t context(registered_code_, "payload test");
        context.with(std::string_view("user"), std::string_view("alice"));

        const auto& payload = context.get_payload();
        auto find_val = [&payload](const std::string& key) -> const std::string& {
            static const std::string empty;
            for (const auto& [k, v] : payload) {
                if (k == key) return v;
            }
            return empty;
        };
        EXPECT_EQ(find_val("user"), "alice");
    }

    TEST_F(error_context_test_t, with_multiple_types_inserts_payload) {
        error_context_t context(registered_code_, "multi type test");
        context.with("int_val", 42)
               .with("bool_val", true)
               .with("double_val", 3.14);

        const auto& payload = context.get_payload();
        auto find_val = [&payload](const std::string& key) -> const std::string& {
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

    TEST_F(error_context_test_t, serialization_contains_registered_metadata_and_payload) {
        error_context_t context(registered_code_, "serialize me");
        context.with("user", "alice");

        const std::string text = error_context_serializer_t::to_string(context);
        const std::string json = error_context_serializer_t::to_json(context);

        EXPECT_NE(text.find("ERR_CTX_REGISTERED"), std::string::npos);
        EXPECT_NE(text.find("Registered error context"), std::string::npos);
        EXPECT_NE(json.find("\"message\":\"serialize me\""), std::string::npos);
        EXPECT_NE(json.find("\"user\":\"alice\""), std::string::npos);
    }

    // ========== to_binary() 因果链测试 ==========

    TEST_F(error_context_test_t, to_binary_includes_cause_chain) {
        error_context_t cause_context(registered_code_, "root cause");
        error_context_t context(registered_code_, "wrapper error");
        auto wrapped = context.wrap(cause_context);

        std::string binary_with_cause = error_context_serializer_t::to_binary(wrapped);

        // 不带 cause 的二进制应小于带 cause 的
        error_context_t ctx_no_cause(registered_code_, "wrapper error");
        std::string binary_no_cause = error_context_serializer_t::to_binary(ctx_no_cause);

        EXPECT_GT(binary_with_cause.size(), binary_no_cause.size());
    }

    TEST_F(error_context_test_t, to_binary_without_cause_has_zero_flag) {
        error_context_t context(registered_code_, "no cause");

        std::string binary = error_context_serializer_t::to_binary(context);
        EXPECT_GT(binary.size(), 0);

        // 无 cause 时最后 1 字节为 has_cause 标志，值为 0
        EXPECT_EQ(static_cast<uint8_t>(binary.back()), 0x00);
    }

    // ========== with_batch() 批量添加 payload ==========

    TEST_F(error_context_test_t, with_batch_adds_multiple_payload_items) {
        error_context_t context(registered_code_, "batch test");
        context.with_batch({
            {"host", "192.168.1.1"},
            {"port", "3306"},
            {"db", "users"}
        });

        const auto& payload = context.get_payload();
        auto find_val = [&payload](const std::string& key) -> const std::string& {
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

    TEST_F(error_context_test_t, is_error_for_registered_error_code) {
        error_context_t context(registered_code_, "error status");
        EXPECT_TRUE(context.is_error());
        EXPECT_FALSE(context.is_success());
    }

    TEST_F(error_context_test_t, is_success_for_success_code) {
        error_context_t context(error_code_t::make_success(), "success status");
        EXPECT_TRUE(context.is_success());
        EXPECT_FALSE(context.is_error());
    }

    // ========== payload overflow（SSO → unordered_map） ==========

    TEST_F(error_context_test_t, payload_overflow_switches_to_unordered_map) {
        error_context_t context(registered_code_, "overflow test");
        context.with("k1", "v1")
               .with("k2", "v2")
               .with("k3", "v3")
               .with("k4", "v4")
               .with("k5", "v5");

        const auto& payload = context.get_payload();
        EXPECT_EQ(payload.size(), 5);

        auto find_val = [&payload](const std::string& key) -> const std::string& {
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

    TEST_F(error_context_test_t, equality_same_code_and_message) {
        error_context_t ctx_a{registered_code_, "same message"};
        error_context_t ctx_b{registered_code_, "same message"};

        EXPECT_TRUE(ctx_a == ctx_b);
        EXPECT_FALSE(ctx_a != ctx_b);
    }

    TEST_F(error_context_test_t, inequality_different_message) {
        error_context_t ctx_a{registered_code_, "message A"};
        error_context_t ctx_b{registered_code_, "message B"};

        EXPECT_FALSE(ctx_a == ctx_b);
        EXPECT_TRUE(ctx_a != ctx_b);
    }

    TEST_F(error_context_test_t, equality_considers_payload) {
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

    TEST_F(error_context_test_t, deep_cause_chain_multiple_levels) {
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

    TEST_F(error_context_test_t, get_payload_value_returns_existing_key) {
        error_context_t ctx{registered_code_, "test"};
        ctx.with("key1", "value1");
        auto val = ctx.get_payload_value("key1");
        ASSERT_TRUE(val.has_value());
        EXPECT_EQ(*val, "value1");
    }

    TEST_F(error_context_test_t, get_payload_value_returns_nullopt_for_missing_key) {
        error_context_t ctx{registered_code_, "test"};
        auto val = ctx.get_payload_value("nonexistent");
        EXPECT_FALSE(val.has_value());
    }

    // ========== with() const char* key 测试 ==========

    TEST_F(error_context_test_t, with_const_char_key_works) {
        error_context_t ctx{registered_code_, "test"};
        ctx.with("c_string_key", 42);
        auto val = ctx.get_payload_value("c_string_key");
        ASSERT_TRUE(val.has_value());
        EXPECT_EQ(*val, "42");
    }

    // ========== from_exception() 测试 ==========

    TEST_F(error_context_test_t, from_exception_creates_context_with_what_message) {
        std::runtime_error ex("something went wrong");
        auto ctx = error_context_t::from_exception(registered_code_, ex);
        EXPECT_EQ(ctx.message, "something went wrong");
        EXPECT_EQ(ctx.get_code().get_code(), registered_code_.get_code());
    }

    // ========== payload_size() / is_payload_empty() 测试 ==========

    TEST_F(error_context_test_t, payload_size_and_is_payload_empty_reflect_state) {
        error_context_t ctx{registered_code_, "size test"};
        // 初始状态：payload 为空
        EXPECT_TRUE(ctx.is_payload_empty());
        EXPECT_EQ(ctx.payload_size(), 0u);

        // 添加 1 项
        ctx.with("k1", "v1");
        EXPECT_FALSE(ctx.is_payload_empty());
        EXPECT_EQ(ctx.payload_size(), 1u);

        // 添加至 SSO 上限（4 项）
        ctx.with("k2", "v2").with("k3", "v3").with("k4", "v4");
        EXPECT_EQ(ctx.payload_size(), 4u);
        EXPECT_FALSE(ctx.is_payload_empty());

        // 第 5 项触发溢出到 unordered_map
        ctx.with("k5", "v5");
        EXPECT_EQ(ctx.payload_size(), 5u);
        EXPECT_FALSE(ctx.is_payload_empty());
    }

    // ========== for_each_payload() 测试 ==========

    TEST_F(error_context_test_t, for_each_payload_visits_sso_and_overflow_items) {
        error_context_t ctx{registered_code_, "for_each test"};
        ctx.with("a", "1")
           .with("b", "2")
           .with("c", "3")
           .with("d", "4")
           .with("e", "5");  // 触发溢出

        std::unordered_map<std::string, std::string> collected;
        ctx.for_each_payload([&](const std::string& k, const std::string& v) {
            collected[k] = v;
        });

        EXPECT_EQ(collected.size(), 5u);
        EXPECT_EQ(collected["a"], "1");
        EXPECT_EQ(collected["c"], "3");
        EXPECT_EQ(collected["e"], "5");
    }

    TEST_F(error_context_test_t, for_each_payload_on_empty_context_does_nothing) {
        error_context_t ctx{registered_code_, "empty"};
        size_t count = 0;
        ctx.for_each_payload([&](const std::string&, const std::string&) { ++count; });
        EXPECT_EQ(count, 0u);
    }

    // ========== wrap() 移动语义测试 ==========

    TEST_F(error_context_test_t, wrap_move_takes_ownership_of_cause) {
        error_context_t cause{registered_code_, "original cause"};
        error_context_t wrapper{registered_code_, "wrapper"};

        // 移动包装：cause 应被转移，源对象 cause 状态不再可靠
        error_context_t wrapped = wrapper.wrap(std::move(cause));

        EXPECT_EQ(wrapped.message, "wrapper");
        ASSERT_NE(wrapped.cause, nullptr);
        EXPECT_EQ(wrapped.cause->message, "original cause");
        EXPECT_EQ(wrapped.cause->cause, nullptr);
    }

    // ========== to_json / to_string 因果链序列化测试 ==========

    TEST_F(error_context_test_t, to_json_includes_cause_chain_field) {
        error_context_t cause{registered_code_, "root failure"};
        cause.with("reason", "timeout");

        error_context_t wrapper{registered_code_, "wrapper error"};
        auto wrapped = wrapper.wrap(cause);

        const std::string json = error_context_serializer_t::to_json(wrapped);
        // JSON 中应包含 cause 字段，且 cause.message 为根因消息
        EXPECT_NE(json.find("\"cause\""), std::string::npos);
        EXPECT_NE(json.find("root failure"), std::string::npos);
    }

    TEST_F(error_context_test_t, to_string_includes_caused_by_marker) {
        error_context_t cause{registered_code_, "lower layer failed"};
        error_context_t wrapper{registered_code_, "upper layer failed"};
        auto wrapped = wrapper.wrap(cause);

        const std::string text = error_context_serializer_t::to_string(wrapped);
        // 文本输出应包含因果链标记 "Caused by"
        EXPECT_NE(text.find("Caused by"), std::string::npos);
        EXPECT_NE(text.find("lower layer failed"), std::string::npos);
        EXPECT_NE(text.find("upper layer failed"), std::string::npos);
    }

}  // namespace error_system::core