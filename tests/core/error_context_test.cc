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

        // 在 error_context 的 payload 中按 key 查找 value，找不到返回空串
        std::string find_payload_value(const error_context_t& context, const std::string& key) {
            std::string result;
            context.for_each_payload([&](const std::string& payload_key, const std::string& payload_value) {
                if (payload_key == key) {
                    result = payload_value;
                }
            });
            return result;
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

        EXPECT_EQ(find_payload_value(context, "illegal_raw_code"), std::to_string(unregistered_code.get_code()));
    }

    TEST_F(error_context_test_t, with_string_view_inserts_payload_without_extra_overload_work) {
        error_context_t context(registered_code_, "payload test");
        context.with(std::string_view("user"), std::string_view("alice"));

        EXPECT_EQ(find_payload_value(context, "user"), "alice");
    }

    TEST_F(error_context_test_t, with_multiple_types_inserts_payload) {
        error_context_t context(registered_code_, "multi type test");
        context.with("int_val", 42)
               .with("bool_val", true)
               .with("double_val", 3.14);

        EXPECT_EQ(find_payload_value(context, "int_val"), "42");
        EXPECT_EQ(find_payload_value(context, "bool_val"), "true");
        EXPECT_EQ(find_payload_value(context, "double_val"), std::to_string(3.14));
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


    TEST_F(error_context_test_t, with_batch_adds_multiple_payload_items) {
        error_context_t context(registered_code_, "batch test");
        context.with_batch({
            {"host", "192.168.1.1"},
            {"port", "3306"},
            {"db", "users"}
        });

        const auto& payload = context.get_payload();
        EXPECT_EQ(find_payload_value(context, "host"), "192.168.1.1");
        EXPECT_EQ(find_payload_value(context, "port"), "3306");
        EXPECT_EQ(find_payload_value(context, "db"), "users");
        EXPECT_EQ(payload.size(), 3);
    }


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


    TEST_F(error_context_test_t, payload_overflow_switches_to_unordered_map) {
        error_context_t context(registered_code_, "overflow test");
        context.with("k1", "v1")
               .with("k2", "v2")
               .with("k3", "v3")
               .with("k4", "v4")
               .with("k5", "v5");

        const auto& payload = context.get_payload();
        EXPECT_EQ(payload.size(), 5);

        EXPECT_EQ(find_payload_value(context, "k1"), "v1");
        EXPECT_EQ(find_payload_value(context, "k3"), "v3");
        EXPECT_EQ(find_payload_value(context, "k5"), "v5");
    }


    TEST_F(error_context_test_t, equality_same_code_and_message) {
        error_context_t context_a{registered_code_, "same message"};
        error_context_t context_b{registered_code_, "same message"};

        EXPECT_TRUE(context_a == context_b);
        EXPECT_FALSE(context_a != context_b);
    }

    TEST_F(error_context_test_t, inequality_different_message) {
        error_context_t context_a{registered_code_, "message A"};
        error_context_t context_b{registered_code_, "message B"};

        EXPECT_FALSE(context_a == context_b);
        EXPECT_TRUE(context_a != context_b);
    }

    TEST_F(error_context_test_t, equality_considers_payload) {
        error_context_t context_a{registered_code_, "msg"};
        context_a.with("key", "val");

        error_context_t context_b{registered_code_, "msg"};
        context_b.with("key", "val");

        error_context_t context_c{registered_code_, "msg"};
        context_c.with("key", "different");

        EXPECT_TRUE(context_a == context_b);
        EXPECT_FALSE(context_a == context_c);
    }


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


    TEST_F(error_context_test_t, get_payload_value_returns_existing_key) {
        error_context_t context{registered_code_, "test"};
        context.with("key1", "value1");
        auto value = context.get_payload_value("key1");
        ASSERT_TRUE(value.has_value());
        EXPECT_EQ(*value, "value1");
    }

    TEST_F(error_context_test_t, get_payload_value_returns_nullopt_for_missing_key) {
        error_context_t context{registered_code_, "test"};
        auto value = context.get_payload_value("nonexistent");
        EXPECT_FALSE(value.has_value());
    }


    TEST_F(error_context_test_t, with_const_char_key_works) {
        error_context_t context{registered_code_, "test"};
        context.with("c_string_key", 42);
        auto value = context.get_payload_value("c_string_key");
        ASSERT_TRUE(value.has_value());
        EXPECT_EQ(*value, "42");
    }


    TEST_F(error_context_test_t, from_exception_creates_context_with_what_message) {
        std::runtime_error exception("something went wrong");
        auto context = error_context_t::from_exception(registered_code_, exception);
        EXPECT_EQ(context.message, "something went wrong");
        EXPECT_EQ(context.get_code().get_code(), registered_code_.get_code());
    }


    TEST_F(error_context_test_t, payload_size_and_is_payload_empty_reflect_state) {
        error_context_t context{registered_code_, "size test"};
        // 初始状态：payload 为空
        EXPECT_TRUE(context.is_payload_empty());
        EXPECT_EQ(context.payload_size(), 0u);

        // 添加 1 项
        context.with("k1", "v1");
        EXPECT_FALSE(context.is_payload_empty());
        EXPECT_EQ(context.payload_size(), 1u);

        // 添加至 SSO 上限（4 项）
        context.with("k2", "v2").with("k3", "v3").with("k4", "v4");
        EXPECT_EQ(context.payload_size(), 4u);
        EXPECT_FALSE(context.is_payload_empty());

        // 第 5 项触发溢出到 unordered_map
        context.with("k5", "v5");
        EXPECT_EQ(context.payload_size(), 5u);
        EXPECT_FALSE(context.is_payload_empty());
    }


    TEST_F(error_context_test_t, for_each_payload_visits_sso_and_overflow_items) {
        error_context_t context{registered_code_, "for_each test"};
        context.with("a", "1")
           .with("b", "2")
           .with("c", "3")
           .with("d", "4")
           .with("e", "5");  // 触发溢出

        std::unordered_map<std::string, std::string> collected;
        context.for_each_payload([&](const std::string& payload_key, const std::string& payload_value) {
            collected[payload_key] = payload_value;
        });

        EXPECT_EQ(collected.size(), 5u);
        EXPECT_EQ(collected["a"], "1");
        EXPECT_EQ(collected["c"], "3");
        EXPECT_EQ(collected["e"], "5");
    }

    TEST_F(error_context_test_t, for_each_payload_on_empty_context_does_nothing) {
        error_context_t context{registered_code_, "empty"};
        size_t count = 0;
        context.for_each_payload([&](const std::string&, const std::string&) { ++count; });
        EXPECT_EQ(count, 0u);
    }


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


    TEST_F(error_context_test_t, from_binary_round_trip_preserves_code_and_message) {
        error_context_t original(registered_code_, "binary round trip");
        const std::string blob = error_context_serializer_t::to_binary(original);

        auto restored = error_context_serializer_t::from_binary(blob);
        ASSERT_TRUE(restored.has_value());
        EXPECT_EQ(restored->get_code().get_code(), original.get_code().get_code());
        EXPECT_EQ(restored->message, "binary round trip");
    }

    TEST_F(error_context_test_t, from_binary_round_trip_preserves_payload) {
        error_context_t original(registered_code_, "with payload");
        original.with("host", "127.0.0.1").with("port", "8080");
        const std::string blob = error_context_serializer_t::to_binary(original);

        auto restored = error_context_serializer_t::from_binary(blob);
        ASSERT_TRUE(restored.has_value());
        EXPECT_EQ(restored->payload_size(), 2u);
        EXPECT_EQ(restored->get_payload_value("host").value_or(""), "127.0.0.1");
        EXPECT_EQ(restored->get_payload_value("port").value_or(""), "8080");
    }

    TEST_F(error_context_test_t, from_binary_round_trip_preserves_cause_chain) {
        error_context_t cause{registered_code_, "root cause"};
        cause.with("reason", "timeout");
        error_context_t wrapper{registered_code_, "wrapper"};
        auto wrapped = wrapper.wrap(cause);
        const std::string blob = error_context_serializer_t::to_binary(wrapped);

        auto restored = error_context_serializer_t::from_binary(blob);
        ASSERT_TRUE(restored.has_value());
        ASSERT_NE(restored->cause, nullptr);
        EXPECT_EQ(restored->cause->message, "root cause");
        EXPECT_EQ(restored->cause->get_payload_value("reason").value_or(""), "timeout");
    }

    TEST_F(error_context_test_t, from_binary_invalid_magic_returns_nullopt) {
        std::string bad = error_context_serializer_t::to_binary(error_context_t{registered_code_, "x"});
        bad[0] = '\x00';  // 破坏魔数
        EXPECT_FALSE(error_context_serializer_t::from_binary(bad).has_value());
    }

    TEST_F(error_context_test_t, from_binary_truncated_data_returns_nullopt) {
        std::string blob = error_context_serializer_t::to_binary(error_context_t{registered_code_, "x"});
        blob.resize(blob.size() / 2);  // 截断
        EXPECT_FALSE(error_context_serializer_t::from_binary(blob).has_value());
    }

    TEST_F(error_context_test_t, from_binary_empty_input_returns_nullopt) {
        EXPECT_FALSE(error_context_serializer_t::from_binary("").has_value());
    }

    TEST_F(error_context_test_t, from_json_round_trip_preserves_code_and_message) {
        error_context_t original(registered_code_, "json round trip");
        const std::string json = error_context_serializer_t::to_json(original);

        auto restored = error_context_serializer_t::from_json(json);
        ASSERT_TRUE(restored.has_value());
        EXPECT_EQ(restored->get_code().get_code(), original.get_code().get_code());
        EXPECT_EQ(restored->message, "json round trip");
    }

    TEST_F(error_context_test_t, from_json_round_trip_preserves_payload) {
        error_context_t original(registered_code_, "json payload");
        original.with("user", "alice").with("count", "3");
        const std::string json = error_context_serializer_t::to_json(original);

        auto restored = error_context_serializer_t::from_json(json);
        ASSERT_TRUE(restored.has_value());
        EXPECT_EQ(restored->payload_size(), 2u);
        EXPECT_EQ(restored->get_payload_value("user").value_or(""), "alice");
        EXPECT_EQ(restored->get_payload_value("count").value_or(""), "3");
    }

    TEST_F(error_context_test_t, from_json_round_trip_preserves_cause_chain) {
        error_context_t cause{registered_code_, "root"};
        error_context_t wrapper{registered_code_, "outer"};
        auto wrapped = wrapper.wrap(cause);
        const std::string json = error_context_serializer_t::to_json(wrapped);

        auto restored = error_context_serializer_t::from_json(json);
        ASSERT_TRUE(restored.has_value());
        ASSERT_NE(restored->cause, nullptr);
        EXPECT_EQ(restored->cause->message, "root");
    }

    TEST_F(error_context_test_t, from_json_invalid_json_returns_nullopt) {
        EXPECT_FALSE(error_context_serializer_t::from_json("{not json").has_value());
        EXPECT_FALSE(error_context_serializer_t::from_json("").has_value());
        EXPECT_FALSE(error_context_serializer_t::from_json("[]").has_value());
    }

    TEST_F(error_context_test_t, from_json_skips_unknown_field_for_forward_compat) {
        // 未知字段应被跳过，不导致解析失败
        const std::string json =
            R"({"code":"12345","message":"ok","future_field":{"nested":42}})";
        auto restored = error_context_serializer_t::from_json(json);
        ASSERT_TRUE(restored.has_value());
        EXPECT_EQ(restored->message, "ok");
    }

    TEST_F(error_context_test_t, from_json_trailing_data_returns_nullopt) {
        error_context_t original(registered_code_, "trail");
        const std::string json = error_context_serializer_t::to_json(original);
        // 尾随非空白数据应拒绝
        EXPECT_FALSE(error_context_serializer_t::from_json(json + " garbage").has_value());
    }

    TEST_F(error_context_test_t, from_binary_to_json_cross_format_code_consistent) {
        // 二进制反序列化后再序列化为 JSON，code 应保持一致
        error_context_t original(registered_code_, "cross format");
        original.with("k", "v");
        const std::string blob = error_context_serializer_t::to_binary(original);

        auto restored = error_context_serializer_t::from_binary(blob);
        ASSERT_TRUE(restored.has_value());
        const std::string json = error_context_serializer_t::to_json(*restored);
        const auto from_json_again = error_context_serializer_t::from_json(json);
        ASSERT_TRUE(from_json_again.has_value());
        EXPECT_EQ(from_json_again->get_code().get_code(), original.get_code().get_code());
        EXPECT_EQ(from_json_again->message, "cross format");
    }

    TEST_F(error_context_test_t, to_string_uses_custom_formatter_when_set) {
        error_context_t context(registered_code_, "custom");
        config::formatter_config_t::set_custom_formatter(
            [](const error_context_t&) noexcept { return std::string{"CUSTOM_FORMATTER_OUTPUT"}; });
        EXPECT_EQ(error_context_serializer_t::to_string(context), "CUSTOM_FORMATTER_OUTPUT");
    }

    TEST_F(error_context_test_t, to_string_custom_formatter_throws_falls_back) {
        error_context_t context(registered_code_, "fallback");
        config::formatter_config_t::set_custom_formatter(
            [](const error_context_t&) -> std::string { throw std::runtime_error("boom"); });
        const auto result = error_context_serializer_t::to_string(context);
        EXPECT_NE(result.find("fallback"), std::string::npos);
    }

    TEST_F(error_context_test_t, to_binary_round_trip_with_location) {
        const std::string json =
            R"({"code":")" + std::to_string(registered_code_.get_code()) +
            R"(","message":"loc","location":{"file":"bin_file.cpp","function":"bin_fn","line":42}})";
        auto with_loc = error_context_serializer_t::from_json(json);
        ASSERT_TRUE(with_loc.has_value());
        ASSERT_NE(with_loc->file_name, nullptr);
        const std::string blob = error_context_serializer_t::to_binary(*with_loc);

        auto restored = error_context_serializer_t::from_binary(blob);
        ASSERT_TRUE(restored.has_value());
        ASSERT_NE(restored->file_name, nullptr);
        EXPECT_EQ(std::string(restored->file_name), "bin_file.cpp");
        EXPECT_EQ(restored->source_location.line(), 42u);
    }

    TEST_F(error_context_test_t, to_json_round_trip_with_location) {
        const std::string json =
            R"({"code":")" + std::to_string(registered_code_.get_code()) +
            R"(","message":"loc","location":{"file":"src.cpp","function":"func","line":100}})";
        auto restored = error_context_serializer_t::from_json(json);
        ASSERT_TRUE(restored.has_value());
        ASSERT_NE(restored->file_name, nullptr);
        EXPECT_EQ(std::string(restored->file_name), "src.cpp");
        EXPECT_EQ(restored->source_location.line(), 100u);
    }

    TEST_F(error_context_test_t, from_json_location_field_with_unknown_member_skipped) {
        const std::string json =
            R"({"code":")" + std::to_string(registered_code_.get_code()) +
            R"(","message":"loc","location":{"file":"f.cpp","function":"fn","line":7,"extra":42}})";
        auto restored = error_context_serializer_t::from_json(json);
        ASSERT_TRUE(restored.has_value());
        ASSERT_NE(restored->file_name, nullptr);
        EXPECT_EQ(std::string(restored->file_name), "f.cpp");
        EXPECT_EQ(restored->source_location.line(), 7u);
    }

    TEST_F(error_context_test_t, from_json_location_field_partial_not_applied) {
        const std::string json =
            R"({"code":")" + std::to_string(registered_code_.get_code()) +
            R"(","message":"partial","location":{"file":"only_file.cpp"}})";
        auto restored = error_context_serializer_t::from_json(json);
        ASSERT_TRUE(restored.has_value());
        EXPECT_EQ(restored->file_name, nullptr);
    }

    TEST_F(error_context_test_t, from_json_empty_object_returns_default_context) {
        auto restored = error_context_serializer_t::from_json("{}");
        ASSERT_TRUE(restored.has_value());
        EXPECT_EQ(restored->message, "");
    }

    TEST_F(error_context_test_t, from_json_unknown_array_field_skipped) {
        const std::string json =
            R"({"code":")" + std::to_string(registered_code_.get_code()) +
            R"(","message":"arr","unknown":[1,2,3,"nested",{"k":"v"}]})";
        auto restored = error_context_serializer_t::from_json(json);
        ASSERT_TRUE(restored.has_value());
        EXPECT_EQ(restored->message, "arr");
    }

    TEST_F(error_context_test_t, from_json_unknown_string_field_skipped) {
        const std::string json =
            R"({"code":")" + std::to_string(registered_code_.get_code()) +
            R"(","message":"str","future":"some_value"})";
        auto restored = error_context_serializer_t::from_json(json);
        ASSERT_TRUE(restored.has_value());
        EXPECT_EQ(restored->message, "str");
    }

    TEST_F(error_context_test_t, from_json_unknown_number_bool_null_skipped) {
        const std::string json =
            R"({"code":")" + std::to_string(registered_code_.get_code()) +
            R"(","message":"mixed","n":123,"b":true,"f":false,"z":null})";
        auto restored = error_context_serializer_t::from_json(json);
        ASSERT_TRUE(restored.has_value());
        EXPECT_EQ(restored->message, "mixed");
    }

    TEST_F(error_context_test_t, from_json_message_with_escape_sequences) {
        const std::string json =
            R"({"code":")" + std::to_string(registered_code_.get_code()) +
            R"(","message":"a\"b\\c\nd\te\u4e2df"})";
        auto restored = error_context_serializer_t::from_json(json);
        ASSERT_TRUE(restored.has_value());
        EXPECT_EQ(restored->message, "a\"b\\c\nd\te中f");
    }

    TEST_F(error_context_test_t, from_json_invalid_escape_returns_nullopt) {
        const std::string json =
            R"({"code":")" + std::to_string(registered_code_.get_code()) +
            R"(","message":"bad\vescape"})";
        EXPECT_FALSE(error_context_serializer_t::from_json(json).has_value());
    }

    TEST_F(error_context_test_t, from_json_non_numeric_code_returns_nullopt) {
        const std::string json = R"({"code":"abc","message":"bad"})";
        EXPECT_FALSE(error_context_serializer_t::from_json(json).has_value());
    }

    TEST_F(error_context_test_t, from_json_empty_payload_object) {
        const std::string json =
            R"({"code":")" + std::to_string(registered_code_.get_code()) +
            R"(","message":"empty","payload":{}})";
        auto restored = error_context_serializer_t::from_json(json);
        ASSERT_TRUE(restored.has_value());
        EXPECT_EQ(restored->payload_size(), 0u);
    }

    TEST_F(error_context_test_t, from_binary_wrong_version_returns_nullopt) {
        error_context_t context(registered_code_, "version");
        std::string blob = error_context_serializer_t::to_binary(context);
        ASSERT_GE(blob.size(), 5u);
        blob[4] = static_cast<char>(0xFF);
        EXPECT_FALSE(error_context_serializer_t::from_binary(blob).has_value());
    }

    TEST_F(error_context_test_t, from_binary_trailing_data_returns_nullopt) {
        error_context_t context(registered_code_, "trail");
        std::string blob = error_context_serializer_t::to_binary(context);
        blob.push_back('\x00');
        EXPECT_FALSE(error_context_serializer_t::from_binary(blob).has_value());
    }

    TEST_F(error_context_test_t, deep_cause_chain_at_max_depth_32) {
        error_context_t leaf(registered_code_, "leaf");
        error_context_t chain = leaf;
        for (int i = 0; i < 33; ++i) {
            error_context_t wrapper(registered_code_, "layer");
            chain = wrapper.wrap(chain);
        }
        const std::string json = error_context_serializer_t::to_json(chain);
        EXPECT_TRUE(error_context_serializer_t::from_json(json).has_value());
        const std::string blob = error_context_serializer_t::to_binary(chain);
        EXPECT_TRUE(error_context_serializer_t::from_binary(blob).has_value());
    }

    TEST_F(error_context_test_t, to_string_includes_stacktrace_section_when_enabled) {
#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
        config::feature_flags_t::set_enable_stacktrace(true);
        error_context_t context(registered_code_, "stack");
        context.stack_frames = {"frame_a", "frame_b"};
        const std::string text = error_context_serializer_t::to_string(context);
        EXPECT_NE(text.find("[Stacktrace]"), std::string::npos);
        EXPECT_NE(text.find("frame_a"), std::string::npos);
#else
        GTEST_SKIP() << "stacktrace disabled at compile time";
#endif
    }

    TEST_F(error_context_test_t, to_json_includes_stack_frames_when_enabled) {
#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
        config::feature_flags_t::set_enable_stacktrace(true);
        error_context_t context(registered_code_, "json stack");
        context.stack_frames = {"s1", "s2"};
        const std::string json = error_context_serializer_t::to_json(context);
        EXPECT_NE(json.find("\"stack_frames\""), std::string::npos);
        EXPECT_NE(json.find("s1"), std::string::npos);
#else
        GTEST_SKIP() << "stacktrace disabled at compile time";
#endif
    }

    TEST_F(error_context_test_t, from_json_stack_frames_round_trip) {
#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE
        config::feature_flags_t::set_enable_stacktrace(true);
        error_context_t context(registered_code_, "rt stack");
        context.stack_frames = {"alpha", "beta"};
        const std::string json = error_context_serializer_t::to_json(context);
        auto restored = error_context_serializer_t::from_json(json);
        ASSERT_TRUE(restored.has_value());
        ASSERT_EQ(restored->stack_frames.size(), 2u);
        EXPECT_EQ(restored->stack_frames[0], "alpha");
        EXPECT_EQ(restored->stack_frames[1], "beta");
#else
        GTEST_SKIP() << "stacktrace disabled at compile time";
#endif
    }

}  // namespace error_system::core