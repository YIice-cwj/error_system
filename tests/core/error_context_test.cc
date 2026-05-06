#include "error_system/core/error_builder.h"
#include "error_system/core/error_code.h"
#include "error_system/core/error_config.h"
#include "error_system/core/error_context.h"
#include "error_system/core/error_level.h"
#include "error_system/core/error_registry.h"
#include "error_system/domain/system_domain.h"
#include "error_system/i18n/json_translator.h"
#include "error_system/i18n/translator_registry.h"
#include "error_system/plugin/plugin_registry.h"

#include <cstring>
#include <gtest/gtest.h>

namespace error_system::core {

    class error_context_test : public ::testing::Test {
        protected:
        void SetUp() override {
            error_config_t::set_enable_validation(false);
            plugin::plugin_registry_t::instance().clear();
        }

        void TearDown() override { plugin::plugin_registry_t::instance().clear(); }
    };

    // ─── 构造与基本属性 ───

    TEST_F(error_context_test, default_construct_is_invalid) {
        error_context_t ctx;
        EXPECT_FALSE(static_cast<bool>(ctx));
        EXPECT_EQ(ctx.code.get_code(), 0);
        EXPECT_TRUE(ctx.message.empty());
        EXPECT_EQ(ctx.cause, nullptr);
        EXPECT_TRUE(ctx.payload.empty());
        EXPECT_TRUE(ctx.stack_frames.empty());
    }

    TEST_F(error_context_test, construct_with_code_and_message) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 1, 2, 100);
        error_context_t ctx(code, "test message");

        EXPECT_TRUE(static_cast<bool>(ctx));
        EXPECT_EQ(ctx.code.get_level(), error_level_t::error);
        EXPECT_EQ(ctx.code.get_system(), domain::system_domain_t::system);
        EXPECT_EQ(ctx.code.get_subsys(), 1);
        EXPECT_EQ(ctx.code.get_module(), 2);
        EXPECT_EQ(ctx.code.get_number(), 100);
        EXPECT_EQ(ctx.message, "test message");
    }

    TEST_F(error_context_test, construct_with_empty_message) {
        auto code = error_builder_t::make_error_code(error_level_t::warn, domain::system_domain_t::database, 0, 0, 0);
        error_context_t ctx(code);

        EXPECT_TRUE(static_cast<bool>(ctx));
        EXPECT_TRUE(ctx.message.empty());
    }

    TEST_F(error_context_test, construct_with_zero_code_is_invalid) {
        error_context_t ctx(error_code_t{0}, "zero code");
        EXPECT_FALSE(static_cast<bool>(ctx));
    }

    TEST_F(error_context_test, construct_with_format_args) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "code={}, msg={}", 404, "not found");

        EXPECT_EQ(ctx.message, "code=404, msg=not found");
    }

    // ─── wrap / cause chain ───

    TEST_F(error_context_test, wrap_creates_cause_chain) {
        auto root_code =
            error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::kernel, 0, 0, 1);
        auto wrap_code =
            error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::middleware, 0, 0, 2);

        error_context_t root_ctx(root_code, "root cause");
        error_context_t wrap_ctx(wrap_code, "wrapped error");

        error_context_t chained = wrap_ctx.wrap(root_ctx);

        EXPECT_TRUE(static_cast<bool>(chained));
        EXPECT_EQ(chained.message, "wrapped error");
        EXPECT_NE(chained.cause, nullptr);
        EXPECT_EQ(chained.cause->message, "root cause");
        EXPECT_EQ(chained.cause->code.get_system(), domain::system_domain_t::kernel);
    }

    TEST_F(error_context_test, wrap_preserves_original_unchanged) {
        auto code1 = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        auto code2 = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::network, 0, 0, 2);

        error_context_t original(code1, "original");
        error_context_t wrapper(code2, "wrapper");

        error_context_t result = wrapper.wrap(original);

        EXPECT_EQ(original.cause, nullptr);
        EXPECT_EQ(original.message, "original");
    }

    TEST_F(error_context_test, wrap_moves_underlying) {
        auto code1 = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        auto code2 = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::network, 0, 0, 2);

        error_context_t original(code1, "original");
        original.with("key", "value");

        error_context_t wrapper(code2, "wrapper");
        error_context_t result = wrapper.wrap(std::move(original));

        EXPECT_NE(result.cause, nullptr);
        EXPECT_EQ(result.cause->message, "original");
        EXPECT_EQ(result.cause->payload.at("key"), "value");
    }

    TEST_F(error_context_test, payload_preserved_in_cause_chain) {
        auto code1 = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        auto code2 = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::network, 0, 0, 2);

        error_context_t root(code1, "root");
        root.with("db_host", "192.168.1.1");

        error_context_t outer(code2, "outer");
        outer.with("api_endpoint", "/v1/users");

        error_context_t chained = outer.wrap(root);

        EXPECT_EQ(chained.payload.at("api_endpoint"), "/v1/users");
        ASSERT_NE(chained.cause, nullptr);
        EXPECT_EQ(chained.cause->payload.at("db_host"), "192.168.1.1");
    }

    // ─── with / payload ───

    TEST_F(error_context_test, with_adds_payload_fields) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "test");

        ctx.with("host", "127.0.0.1").with("port", "8080");

        EXPECT_EQ(ctx.payload.size(), 2);
        EXPECT_EQ(ctx.payload.at("host"), "127.0.0.1");
        EXPECT_EQ(ctx.payload.at("port"), "8080");
    }

    TEST_F(error_context_test, with_overwrites_existing_key) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "test");

        ctx.with("key", "old");
        ctx.with("key", "new");

        EXPECT_EQ(ctx.payload.at("key"), "new");
    }

    TEST_F(error_context_test, with_rvalue_strings) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "test");

        std::string k = "key";
        std::string v = "value";
        ctx.with(std::move(k), std::move(v));

        EXPECT_EQ(ctx.payload.at("key"), "value");
    }

    TEST_F(error_context_test, default_construct_has_empty_payload) {
        error_context_t ctx;
        EXPECT_TRUE(ctx.payload.empty());
    }

    // ─── to_string ───

    TEST_F(error_context_test, to_string_basic) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "basic message");

        std::string str = ctx.to_string();
        EXPECT_NE(str.find("basic message"), std::string::npos);
    }

    TEST_F(error_context_test, to_string_includes_payload) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "message");
        ctx.with("a", "1").with("b", "2");

        std::string str = ctx.to_string();
        EXPECT_NE(str.find("a=1"), std::string::npos);
        EXPECT_NE(str.find("b=2"), std::string::npos);
        EXPECT_NE(str.find("{"), std::string::npos);
        EXPECT_NE(str.find("}"), std::string::npos);
    }

    TEST_F(error_context_test, to_string_without_payload_no_braces) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "no payload");

        std::string str = ctx.to_string();
        EXPECT_EQ(str.find("{"), std::string::npos);
    }

    TEST_F(error_context_test, to_string_with_cause_without_translator) {
        auto code1 = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        auto code2 =
            error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::middleware, 0, 0, 2);

        error_context_t root(code1, "root");
        error_context_t ctx(code2, "main");
        ctx = ctx.wrap(root);

        std::string str = ctx.to_string();
        EXPECT_NE(str.find("main"), std::string::npos);
        EXPECT_NE(str.find("root"), std::string::npos);
        EXPECT_NE(str.find("Caused by"), std::string::npos);
    }

    TEST_F(error_context_test, to_string_deep_cause_chain) {
        auto code1 = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        auto code2 = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::kernel, 0, 0, 2);
        auto code3 =
            error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::middleware, 0, 0, 3);

        error_context_t level1(code1, "level1");
        error_context_t level2(code2, "level2");
        error_context_t level3(code3, "level3");

        level2 = level2.wrap(level1);
        level3 = level3.wrap(level2);

        std::string str = level3.to_string();
        EXPECT_NE(str.find("level3"), std::string::npos);
        EXPECT_NE(str.find("level2"), std::string::npos);
        EXPECT_NE(str.find("level1"), std::string::npos);
    }

    TEST_F(error_context_test, to_string_with_translator) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 0);
        error_context_t ctx(code, "translated error");

        i18n::json_translator_t translator(i18n::language_t::zh_cn);
        std::string str = ctx.to_string(&translator);

        EXPECT_NE(str.find("translated error"), std::string::npos);
    }

    TEST_F(error_context_test, to_string_uses_registry_translator) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 0);
        error_context_t ctx(code, "registry error");

        i18n::json_translator_t translator(i18n::language_t::en_us);
        i18n::translator_registry_t::instance().set(&translator);

        std::string str = ctx.to_string();
        EXPECT_NE(str.find("registry error"), std::string::npos);

        i18n::translator_registry_t::instance().set(nullptr);
    }

    TEST_F(error_context_test, to_string_with_stack_frames) {
        auto code = error_builder_t::make_error_code(error_level_t::fatal, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "stack test");
        ctx.stack_frames = {"frame1", "frame2"};

        std::string str = ctx.to_string();
        EXPECT_NE(str.find("[Stacktrace]"), std::string::npos);
        EXPECT_NE(str.find("frame1"), std::string::npos);
        EXPECT_NE(str.find("frame2"), std::string::npos);
    }

    // ─── to_json ───

    TEST_F(error_context_test, to_json_basic) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 42);
        error_context_t ctx(code, "json test");

        std::string json = ctx.to_json();
        EXPECT_NE(json.find("\"code\":"), std::string::npos);
        EXPECT_NE(json.find("\"message\":\"json test\""), std::string::npos);
    }

    TEST_F(error_context_test, to_json_with_payload) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "msg");
        ctx.with("k1", "v1").with("k2", "v2");

        std::string json = ctx.to_json();
        EXPECT_NE(json.find("\"payload\""), std::string::npos);
        EXPECT_NE(json.find("\"k1\":\"v1\""), std::string::npos);
        EXPECT_NE(json.find("\"k2\":\"v2\""), std::string::npos);
    }

    TEST_F(error_context_test, to_json_with_stack_frames) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "msg");
        ctx.stack_frames = {"f1", "f2"};

        std::string json = ctx.to_json();
        EXPECT_NE(json.find("\"stack_frames\""), std::string::npos);
        EXPECT_NE(json.find("\"f1\""), std::string::npos);
        EXPECT_NE(json.find("\"f2\""), std::string::npos);
    }

    TEST_F(error_context_test, to_json_with_cause) {
        auto code1 = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        auto code2 = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::kernel, 0, 0, 2);

        error_context_t root(code1, "root");
        error_context_t outer(code2, "outer");
        outer = outer.wrap(root);

        std::string json = outer.to_json();
        EXPECT_NE(json.find("\"cause\""), std::string::npos);
        EXPECT_NE(json.find("\"root\""), std::string::npos);
    }

    TEST_F(error_context_test, to_json_escapes_special_chars) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "quote\"newline\nbackslash\\");

        std::string json = ctx.to_json();
        EXPECT_NE(json.find("\\\""), std::string::npos);
        EXPECT_NE(json.find("\\n"), std::string::npos);
        EXPECT_NE(json.find("\\\\"), std::string::npos);
    }

    // ─── to_binary ───

    TEST_F(error_context_test, to_binary_basic) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 42);
        error_context_t ctx(code, "binary");

        std::string bin = ctx.to_binary();
        EXPECT_GE(bin.size(), sizeof(uint64_t));

        uint64_t raw_code = 0;
        std::memcpy(&raw_code, bin.data(), sizeof(raw_code));
        EXPECT_EQ(raw_code, code.get_code());
    }

    TEST_F(error_context_test, to_binary_with_payload) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "msg");
        ctx.with("k", "v");

        std::string bin = ctx.to_binary();
        EXPECT_GE(bin.size(), sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint32_t) + 1 + sizeof(uint32_t) + 1);
    }

    TEST_F(error_context_test, to_binary_empty_message) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "");

        std::string bin = ctx.to_binary();
        uint64_t raw_code = 0;
        std::memcpy(&raw_code, bin.data(), sizeof(raw_code));
        EXPECT_EQ(raw_code, code.get_code());

        uint32_t msg_len = 0;
        std::memcpy(&msg_len, bin.data() + sizeof(uint64_t), sizeof(msg_len));
        EXPECT_EQ(msg_len, 0u);
    }

    // ─── 插件通知 ───

    class counting_plugin_t : public plugin::i_error_plugin_t {
        private:
        std::string name_;
        std::atomic<size_t> count_{0};

        public:
        explicit counting_plugin_t(std::string name) : name_(std::move(name)) {}

        std::string_view name() const noexcept override { return name_; }

        void on_error(const error_context_t&) noexcept override { ++count_; }

        size_t count() const noexcept { return count_.load(); }
    };

    TEST_F(error_context_test, construct_notifies_plugins) {
        counting_plugin_t plugin("test");
        plugin::plugin_registry_t::instance().register_plugin(&plugin);

        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "plugin test");

        EXPECT_EQ(plugin.count(), 1);
    }

    TEST_F(error_context_test, zero_code_does_not_notify_plugins) {
        counting_plugin_t plugin("test");
        plugin::plugin_registry_t::instance().register_plugin(&plugin);

        error_context_t ctx(error_code_t{0}, "no notify");

        EXPECT_EQ(plugin.count(), 0);
    }

    // ─── 错误码验证 ───

    TEST_F(error_context_test, validation_replaces_unregistered_code) {
        error_config_t::set_enable_validation(true);
        error_registry_t::instance().unregister_all();

        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 99, 99, 1);
        error_context_t ctx(code, "unregistered");

        EXPECT_EQ(ctx.code.get_level(), error_level_t::fatal);
        EXPECT_EQ(ctx.code.get_system(), domain::system_domain_t::none);
        EXPECT_EQ(ctx.code.get_number(), 0xFFFF);
        EXPECT_NE(ctx.payload.find("illegal_raw_code"), ctx.payload.end());
        EXPECT_NE(ctx.message.find("[UNREGISTERED CODE]"), std::string::npos);

        error_config_t::set_enable_validation(false);
    }

    TEST_F(error_context_test, validation_preserves_registered_code) {
        error_config_t::set_enable_validation(true);
        error_registry_t::instance().unregister_all();

        auto code = error_builder_t::make_error_code(error_level_t::warn, domain::system_domain_t::database, 0, 0, 7);
        error_registry_t::instance().register_error(code, "TEST_WARN", "test warning");

        error_context_t ctx(code, "registered");

        EXPECT_EQ(ctx.code.get_level(), error_level_t::warn);
        EXPECT_EQ(ctx.code.get_system(), domain::system_domain_t::database);
        EXPECT_EQ(ctx.code.get_number(), 7);
        EXPECT_EQ(ctx.payload.find("illegal_raw_code"), ctx.payload.end());

        error_registry_t::instance().unregister_all();
        error_config_t::set_enable_validation(false);
    }

    // ─── 堆栈追踪 ───

    TEST_F(error_context_test, stacktrace_captured_for_high_level) {
        error_config_t::set_enable_stacktrace(true);
        error_config_t::set_stacktrace_level(error_level_t::error);

        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "stack test");

        if (error_config_t::is_stacktrace_enabled()) {
            EXPECT_FALSE(ctx.stack_frames.empty());
        }
    }

    TEST_F(error_context_test, stacktrace_not_captured_for_low_level) {
        error_config_t::set_enable_stacktrace(true);
        error_config_t::set_stacktrace_level(error_level_t::error);

        auto code = error_builder_t::make_error_code(error_level_t::info, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "no stack");

        EXPECT_TRUE(ctx.stack_frames.empty());
    }

}  // namespace error_system::core
